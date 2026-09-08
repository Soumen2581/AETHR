#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "Core/AudioMath.h"
#include "EngineTestSupport.h"
#include "PitchEstimator.h"

/**
    Phase 3 acceptance: does the loop ring at the frequency the MIDI note asked for?

    A Karplus–Strong loop tuned by rounding its delay to whole samples is wrong by up
    to half a sample, which at C6 and 48 kHz is over 20 cents. These tests measure the
    error in cents on rendered audio, so they fail if the phase compensation regresses
    even though nothing about the code looks broken.

    @see docs/DSP_NOTES.md
*/

using namespace aethr;

namespace
{
    /** Acceptance threshold from the specification, for the modes that claim to meet it. */
    constexpr double toleranceCents = 1.0;

    /**
        Looser bound for linear interpolation at the very top of the keyboard.

        Linear interpolation is not merely less accurate, it is lossy, and its loss rises
        with frequency. Inside a loop that broadens the resonance and tilts it downwards,
        which pulls the *perceived* peak flat even though the phase condition is solved
        just as exactly as for the other modes. This is why linear is not the default; it
        is offered as a CPU fallback and documented as such.
    */
    constexpr double linearTopOctaveToleranceCents = 3.0;

    /** Analysis window: long enough to resolve the peak, and at least this many periods. */
    constexpr double minimumAnalysisSeconds = 0.5;
    constexpr double minimumAnalysisPeriods = 90.0;

    /** Skipped so the attack transient and the excitation burst are outside the window. */
    constexpr double attackSkipSeconds = 0.10;

    constexpr int analysisBlockSize = 512;

    struct Measurement
    {
        int midiNote { 0 };
        double expectedHz { 0.0 };
        double measuredHz { 0.0 };
        double centsError { 0.0 };
        double peak { 0.0 };
        bool valid { false };
    };

    [[nodiscard]] Measurement measureNote (int midiNote,
                                           double sampleRate,
                                           dsp::InterpolationMode interpolation,
                                           float dampingPercent = 10.0f)
    {
        AethrProcessor processor;

        testing::applyTuningTestSettings (processor, dampingPercent);
        testing::setParameter (processor, params::resonator::interpolation,
                               static_cast<float> (static_cast<int> (interpolation)));

        Measurement measurement;
        measurement.midiNote = midiNote;
        measurement.expectedHz = math::midiNoteToHertz (static_cast<double> (midiNote));

        const auto analysisSeconds = std::max (minimumAnalysisSeconds,
                                               minimumAnalysisPeriods / measurement.expectedHz);
        const auto renderSeconds = attackSkipSeconds + analysisSeconds + 0.05;

        const auto rendered = testing::renderNote (processor, midiNote, 1.0f,
                                                   sampleRate, analysisBlockSize, renderSeconds);

        measurement.peak = rendered.peak();

        const auto window = rendered.leftWindow (static_cast<std::size_t> (attackSkipSeconds * sampleRate),
                                                 static_cast<std::size_t> (analysisSeconds * sampleRate));

        const auto estimate = testing::estimateFundamental (window, sampleRate, measurement.expectedHz);

        measurement.valid = estimate.valid && rendered.isFinite();
        measurement.measuredHz = estimate.frequencyHz;
        measurement.centsError = math::ratioToCents (estimate.frequencyHz, measurement.expectedHz);

        return measurement;
    }

    /** A0, every C from C1 to C8, plus off-C semitones so nothing can pass by luck. */
    [[nodiscard]] std::vector<int> notesUnderTest()
    {
        return { 21, 24, 36, 48, 60, 72, 84, 96, 108,   // A0, C1..C8
                 31, 43, 54, 67, 75, 91, 101 };         // G1, G2, F#3, G4, D#5, G6, F7
    }

    [[nodiscard]] const char* interpolationName (dsp::InterpolationMode mode)
    {
        switch (mode)
        {
            case dsp::InterpolationMode::linear:        return "Linear";
            case dsp::InterpolationMode::lagrangeCubic: return "Lagrange 3";
            case dsp::InterpolationMode::allpass:       return "Allpass";
            case dsp::InterpolationMode::numModes:
            default:                                    return "?";
        }
    }
}

