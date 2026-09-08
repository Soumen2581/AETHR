#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <vector>

#include "DSP/Exciter.h"

/**
    Excitation tests.

    Two properties matter beyond "it makes a noise". First, determinism: a preset that
    uses randomisation must still reproduce exactly when its seed is locked, or presets
    are not really presets. Second, the excitation must be DC-free and must start and
    end at zero, because it is injected into a feedback loop where a step or an offset
    does not decay at the rate the ear expects it to.
*/

using namespace aethr::dsp;

namespace
{
    constexpr double sampleRate = 48000.0;

    struct Burst
    {
        std::vector<double> left;
        std::vector<double> right;
    };

    [[nodiscard]] Burst render (Exciter& exciter, std::size_t numSamples)
    {
        Burst burst;
        burst.left.resize (numSamples);
        burst.right.resize (numSamples);

        for (std::size_t n = 0; n < numSamples; ++n)
        {
            const auto sample = exciter.processSample();
            burst.left[n] = sample.left;
            burst.right[n] = sample.right;
        }

        return burst;
    }

    [[nodiscard]] double energyOf (const std::vector<double>& samples)
    {
        return std::inner_product (samples.begin(), samples.end(), samples.begin(), 0.0);
    }

    [[nodiscard]] double meanOf (const std::vector<double>& samples)
    {
        if (samples.empty())
            return 0.0;

        return std::accumulate (samples.begin(), samples.end(), 0.0) / static_cast<double> (samples.size());
    }

    [[nodiscard]] double peakOf (const std::vector<double>& samples)
    {
        auto peak = 0.0;

        for (const auto value : samples)
            peak = std::max (peak, std::abs (value));

        return peak;
    }

    [[nodiscard]] bool allFinite (const std::vector<double>& samples)
    {
        return std::all_of (samples.begin(), samples.end(),
                            [] (double value) { return std::isfinite (value); });
    }

    [[nodiscard]] double correlationOf (const std::vector<double>& a, const std::vector<double>& b)
    {
        const auto denominator = std::sqrt (energyOf (a) * energyOf (b));

        if (denominator <= 0.0)
            return 0.0;

        return std::inner_product (a.begin(), a.end(), b.begin(), 0.0) / denominator;
    }

    [[nodiscard]] Exciter::Settings defaultSettings (ExcitationType type)
    {
        Exciter::Settings settings;
        settings.type = type;
        settings.burstMilliseconds = 8.0;
        settings.attack = 0.2;
        settings.colour = 0.0;
        settings.brightness = 0.7;
        settings.level = 1.0;
        settings.randomAmount = 0.0;
        settings.stereoSpread = 0.0;
        settings.velocityAmount = 0.0;

        return settings;
    }

    [[nodiscard]] const char* nameOf (ExcitationType type)
    {
        switch (type)
        {
            case ExcitationType::pluck:      return "pluck";
            case ExcitationType::noiseBurst: return "noise";
            case ExcitationType::pinkBurst:  return "pink";
            case ExcitationType::mallet:     return "mallet";
            case ExcitationType::click:      return "click";
            case ExcitationType::metallic:   return "metallic";
            case ExcitationType::bow:        return "bow";
            case ExcitationType::blow:       return "blow";
            case ExcitationType::numTypes:
            default:                         return "?";
        }
    }

    [[nodiscard]] std::vector<ExcitationType> allTypes()
    {
        std::vector<ExcitationType> types;

        for (auto index = 0; index < static_cast<int> (ExcitationType::numTypes); ++index)
            types.push_back (static_cast<ExcitationType> (index));

        return types;
    }
}

//==============================================================================
TEST_CASE ("Every excitation type produces finite, non-trivial output", "[exciter]")
{
    for (const auto type : allTypes())
    {
        Exciter exciter;
        exciter.prepare (sampleRate);
        exciter.setSettings (defaultSettings (type));
        exciter.noteOn (1.0, 12345u);

        const auto burst = render (exciter, 4096);

        CAPTURE (nameOf (type));

        REQUIRE (allFinite (burst.left));
        REQUIRE (allFinite (burst.right));
        REQUIRE (energyOf (burst.left) > 1.0e-9);
        REQUIRE (peakOf (burst.left) < 8.0);
    }
}

