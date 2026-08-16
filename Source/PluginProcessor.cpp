#include "PluginProcessor.h"

#include "Core/AudioMath.h"
#include "Core/Branding.h"
#include "Core/RealtimeGuards.h"
#include "Parameters/ParameterIDs.h"
#include "PluginEditor.h"

namespace strata
{

namespace
{
    /** Output gain ramp length. Long enough to be inaudible, short enough to feel immediate. */
    constexpr double outputGainRampSeconds = 0.02;

    /** Below this dB value the output parameter means true silence. Matches the parameter's minimum. */
    constexpr float outputGainMinusInfinityDb = -60.0f;
} // namespace

//==============================================================================
juce::AudioProcessor::BusesProperties StrataProcessor::makeBusesProperties()
{
    // Instrument: no inputs. Stereo is the default output; mono is accepted so
    // the plugin remains usable on mono instrument tracks.
    return BusesProperties()
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true);
}

//==============================================================================
StrataProcessor::StrataProcessor()
    : juce::AudioProcessor (makeBusesProperties()),
      valueTreeState (*this, nullptr, params::stateTreeType, params::createParameterLayout())
{
    outputGainDbParam = valueTreeState.getRawParameterValue (params::output::gain);
    polyphonyParam    = valueTreeState.getRawParameterValue (params::voice::polyphony);

    // A missing pointer means an ID in ParameterIDs.h and ParameterLayout.cpp
    // have drifted apart. That is a programming error, not a runtime condition.
    jassert (outputGainDbParam != nullptr);
    jassert (polyphonyParam != nullptr);
}

StrataProcessor::~StrataProcessor() = default;

const juce::String StrataProcessor::getName() const
{
    return branding::productName;
}

double StrataProcessor::getTailLengthSeconds() const
{
    // Phase 1 renders silence and has no tail. Once the resonator and reverb
    // exist this must report the longest decay the current settings can produce,
    // otherwise hosts truncate release tails when rendering offline.
    return 0.0;
}

int StrataProcessor::getSelectedPolyphony() const noexcept
{
    if (polyphonyParam == nullptr)
        return params::polyphonyOptions[params::defaultPolyphonyIndex];

    const auto raw = static_cast<int> (std::lround (polyphonyParam->load (std::memory_order_relaxed)));
    const auto index = std::clamp (raw, 0, params::numPolyphonyOptions - 1);

    return params::polyphonyOptions[index];
}

//==============================================================================
void StrataProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = maximumExpectedSamplesPerBlock;

    // Sized with headroom because some hosts occasionally deliver a block larger
    // than the maximum they promised; processBlock() must never reallocate.
    outputGainRamp.assign (static_cast<std::size_t> (std::max (1, maximumExpectedSamplesPerBlock)) * 2, 0.0);

    outputGain.reset (sampleRate, outputGainRampSeconds);

    if (outputGainDbParam != nullptr)
    {
        const auto initialGain = math::decibelsToGain (static_cast<double> (outputGainDbParam->load (std::memory_order_relaxed)),
                                                       static_cast<double> (outputGainMinusInfinityDb));
        outputGain.setCurrentAndTargetValue (initialGain);
    }

    outputPeakLevel.store (0.0f, std::memory_order_relaxed);
    midiEventCount.store (0, std::memory_order_relaxed);
    blockSizeOverruns.store (0, std::memory_order_relaxed);
}

void StrataProcessor::releaseResources()
{
    // Nothing is held that must be freed between playback sessions; buffers are
    // sized in prepareToPlay() and reused.
}

bool StrataProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannels() != 0)
        return false;

    const auto& output = layouts.getMainOutputChannelSet();

    return output == juce::AudioChannelSet::mono()
        || output == juce::AudioChannelSet::stereo();
}

//==============================================================================
void StrataProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    processBlockInternal (buffer, midiMessages);
}

void StrataProcessor::processBlock (juce::AudioBuffer<double>& buffer, juce::MidiBuffer& midiMessages)
{
    processBlockInternal (buffer, midiMessages);
}

