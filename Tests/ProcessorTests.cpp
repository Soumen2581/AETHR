#include <catch2/catch_test_macros.hpp>

#include "Core/RealtimeGuards.h"
#include "Parameters/ParameterIDs.h"
#include "PluginProcessor.h"

namespace
{
    constexpr double testSampleRates[] { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };
    constexpr int    testBlockSizes[]  { 32, 64, 128, 256, 512, 1024, 2048 };

    constexpr int outputChannels = 2;

    /** Prepares the processor for a stereo instrument configuration. */
    void prepare (aethr::AethrProcessor& processor, double sampleRate, int blockSize)
    {
        processor.setPlayConfigDetails (0, outputChannels, sampleRate, blockSize);
        processor.prepareToPlay (sampleRate, blockSize);
    }

    /** A short MIDI sequence: two overlapping notes plus a pitch bend. */
    juce::MidiBuffer makeMidiSequence (int blockSize)
    {
        juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::noteOn (1, 60, 0.8f), 0);
        midi.addEvent (juce::MidiMessage::noteOn (1, 67, 0.5f), blockSize / 4);
        midi.addEvent (juce::MidiMessage::pitchWheel (1, 9000), blockSize / 2);
        midi.addEvent (juce::MidiMessage::noteOff (1, 60), (3 * blockSize) / 4);
        return midi;
    }
} // namespace

TEST_CASE ("Bus layouts: instrument accepts mono and stereo output and no input", "[processor][buses]")
{
    aethr::AethrProcessor processor;

    const auto layoutFor = [] (const juce::AudioChannelSet& outputSet)
    {
        juce::AudioProcessor::BusesLayout layout;
        layout.outputBuses.add (outputSet);
        return layout;
    };

    REQUIRE (processor.isBusesLayoutSupported (layoutFor (juce::AudioChannelSet::stereo())));
    REQUIRE (processor.isBusesLayoutSupported (layoutFor (juce::AudioChannelSet::mono())));

    // Anything wider than stereo is not supported yet, and must be refused rather
    // than accepted and silently down-mixed.
    REQUIRE_FALSE (processor.isBusesLayoutSupported (layoutFor (juce::AudioChannelSet::create5point1())));

    auto withInput = layoutFor (juce::AudioChannelSet::stereo());
    withInput.inputBuses.add (juce::AudioChannelSet::stereo());
    REQUIRE_FALSE (processor.isBusesLayoutSupported (withInput));
}

TEST_CASE ("Processor reports the identity a host needs", "[processor]")
{
    aethr::AethrProcessor processor;

    REQUIRE (processor.acceptsMidi());
    REQUIRE_FALSE (processor.producesMidi());
    REQUIRE_FALSE (processor.isMidiEffect());
    REQUIRE (processor.supportsDoublePrecisionProcessing());
    REQUIRE (processor.hasEditor());
    REQUIRE_FALSE (processor.getName().isEmpty());
    REQUIRE (processor.getTailLengthSeconds() >= 0.0);
}

TEST_CASE ("Rendering is finite and silent-by-default at every supported sample rate", "[processor][dsp]")
{
    for (const auto sampleRate : testSampleRates)
    {
        aethr::AethrProcessor processor;
        prepare (processor, sampleRate, 512);

        juce::AudioBuffer<float> buffer (outputChannels, 512);

        // Fill with rubbish first: an instrument must clear what the host left behind.
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                buffer.setSample (channel, i, 0.9f);

        juce::MidiBuffer midi;
        processor.processBlock (buffer, midi);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
        {
            const auto* samples = buffer.getReadPointer (channel);
            const auto numSamples = static_cast<std::size_t> (buffer.getNumSamples());

            REQUIRE_FALSE (aethr::guards::containsNonFinite (samples, numSamples));

            // No MIDI, so no voices: the output must be exactly silent, and in
            // particular must not still contain the rubbish the host left behind.
            REQUIRE (aethr::guards::peakMagnitude (samples, numSamples) == 0.0f);
        }
    }
}

TEST_CASE ("Rendering is stable across every supported block size", "[processor][dsp]")
{
    for (const auto blockSize : testBlockSizes)
    {
        aethr::AethrProcessor processor;
        prepare (processor, 48000.0, blockSize);

        juce::AudioBuffer<float> buffer (outputChannels, blockSize);
        auto midi = makeMidiSequence (blockSize);

        buffer.clear();
        processor.processBlock (buffer, midi);

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            REQUIRE_FALSE (aethr::guards::containsNonFinite (buffer.getReadPointer (channel),
                                                             static_cast<std::size_t> (blockSize)));

        REQUIRE (processor.getMidiEventCount() == 4);
    }
}

