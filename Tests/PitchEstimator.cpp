#include "PitchEstimator.h"

#include <algorithm>
#include <cmath>
#include <complex>

#include "Core/AudioMath.h"

namespace aethr::testing
{

namespace
{
    /** Renormalisation interval for the phasor recurrence, in samples. */
    constexpr std::size_t phasorRenormaliseInterval = 512;

    /** Grid resolutions, in cents. Coarse locates the peak, fine resolves it. */
    constexpr double coarseStepCents = 2.0;
    constexpr double fineSpanCents = 4.0;
    constexpr double fineStepCents = 0.05;

    /**
        Windowed DTFT magnitude at one frequency.

        The phasor is advanced by complex multiplication rather than by calling `cos`
        and `sin` per sample, and renormalised periodically so the accumulated
        magnitude drift stays far below the precision the measurement needs.
    */
    [[nodiscard]] double magnitudeAt (const double* samples,
                                      std::size_t numSamples,
                                      const double* window,
                                      double sampleRate,
                                      double frequencyHz)
    {
        const auto omega = math::twoPi * frequencyHz / sampleRate;
        const std::complex<double> step { std::cos (omega), -std::sin (omega) };

        std::complex<double> phasor { 1.0, 0.0 };
        std::complex<double> sum { 0.0, 0.0 };

        for (std::size_t n = 0; n < numSamples; ++n)
        {
            sum += (window[n] * samples[n]) * phasor;
            phasor *= step;

            if (n % phasorRenormaliseInterval == phasorRenormaliseInterval - 1)
                phasor /= std::abs (phasor);
        }

        return std::abs (sum);
    }

    /**
        Peak of the parabola through three equally spaced points, as an offset in steps.

        Returns 0 when the samples do not describe a maximum.
    */
    [[nodiscard]] double parabolicPeakOffset (double left, double centre, double right)
    {
        const auto denominator = left - 2.0 * centre + right;

        if (! (denominator < 0.0))
            return 0.0;

        return 0.5 * (left - right) / denominator;
    }
}

//==============================================================================
PitchEstimate estimateFundamental (const double* samples,
                                   std::size_t numSamples,
                                   double sampleRate,
                                   double expectedHz,
                                   double searchCents)
{
    PitchEstimate estimate;

    if (samples == nullptr || numSamples < 64 || sampleRate <= 0.0 || expectedHz <= 0.0)
        return estimate;

    // Hann window: its transform has low enough sidelobes that neighbouring partials
    // do not shift the fundamental's peak.
    std::vector<double> window (numSamples);

    for (std::size_t n = 0; n < numSamples; ++n)
    {
        const auto progress = static_cast<double> (n) / static_cast<double> (numSamples - 1);
        window[n] = 0.5 - 0.5 * std::cos (math::twoPi * progress);
    }

    const auto evaluate = [&] (double cents)
    {
        return magnitudeAt (samples, numSamples, window.data(), sampleRate,
                            expectedHz * math::centsToRatio (cents));
    };

    // Coarse pass: locate the peak without assuming it is where it was asked for.
    auto bestCents = 0.0;
    auto bestMagnitude = -1.0;

    for (auto cents = -searchCents; cents <= searchCents; cents += coarseStepCents)
    {
        const auto magnitude = evaluate (cents);

        if (magnitude > bestMagnitude)
        {
            bestMagnitude = magnitude;
            bestCents = cents;
        }
    }

    // Fine pass, then parabolic interpolation between the three best grid points.
    auto refinedCents = bestCents;
    auto refinedMagnitude = bestMagnitude;

    for (auto cents = bestCents - fineSpanCents; cents <= bestCents + fineSpanCents; cents += fineStepCents)
    {
        const auto magnitude = evaluate (cents);

        if (magnitude > refinedMagnitude)
        {
            refinedMagnitude = magnitude;
            refinedCents = cents;
        }
    }

    const auto left = evaluate (refinedCents - fineStepCents);
    const auto right = evaluate (refinedCents + fineStepCents);
    const auto offset = parabolicPeakOffset (left, refinedMagnitude, right);

    const auto finalCents = refinedCents + offset * fineStepCents;

    estimate.frequencyHz = expectedHz * math::centsToRatio (finalCents);
    estimate.peakMagnitude = refinedMagnitude;
    estimate.valid = refinedMagnitude > 0.0;

    return estimate;
}

PitchEstimate estimateFundamental (const std::vector<double>& samples,
                                   double sampleRate,
                                   double expectedHz,
                                   double searchCents)
{
    return estimateFundamental (samples.data(), samples.size(), sampleRate, expectedHz, searchCents);
}

} // namespace aethr::testing
