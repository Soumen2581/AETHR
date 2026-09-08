#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <limits>

#include "Core/AudioMath.h"
#include "Core/TempoSync.h"

using Catch::Approx;
namespace math = aethr::math;

TEST_CASE ("MIDI note to frequency matches 12-TET reference values", "[math][pitch]")
{
    // Reference values for A4 = 440 Hz, from the standard 12-TET table.
    struct Reference { double note; double hertz; };

    const Reference references[]
    {
        { 21.0,   27.5000 },   // A0
        { 24.0,   32.7032 },   // C1
        { 36.0,   65.4064 },   // C2
        { 48.0,  130.8128 },   // C3
        { 60.0,  261.6256 },   // C4 (middle C)
        { 69.0,  440.0000 },   // A4
        { 72.0,  523.2511 },   // C5
        { 84.0, 1046.5023 },   // C6
        { 96.0, 2093.0045 },   // C7
        { 108.0, 4186.0090 }   // C8
    };

    for (const auto& reference : references)
    {
        const auto computed = math::midiNoteToHertz (reference.note);

        // Agreement to 0.01 cent; the reference table itself is only quoted to
        // four decimal places, which is the limiting factor here.
        REQUIRE (std::abs (math::ratioToCents (computed, reference.hertz)) < 0.01);
    }
}

TEST_CASE ("Frequency and MIDI note conversions round-trip", "[math][pitch]")
{
    for (double note = 12.0; note <= 120.0; note += 0.25)
    {
        const auto hertz = math::midiNoteToHertz (note);
        REQUIRE (math::hertzToMidiNote (hertz) == Approx (note).epsilon (1.0e-12));
    }
}

TEST_CASE ("Cent and semitone ratios are consistent", "[math][pitch]")
{
    REQUIRE (math::semitonesToRatio (12.0) == Approx (2.0).epsilon (1.0e-12));
    REQUIRE (math::semitonesToRatio (0.0) == Approx (1.0).epsilon (1.0e-12));
    REQUIRE (math::centsToRatio (100.0) == Approx (math::semitonesToRatio (1.0)).epsilon (1.0e-12));
    REQUIRE (math::centsToRatio (-1200.0) == Approx (0.5).epsilon (1.0e-12));

    // One cent up from 440 Hz must measure as exactly one cent.
    REQUIRE (math::ratioToCents (440.0 * math::centsToRatio (1.0), 440.0) == Approx (1.0).epsilon (1.0e-9));
}

TEST_CASE ("Decibel conversions round-trip and clamp at silence", "[math][gain]")
{
    for (double db = -90.0; db <= 12.0; db += 0.5)
        REQUIRE (math::gainToDecibels (math::decibelsToGain (db)) == Approx (db).epsilon (1.0e-9));

    REQUIRE (math::decibelsToGain (0.0) == Approx (1.0));
    REQUIRE (math::decibelsToGain (-6.0206) == Approx (0.5).epsilon (1.0e-4));

    // At or below the floor, the gain must be exactly zero rather than very small,
    // so a fader at minimum produces true silence.
    REQUIRE (math::decibelsToGain (-100.0) == 0.0);
    REQUIRE (math::decibelsToGain (-120.0) == 0.0);
    REQUIRE (math::gainToDecibels (0.0) == -100.0);
}

TEST_CASE ("Loop gain and T60 decay time are exact inverses", "[math][decay]")
{
    const double sampleRates[] { 44100.0, 48000.0, 96000.0, 192000.0 };
    const double delaysInSamples[] { 16.0, 100.0, 441.0, 2205.0 };
    const double decayTimes[] { 0.05, 0.5, 2.0, 30.0 };

    for (const auto sampleRate : sampleRates)
    {
        for (const auto delay : delaysInSamples)
        {
            for (const auto t60 : decayTimes)
            {
                const auto gain = math::decayTimeToLoopGain (t60, delay, sampleRate);

                REQUIRE (gain > 0.0);
                REQUIRE (gain < 1.0);
                REQUIRE (math::loopGainToDecayTime (gain, delay, sampleRate) == Approx (t60).epsilon (1.0e-9));
            }
        }
    }
}

TEST_CASE ("Loop gain produces the requested 60 dB decay when iterated", "[math][decay]")
{
    // Simulate the loop directly: after t60 seconds the amplitude must be -60 dB.
    constexpr double sampleRate = 48000.0;
    constexpr double delaySamples = 240.0;   // 200 Hz fundamental
    constexpr double t60 = 1.5;

    const auto gain = math::decayTimeToLoopGain (t60, delaySamples, sampleRate);
    const auto iterations = static_cast<int> ((t60 * sampleRate) / delaySamples);

    auto amplitude = 1.0;

    for (int i = 0; i < iterations; ++i)
        amplitude *= gain;

    REQUIRE (math::gainToDecibels (amplitude) == Approx (-60.0).margin (0.1));
}

TEST_CASE ("Degenerate decay arguments fail safe rather than producing infinities", "[math][decay]")
{
    REQUIRE (math::decayTimeToLoopGain (0.0, 100.0, 48000.0) == 0.0);
    REQUIRE (math::decayTimeToLoopGain (-1.0, 100.0, 48000.0) == 0.0);
    REQUIRE (math::decayTimeToLoopGain (1.0, 0.0, 48000.0) == 0.0);
    REQUIRE (math::decayTimeToLoopGain (1.0, 100.0, 0.0) == 0.0);
    REQUIRE (std::isfinite (math::decayTimeToLoopGain (1.0e9, 1.0, 48000.0)));
}

TEST_CASE ("Musical divisions convert to seconds and hertz against tempo", "[math][tempo]")
{
    namespace sync = aethr::sync;

    REQUIRE (sync::seconds (sync::defaultDivisionIndex, 120.0) == Approx (0.5).epsilon (1.0e-12));
    REQUIRE (sync::hertz (sync::defaultDivisionIndex, 120.0) == Approx (2.0).epsilon (1.0e-12));
    REQUIRE (sync::seconds (3, 120.0) == Approx (0.25).epsilon (1.0e-12));   // 1/8
    REQUIRE (sync::hertz (3, 120.0) == Approx (4.0).epsilon (1.0e-12));
    REQUIRE (sync::seconds (7, 120.0) == Approx (0.75).epsilon (1.0e-12));   // 1/4 dotted
    REQUIRE (sync::seconds (11, 120.0) == Approx (1.0 / 3.0).epsilon (1.0e-12)); // 1/4 triplet
    REQUIRE (sync::hertz (11, 120.0) == Approx (3.0).epsilon (1.0e-12));

    REQUIRE (sync::clampBpm (0.0) == sync::minBpm);
    REQUIRE (sync::clampBpm (1000.0) == sync::maxBpm);
    REQUIRE (sync::clampBpm (std::numeric_limits<double>::quiet_NaN()) == sync::defaultBpm);

    REQUIRE (sync::delaySeconds (0, 20.0) == Approx (sync::maxDelaySeconds).epsilon (1.0e-12));
}
