#pragma once

#include <array>
#include <cstdint>

#include "Engine/Voice.h"

namespace aethr::engine
{

/**
    Minimal note handler and voice pool.

    Allocation is first idle voice, otherwise steal the oldest that is no longer held.
    Inactive layer-B and body paths are skipped so unused DSP is not paid for.
*/
class VoiceEngine
{
public:
    /** Interim voice ceiling. Phase 8 raises this and honours the full polyphony parameter. */
    static constexpr int maximumVoices = 64;

    //==============================================================================
    void prepare (double sampleRate);
    void reset() noexcept;

    /** Copies in the per-block settings and pushes them to every sounding voice. */
    void setSettings (const Settings& newSettings) noexcept;

    [[nodiscard]] const Settings& getSettings() const noexcept { return settings; }

    //==============================================================================
    void noteOn (int midiNoteNumber, double velocity) noexcept;
    void noteOff (int midiNoteNumber) noexcept;

    /** @param allowTailOff  when false, voices stop instantly instead of ringing out. */
    void allNotesOff (bool allowTailOff) noexcept;

    void setPitchBendSemitones (double semitones) noexcept;
    void setSustainPedal (bool down) noexcept;

    /** Adds all sounding voices into the buffers. Does not clear them. */
    void renderAdding (double* left, double* right, int numSamples) noexcept;

    //==============================================================================
    [[nodiscard]] int getActiveVoiceCount() const noexcept;

    /** Exposed for tests that need to inspect what the engine tuned a note to. */
    [[nodiscard]] const Voice* getVoicePlayingNote (int midiNoteNumber) const noexcept;

private:
    [[nodiscard]] Voice* findVoiceToUse (int midiNoteNumber) noexcept;
    [[nodiscard]] std::uint32_t seedForNote (int midiNoteNumber) noexcept;
    [[nodiscard]] int getVoiceLimit() const noexcept;

    std::array<Voice, static_cast<std::size_t> (maximumVoices)> voices;

    Settings settings;
    double pitchBendSemitones { 0.0 };
    bool sustainPedalDown { false };

    std::uint64_t startOrderCounter { 0 };
    std::uint32_t seedCounter { 0 };
};

} // namespace aethr::engine
