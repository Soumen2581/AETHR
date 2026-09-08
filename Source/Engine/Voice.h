#pragma once

#include <cstdint>
#include <vector>

#include "DSP/KarplusResonator.h"
#include "Engine/EngineSettings.h"
#include "Engine/KarplusStringEngine.h"
#include "Engine/ModalEngine.h"

namespace aethr::engine
{

class Voice
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;

    void applySettings (const Settings& settings) noexcept;

    void noteOn (int midiNoteNumber, double velocity, std::uint32_t seed, const Settings& settings) noexcept;
    void noteOff() noexcept;
    void markKeyUp() noexcept { keyDown = false; }
    void kill() noexcept;
    void setPitchOffsetSemitones (double semitones) noexcept;
    void renderAdding (double* left, double* right, int numSamples) noexcept;

    [[nodiscard]] bool isActive() const noexcept { return active; }
    [[nodiscard]] bool isHeld() const noexcept { return held; }
    [[nodiscard]] bool isKeyDown() const noexcept { return keyDown; }
    [[nodiscard]] int  getNoteNumber() const noexcept { return noteNumber; }

    [[nodiscard]] std::uint64_t getStartOrder() const noexcept { return startOrder; }
    void setStartOrder (std::uint64_t order) noexcept { startOrder = order; }

    [[nodiscard]] const dsp::KarplusResonator& getResonator() const noexcept
    {
        return stringEngine.getPrimaryResonator();
    }

private:
    KarplusStringEngine stringEngine;
    ModalEngine modalEngine;
    Settings currentSettings;
    std::vector<double> scratchLeft;
    std::vector<double> scratchRight;

    bool active { false };
    bool held { false };
    bool keyDown { false };
    int  noteNumber { 60 };
    double pitchOffsetSemitones { 0.0 };
    std::uint64_t startOrder { 0 };
};

} // namespace aethr::engine
