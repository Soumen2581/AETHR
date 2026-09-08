#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <complex>
#include <vector>

#include "DSP/FractionalDelayLine.h"
#include "DSP/LoopFilter.h"
#include "DSP/PhaseDelay.h"

/**
    Verifies the closed-form phase-delay expressions against measurement.

    The closed forms are the foundation the whole tuning system stands on, and they are
    the kind of algebra that can be subtly wrong — a sign, a factor of two, a wrapped
    branch — while still producing plausible-looking numbers. So they are checked
    against a brute-force DFT of the actual impulse response of the actual filter,
    computed here rather than by the code under test.
*/

using namespace aethr::dsp;

namespace
{
    constexpr double testTolerance = 1.0e-6;

    /** Independent DFT: phase delay of an impulse response, unwrapped against a nominal value. */
    [[nodiscard]] double measuredPhaseDelay (const std::vector<double>& impulseResponse,
                                             double omega,
                                             double nominalDelay)
    {
        std::complex<double> sum { 0.0, 0.0 };

        for (std::size_t n = 0; n < impulseResponse.size(); ++n)
        {
            const auto angle = omega * static_cast<double> (n);
            sum += impulseResponse[n] * std::complex<double> { std::cos (angle), -std::sin (angle) };
        }

        const auto principal = -std::atan2 (sum.imag(), sum.real()) / omega;
        const auto samplesPerWrap = 2.0 * aethr::dsp::phase::pi / omega;
        const auto wraps = std::round ((nominalDelay - principal) / samplesPerWrap);

        return principal + wraps * samplesPerWrap;
    }

    /** Impulse response of the first-order allpass A(z) = (c + z^-1) / (1 + c z^-1). */
    [[nodiscard]] std::vector<double> allpassImpulseResponse (double coefficient, std::size_t length)
    {
        std::vector<double> response (length, 0.0);

        auto lastInput = 0.0;
        auto lastOutput = 0.0;

        for (std::size_t n = 0; n < length; ++n)
        {
            const auto input = n == 0 ? 1.0 : 0.0;
            const auto output = coefficient * input + lastInput - coefficient * lastOutput;

            lastInput = input;
            lastOutput = output;
            response[n] = output;
        }

        return response;
    }

    /** Impulse response of the one-pole lowpass H(z) = (1-a) / (1 - a z^-1). */
    [[nodiscard]] std::vector<double> onePoleImpulseResponse (double a, std::size_t length)
    {
        std::vector<double> response (length, 0.0);
        auto state = 0.0;

        for (std::size_t n = 0; n < length; ++n)
        {
            const auto input = n == 0 ? 1.0 : 0.0;
            state = (1.0 - a) * input + a * state;
            response[n] = state;
        }

        return response;
    }
}

//==============================================================================
TEST_CASE ("Allpass phase delay matches its measured impulse response", "[dsp][phase]")
{
    constexpr std::size_t responseLength = 20000;

    for (const auto delta : { 0.5, 0.75, 1.0, 1.25, 1.4999 })
    {
        const auto coefficient = phase::allpassCoefficientForDelay (delta);
        const auto response = allpassImpulseResponse (coefficient, responseLength);

        for (const auto omega : { 0.01, 0.1, 0.5, 1.0, 2.0, 3.0 })
        {
            const auto closedForm = phase::allpassPhaseDelay (coefficient, omega);
            const auto measured = measuredPhaseDelay (response, omega, closedForm);

            CAPTURE (delta, coefficient, omega, closedForm, measured);
            REQUIRE (closedForm == Catch::Approx (measured).epsilon (testTolerance));
        }
    }
}

TEST_CASE ("Allpass delay equals its nominal fraction at low frequency", "[dsp][phase]")
{
    // The coefficient is derived from a target delay that is only exact at DC; the
    // whole point of the tuning solver is that it drifts above DC.
    for (const auto delta : { 0.5, 0.8, 1.0, 1.3 })
    {
        const auto coefficient = phase::allpassCoefficientForDelay (delta);

        REQUIRE (phase::allpassPhaseDelay (coefficient, 1.0e-4) == Catch::Approx (delta).epsilon (1.0e-6));
    }

    // Unity delay must be exactly z^-1.
    REQUIRE (phase::allpassCoefficientForDelay (1.0) == Catch::Approx (0.0));
}