//==============================================================================
TEST_CASE ("Rendered pitch tracks MIDI within a cent across the keyboard", "[pitch]")
{
    constexpr double sampleRate = 48000.0;

    for (const auto midiNote : notesUnderTest())
    {
        const auto measurement = measureNote (midiNote, sampleRate, dsp::InterpolationMode::allpass);

        CAPTURE (midiNote, measurement.expectedHz, measurement.measuredHz, measurement.centsError);

        REQUIRE (measurement.valid);
        REQUIRE (measurement.peak > 1.0e-3);
        REQUIRE (std::abs (measurement.centsError) < toleranceCents);
    }
}

TEST_CASE ("Pitch accuracy holds at every supported sample rate", "[pitch]")
{
    // 192 kHz is the interesting one: the loop is four times longer, so an error in
    // the fractional part is four times smaller in cents and could hide a bug that
    // 44.1 kHz would expose. 44.1 kHz is the interesting one in the other direction.
    for (const auto sampleRate : { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 })
    {
        for (const auto midiNote : { 21, 45, 69, 96, 108 })
        {
            const auto measurement = measureNote (midiNote, sampleRate, dsp::InterpolationMode::allpass);

            CAPTURE (sampleRate, midiNote, measurement.measuredHz, measurement.centsError);

            REQUIRE (measurement.valid);
            REQUIRE (std::abs (measurement.centsError) < toleranceCents);
        }
    }
}

TEST_CASE ("Pitch accuracy survives heavy loop damping", "[pitch]")
{
    // Damping puts real phase delay into the loop filter. If that phase were ignored
    // the pitch would drift sharp as damping rose, most visibly at the top.
    constexpr double sampleRate = 48000.0;

    for (const auto dampingPercent : { 0.0f, 50.0f, 100.0f })
    {
        for (const auto midiNote : { 36, 60, 84, 96 })
        {
            const auto measurement = measureNote (midiNote, sampleRate,
                                                  dsp::InterpolationMode::allpass, dampingPercent);

            CAPTURE (dampingPercent, midiNote, measurement.measuredHz, measurement.centsError);

            REQUIRE (measurement.valid);
            REQUIRE (std::abs (measurement.centsError) < toleranceCents);
        }
    }
}

TEST_CASE ("Every interpolation mode tunes accurately", "[pitch]")
{
    constexpr double sampleRate = 48000.0;

    // The compensation is generic: it inverts whatever phase response the selected
    // interpolator actually has, so all three modes solve the phase condition exactly.
    for (const auto mode : { dsp::InterpolationMode::linear,
                             dsp::InterpolationMode::lagrangeCubic,
                             dsp::InterpolationMode::allpass })
    {
        for (const auto midiNote : { 24, 48, 60, 84, 96 })
        {
            const auto measurement = measureNote (midiNote, sampleRate, mode);

            CAPTURE (interpolationName (mode), midiNote, measurement.centsError);

            REQUIRE (measurement.valid);
            REQUIRE (std::abs (measurement.centsError) < toleranceCents);
        }

        // C8 separates them: only the modes with a flat magnitude response keep the
        // perceived pitch on target where the loop is barely ten samples long.
        const auto topOfKeyboard = measureNote (108, sampleRate, mode);
        const auto tolerance = mode == dsp::InterpolationMode::linear
                                 ? linearTopOctaveToleranceCents
                                 : toleranceCents;

        CAPTURE (interpolationName (mode), topOfKeyboard.centsError, tolerance);

        REQUIRE (topOfKeyboard.valid);
        REQUIRE (std::abs (topOfKeyboard.centsError) < tolerance);
    }
}