TEST_CASE ("A locked seed reproduces the excitation exactly", "[exciter][determinism]")
{
    for (const auto type : allTypes())
    {
        auto settings = defaultSettings (type);
        settings.randomAmount = 1.0;      // maximum variation, so a mismatch would show
        settings.stereoSpread = 0.6;

        Exciter first;
        first.prepare (sampleRate);
        first.setSettings (settings);
        first.noteOn (0.8, 777u);

        Exciter second;
        second.prepare (sampleRate);
        second.setSettings (settings);
        second.noteOn (0.8, 777u);

        const auto a = render (first, 8192);
        const auto b = render (second, 8192);

        CAPTURE (nameOf (type));

        REQUIRE (a.left == b.left);
        REQUIRE (a.right == b.right);
    }
}

TEST_CASE ("Different seeds produce different excitations", "[exciter][determinism]")
{
    auto settings = defaultSettings (ExcitationType::pluck);
    settings.randomAmount = 0.5;

    Exciter first;
    first.prepare (sampleRate);
    first.setSettings (settings);
    first.noteOn (0.8, 1u);

    Exciter second;
    second.prepare (sampleRate);
    second.setSettings (settings);
    second.noteOn (0.8, 2u);

    const auto a = render (first, 4096);
    const auto b = render (second, 4096);

    REQUIRE (a.left != b.left);

    // Different, but not merely a level difference: the noise itself is decorrelated.
    REQUIRE (std::abs (correlationOf (a.left, b.left)) < 0.5);
}

TEST_CASE ("The excitation is DC-free", "[exciter]")
{
    // The loop's damping filter has unity gain at DC, so a DC offset in the excitation
    // sits in the resonator decaying only at the loop-gain rate: audible as a thump and
    // as a bias on everything that follows.
    for (const auto type : allTypes())
    {
        auto settings = defaultSettings (type);
        settings.burstMilliseconds = 50.0;

        Exciter exciter;
        exciter.prepare (sampleRate);
        exciter.setSettings (settings);
        exciter.noteOn (1.0, 4242u);

        const auto burst = render (exciter, 16384);
        const auto mean = meanOf (burst.left);
        const auto peak = peakOf (burst.left);

        CAPTURE (nameOf (type), mean, peak);

        REQUIRE (std::abs (mean) < 0.02 * std::max (peak, 1.0e-6));
    }
}

TEST_CASE ("One-shot excitations start and finish at silence", "[exciter]")
{
    for (const auto type : allTypes())
    {
        if (isSustainedExcitation (type))
            continue;

        auto settings = defaultSettings (type);
        settings.burstMilliseconds = 10.0;
        settings.attack = 0.3;

        Exciter exciter;
        exciter.prepare (sampleRate);
        exciter.setSettings (settings);
        exciter.noteOn (1.0, 99u);

        const auto burst = render (exciter, 4096);

        CAPTURE (nameOf (type));

        // A raised-cosine envelope leaves from zero, so the first sample cannot be a step.
        REQUIRE (std::abs (burst.left[0]) < 1.0e-9);

        // And the burst really ends: no trickle to keep a voice awake forever.
        REQUIRE_FALSE (exciter.isActive());
        REQUIRE (burst.left.back() == 0.0);
    }
}

TEST_CASE ("Sustained excitations run until released", "[exciter]")
{
    for (const auto type : { ExcitationType::bow, ExcitationType::blow })
    {
        Exciter exciter;
        exciter.prepare (sampleRate);
        exciter.setSettings (defaultSettings (type));
        exciter.noteOn (1.0, 31337u);

        const auto held = render (exciter, static_cast<std::size_t> (sampleRate));

        CAPTURE (nameOf (type));

        REQUIRE (exciter.isActive());
        REQUIRE (exciter.isDriving());
        REQUIRE (energyOf (held.left) > 1.0e-6);

        exciter.noteOff();

        const auto releasing = render (exciter, static_cast<std::size_t> (sampleRate * 0.2));

        REQUIRE_FALSE (exciter.isActive());
        REQUIRE (allFinite (releasing.left));

        // The release must actually reach zero rather than asymptote audibly.
        REQUIRE (std::abs (releasing.left.back()) < 1.0e-9);
    }
}

