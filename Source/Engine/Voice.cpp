#include "Engine/Voice.h"

#include <algorithm>

#include "Engine/EngineType.h"

namespace aethr::engine
{

void Voice::prepare (double sampleRate)
{
    stringEngine.prepare (sampleRate);
    modalEngine.prepare (sampleRate);
    scratchLeft.assign (8192, 0.0);
    scratchRight.assign (8192, 0.0);
    reset();
}

void Voice::reset() noexcept
{
    stringEngine.reset();
    modalEngine.reset();
    active = false;
    held = false;
    keyDown = false;
    pitchOffsetSemitones = 0.0;
}

void Voice::kill() noexcept
{
    stringEngine.kill();
    modalEngine.kill();
    active = false;
    held = false;
    keyDown = false;
}

void Voice::applySettings (const Settings& settings) noexcept
{
    currentSettings = settings;
    stringEngine.applySettings (settings, noteNumber, pitchOffsetSemitones, held);
    modalEngine.applySettings (settings, noteNumber, pitchOffsetSemitones, held);
}

void Voice::noteOn (int midiNoteNumber, double velocity, std::uint32_t seed, const Settings& settings) noexcept
{
    noteNumber = midiNoteNumber;
    held = true;
    keyDown = true;
    active = true;
    currentSettings = settings;

    stringEngine.noteOn (midiNoteNumber, velocity, seed, settings, pitchOffsetSemitones);
    modalEngine.noteOn (midiNoteNumber, velocity, seed ^ 0x9E3779B9u, settings, pitchOffsetSemitones);
}

void Voice::noteOff() noexcept
{
    if (! active)
        return;

    keyDown = false;
    held = false;
    stringEngine.noteOff();
    modalEngine.noteOff();
}

void Voice::setPitchOffsetSemitones (double semitones) noexcept
{
    pitchOffsetSemitones = semitones;
    stringEngine.setPitchOffsetSemitones (semitones);
    modalEngine.setPitchOffsetSemitones (semitones);
}

void Voice::renderAdding (double* left, double* right, int numSamples) noexcept
{
    if (! active)
        return;

    const auto type = currentSettings.engineType;

    if (usesHybridCore (type))
    {
        auto remaining = numSamples;
        auto offset = 0;

        while (remaining > 0)
        {
            const auto chunk = std::min (remaining, static_cast<int> (scratchLeft.size()));
            std::fill_n (scratchLeft.data(), static_cast<std::size_t> (chunk), 0.0);
            std::fill_n (scratchRight.data(), static_cast<std::size_t> (chunk), 0.0);
            stringEngine.renderAdding (scratchLeft.data(), scratchRight.data(), chunk);
            modalEngine.renderFrom (scratchLeft.data(), scratchRight.data(),
                                    left + offset, right + offset, chunk,
                                    currentSettings.controlA);

            remaining -= chunk;
            offset += chunk;
        }
    }
    else if (usesModalCore (type))
    {
        modalEngine.renderAdding (left, right, numSamples);
    }
    else
    {
        stringEngine.renderAdding (left, right, numSamples);
    }

    const auto asleep = usesHybridCore (type)
                            ? (stringEngine.wantsToSleep() && modalEngine.wantsToSleep())
                            : usesModalCore (type) ? modalEngine.wantsToSleep()
                                                   : stringEngine.wantsToSleep();

    if (asleep)
        kill();
}

} // namespace aethr::engine
