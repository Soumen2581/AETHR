#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "Core/AudioMath.h"
#include "DSP/KarplusResonator.h"

/**
    Resonator-level tests: tuning, decay and — above all — stability.

    A feedback loop with a user-controllable gain is the one place in a synthesiser
    where a bad parameter combination can produce not a wrong sound but an escalating
    one. These tests try to make that happen.
*/

using namespace aethr;
using namespace aethr::dsp;

namespace
{
    /** Excites the resonator with a short burst and returns its output. */
    [[nodiscard]] std::vector<double> renderResonator (KarplusResonator& resonator,
                                                       std::size_t numSamples,
                                                       std::size_t burstSamples = 32,
                                                       double burstAmplitude = 1.0)
    {
        std::vector<double> output (numSamples, 0.0);

        // Deterministic pseudo-noise burst; the resonator does not care what shape it is.
        std::uint32_t state = 22222u;

        for (std::size_t n = 0; n < numSamples; ++n)
        {
            auto excitation = 0.0;

            if (n < burstSamples)
            {
                state ^= state << 13;
                state ^= state >> 17;
                state ^= state << 5;
                excitation = burstAmplitude * (static_cast<double> (state) * (2.0 / 4294967296.0) - 1.0);
            }

            output[n] = resonator.processSample (excitation);
        }

        return output;
    }

    [[nodiscard]] double peakOf (const std::vector<double>& samples, std::size_t from, std::size_t to)
    {
        auto peak = 0.0;

        for (auto n = from; n < std::min (to, samples.size()); ++n)
            peak = std::max (peak, std::abs (samples[n]));

        return peak;
    }

    [[nodiscard]] bool allFinite (const std::vector<double>& samples)
    {
        return std::all_of (samples.begin(), samples.end(),
                            [] (double value) { return std::isfinite (value); });
    }

    /** Measures T60 from the decay of the block-wise peak envelope. */
    [[nodiscard]] double measureDecayTime (const std::vector<double>& samples, double sampleRate)
    {
        constexpr std::size_t windowSamples = 256;

        // Reference taken after the excitation has finished, so the burst itself does
        // not count as part of the decay.
        const auto referenceStart = windowSamples * 4;
        const auto reference = peakOf (samples, referenceStart, referenceStart + windowSamples);

        if (reference <= 0.0)
            return 0.0;

        const auto target = reference * 0.001;   // -60 dB

        for (auto start = referenceStart; start + windowSamples < samples.size(); start += windowSamples)
            if (peakOf (samples, start, start + windowSamples) < target)
                return static_cast<double> (start - referenceStart) / sampleRate;

        return std::numeric_limits<double>::infinity();
    }
}

//==============================================================================
TEST_CASE ("The resonator tunes its loop to the requested frequency", "[resonator][tuning]")
{
    for (const auto sampleRate : { 44100.0, 48000.0, 96000.0, 192000.0 })
    {
        KarplusResonator resonator;
        resonator.prepare (sampleRate);

        for (const auto mode : { InterpolationMode::linear,
                                 InterpolationMode::lagrangeCubic,
                                 InterpolationMode::allpass })
        {
            resonator.setInterpolationMode (mode);

            for (const auto filterMode : { LoopFilterMode::oneZero,
                                           LoopFilterMode::onePole,
                                           LoopFilterMode::twoPole })
            {
                resonator.setLoopFilterMode (filterMode);

                for (const auto damping : { 0.0, 0.4, 1.0 })
                {
                    resonator.setDamping (damping);

                    for (const auto frequency : { 27.5, 55.0, 220.0, 440.0, 1760.0, 4186.0 })
                    {
                        resonator.setFrequency (frequency);
                        resonator.setDecayTime (5.0);
                        resonator.updateCoefficients();
                        resonator.snapToTargets();

                        const auto tuned = resonator.getTunedFrequency();
                        const auto error = math::ratioToCents (tuned, frequency);

                        CAPTURE (sampleRate, static_cast<int> (mode), static_cast<int> (filterMode),
                                 damping, frequency, tuned, error);

                        // The loop's own arithmetic must be essentially exact; audible
                        // pitch error is measured separately in PitchTests.
                        REQUIRE (std::abs (error) < 0.01);
                    }
                }
            }
        }
    }
}