TEST_CASE ("Master tuning offsets shift pitch by exactly the requested amount", "[pitch]")
{
    constexpr double sampleRate = 48000.0;
    constexpr int midiNote = 60;

    struct Offset
    {
        float octaves;
        float semitones;
        float cents;
    };

    for (const auto offset : { Offset { 0.0f, 0.0f, 25.0f },
                               Offset { 0.0f, 7.0f, 0.0f },
                               Offset { -1.0f, 0.0f, -40.0f },
                               Offset { 1.0f, -5.0f, 12.5f } })
    {
        AethrProcessor processor;
        testing::applyTuningTestSettings (processor);

        testing::setParameter (processor, params::master::tuneOctave, offset.octaves);
        testing::setParameter (processor, params::master::tuneSemitones, offset.semitones);
        testing::setParameter (processor, params::master::tuneCents, offset.cents);

        const auto offsetSemitones = static_cast<double> (offset.octaves) * math::semitonesPerOctave
                                   + static_cast<double> (offset.semitones)
                                   + static_cast<double> (offset.cents) / math::centsPerSemitone;

        const auto expectedHz = math::midiNoteToHertz (static_cast<double> (midiNote) + offsetSemitones);

        const auto analysisSeconds = std::max (minimumAnalysisSeconds, minimumAnalysisPeriods / expectedHz);
        const auto rendered = testing::renderNote (processor, midiNote, 1.0f, sampleRate,
                                                   analysisBlockSize,
                                                   attackSkipSeconds + analysisSeconds + 0.05);

        const auto window = rendered.leftWindow (static_cast<std::size_t> (attackSkipSeconds * sampleRate),
                                                 static_cast<std::size_t> (analysisSeconds * sampleRate));

        const auto estimate = testing::estimateFundamental (window, sampleRate, expectedHz);
        const auto centsError = math::ratioToCents (estimate.frequencyHz, expectedHz);

        CAPTURE (offset.octaves, offset.semitones, offset.cents, expectedHz, estimate.frequencyHz, centsError);

        REQUIRE (estimate.valid);
        REQUIRE (std::abs (centsError) < toleranceCents);
    }
}

//==============================================================================
TEST_CASE ("Pitch error report", "[pitch][report]")
{
    // Not an assertion so much as the evidence behind the numbers quoted in
    // docs/DSP_NOTES.md. Run with `--success` or by tag to see the table.
    constexpr double sampleRate = 48000.0;

    std::printf ("\n  note     expected Hz    ");

    for (const auto mode : { dsp::InterpolationMode::linear,
                             dsp::InterpolationMode::lagrangeCubic,
                             dsp::InterpolationMode::allpass })
        std::printf ("%12s ", interpolationName (mode));

    std::printf ("\n");

    auto worstLinear = 0.0;
    auto worstLagrange = 0.0;
    auto worstAllpass = 0.0;

    for (const auto midiNote : notesUnderTest())
    {
        std::printf ("  %4d  %12.4f    ", midiNote, math::midiNoteToHertz (static_cast<double> (midiNote)));

        for (const auto mode : { dsp::InterpolationMode::linear,
                                 dsp::InterpolationMode::lagrangeCubic,
                                 dsp::InterpolationMode::allpass })
        {
            const auto measurement = measureNote (midiNote, sampleRate, mode);
            std::printf ("%+11.4f¢ ", measurement.centsError);

            auto& worst = mode == dsp::InterpolationMode::linear ? worstLinear
                        : mode == dsp::InterpolationMode::lagrangeCubic ? worstLagrange
                                                                       : worstAllpass;

            worst = std::max (worst, std::abs (measurement.centsError));
        }

        std::printf ("\n");
    }

    std::printf ("\n  worst absolute error, A0 to C8 at 48 kHz:\n");
    std::printf ("    Linear      %.4f cents\n", worstLinear);
    std::printf ("    Lagrange 3  %.4f cents\n", worstLagrange);
    std::printf ("    Allpass     %.4f cents  (default)\n\n", worstAllpass);

    REQUIRE (worstAllpass < toleranceCents);
    REQUIRE (worstLagrange < toleranceCents);
    REQUIRE (worstLinear < linearTopOctaveToleranceCents);
}
