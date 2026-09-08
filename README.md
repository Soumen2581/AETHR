# AETHR — Physical Resonance Engine

A polyphonic physical-modelling instrument built around an extended Karplus–Strong resonator.
VST3, Audio Unit and Standalone.

AETHR is designed to reach plucked strings, metallic resonances, tuned percussion, bells, mallets,
glassy tones, organic acoustic textures, evolving drones, aggressive distorted resonances, clean
musical basses and deliberately unstable experimental material — from one engine, because the
resonator's tuning, decay and timbre are independently controllable rather than tangled together.

> **Status: Phase 1 of 16 complete.** The build system, parameter and state layer, numerical-safety
> library, audio maths, test harness and host integration are done and validated. The synthesis engine
> itself begins in Phase 2 — at this commit the plugin loads, passes validation and renders silence.
> See [`docs/ROADMAP.md`](docs/ROADMAP.md).

## Why it is not a delay with feedback

The naive Karplus–Strong loop tunes itself by setting a delay of \(f_s/f_0\) samples. That is wrong,
because the loop filter, the tuning allpass and the dispersion chain each add their own phase delay —
about one to two samples in total. At C8 and 48 kHz the period is only 11.5 samples, so the error is
roughly **230 cents**, more than a whole tone.

AETHR solves the full resonance condition instead, compensating for every element in the loop, and
derives its feedback gain from a target 60 dB decay *time* rather than exposing a raw feedback number.
The result is pitch that tracks accurately across the whole keyboard and decay that behaves the same
way at the top of the range as at the bottom. The mathematics is in
[`docs/DSP_NOTES.md`](docs/DSP_NOTES.md).

## Building

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Requires CMake ≥ 3.25 and a C++23 compiler. JUCE 9.0.1 and Catch2 v3.9.1 are fetched automatically at
pinned tags — nothing to install by hand. Full details, options and macOS specifics in
[`docs/BUILD.md`](docs/BUILD.md).

## Validation status

| Check | Result |
| --- | --- |
| Unit and integration tests | 28 passing, 0.44 s |
| pluginval 1.0.4, strictness 10 (maximum) | **SUCCESS** |
| `auval` (Audio Unit) | **AU VALIDATION SUCCEEDED** |
| Compiler warnings in first-party code | none, with `-Werror` enabled |

Verified at 44.1, 48, 88.2, 96, 176.4 and 192 kHz, and at block sizes from 32 to 2048 — plus the case
where the host exceeds the block size it promised.

## Documentation

| Document | Contents |
| --- | --- |
| [ARCHITECTURE.md](docs/ARCHITECTURE.md) | Repository and toolchain assessment, JUCE 9 API verification, signal flow, module decomposition, threading model, parameter contract |
| [DSP_NOTES.md](docs/DSP_NOTES.md) | The mathematics: loop tuning, allpass fractional delay, loss filters, decay derivation, dispersion, modal bodies, oversampling, numerical safety |
| [ROADMAP.md](docs/ROADMAP.md) | The 16 phases with concrete exit criteria |
| [RISKS.md](docs/RISKS.md) | Risk register with mitigations and status |
| [TESTING.md](docs/TESTING.md) | Test methodology, current coverage, planned coverage per phase |
| [PERFORMANCE.md](docs/PERFORMANCE.md) | Profiling policy, reference machine, measured baselines, benchmark plans |
| [PRESET_DESIGN.md](docs/PRESET_DESIGN.md) | Preset categories and rules, macro mappings, randomisation design |
| [BUILD.md](docs/BUILD.md) | Requirements, presets, options, renaming, macOS packaging notes |

## Design constraints

Non-negotiable, and enforced by review and by tests:

- `processBlock` never allocates, locks, logs, touches the filesystem, or calls into the UI.
- No non-finite sample ever reaches the host, and no non-finite value is ever stored in recursive
  filter state.
- Parameter IDs are permanent. Renaming one breaks automation in every saved session.
- Pitch and decay maths is done in `double`; fast-math is never enabled.
- Only nonlinear sections are oversampled.

## Renaming

The product name is not locked in. All branding lives in CMake cache variables at the top of
`CMakeLists.txt` and reaches the source through `Source/Core/Branding.h`; renaming requires no source
edits. See [`docs/BUILD.md`](docs/BUILD.md).

## Licence

Not yet determined. Note that JUCE 9 is dual-licensed (commercial or AGPLv3) — shipping commercially
requires an appropriate JUCE licence.
