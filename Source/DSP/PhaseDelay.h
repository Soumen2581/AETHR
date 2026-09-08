#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>

/**
    Phase-delay analysis for the elements that sit inside the resonator loop.

    Tuning a Karplus–Strong loop is a phase problem, not a length problem. The loop
    resonates at \f$f_0\f$ when the *total* phase delay around it equals one period:

        fs / f0 = D_delayline(w0) + D_loopfilter(w0)

    so every element must be able to report the phase delay it contributes at a
    given frequency. That is what this header provides. Getting it wrong is not a
    subtle error: at C8 and 48 kHz a period is only 11.5 samples, so neglecting the
    ~1.5 samples contributed by an interpolator and a one-pole filter is a pitch
    error of roughly 230 cents.

    Phase delay of a system \f$H\f$ at normalised frequency \f$\omega\f$ is

        D(w) = -arg(H(e^{jw})) / w

    Everything here is `double`, free of JUCE and of state, so it can be tested
    directly against a brute-force DFT (see Tests/PhaseDelayTests.cpp).

    @see docs/DSP_NOTES.md
*/
namespace aethr::dsp::phase
{

inline constexpr double pi = std::numbers::pi_v<double>;

/** Below this frequency the phase-delay formulas are numerically unusable (0/0). */
inline constexpr double minimumOmega = 1.0e-9;

//==============================================================================
/**
    Unwraps a phase measurement into a phase delay near a known nominal value.

    `atan2` returns a principal value in \f$(-\pi, \pi]\f$, so for any system whose
    delay exceeds one sample the measured phase wraps once \f$\omega D > \pi\f$ —
    which happens above \f$f_s/4\f$ for a two-sample delay. Left unhandled, the
    computed delay jumps by \f$2\pi/\omega\f$ samples and the tuning solver chases a
    discontinuity. The nominal delay tells us which branch we are on.
*/
[[nodiscard]] inline double unwrapPhaseDelay (double realPart,
                                              double imaginaryPart,
                                              double omega,
                                              double nominalDelay) noexcept
{
    if (omega < minimumOmega)
        return nominalDelay;

    const auto principalPhase = std::atan2 (imaginaryPart, realPart);
    const auto principalDelay = -principalPhase / omega;

    // Choose the branch whose delay is closest to the nominal expectation.
    const auto samplesPerWrap = 2.0 * pi / omega;
    const auto wraps = std::round ((nominalDelay - principalDelay) / samplesPerWrap);

    return principalDelay + wraps * samplesPerWrap;
}

//==============================================================================
/**
    Allpass coefficient giving a fractional delay of `delta` samples.

    \f$A(z) = (c + z^{-1}) / (1 + c z^{-1})\f$ with \f$c = (1-\Delta)/(1+\Delta)\f$.
    \f$\Delta = 0\f$ gives \f$c = 1\f$ (a pass-through), \f$\Delta = 1\f$ gives
    \f$c = 0\f$ (exactly \f$z^{-1}\f$). Stable for \f$|c| < 1\f$, i.e. \f$\Delta > 0\f$.
*/
[[nodiscard]] inline double allpassCoefficientForDelay (double delta) noexcept
{
    return (1.0 - delta) / (1.0 + delta);
}

/**
    Allpass coefficient giving a phase delay of exactly `delay` samples *at* `omega`.

    Inverting the phase-delay expression below for c gives

        c = sin(theta) / sin(omega - theta),   theta = (1 - D) * omega / 2

    which is worth having rather than iterating towards, and not only for speed. The
    naive approach — set the coefficient from the delay as if the response were flat,
    then iterate to correct for the fact that it is not — fails at short loop lengths.
    Its achievable phase delays have *gaps*: the allpass delay at the top of the
    fractional window falls short of one sample above the delay at the bottom, so a
    band of delays between consecutive integer taps simply cannot be reached, and the
    iteration oscillates across the hole. At C8 and 48 kHz that hole was worth 7 cents.

    With this form the realised phase delay is exact by construction, so consecutive
    integer taps tile the delay axis with no gaps at all.

    Stability requires |c| < 1, which holds while 0 < D < pi/omega. The caller clamps.
*/
[[nodiscard]] inline double allpassCoefficientForPhaseDelay (double delay, double omega) noexcept
{
    if (omega < minimumOmega)
        return allpassCoefficientForDelay (delay);

    const auto theta = 0.5 * (1.0 - delay) * omega;
    const auto denominator = std::sin (omega - theta);

    if (std::abs (denominator) < minimumOmega)
        return allpassCoefficientForDelay (delay);

    return std::sin (theta) / denominator;
}

/**
    Phase delay of the first-order allpass, in samples.

    \f[ D(\omega) = 1 - \frac{2}{\omega}\arctan\frac{c\sin\omega}{1 + c\cos\omega} \f]

    `atan` rather than `atan2` is correct here because \f$1 + c\cos\omega > 0\f$ for
    all \f$|c| < 1\f$, so the argument never leaves the principal branch. The delay
    equals \f$\Delta\f$ exactly at DC and drifts from it as frequency rises, which is
    precisely the drift the tuning solver has to account for.
*/
[[nodiscard]] inline double allpassPhaseDelay (double coefficient, double omega) noexcept
{
    if (omega < minimumOmega)
    {
        // DC limit: D -> 1 - 2c/(1+c), which is the delta the coefficient encodes.
        return 1.0 - (2.0 * coefficient) / (1.0 + coefficient);
    }

    const auto numerator   = coefficient * std::sin (omega);
    const auto denominator = 1.0 + coefficient * std::cos (omega);

    return 1.0 - (2.0 / omega) * std::atan (numerator / denominator);
}

//==============================================================================
/**
    Phase delay of an FIR whose taps are at delays `firstTapDelay + k`.

    Used for the linear and Lagrange interpolators, whose read is a short FIR.
*/
[[nodiscard]] inline double firPhaseDelay (const double* coefficients,
                                           std::size_t numCoefficients,
                                           double firstTapDelay,
                                           double omega,
                                           double nominalDelay) noexcept
{
    if (omega < minimumOmega)
        return nominalDelay;

    auto realPart = 0.0;
    auto imaginaryPart = 0.0;

    for (std::size_t k = 0; k < numCoefficients; ++k)
    {
        const auto delay = firstTapDelay + static_cast<double> (k);
        const auto angle = omega * delay;

        realPart      += coefficients[k] * std::cos (angle);
        imaginaryPart -= coefficients[k] * std::sin (angle);
    }

    return unwrapPhaseDelay (realPart, imaginaryPart, omega, nominalDelay);
}

/** Magnitude response of the same FIR. Needed because non-allpass interpolators lose gain. */
[[nodiscard]] inline double firMagnitude (const double* coefficients,
                                          std::size_t numCoefficients,
                                          double omega) noexcept
{
    auto realPart = 0.0;
    auto imaginaryPart = 0.0;

    for (std::size_t k = 0; k < numCoefficients; ++k)
    {
        const auto angle = omega * static_cast<double> (k);

        realPart      += coefficients[k] * std::cos (angle);
        imaginaryPart -= coefficients[k] * std::sin (angle);
    }

    return std::sqrt (realPart * realPart + imaginaryPart * imaginaryPart);
}

//==============================================================================
/** Two-tap linear interpolation weights for a fractional delay `d` in [0, 1]. */
inline void linearCoefficients (double d, double* coefficients) noexcept
{
    coefficients[0] = 1.0 - d;
    coefficients[1] = d;
}

/**
    Four-tap, third-order Lagrange weights for a fractional delay `d` in [0, 1].

    The taps sit at delays \f$I-1, I, I+1, I+2\f$ and the interpolated point is at
    \f$I + d\f$, i.e. between the two inner taps. Centring the interpolation on the
    inner interval is what makes the 4-point form accurate; using the outer interval
    would waste most of its advantage over linear.

    Verified at the interval ends: \f$d=0\f$ yields \f$(0,1,0,0)\f$ and \f$d=1\f$
    yields \f$(0,0,1,0)\f$, so the interpolator passes exactly through the samples.
*/
inline void lagrangeCubicCoefficients (double d, double (&coefficients)[4]) noexcept
{
    const auto dPlus1  = d + 1.0;
    const auto dMinus1 = d - 1.0;
    const auto dMinus2 = d - 2.0;

    coefficients[0] = -d * dMinus1 * dMinus2 / 6.0;
    coefficients[1] =  dPlus1 * dMinus1 * dMinus2 / 2.0;
    coefficients[2] = -dPlus1 * d * dMinus2 / 2.0;
    coefficients[3] =  dPlus1 * d * dMinus1 / 6.0;
}

//==============================================================================
/**
    Phase delay of the one-pole lowpass \f$H(z) = (1-a)/(1 - a z^{-1})\f$.

    \f[ D(\omega) = \frac{1}{\omega}\arctan\frac{a\sin\omega}{1 - a\cos\omega} \f]

    \f$1 - a\cos\omega > 0\f$ for \f$0 \le a < 1\f$, so `atan` is safe. \f$a = 0\f$
    gives zero delay, as a pure gain must.
*/
[[nodiscard]] inline double onePolePhaseDelay (double a, double omega) noexcept
{
    if (omega < minimumOmega)
        return a / (1.0 - a);   // DC limit of the expression above

    return std::atan ((a * std::sin (omega)) / (1.0 - a * std::cos (omega))) / omega;
}

/** Magnitude of the same one-pole lowpass: \f$(1-a)/\sqrt{1 - 2a\cos\omega + a^2}\f$. */
[[nodiscard]] inline double onePoleMagnitude (double a, double omega) noexcept
{
    const auto denominator = std::sqrt (1.0 - 2.0 * a * std::cos (omega) + a * a);

    return denominator > 0.0 ? (1.0 - a) / denominator : 0.0;
}

//==============================================================================
/** Phase delay of the two-point average \f$(1 + z^{-1})/2\f$: exactly half a sample. */
[[nodiscard]] inline constexpr double twoPointAveragePhaseDelay() noexcept
{
    return 0.5;
}

/** Magnitude of the two-point average: \f$|\cos(\omega/2)|\f$. */
[[nodiscard]] inline double twoPointAverageMagnitude (double omega) noexcept
{
    return std::abs (std::cos (0.5 * omega));
}

} // namespace aethr::dsp::phase
