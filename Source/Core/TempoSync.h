#pragma once

#include <algorithm>
#include <cmath>

/**
    Musical time conversions against host tempo.

    A division is the period of one LFO/chaos/chorus/phaser cycle, or the delay
    time of one tap. A quarter note is one beat. Hosts that do not publish a
    tempo fall back to 120 BPM so synced rates stay defined in the standalone.
*/
namespace aethr::sync
{

inline constexpr int numDivisions = 14;
inline constexpr int defaultDivisionIndex = 2;     // 1/4
inline constexpr int defaultDelayDivisionL = 3;    // 1/8
inline constexpr int defaultDelayDivisionR = 8;    // 1/8 dotted

inline constexpr const char* labels[numDivisions] {
    "1/1", "1/2", "1/4", "1/8", "1/16", "1/32",
    "1/2 D", "1/4 D", "1/8 D", "1/16 D",
    "1/2 T", "1/4 T", "1/8 T", "1/16 T"
};

/** Length of each division in quarter-note beats. */
inline constexpr double beats[numDivisions] {
    4.0, 2.0, 1.0, 0.5, 0.25, 0.125,
    3.0, 1.5, 0.75, 0.375,
    4.0 / 3.0, 2.0 / 3.0, 1.0 / 3.0, 1.0 / 6.0
};

inline constexpr double defaultBpm = 120.0;
inline constexpr double minBpm = 20.0;
inline constexpr double maxBpm = 400.0;

/** Delay buffer is two seconds; leave a sample of headroom. */
inline constexpr double minDelaySeconds = 0.01;
inline constexpr double maxDelaySeconds = 1.9;

[[nodiscard]] inline double clampBpm (double bpm) noexcept
{
    if (! std::isfinite (bpm))
        return defaultBpm;

    return std::clamp (bpm, minBpm, maxBpm);
}

[[nodiscard]] inline int clampDivision (int index) noexcept
{
    return std::clamp (index, 0, numDivisions - 1);
}

[[nodiscard]] inline double seconds (int divisionIndex, double bpm) noexcept
{
    return beats[clampDivision (divisionIndex)] * (60.0 / clampBpm (bpm));
}

[[nodiscard]] inline double hertz (int divisionIndex, double bpm) noexcept
{
    const auto period = seconds (divisionIndex, bpm);
    return period > 1.0e-12 ? 1.0 / period : 1.0;
}

[[nodiscard]] inline double delaySeconds (int divisionIndex, double bpm) noexcept
{
    return std::clamp (seconds (divisionIndex, bpm), minDelaySeconds, maxDelaySeconds);
}

} // namespace aethr::sync