TEST_CASE ("One-pole phase delay and magnitude match measurement", "[dsp][phase]")
{
    constexpr std::size_t responseLength = 60000;

    for (const auto a : { 0.0, 0.1, 0.5, 0.9, 0.98 })
    {
        const auto response = onePoleImpulseResponse (a, responseLength);

        for (const auto omega : { 0.01, 0.2, 0.7, 1.5, 2.8 })
        {
            const auto closedForm = phase::onePolePhaseDelay (a, omega);
            const auto measured = measuredPhaseDelay (response, omega, closedForm);

            CAPTURE (a, omega, closedForm, measured);
            REQUIRE (closedForm == Catch::Approx (measured).epsilon (1.0e-5));

            // Magnitude, measured the same independent way.
            std::complex<double> sum { 0.0, 0.0 };

            for (std::size_t n = 0; n < response.size(); ++n)
            {
                const auto angle = omega * static_cast<double> (n);
                sum += response[n] * std::complex<double> { std::cos (angle), -std::sin (angle) };
            }

            REQUIRE (phase::onePoleMagnitude (a, omega) == Catch::Approx (std::abs (sum)).epsilon (1.0e-5));
        }
    }
}

TEST_CASE ("Phase unwrapping recovers delays longer than half a period", "[dsp][phase]")
{
    // A three-sample delay at omega = 2.4 has a phase of -7.2 radians, which atan2
    // reports as +5.4. Without unwrapping the delay would read as -2.2 samples.
    constexpr double omega = 2.4;
    constexpr double trueDelay = 3.0;

    const auto realPart = std::cos (omega * trueDelay);
    const auto imaginaryPart = -std::sin (omega * trueDelay);

    REQUIRE (phase::unwrapPhaseDelay (realPart, imaginaryPart, omega, trueDelay)
             == Catch::Approx (trueDelay).epsilon (1.0e-9));

    // And it must not "fix" a value that was already on the right branch.
    REQUIRE (phase::unwrapPhaseDelay (std::cos (0.5), -std::sin (0.5), 0.5, 1.0)
             == Catch::Approx (1.0).epsilon (1.0e-9));
}

TEST_CASE ("Interpolator coefficient sets are well formed", "[dsp][phase]")
{
    SECTION ("linear weights sum to one")
    {
        for (const auto d : { 0.0, 0.25, 0.5, 0.75, 1.0 })
        {
            double taps[2];
            phase::linearCoefficients (d, taps);

            REQUIRE (taps[0] + taps[1] == Catch::Approx (1.0));
        }
    }

    SECTION ("Lagrange weights sum to one and interpolate exactly at the knots")
    {
        for (const auto d : { 0.0, 0.1, 0.5, 0.9, 1.0 })
        {
            double taps[4];
            phase::lagrangeCubicCoefficients (d, taps);

            REQUIRE (taps[0] + taps[1] + taps[2] + taps[3] == Catch::Approx (1.0).margin (1.0e-12));
        }

        double atZero[4];
        phase::lagrangeCubicCoefficients (0.0, atZero);
        REQUIRE (atZero[1] == Catch::Approx (1.0));
        REQUIRE (atZero[0] == Catch::Approx (0.0));
        REQUIRE (atZero[2] == Catch::Approx (0.0));
        REQUIRE (atZero[3] == Catch::Approx (0.0));

        double atOne[4];
        phase::lagrangeCubicCoefficients (1.0, atOne);
        REQUIRE (atOne[2] == Catch::Approx (1.0));
        REQUIRE (atOne[0] == Catch::Approx (0.0));
        REQUIRE (atOne[1] == Catch::Approx (0.0));
        REQUIRE (atOne[3] == Catch::Approx (0.0));
    }
}

//==============================================================================
TEST_CASE ("The delay line realises the phase delay it is asked for", "[dsp][delay]")
{
    FractionalDelayLine line;
    line.prepare (4096);

    for (const auto mode : { InterpolationMode::linear,
                             InterpolationMode::lagrangeCubic,
                             InterpolationMode::allpass })
    {
        line.setInterpolation (mode);

        for (const auto omega : { 0.005, 0.05, 0.4, 1.2 })
        {
            for (const auto target : { 8.3, 47.5, 100.0, 480.75, 1000.2 })
            {
                const auto request = line.solveDelayForPhaseDelay (target, omega);
                const auto achieved = line.phaseDelayFor (request, omega);

                CAPTURE (static_cast<int> (mode), omega, target, request, achieved);
                REQUIRE (achieved == Catch::Approx (target).epsilon (1.0e-9));
            }
        }
    }
}