TEST_CASE ("Loop gain is always strictly below unity", "[resonator][stability]")
{
    KarplusResonator resonator;
    resonator.prepare (48000.0);

    // Sweep the entire reachable parameter space, including the combinations a user
    // would reach for when trying to make it misbehave.
    for (const auto decay : { 0.0, 0.001, 0.02, 1.0, 30.0, 1000.0, 1.0e9 })
    {
        for (const auto damping : { 0.0, 0.5, 1.0 })
        {
            for (const auto brightness : { 0.0, 0.5, 1.0 })
            {
                for (const auto frequency : { 6.0, 27.5, 440.0, 4186.0, 12000.0, 30000.0 })
                {
                    resonator.setDecayTime (decay);
                    resonator.setDamping (damping);
                    resonator.setBrightness (brightness);
                    resonator.setFrequency (frequency);
                    resonator.updateCoefficients();
                    resonator.snapToTargets();

                    CAPTURE (decay, damping, brightness, frequency, resonator.getLoopGain());

                    REQUIRE (std::isfinite (resonator.getLoopGain()));
                    REQUIRE (resonator.getLoopGain() >= 0.0);
                    REQUIRE (resonator.getLoopGain() <= KarplusResonator::maximumLoopGain);
                }
            }
        }
    }
}

TEST_CASE ("An extreme setting decays rather than growing", "[resonator][stability]")
{
    // The worst case for runaway: the longest decay the parameter offers, no damping
    // to remove energy, and a high note so the loop is short and recirculates often.
    for (const auto sampleRate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        KarplusResonator resonator;
        resonator.prepare (sampleRate);
        resonator.setDecayTime (30.0);
        resonator.setDamping (0.0);
        resonator.setBrightness (1.0);
        resonator.setFrequency (3000.0);
        resonator.updateCoefficients();
        resonator.snapToTargets();

        const auto output = renderResonator (resonator, static_cast<std::size_t> (sampleRate * 2.0));

        const auto earlyPeak = peakOf (output, 64, 4096);
        const auto latePeak = peakOf (output, output.size() - 4096, output.size());

        CAPTURE (sampleRate, resonator.getLoopGain(), earlyPeak, latePeak);

        REQUIRE (allFinite (output));
        REQUIRE (earlyPeak > 0.0);

        // Not merely bounded: strictly non-increasing over two seconds.
        REQUIRE (latePeak <= earlyPeak);
    }
}

TEST_CASE ("A poisoned excitation cannot poison the loop", "[resonator][stability]")
{
    KarplusResonator resonator;
    resonator.prepare (48000.0);
    resonator.setFrequency (220.0);
    resonator.setDecayTime (4.0);
    resonator.updateCoefficients();
    resonator.snapToTargets();

    // Feed it the three values that would normally end the session.
    for (const auto poison : { std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity() })
    {
        for (auto n = 0; n < 64; ++n)
            (void) resonator.processSample (poison);

        // Then a clean burst, and it must be producing usable audio again.
        const auto recovery = renderResonator (resonator, 8192);

        CAPTURE (poison);
        REQUIRE (allFinite (recovery));
    }
}

TEST_CASE ("A huge excitation stays bounded", "[resonator][stability]")
{
    KarplusResonator resonator;
    resonator.prepare (48000.0);
    resonator.setFrequency (110.0);
    resonator.setDecayTime (30.0);
    resonator.setDamping (0.0);
    resonator.updateCoefficients();
    resonator.snapToTargets();

    const auto output = renderResonator (resonator, 200000, 20000, 1000.0);

    REQUIRE (allFinite (output));

    // The ceiling is a backstop, not a limiter that is expected to work musically,
    // but it must hold absolutely.
    REQUIRE (peakOf (output, 0, output.size()) <= static_cast<double> (guards::feedbackCeiling) + 1.0e-9);
}

