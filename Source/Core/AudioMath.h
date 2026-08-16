#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

/**
    Small, dependency-free maths helpers shared by the DSP layer.

    Everything here is header-only, `constexpr` where the standard allows it, and
    free of JUCE types so that the offline test suite can exercise it without
    constructing a plugin. Pitch-critical maths is done in `double`: a 1-cent
    error at 20 Hz is ~0.012 Hz, which is below the resolution that `float`
    reliably delivers once a delay length is derived from it.

    @see docs/DSP_NOTES.md
*/
namespace strata::math
{

inline constexpr double pi    = std::numbers::pi_v<double>;
inline constexpr double twoPi = 2.0 * pi;

/** Semitones per octave in 12-TET. Named to avoid a bare 12.0 in tuning maths. */
inline constexpr double semitonesPerOctave = 12.0;

/** Cents per semitone. */
inline constexpr double centsPerSemitone = 100.0;

/** MIDI note number of A4, the tuning anchor. */
inline constexpr double midiNoteOfA4 = 69.0;

/** Gains quieter than this are treated as silence (-120 dB). */
inline constexpr double silenceGain = 1.0e-6;

//==============================================================================
/** Converts decibels to a linear gain. `decibels <= minusInfinityDb` yields 0. */
template <typename FloatType>
[[nodiscard]] FloatType decibelsToGain (FloatType decibels,
                                        FloatType minusInfinityDb = static_cast<FloatType> (-100)) noexcept
{
    if (decibels <= minusInfinityDb)
        return static_cast<FloatType> (0);

    return std::pow (static_cast<FloatType> (10), decibels * static_cast<FloatType> (0.05));
}

/** Converts a linear gain to decibels, clamped at `minusInfinityDb`. */
template <typename FloatType>
[[nodiscard]] FloatType gainToDecibels (FloatType gain,
                                        FloatType minusInfinityDb = static_cast<FloatType> (-100)) noexcept
{
    if (gain <= static_cast<FloatType> (0))
        return minusInfinityDb;

    return std::max (minusInfinityDb,
                     static_cast<FloatType> (20) * std::log10 (gain));
}

//==============================================================================
/** Frequency ratio for a signed interval in semitones. */
[[nodiscard]] inline double semitonesToRatio (double semitones) noexcept
{
    return std::exp2 (semitones / semitonesPerOctave);
}

/** Frequency ratio for a signed detune in cents. */
[[nodiscard]] inline double centsToRatio (double cents) noexcept
{
    return std::exp2 (cents / (centsPerSemitone * semitonesPerOctave));
}

/** Difference between two frequencies expressed in cents. Returns 0 if either is non-positive. */
[[nodiscard]] inline double ratioToCents (double frequency, double referenceFrequency) noexcept
{
    if (frequency <= 0.0 || referenceFrequency <= 0.0)
        return 0.0;

    return centsPerSemitone * semitonesPerOctave * std::log2 (frequency / referenceFrequency);
}

/**
    Converts a (possibly fractional) MIDI note number to Hertz.

    @param midiNote          MIDI note number; fractional values are valid so
                             that pitch bend and modulation can be folded in
                             before conversion.
    @param frequencyOfA4     tuning reference, normally 440 Hz.
*/
[[nodiscard]] inline double midiNoteToHertz (double midiNote, double frequencyOfA4 = 440.0) noexcept
{
    return frequencyOfA4 * std::exp2 ((midiNote - midiNoteOfA4) / semitonesPerOctave);
}

/** Inverse of midiNoteToHertz(). Returns a fractional MIDI note number. */
[[nodiscard]] inline double hertzToMidiNote (double hertz, double frequencyOfA4 = 440.0) noexcept
{
    if (hertz <= 0.0)
        return 0.0;

    return midiNoteOfA4 + semitonesPerOctave * std::log2 (hertz / frequencyOfA4);
}

//==============================================================================
/** Linear interpolation. `t` is not clamped. */
template <typename FloatType>
[[nodiscard]] constexpr FloatType lerp (FloatType a, FloatType b, FloatType t) noexcept
{
    return a + t * (b - a);
}

/** Maps `value` from [inMin, inMax] onto [outMin, outMax] without clamping. */
template <typename FloatType>
[[nodiscard]] constexpr FloatType mapRange (FloatType value,
                                            FloatType inMin, FloatType inMax,
                                            FloatType outMin, FloatType outMax) noexcept
{
    const auto span = inMax - inMin;
    const auto normalised = (span > std::numeric_limits<FloatType>::min() || span < -std::numeric_limits<FloatType>::min())
                          ? (value - inMin) / span
                          : static_cast<FloatType> (0);
    return outMin + normalised * (outMax - outMin);
}

/**
    Converts a -60 dB decay time into the per-sample feedback coefficient of a
    one-pole/comb loop.

    A loop that multiplies by `g` once per `delaySamples` decays by 60 dB after
    t60 seconds when

        g = 10 ^ (-3 * delaySamples / (t60 * sampleRate))

    which follows from requiring g^(t60 * fs / delaySamples) = 10^-3.

    @param t60Seconds     time to decay by 60 dB; values <= 0 return 0 (instant decay).
    @param delaySamples   loop length in samples; must be > 0.
    @param sampleRate     sample rate in Hz; must be > 0.
    @returns              loop gain in [0, 1).
*/
[[nodiscard]] inline double decayTimeToLoopGain (double t60Seconds,
                                                 double delaySamples,
                                                 double sampleRate) noexcept
{
    if (t60Seconds <= 0.0 || delaySamples <= 0.0 || sampleRate <= 0.0)
        return 0.0;

    const auto exponent = -3.0 * delaySamples / (t60Seconds * sampleRate);
    return std::clamp (std::pow (10.0, exponent), 0.0, 1.0);
}

/** Inverse of decayTimeToLoopGain(). Returns infinity for a unity-gain loop. */
[[nodiscard]] inline double loopGainToDecayTime (double loopGain,
                                                 double delaySamples,
                                                 double sampleRate) noexcept
{
    if (loopGain <= 0.0 || delaySamples <= 0.0 || sampleRate <= 0.0)
        return 0.0;

    if (loopGain >= 1.0)
        return std::numeric_limits<double>::infinity();

    return -3.0 * delaySamples / (sampleRate * std::log10 (loopGain));
}

} // namespace strata::math