TEST_CASE ("Only the allpass interpolator preserves magnitude", "[dsp][delay]")
{
    FractionalDelayLine line;
    line.prepare (1024);

    // Half-sample offsets are the worst case for an interpolating read. Inside a
    // feedback loop this loss is applied once per round trip, so a mode that loses
    // 0.5 dB there would make decay time depend on which note was played.
    constexpr double omega = 1.5;
    constexpr double halfSampleDelay = 100.5;

    line.setInterpolation (InterpolationMode::allpass);
    REQUIRE (line.magnitudeFor (halfSampleDelay, omega) == Catch::Approx (1.0));

    line.setInterpolation (InterpolationMode::linear);
    const auto linearMagnitude = line.magnitudeFor (halfSampleDelay, omega);
    REQUIRE (linearMagnitude < 0.95);

    line.setInterpolation (InterpolationMode::lagrangeCubic);
    const auto lagrangeMagnitude = line.magnitudeFor (halfSampleDelay, omega);

    // Lagrange is much flatter than linear, but still not flat.
    REQUIRE (lagrangeMagnitude > linearMagnitude);
    REQUIRE (lagrangeMagnitude < 1.0);
}

TEST_CASE ("Integer reads return exactly the sample that was written", "[dsp][delay]")
{
    FractionalDelayLine line;
    line.prepare (256);
    line.setInterpolation (InterpolationMode::linear);
    line.setDelay (4.0);

    // Write a ramp, then confirm the read lands on the expected element.
    for (auto i = 1; i <= 32; ++i)
    {
        const auto read = line.read();
        line.write (static_cast<double> (i));

        if (i > 4)
            REQUIRE (read == Catch::Approx (static_cast<double> (i - 4)));
    }
}

//==============================================================================
TEST_CASE ("Loop filter reports the phase and magnitude it actually has", "[dsp][loopfilter]")
{
    LoopFilter filter;
    filter.prepare (48000.0);

    SECTION ("one-zero at maximum damping is the two-point average")
    {
        filter.setMode (LoopFilterMode::oneZero);
        filter.setCoefficient (LoopFilter::maximumZeroWeight);

        REQUIRE (filter.phaseDelayAt (1.0) == Catch::Approx (phase::twoPointAveragePhaseDelay()));
        REQUIRE (filter.magnitudeAt (1.0) == Catch::Approx (phase::twoPointAverageMagnitude (1.0)));
    }

    SECTION ("a zero coefficient is a true bypass in every mode")
    {
        // This is what lets damping = 0 reach arbitrarily long decays: with any residual
        // loss the top of the keyboard would be capped at a fraction of a second.
        for (const auto mode : { LoopFilterMode::oneZero, LoopFilterMode::onePole, LoopFilterMode::twoPole })
        {
            filter.setMode (mode);
            filter.setCoefficient (0.0);

            REQUIRE (filter.magnitudeAt (0.7) == Catch::Approx (1.0));
            REQUIRE (filter.phaseDelayAt (0.7) == Catch::Approx (0.0).margin (1.0e-12));
            REQUIRE (filter.process (1.0) == Catch::Approx (1.0));
        }
    }

    SECTION ("two-pole loses twice the decibels and twice the phase of one-pole")
    {
        constexpr double omega = 0.6;
        constexpr double coefficient = 0.5;

        filter.setMode (LoopFilterMode::onePole);
        filter.setCoefficient (coefficient);
        const auto singleMagnitude = filter.magnitudeAt (omega);
        const auto singleDelay = filter.phaseDelayAt (omega);

        filter.setMode (LoopFilterMode::twoPole);
        filter.setCoefficient (coefficient);

        REQUIRE (filter.magnitudeAt (omega) == Catch::Approx (singleMagnitude * singleMagnitude));
        REQUIRE (filter.phaseDelayAt (omega) == Catch::Approx (2.0 * singleDelay));
    }

    SECTION ("coefficients are clamped into the stable range")
    {
        filter.setMode (LoopFilterMode::onePole);
        filter.setCoefficient (5.0);
        REQUIRE (filter.getCoefficient() <= LoopFilter::maximumPoleRadius);

        filter.setCoefficient (std::nan (""));
        REQUIRE (std::isfinite (filter.getCoefficient()));
    }
}
