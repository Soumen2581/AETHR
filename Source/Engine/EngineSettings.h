#pragma once

#include "DSP/BodyResonator.h"
#include "DSP/Exciter.h"
#include "DSP/FractionalDelayLine.h"
#include "DSP/LoopFilter.h"
#include "DSP/Modulation.h"
#include "Engine/EngineType.h"

namespace aethr::engine
{

/**
    A snapshot of every host-controlled value the engine needs.

    Built once per block on the audio thread by reading each cached parameter atomic
    exactly once, then passed down by const reference. Reading a parameter twice in
    one block is a real bug in a plugin, not a stylistic preference: the host can
    change it between the two reads, and code that assumed consistency between, say,
    a decay time and the loop gain derived from it would then disagree with itself.
*/
struct Settings
{
    EngineType engineType { EngineType::string };

    dsp::Exciter::Settings exciter;

    /** T60 of the fundamental while a note is held, in seconds. */
    double decayTimeSeconds { 1.6 };

    /** T60 applied once the note is released. Shorter values behave like a damped string. */
    double releaseTimeSeconds { 0.25 };

    /** 0 bypasses the loop filter entirely; 1 is full damping at the brightness corner. */
    double damping { 0.35 };

    /** Where the damping starts, as a multiple of the fundamental. */
    double brightness { 0.6 };

    /** Inharmonicity. 0 is an ideal string. */
    double stiffness { 0.0 };

    /**
        Karplus–Strong loop recirculation, 0–1.

        Multiplies the T60-derived loop gain. 1 is the current decay behaviour;
        0 opens the loop so the delay fires once and dies.
    */
    double loopFeedback { 1.0 };

    double controlA { 0.5 };
    double controlB { 0.5 };
    double controlC { 0.5 };
    double controlD { 0.5 };

    dsp::LoopFilterMode loopFilterMode { dsp::LoopFilterMode::onePole };
    dsp::InterpolationMode interpolation { dsp::InterpolationMode::allpass };

    dsp::BodyResonator::Settings body;

    bool   layerBEnabled { false };
    double layerBIntervalSemitones { 12.0 };
    double layerBDetuneCents { 6.0 };
    double layerBLevel { 0.0 };

    /** Master transpose plus fine tune, folded into a single signed semitone offset. */
    double tuningOffsetSemitones { 0.0 };

    int numVoices { 8 };

    /** When true, every note-on for a given key uses the same excitation noise. */
    bool seedLocked { false };
    int  seed { 0 };
};

} // namespace aethr::engine