//==============================================================================
TEST_CASE ("Decay time is delivered, or honestly reported as limited", "[resonator][decay]")
{
    constexpr double sampleRate = 48000.0;

    for (const auto requested : { 0.15, 0.5, 2.0, 6.0 })
    {
        KarplusResonator resonator;
        resonator.prepare (sampleRate);
        resonator.setFrequency (220.0);
        resonator.setDamping (0.0);          // isolate the loop gain from filter loss
        resonator.setDecayTime (requested);
        resonator.updateCoefficients();
        resonator.snapToTargets();

        REQUIRE_FALSE (resonator.isDecayLimitedByLoss());
        REQUIRE (resonator.getAchievedDecayTime() == Catch::Approx (requested).epsilon (0.02));

        const auto output = renderResonator (resonator, static_cast<std::size_t> (sampleRate * (requested * 1.6 + 0.2)));
        const auto measured = measureDecayTime (output, sampleRate);

        CAPTURE (requested, resonator.getLoopGain(), measured);

        // A pluck's envelope is not a clean exponential - the higher partials leave
        // first - so this checks the right order of magnitude, not the third decimal.
        REQUIRE (measured > requested * 0.5);
        REQUIRE (measured < requested * 1.8);
    }
}

TEST_CASE ("Damping shortens the decay and the resonator says so", "[resonator][decay]")
{
    KarplusResonator resonator;
    resonator.prepare (48000.0);
    resonator.setFrequency (2000.0);
    resonator.setBrightness (0.3);
    resonator.setDecayTime (30.0);

    resonator.setDamping (0.0);
    resonator.updateCoefficients();
    resonator.snapToTargets();
    const auto undampedDecay = resonator.getAchievedDecayTime();

    resonator.setDamping (1.0);
    resonator.updateCoefficients();
    resonator.snapToTargets();
    const auto dampedDecay = resonator.getAchievedDecayTime();

    CAPTURE (undampedDecay, dampedDecay);

    // Heavy damping at 2 kHz cannot sustain for 30 seconds - each of the thousands of
    // round trips per second loses energy - and the resonator must admit that rather
    // than silently pretending it complied.
    REQUIRE (dampedDecay < undampedDecay);
    REQUIRE (resonator.isDecayLimitedByLoss());
}

TEST_CASE ("Zero damping is a genuine bypass, so long decays stay reachable", "[resonator][decay]")
{
    // High notes are the test: with any residual filter loss, the decay at C8 would be
    // capped in the hundreds of milliseconds no matter what the decay control said.
    KarplusResonator resonator;
    resonator.prepare (48000.0);
    resonator.setFrequency (4186.0);
    resonator.setDamping (0.0);
    resonator.setDecayTime (20.0);
    resonator.updateCoefficients();
    resonator.snapToTargets();

    REQUIRE (resonator.getFilterCoefficient() == Catch::Approx (0.0));
    REQUIRE_FALSE (resonator.isDecayLimitedByLoss());
    REQUIRE (resonator.getAchievedDecayTime() == Catch::Approx (20.0).epsilon (0.05));
}

TEST_CASE ("Changing pitch mid-note glides without discontinuity", "[resonator][smoothing]")
{
    constexpr double sampleRate = 48000.0;

    KarplusResonator resonator;
    resonator.prepare (sampleRate);
    resonator.setFrequency (220.0);
    resonator.setDecayTime (10.0);
    resonator.updateCoefficients();
    resonator.snapToTargets();

    (void) renderResonator (resonator, 8192);

    // Jump a fifth. A stepped delay length would show up as a sample-to-sample jump
    // far larger than the signal's own slew rate.
    resonator.setFrequency (330.0);
    resonator.updateCoefficients();

    std::vector<double> output (4096, 0.0);
    auto largestStep = 0.0;
    auto previous = 0.0;

    for (std::size_t n = 0; n < output.size(); ++n)
    {
        output[n] = resonator.processSample (0.0);
        largestStep = std::max (largestStep, std::abs (output[n] - previous));
        previous = output[n];
    }

    CAPTURE (largestStep, peakOf (output, 0, output.size()));

    REQUIRE (allFinite (output));
    REQUIRE (largestStep < 0.5 * peakOf (output, 0, output.size()) + 0.05);
}
