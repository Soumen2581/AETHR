#pragma once

#include "DSP/BodyResonator.h"
#include "DSP/Exciter.h"
#include "DSP/KarplusResonator.h"
#include "Engine/EngineSettings.h"
#include "Engine/EngineType.h"

#include <array>

namespace aethr::engine
{

/**
    Flagship physical engine: exciter → dual Karplus–Strong waveguides → optional
    Layer B → modal body.

    This is the shipped STRING engine. Voice owns MIDI lifecycle; this class owns
    every delay line, filter and exciter state. Allocates only in prepare().
*/
class KarplusStringEngine
{
public:
    void prepare (double sampleRate);
    void reset() noexcept;
    void kill() noexcept;

    void applySettings (const Settings& settings,
                        int noteNumber,
                        double pitchOffsetSemitones,
                        bool held) noexcept;

    void noteOn (int midiNoteNumber,
                 double velocity,
                 std::uint32_t seed,
                 const Settings& settings,
                 double pitchOffsetSemitones) noexcept;

    void noteOff() noexcept;
    void setPitchOffsetSemitones (double semitones) noexcept;

    void renderAdding (double* left, double* right, int numSamples) noexcept;

    [[nodiscard]] bool wantsToSleep() const noexcept { return sleeping; }
    [[nodiscard]] double getTunedFrequencyHz() const noexcept { return leftResonator.getTunedFrequency(); }

    /** Pitch tests inspect the primary loop. */
    [[nodiscard]] const dsp::KarplusResonator& getPrimaryResonator() const noexcept { return leftResonator; }

private:
    static constexpr double silenceThreshold = 1.0e-5;
    static constexpr double silenceHoldSeconds = 0.5;
    static constexpr double minimumSustainedDrive = 1.0e-3;

    void updateFrequency() noexcept;
    void applyCharacter() noexcept;
    [[nodiscard]] dsp::Exciter::StereoSample colourExcitation (dsp::Exciter::StereoSample excitation) noexcept;

    dsp::Exciter exciter;
    dsp::KarplusResonator leftResonator;
    dsp::KarplusResonator rightResonator;
    dsp::KarplusResonator layerBLeft;
    dsp::KarplusResonator layerBRight;
    dsp::BodyResonator bodyLeft;
    dsp::BodyResonator bodyRight;

    Settings currentSettings;
    Settings hostSettings;
    double sampleRate { 44100.0 };
    double lastOutL { 0.0 };
    double lastOutR { 0.0 };
    double grainPhase { 0.0 };
    std::uint32_t grainSeed { 1 };
    std::array<double, 2048> posL {};
    std::array<double, 2048> posR {};
    int posWrite { 0 };

    int noteNumber { 60 };
    double pitchOffsetSemitones { 0.0 };
    double tuningOffsetSemitones { 0.0 };
    bool held { true };
    bool sleeping { false };
    int quietSamples { 0 };
    int silenceHoldSamples { 1 };
};

} // namespace aethr::engine
