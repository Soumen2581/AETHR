#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "Core/AudioMath.h"
#include "Core/RealtimeGuards.h"

namespace aethr::dsp
{

enum class LfoWave
{
    sine = 0,
    triangle,
    saw,
    reverseSaw,
    square,
    sampleHold,
    smoothRandom,
    numWaves
};

enum class ModDestination
{
    none = 0,
    pitch,
    decay,
    damping,
    brightness,
    excitation,
    stiffness,
    filterCutoff,
    delayMix,
    reverbMix,
    layerMix,
    drive,
    stereo,
    numDestinations
};

class Lfo
{
public:
    void prepare (double newSampleRate) noexcept
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0;
        reset();
    }

    void reset() noexcept
    {
        phase = 0.0;
        held = 0.0;
        smooth = 0.0;
    }

    void set (LfoWave newWave, double hz, double newDepth) noexcept
    {
        wave = newWave;
        rateHz = std::clamp (hz, 0.01, 40.0);
        depth = std::clamp (newDepth, 0.0, 1.0);
    }

    /** Advances one sample and returns bipolar [-depth, +depth]. */
    [[nodiscard]] double processSample() noexcept
    {
        const auto increment = rateHz / sampleRate;
        phase += increment;

        if (phase >= 1.0)
        {
            phase -= 1.0;
            held = nextRandom();
        }

        const auto raw = valueAt (phase);
        return raw * depth;
    }

    /** Block-rate convenience: advance `numSamples` and return the last value. */
    [[nodiscard]] double processBlock (int numSamples) noexcept
    {
        auto last = 0.0;

        for (int i = 0; i < numSamples; ++i)
            last = processSample();

        return last;
    }

private:
    [[nodiscard]] double valueAt (double p) noexcept
    {
        switch (wave)
        {
            case LfoWave::sine:        return std::sin (math::twoPi * p);
            case LfoWave::triangle:    return 4.0 * std::abs (p - 0.5) - 1.0;
            case LfoWave::saw:         return 2.0 * p - 1.0;
            case LfoWave::reverseSaw:  return 1.0 - 2.0 * p;
            case LfoWave::square:      return p < 0.5 ? 1.0 : -1.0;
            case LfoWave::sampleHold:  return held;
            case LfoWave::smoothRandom:
            {
                smooth += 0.002 * (held - smooth);
                return smooth;
            }
            case LfoWave::numWaves:
            default:
                return 0.0;
        }
    }

    [[nodiscard]] double nextRandom() noexcept
    {
        rng = rng * 1664525u + 1013904223u;
        return (static_cast<double> (rng >> 8) / 16777215.0) * 2.0 - 1.0;
    }

    double sampleRate { 44100.0 };
    double phase { 0.0 };
    double rateHz { 1.0 };
    double depth { 0.0 };
    double held { 0.0 };
    double smooth { 0.0 };
    LfoWave wave { LfoWave::sine };
    std::uint32_t rng { 0xA5A5A5A5u };
};

class EnvelopeFollower
{
public:
    void prepare (double newSampleRate) noexcept { sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0; }

    void set (double attackS, double decayS, double sustainLevel, double releaseS, double newDepth) noexcept
    {
        attackCoeff  = coeff (attackS);
        decayCoeff   = coeff (decayS);
        releaseCoeff = coeff (releaseS);
        sustain = std::clamp (sustainLevel, 0.0, 1.0);
        depth = std::clamp (newDepth, 0.0, 1.0);
    }

    void noteOn() noexcept  { gate = true; }
    void noteOff() noexcept { gate = false; }

    [[nodiscard]] double processSample() noexcept
    {
        const auto target = gate ? 1.0 : 0.0;
        const auto c = ! gate ? releaseCoeff
                      : (value < target ? attackCoeff : decayCoeff);
        const auto dest = gate ? (value < 1.0 ? 1.0 : sustain) : 0.0;
        value += (dest - value) * c;
        return value * depth;
    }

