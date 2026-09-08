#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#include "DSP/FxRack.h"

using Catch::Approx;

namespace
{
    [[nodiscard]] double maxSampleDelta (const std::vector<double>& signal)
    {
        double peak = 0.0;

        for (std::size_t i = 1; i < signal.size(); ++i)
            peak = std::max (peak, std::abs (signal[i] - signal[i - 1]));

        return peak;
    }
} // namespace

TEST_CASE ("Delay time jumps stay finite and crossfade instead of hard-cutting", "[fx][delay][regression]")
{
    aethr::dsp::FxRack rack;
    constexpr double sampleRate = 48000.0;
    rack.prepare (sampleRate, 512);

    aethr::dsp::FxRack::Settings settings;
    settings.delayMix = 1.0;
    settings.delayFeedback = 0.35;
    settings.delayTimeL = 0.12;
    settings.delayTimeR = 0.18;
    settings.filterMix = 0.0;
    settings.satMix = 0.0;
    settings.chorusMix = 0.0;
    settings.phaserMix = 0.0;
    settings.reverbMix = 0.0;
    rack.setSettings (settings);

    std::vector<double> left (2048, 0.0);
    std::vector<double> right (2048, 0.0);

    // Seed the delay with a short burst so a time jump has energy to mis-cut.
    for (int i = 0; i < 64; ++i)
    {
        const auto env = 1.0 - static_cast<double> (i) / 64.0;
        left[static_cast<std::size_t> (i)] = 0.7 * env;
        right[static_cast<std::size_t> (i)] = -0.55 * env;
    }

    rack.process (left.data(), right.data(), 512);

    settings.delayTimeL = 0.42;
    settings.delayTimeR = 0.08;
    rack.setSettings (settings);

    std::fill (left.begin() + 512, left.begin() + 1024, 0.0);
    std::fill (right.begin() + 512, right.begin() + 1024, 0.0);
    rack.process (left.data() + 512, right.data() + 512, 512);

    std::vector<double> jumpRegion (left.begin() + 512, left.begin() + 1024);

    for (const auto sample : jumpRegion)
    {
        REQUIRE (std::isfinite (sample));
        REQUIRE (std::abs (sample) < 2.0);
    }

    // A hard read-pointer jump on wet-only delay typically produces >0.5 spikes.
    // Crossfade keeps the discontinuity well below that for this seeded burst.
    REQUIRE (maxSampleDelta (jumpRegion) < 0.55);
}
