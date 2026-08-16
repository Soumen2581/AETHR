#pragma once

/**
    Stable parameter identifiers.

    THESE STRINGS ARE PART OF THE PUBLIC CONTRACT WITH EVERY HOST PROJECT EVER
    SAVED. Renaming one silently breaks automation and saved state in users'
    sessions. Rules:

      - IDs are lowercase, dot-separated, and namespaced by subsystem.
      - An ID is never reused for a different meaning.
      - A retired parameter's ID stays retired; add a new one instead.
      - `versionHint` must stay 1 for every parameter shipped in the 1.x series.
        VST3 uses it to derive stable parameter hashes, so changing it
        invalidates host automation. New parameters added in a later minor
        release keep hint 1; only a deliberate, documented breaking release
        increments it.

    Display names, ranges and grouping live in ParameterLayout.cpp; this header
    is intentionally free of anything that could change for cosmetic reasons.

    @see docs/ARCHITECTURE.md ("Parameter contract")
*/
namespace strata::params
{

/** VST3 parameter-hash version hint. See the note above before touching this. */
inline constexpr int versionHint = 1;

//==============================================================================
namespace output
{
    /** Master output level in decibels. */
    inline constexpr auto gain = "output.gain";
}

//==============================================================================
namespace master
{
    /** Global transpose in octaves. */
    inline constexpr auto tuneOctave = "master.tune.octave";

    /** Global transpose in semitones. */
    inline constexpr auto tuneSemitones = "master.tune.semitones";

    /** Global detune in cents. */
    inline constexpr auto tuneCents = "master.tune.cents";
}

//==============================================================================
namespace voice
{
    /** Maximum simultaneous voices. Changes voice allocation, so it is not automatable. */
    inline constexpr auto polyphony = "voice.polyphony";

    /** How strongly MIDI velocity scales excitation level and brightness. */
    inline constexpr auto velocityAmount = "voice.velocity.amount";
}

} // namespace strata::params