TEST_CASE ("Stereo spread controls channel decorrelation", "[exciter][stereo]")
{
    auto settings = defaultSettings (ExcitationType::noiseBurst);
    settings.burstMilliseconds = 40.0;

    const auto renderWithSpread = [&settings] (double spread)
    {
        settings.stereoSpread = spread;

        Exciter exciter;
        exciter.prepare (sampleRate);
        exciter.setSettings (settings);
        exciter.noteOn (1.0, 5150u);

        return render (exciter, 4096);
    };

    const auto mono = renderWithSpread (0.0);
    REQUIRE (mono.left == mono.right);

    const auto wide = renderWithSpread (1.0);
    REQUIRE (std::abs (correlationOf (wide.left, wide.right)) < 0.2);

    // Level must not dip in the middle of the control, which a naive blend would cause.
    const auto middle = renderWithSpread (0.5);
    const auto monoEnergy = energyOf (mono.left);
    const auto middleEnergy = energyOf (middle.left);

    CAPTURE (monoEnergy, middleEnergy);
    REQUIRE (middleEnergy > monoEnergy * 0.5);
    REQUIRE (middleEnergy < monoEnergy * 2.0);
}

TEST_CASE ("Velocity scales the excitation only when asked to", "[exciter][velocity]")
{
    const auto peakForVelocity = [] (double velocity, double amount)
    {
        auto settings = defaultSettings (ExcitationType::pluck);
        settings.velocityAmount = amount;

        Exciter exciter;
        exciter.prepare (sampleRate);
        exciter.setSettings (settings);
        exciter.noteOn (velocity, 1000u);

        return peakOf (render (exciter, 4096).left);
    };

    // Ignored at zero amount.
    REQUIRE (peakForVelocity (0.2, 0.0) == Catch::Approx (peakForVelocity (1.0, 0.0)));

    // Monotonic when enabled.
    REQUIRE (peakForVelocity (0.2, 1.0) < peakForVelocity (0.6, 1.0));
    REQUIRE (peakForVelocity (0.6, 1.0) < peakForVelocity (1.0, 1.0));
}

TEST_CASE ("Burst length and attack behave as described", "[exciter]")
{
    const auto activeSamples = [] (double milliseconds, double attack)
    {
        auto settings = defaultSettings (ExcitationType::noiseBurst);
        settings.burstMilliseconds = milliseconds;
        settings.attack = attack;

        Exciter exciter;
        exciter.prepare (sampleRate);
        exciter.setSettings (settings);
        exciter.noteOn (1.0, 8u);

        std::size_t count = 0;

        while (exciter.isActive() && count < 1000000)
        {
            (void) exciter.processSample();
            ++count;
        }

        return count;
    };

    const auto shortBurst = activeSamples (2.0, 0.2);
    const auto longBurst = activeSamples (100.0, 0.2);

    CAPTURE (shortBurst, longBurst);

    REQUIRE (longBurst > shortBurst * 10);

    // 100 ms at 48 kHz is 4800 samples, give or take the envelope's own rounding.
    REQUIRE (longBurst > 4000);
    REQUIRE (longBurst < 5600);

    // Attack shape changes the envelope, not the total length.
    REQUIRE (activeSamples (50.0, 0.0) == Catch::Approx (static_cast<double> (activeSamples (50.0, 0.9))).epsilon (0.05));
}

TEST_CASE ("Colour tilts the spectrum in the direction it says", "[exciter]")
{
    // Measured as the energy of the first difference, which rises with high-frequency
    // content: a cheap but unambiguous brightness proxy.
    const auto highFrequencyRatio = [] (double colour)
    {
        auto settings = defaultSettings (ExcitationType::noiseBurst);
        settings.burstMilliseconds = 60.0;
        settings.colour = colour;
        settings.brightness = 1.0;

        Exciter exciter;
        exciter.prepare (sampleRate);
        exciter.setSettings (settings);
        exciter.noteOn (1.0, 606u);

        const auto burst = render (exciter, 8192);

        std::vector<double> difference (burst.left.size(), 0.0);

        for (std::size_t n = 1; n < burst.left.size(); ++n)
            difference[n] = burst.left[n] - burst.left[n - 1];

        const auto total = energyOf (burst.left);

        return total > 0.0 ? energyOf (difference) / total : 0.0;
    };

    const auto dark = highFrequencyRatio (-1.0);
    const auto neutral = highFrequencyRatio (0.0);
    const auto bright = highFrequencyRatio (1.0);

    CAPTURE (dark, neutral, bright);

    REQUIRE (dark < neutral);
    REQUIRE (neutral < bright);
}