template <typename FloatType>
void StrataProcessor::processBlockInternal (juce::AudioBuffer<FloatType>& buffer, juce::MidiBuffer& midiMessages)
{
    const juce::ScopedNoDenormals noDenormals;

    // Instruments must clear anything the host left in the buffer, including
    // channels beyond the ones the engine writes to.
    buffer.clear();

    renderEngine (buffer, midiMessages);
    applyOutputStage (buffer);
}

template <typename FloatType>
void StrataProcessor::renderEngine ([[maybe_unused]] juce::AudioBuffer<FloatType>& buffer,
                                    juce::MidiBuffer& midiMessages)
{
    // Phase 1: account for incoming MIDI so the editor can show that events are
    // arriving, but generate no audio. Phase 2 replaces this with voice dispatch.
    const auto seen = midiEventCount.load (std::memory_order_relaxed);
    midiEventCount.store (seen + midiMessages.getNumEvents(), std::memory_order_relaxed);
}

template <typename FloatType>
void StrataProcessor::applyOutputStage (juce::AudioBuffer<FloatType>& buffer)
{
    const auto numSamples  = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numSamples <= 0 || numChannels <= 0)
        return;

    if (outputGainDbParam != nullptr)
        outputGain.setTargetValue (math::decibelsToGain (static_cast<double> (outputGainDbParam->load (std::memory_order_relaxed)),
                                                         static_cast<double> (outputGainMinusInfinityDb)));

    const auto rampSamples = static_cast<std::size_t> (numSamples);

    if (rampSamples <= outputGainRamp.size())
    {
        // The smoother advances once per sample, so the ramp is materialised once
        // and then applied to every channel. Doing it per channel instead would
        // desynchronise the channels.
        for (std::size_t i = 0; i < rampSamples; ++i)
            outputGainRamp[i] = outputGain.getNextValue();

        for (int channel = 0; channel < numChannels; ++channel)
        {
            auto* samples = buffer.getWritePointer (channel);

            for (std::size_t i = 0; i < rampSamples; ++i)
                samples[i] *= static_cast<FloatType> (outputGainRamp[i]);
        }
    }
    else
    {
        // The host exceeded the block size it promised in prepareToPlay(). Skip the
        // ramp rather than allocate: a single block at the target gain is far less
        // harmful than a malloc on the audio thread. The occurrence is counted so it
        // is visible to diagnostics and to tests - deliberately not asserted, since
        // jassertfalse would log from the audio thread.
        blockSizeOverruns.fetch_add (1, std::memory_order_relaxed);

        const auto target = outputGain.getTargetValue();
        outputGain.setCurrentAndTargetValue (target);

        for (int channel = 0; channel < numChannels; ++channel)
            buffer.applyGain (channel, 0, numSamples, static_cast<FloatType> (target));
    }

    auto peak = 0.0f;

    for (int channel = 0; channel < numChannels; ++channel)
    {
        auto* samples = buffer.getWritePointer (channel);

        // Last line of defence: a non-finite sample must never reach the host,
        // where it would poison downstream plugins and metering.
        guards::sanitiseBlock (samples, static_cast<std::size_t> (numSamples));
        peak = std::max (peak,
                         static_cast<float> (guards::peakMagnitude (samples, static_cast<std::size_t> (numSamples))));
    }

    outputPeakLevel.store (peak, std::memory_order_relaxed);
}

//==============================================================================
juce::AudioProcessorEditor* StrataProcessor::createEditor()
{
    return new StrataEditor (*this);
}

//==============================================================================
void StrataProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Called on the message thread. Copying the tree first keeps the snapshot
    // consistent if a parameter changes while we serialise.
    const auto state = valueTreeState.copyState();

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void StrataProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr)
        return;

    // Reject state belonging to another plugin, and state whose root node we do
    // not recognise, rather than half-applying it.
    if (! xml->hasTagName (valueTreeState.state.getType()))
        return;

    valueTreeState.replaceState (juce::ValueTree::fromXml (*xml));
}

} // namespace strata

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new strata::StrataProcessor();
}