    [[nodiscard]] double processBlock (int numSamples) noexcept
    {
        auto last = 0.0;

        for (int i = 0; i < numSamples; ++i)
            last = processSample();

        return last;
    }

private:
    [[nodiscard]] double coeff (double seconds) const noexcept
    {
        const auto s = std::max (0.001, seconds);
        return 1.0 - std::exp (-1.0 / (s * sampleRate));
    }

    double sampleRate { 44100.0 };
    double value { 0.0 };
    double attackCoeff { 0.01 };
    double decayCoeff { 0.01 };
    double releaseCoeff { 0.01 };
    double sustain { 0.7 };
    double depth { 0.0 };
    bool gate { false };
};

struct ModulationOffsets
{
    double pitchSemitones { 0.0 };
    double decayScale { 1.0 };
    double dampingOffset { 0.0 };
    double brightnessOffset { 0.0 };
    double excitationOffset { 0.0 };
    double stiffnessOffset { 0.0 };
    double filterCutoffOffset { 0.0 };
    double delayMixOffset { 0.0 };
    double reverbMixOffset { 0.0 };
    double layerMixOffset { 0.0 };
    double driveOffset { 0.0 };
    double stereoOffset { 0.0 };
};

inline void applyDestination (ModulationOffsets& offsets, ModDestination dest, double amount) noexcept
{
    switch (dest)
    {
        case ModDestination::pitch:         offsets.pitchSemitones += amount * 2.0; break;
        case ModDestination::decay:         offsets.decayScale *= std::exp2 (amount); break;
        case ModDestination::damping:       offsets.dampingOffset += amount * 0.4; break;
        case ModDestination::brightness:    offsets.brightnessOffset += amount * 0.4; break;
        case ModDestination::excitation:    offsets.excitationOffset += amount * 0.5; break;
        case ModDestination::stiffness:     offsets.stiffnessOffset += amount * 0.5; break;
        case ModDestination::filterCutoff:  offsets.filterCutoffOffset += amount; break;
        case ModDestination::delayMix:      offsets.delayMixOffset += amount * 0.5; break;
        case ModDestination::reverbMix:     offsets.reverbMixOffset += amount * 0.5; break;
        case ModDestination::layerMix:      offsets.layerMixOffset += amount * 0.5; break;
        case ModDestination::drive:         offsets.driveOffset += amount * 0.5; break;
        case ModDestination::stereo:        offsets.stereoOffset += amount * 0.5; break;
        case ModDestination::none:
        case ModDestination::numDestinations:
        default: break;
    }
}

class ChaosWalk
{
public:
    void prepare (double newSampleRate) noexcept { sampleRate = newSampleRate > 0.0 ? newSampleRate : 44100.0; }

    void set (double newAmount, double rateHz, double newBias) noexcept
    {
        amount = std::clamp (newAmount, 0.0, 1.0);
        rate = std::clamp (rateHz, 0.05, 20.0);
        bias = std::clamp (newBias, -1.0, 1.0);
    }

    [[nodiscard]] double processBlock (int numSamples) noexcept
    {
        if (amount <= 1.0e-6)
            return 0.0;

        phase += rate * static_cast<double> (numSamples) / sampleRate;

        if (phase >= 1.0)
        {
            phase -= std::floor (phase);
            rng = rng * 1664525u + 1013904223u;
            target = (static_cast<double> (rng >> 8) / 16777215.0) * 2.0 - 1.0;
        }

        current += 0.08 * (target - current);
        const auto bounded = std::clamp (current + bias * 0.3, -1.0, 1.0);
        return bounded * amount;
    }

private:
    double sampleRate { 44100.0 };
    double amount { 0.0 };
    double rate { 0.4 };
    double bias { 0.0 };
    double phase { 0.0 };
    double current { 0.0 };
    double target { 0.0 };
    std::uint32_t rng { 0xC0FFEEu };
};

} // namespace aethr::dsp
