#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

/**
    Numerical-safety primitives for the realtime path.

    Every feedback structure in the engine (resonator loop, feed loop, delay,
    reverb) is capable of producing a non-finite sample if it is driven with an
    invalid coefficient. A single NaN entering a recursive filter poisons the
    filter state permanently, so it must be stopped at the point of production
    rather than detected at the output.

    The rules these helpers exist to enforce:

      1. No non-finite value is ever written into recursive state.
      2. A denormal is flushed rather than allowed to stall the FPU. JUCE's
         `ScopedNoDenormals` covers this at block level; `flushDenormals()` is for
         state that persists between blocks and is therefore not covered.
      3. Recovery is graceful and silent: substitute zero and keep going. Never
         log, throw, or allocate from the audio thread.

    All functions here are `noexcept`, allocation-free and branch-cheap.

    @see docs/DSP_NOTES.md, docs/TESTING.md
*/
namespace aethr::guards
{

/** Magnitudes below this are treated as denormal and flushed to zero. */
inline constexpr float denormalThreshold = 1.0e-15f;

/** Hard ceiling applied to any signal leaving a feedback structure. */
inline constexpr float feedbackCeiling = 4.0f;

//==============================================================================
/** Returns `value` if it is finite, otherwise 0. */
template <typename FloatType>
[[nodiscard]] inline FloatType sanitise (FloatType value) noexcept
{
    return std::isfinite (value) ? value : static_cast<FloatType> (0);
}

/** Flushes denormal magnitudes to zero, leaving everything else untouched. */
template <typename FloatType>
[[nodiscard]] inline FloatType flushDenormals (FloatType value) noexcept
{
    return std::abs (value) < static_cast<FloatType> (denormalThreshold)
             ? static_cast<FloatType> (0)
             : value;
}

/**
    Combined guard for values that are about to be stored in recursive state:
    replaces non-finite values with zero, flushes denormals, and clamps the
    magnitude to `ceiling` so a momentarily unstable loop decays instead of
    escalating to infinity.
*/
template <typename FloatType>
[[nodiscard]] inline FloatType sanitiseState (FloatType value,
                                              FloatType ceiling = static_cast<FloatType> (feedbackCeiling)) noexcept
{
    if (! std::isfinite (value))
        return static_cast<FloatType> (0);

    if (std::abs (value) < static_cast<FloatType> (denormalThreshold))
        return static_cast<FloatType> (0);

    return std::clamp (value, -ceiling, ceiling);
}

//==============================================================================
/** True if any sample in the range is NaN or infinite. Intended for tests and debug assertions. */
template <typename FloatType>
[[nodiscard]] inline bool containsNonFinite (const FloatType* data, std::size_t numSamples) noexcept
{
    for (std::size_t i = 0; i < numSamples; ++i)
        if (! std::isfinite (data[i]))
            return true;

    return false;
}

/** Replaces every non-finite sample in the range with zero. Returns the number replaced. */
template <typename FloatType>
inline std::size_t sanitiseBlock (FloatType* data, std::size_t numSamples) noexcept
{
    std::size_t replaced = 0;

    for (std::size_t i = 0; i < numSamples; ++i)
    {
        if (! std::isfinite (data[i]))
        {
            data[i] = static_cast<FloatType> (0);
            ++replaced;
        }
    }

    return replaced;
}

/** Largest absolute sample in the range, or 0 for an empty range. Non-finite samples are ignored. */
template <typename FloatType>
[[nodiscard]] inline FloatType peakMagnitude (const FloatType* data, std::size_t numSamples) noexcept
{
    auto peak = static_cast<FloatType> (0);

    for (std::size_t i = 0; i < numSamples; ++i)
        if (std::isfinite (data[i]))
            peak = std::max (peak, std::abs (data[i]));

    return peak;
}

//==============================================================================
/**
    Clamps a feedback/loop coefficient into a strictly stable range.

    A loop gain of exactly 1 is marginally stable in theory and divergent in
    practice once a nonlinearity or a filter with any passband gain sits inside
    the loop, so the maximum is kept just below unity.
*/
[[nodiscard]] inline double clampLoopGain (double gain, double maximum = 0.9995) noexcept
{
    if (! std::isfinite (gain))
        return 0.0;

    return std::clamp (gain, 0.0, maximum);
}

/** True if `value` is finite and within [minimum, maximum]; for debug assertions on coefficients. */
[[nodiscard]] inline bool isValidCoefficient (double value,
                                              double minimum = -std::numeric_limits<double>::max(),
                                              double maximum = std::numeric_limits<double>::max()) noexcept
{
    return std::isfinite (value) && value >= minimum && value <= maximum;
}

} // namespace aethr::guards
