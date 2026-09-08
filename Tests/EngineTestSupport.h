#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include "Parameters/ParameterIDs.h"
#include "PluginProcessor.h"

namespace aethr::testing
{

/** A rendered note, split by channel, in the engine's own precision. */
struct RenderedNote
{
    std::vector<double> left;
    std::vector<double> right;

    [[nodiscard]] std::size_t size() const noexcept { return left.size(); }

    [[nodiscard]] double peak() const noexcept
    {
        auto highest = 0.0;

        for (std::size_t i = 0; i < left.size(); ++i)
            highest = std::max (highest, std::max (std::abs (left[i]), std::abs (right[i])));

        return highest;
    }

    [[nodiscard]] bool isFinite() const noexcept
    {
        const auto finite = [] (const std::vector<double>& channel)
        {
            return std::all_of (channel.begin(), channel.end(),
                                [] (double value) { return std::isfinite (value); });
        };

        return finite (left) && finite (right);
    }

    /** Copy of a time slice of the left channel, for analysis. */
    [[nodiscard]] std::vector<double> leftWindow (std::size_t startSample, std::size_t length) const
    {
        if (startSample >= left.size())
            return {};

        const auto available = std::min (length, left.size() - startSample);

        return { left.begin() + static_cast<std::ptrdiff_t> (startSample),
                 left.begin() + static_cast<std::ptrdiff_t> (startSample + available) };
    }
};

//==============================================================================
/** Sets a parameter by its real (denormalised) value. Returns false if the ID is unknown. */
inline bool setParameter (AethrProcessor& processor, const char* identifier, float realValue)
{
    auto* parameter = processor.getValueTreeState().getParameter (identifier);

    if (parameter == nullptr)
        return false;

    parameter->setValueNotifyingHost (parameter->convertTo0to1 (realValue));

    return true;
}

/**
    A configuration that isolates the loop's tuning from everything that could blur it.

    Long decay so the note is still ringing when it is analysed, no randomisation so
    the burst is identical every time, no stereo spread so both channels are the same
    signal, and a short bright burst so the fundamental is excited cleanly.
*/
inline void applyTuningTestSettings (AethrProcessor& processor,
                                     float dampingPercent = 10.0f,
                                     float brightnessPercent = 70.0f)
{
    setParameter (processor, params::resonator::decayTime, 25.0f);
    setParameter (processor, params::resonator::releaseTime, 25.0f);
    setParameter (processor, params::resonator::damping, dampingPercent);
    setParameter (processor, params::resonator::brightness, brightnessPercent);

    setParameter (processor, params::exciter::type, 0.0f);          // pluck
    setParameter (processor, params::exciter::burstTime, 4.0f);
    setParameter (processor, params::exciter::attack, 5.0f);
    setParameter (processor, params::exciter::colour, 0.0f);
    setParameter (processor, params::exciter::brightness, 85.0f);
    setParameter (processor, params::exciter::level, 90.0f);
    setParameter (processor, params::exciter::randomAmount, 0.0f);
    setParameter (processor, params::exciter::stereoSpread, 0.0f);

    setParameter (processor, params::voice::velocityAmount, 0.0f);
    setParameter (processor, params::output::gain, 0.0f);

    setParameter (processor, params::master::tuneOctave, 0.0f);
    setParameter (processor, params::master::tuneSemitones, 0.0f);
    setParameter (processor, params::master::tuneCents, 0.0f);
}

//==============================================================================
/**
    Renders a single note offline.

    Parameters must already be set; this only prepares, sends the note and pumps
    blocks. Double precision throughout, which also exercises the double processBlock
    path that hosts like Reaper can select.

    @param releaseAfterSeconds  negative leaves the note held for the whole render.
*/
inline RenderedNote renderNote (AethrProcessor& processor,
                                int midiNote,
                                float velocity,
                                double sampleRate,
                                int blockSize,
                                double seconds,
                                double releaseAfterSeconds = -1.0)
{
    processor.setRateAndBufferSizeDetails (sampleRate, blockSize);
    processor.prepareToPlay (sampleRate, blockSize);

    const auto totalSamples = static_cast<int> (std::ceil (seconds * sampleRate));
    const auto releaseSample = releaseAfterSeconds >= 0.0
                                 ? static_cast<int> (releaseAfterSeconds * sampleRate)
                                 : -1;

    RenderedNote result;
    result.left.reserve (static_cast<std::size_t> (totalSamples));
    result.right.reserve (static_cast<std::size_t> (totalSamples));

    juce::AudioBuffer<double> buffer { std::max (1, processor.getTotalNumOutputChannels()), blockSize };

    for (auto rendered = 0; rendered < totalSamples; rendered += blockSize)
    {
        juce::MidiBuffer midi;

        if (rendered == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, midiNote, velocity), 0);

        if (releaseSample >= rendered && releaseSample < rendered + blockSize)
            midi.addEvent (juce::MidiMessage::noteOff (1, midiNote), releaseSample - rendered);

        buffer.clear();
        processor.processBlock (buffer, midi);

        const auto* left = buffer.getReadPointer (0);
        const auto* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : left;

        const auto toCopy = std::min (blockSize, totalSamples - rendered);

        for (auto i = 0; i < toCopy; ++i)
        {
            result.left.push_back (left[i]);
            result.right.push_back (right[i]);
        }
    }

    return result;
}

} // namespace aethr::testing