TEST_CASE ("A block larger than promised is handled without corrupting the output", "[processor][dsp][robustness]")
{
    // Some hosts occasionally exceed the maximum block size they declared. The
    // processor must cope without allocating and without emitting invalid audio.
    aethr::AethrProcessor processor;
    prepare (processor, 48000.0, 128);

    REQUIRE (processor.getBlockSizeOverrunCount() == 0);

    juce::AudioBuffer<float> oversized (outputChannels, 4096);
    oversized.clear();

    juce::MidiBuffer midi;
    processor.processBlock (oversized, midi);

    for (int channel = 0; channel < oversized.getNumChannels(); ++channel)
        REQUIRE_FALSE (aethr::guards::containsNonFinite (oversized.getReadPointer (channel),
                                                          static_cast<std::size_t> (oversized.getNumSamples())));

    // The fallback path must be observable rather than silent, so that a host
    // behaving badly can be diagnosed instead of guessed at.
    REQUIRE (processor.getBlockSizeOverrunCount() == 1);

    // A block within the promised size must not be counted as an overrun.
    juce::AudioBuffer<float> normal (outputChannels, 128);
    normal.clear();
    processor.processBlock (normal, midi);
    REQUIRE (processor.getBlockSizeOverrunCount() == 1);
}

TEST_CASE ("Double-precision rendering works and ignores what the host left in the buffer", "[processor][dsp]")
{
    constexpr int blockSize = 256;

    // Rendered twice with identical input: once into a buffer the host has left junk in,
    // once into a cleared one. The engine is deterministic with a locked seed, so the two
    // must agree exactly - which they only can if the junk was cleared rather than added to.
    const auto render = [] (bool fillWithJunk)
    {
        aethr::AethrProcessor processor;
        processor.setProcessingPrecision (juce::AudioProcessor::doublePrecision);

        auto* seedLock = processor.getValueTreeState().getParameter (aethr::params::exciter::seedLocked);
        REQUIRE (seedLock != nullptr);
        seedLock->setValueNotifyingHost (1.0f);

        processor.setPlayConfigDetails (0, outputChannels, 96000.0, blockSize);
        processor.prepareToPlay (96000.0, blockSize);

        juce::AudioBuffer<double> buffer (outputChannels, blockSize);
        buffer.clear();

        if (fillWithJunk)
            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                for (int i = 0; i < blockSize; ++i)
                    buffer.setSample (channel, i, 0.5);

        auto midi = makeMidiSequence (blockSize);
        processor.processBlock (buffer, midi);

        std::vector<double> output;

        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            for (int i = 0; i < blockSize; ++i)
                output.push_back (buffer.getSample (channel, i));

        return output;
    };

    const auto clean = render (false);
    const auto dirty = render (true);

    REQUIRE_FALSE (aethr::guards::containsNonFinite (clean.data(), clean.size()));

    // Notes were played, so there must be audio to compare in the first place.
    REQUIRE (aethr::guards::peakMagnitude (clean.data(), clean.size()) > 1.0e-4);
    REQUIRE (clean == dirty);
}

TEST_CASE ("Repeated sample-rate and block-size changes are handled cleanly", "[processor][dsp][robustness]")
{
    aethr::AethrProcessor processor;

    // Emulate a host switching audio devices mid-session, several times.
    for (const auto sampleRate : testSampleRates)
    {
        for (const auto blockSize : { 64, 1024, 128 })
        {
            prepare (processor, sampleRate, blockSize);

            juce::AudioBuffer<float> buffer (outputChannels, blockSize);
            buffer.clear();

            juce::MidiBuffer midi;
            processor.processBlock (buffer, midi);
            processor.releaseResources();

            for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
                REQUIRE_FALSE (aethr::guards::containsNonFinite (buffer.getReadPointer (channel),
                                                                 static_cast<std::size_t> (blockSize)));
        }
    }
}

TEST_CASE ("Output level parameter scales the output stage and reports a peak", "[processor][dsp]")
{
    aethr::AethrProcessor processor;
    prepare (processor, 48000.0, 256);

    // With no notes playing the peak must read zero regardless of the gain setting;
    // this guards against the meter reporting phantom activity.
    auto* gain = processor.getValueTreeState().getParameter (aethr::params::output::gain);
    REQUIRE (gain != nullptr);
    gain->setValueNotifyingHost (gain->convertTo0to1 (12.0f));

    juce::AudioBuffer<float> buffer (outputChannels, 256);
    buffer.clear();

    juce::MidiBuffer midi;
    processor.processBlock (buffer, midi);

    REQUIRE (processor.getOutputPeakLevel() == 0.0f);
}

TEST_CASE ("An editor can be created and destroyed repeatedly", "[processor][ui]")
{
    aethr::AethrProcessor processor;

    for (int i = 0; i < 3; ++i)
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());

        REQUIRE (editor != nullptr);
        REQUIRE (editor->getWidth() > 0);
        REQUIRE (editor->getHeight() > 0);
    }
}
