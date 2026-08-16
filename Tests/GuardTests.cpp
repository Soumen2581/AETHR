#include <catch2/catch_test_macros.hpp>

#include <array>
#include <limits>

#include "Core/RealtimeGuards.h"

namespace guards = strata::guards;

namespace
{
    constexpr auto quietNaN  = std::numeric_limits<float>::quiet_NaN();
    constexpr auto positiveInfinity = std::numeric_limits<float>::infinity();
    constexpr auto negativeInfinity = -std::numeric_limits<float>::infinity();
    constexpr auto denormal = std::numeric_limits<float>::denorm_min();
} // namespace

TEST_CASE ("sanitise replaces non-finite values with zero and passes everything else through", "[guards]")
{
    REQUIRE (guards::sanitise (quietNaN) == 0.0f);
    REQUIRE (guards::sanitise (positiveInfinity) == 0.0f);
    REQUIRE (guards::sanitise (negativeInfinity) == 0.0f);

    REQUIRE (guards::sanitise (0.5f) == 0.5f);
    REQUIRE (guards::sanitise (-1.75f) == -1.75f);
    REQUIRE (guards::sanitise (0.0f) == 0.0f);

    // Large but finite values must survive: clipping is the output stage's job,
    // not the NaN guard's.
    REQUIRE (guards::sanitise (1000.0f) == 1000.0f);
}

TEST_CASE ("flushDenormals zeroes only denormal magnitudes", "[guards]")
{
    REQUIRE (guards::flushDenormals (denormal) == 0.0f);
    REQUIRE (guards::flushDenormals (-denormal) == 0.0f);
    REQUIRE (guards::flushDenormals (1.0e-20f) == 0.0f);

    REQUIRE (guards::flushDenormals (1.0e-6f) == 1.0e-6f);
    REQUIRE (guards::flushDenormals (1.0f) == 1.0f);
}

TEST_CASE ("sanitiseState makes any input safe to store in recursive state", "[guards]")
{
    // Non-finite input is neutralised.
    REQUIRE (guards::sanitiseState (quietNaN) == 0.0f);
    REQUIRE (guards::sanitiseState (positiveInfinity) == 0.0f);

    // Denormals are flushed so a decaying loop settles instead of stalling the FPU.
    REQUIRE (guards::sanitiseState (denormal) == 0.0f);

    // Runaway magnitudes are clamped, not zeroed: the loop stays continuous.
    REQUIRE (guards::sanitiseState (1000.0f) == guards::feedbackCeiling);
    REQUIRE (guards::sanitiseState (-1000.0f) == -guards::feedbackCeiling);

    // Ordinary audio passes through untouched.
    REQUIRE (guards::sanitiseState (0.25f) == 0.25f);

    // An explicit ceiling is honoured.
    REQUIRE (guards::sanitiseState (5.0f, 1.0f) == 1.0f);
}

TEST_CASE ("Block helpers detect, repair and measure buffers", "[guards]")
{
    std::array<float, 8> block { 0.0f, 0.5f, quietNaN, -0.25f, positiveInfinity, 0.1f, -2.0f, 0.0f };

    REQUIRE (guards::containsNonFinite (block.data(), block.size()));

    // peakMagnitude must ignore the poisoned samples rather than returning NaN.
    REQUIRE (guards::peakMagnitude (block.data(), block.size()) == 2.0f);

    REQUIRE (guards::sanitiseBlock (block.data(), block.size()) == 2u);
    REQUIRE_FALSE (guards::containsNonFinite (block.data(), block.size()));
    REQUIRE (block[2] == 0.0f);
    REQUIRE (block[4] == 0.0f);

    // Untouched samples must be preserved exactly.
    REQUIRE (block[1] == 0.5f);
    REQUIRE (block[3] == -0.25f);

    const std::array<float, 4> clean { 0.0f, 0.1f, -0.2f, 0.05f };
    REQUIRE_FALSE (guards::containsNonFinite (clean.data(), clean.size()));
    REQUIRE (guards::sanitiseBlock (std::array<float, 1> { 1.0f }.data(), 1u) == 0u);
}

TEST_CASE ("clampLoopGain keeps feedback strictly stable", "[guards][stability]")
{
    REQUIRE (guards::clampLoopGain (0.5) == 0.5);

    // Unity and above must be pulled below 1 so the loop cannot sustain or grow.
    REQUIRE (guards::clampLoopGain (1.0) < 1.0);
    REQUIRE (guards::clampLoopGain (5.0) < 1.0);

    // Negative and invalid requests collapse to zero.
    REQUIRE (guards::clampLoopGain (-0.5) == 0.0);
    REQUIRE (guards::clampLoopGain (std::numeric_limits<double>::quiet_NaN()) == 0.0);
    REQUIRE (guards::clampLoopGain (std::numeric_limits<double>::infinity()) == 0.0);

    // A caller-supplied maximum is respected.
    REQUIRE (guards::clampLoopGain (0.99, 0.9) == 0.9);
}

TEST_CASE ("isValidCoefficient rejects values that would destabilise a filter", "[guards][stability]")
{
    REQUIRE (guards::isValidCoefficient (0.5));
    REQUIRE (guards::isValidCoefficient (0.5, 0.0, 1.0));

    REQUIRE_FALSE (guards::isValidCoefficient (std::numeric_limits<double>::quiet_NaN()));
    REQUIRE_FALSE (guards::isValidCoefficient (std::numeric_limits<double>::infinity()));
    REQUIRE_FALSE (guards::isValidCoefficient (1.5, 0.0, 1.0));
    REQUIRE_FALSE (guards::isValidCoefficient (-0.1, 0.0, 1.0));
}
