# AETHR — Development roadmap

STRING (Karplus–Strong) is the shipped flagship engine. The multi-engine platform
(`engine.type`, `KarplusStringEngine`, taxonomy UI) is in place. Further engines are
built one at a time against the gate in [`ENGINE_REFERENCE.md`](ENGINE_REFERENCE.md).

Each phase ends with a **stable, committed checkpoint**. The gate for every phase is the same:
build clean, tests pass, warnings read and resolved, DSP behaviour inspected, profiled where
relevant. No phase begins while the previous one has outstanding failures — the whole point is to
avoid accumulating a large body of untested code.

Exit criteria below are the specific, checkable conditions, not vague intentions.

---

## Phase 1 — Project skeleton and build system ✅ COMPLETE

Delivered:

- CMake 4-compatible build, C++23, JUCE 9.0.1 and Catch2 v3.9.1 pinned to exact tags
- VST3 + AU + Standalone building on macOS arm64 with Command Line Tools only
- Parameter layer with the stable-ID contract, six parameters exercising float/int/choice types
- State save/restore with validation and rejection of foreign state
- Output stage with smoothed gain, over-long-block handling, and NaN backstop
- Numerical guard and audio-maths libraries, fully unit-tested
- Editor shell establishing the visual language and region-scoped repaint discipline
- 28 tests passing; pluginval strictness 10 SUCCESS; auval SUCCEEDED
- Documentation: ARCHITECTURE, DSP_NOTES, TESTING, PERFORMANCE, PRESET_DESIGN, BUILD, RISKS

Exit criteria, all met: plugin loads in a validator; state round-trips; no warnings in first-party
code; tests green.

---

## Phase 2 — Mono Karplus–Strong resonator

Build `FractionalDelay`, `LoopFilter` and `KarplusResonator` and make one note sound.

Exit criteria: a plucked note is audible and decays smoothly to silence; no clicks at note-on or
note-off; the resonator survives a stability sweep of decay × damping without producing NaN; output
is finite at all six sample rates.

## Phase 3 — Pitch accuracy

Implement the phase-delay compensation of `DSP_NOTES.md` §1–3 and the fixed-point solver. Benchmark
allpass versus Lagrange for cents error and CPU.

Exit criteria: **< ±1 cent** across A0–C8 and intermediate semitones, measured from rendered audio,
at all six sample rates, with dispersion neutral. Benchmark results recorded in PERFORMANCE.md and
the chosen default justified.

## Phase 4 — Excitation

All twelve excitation modes, the shared seeded PRNG, burst windowing, colour/brightness shaping,
velocity response, stereo spread, excitation position comb.

Exit criteria: identical seed produces bit-identical output; no click from any burst setting; every
mode produces finite audio at every sample rate.

## Phase 5 — Decay, damping and loop filtering

All loop-filter types with independent cutoff/resonance/damping/brightness, and the \(T_{60}\)
derivation including filter-loss compensation.

Exit criteria: measured \(T_{60}\) matches the requested value within tolerance across the keyboard;
no filter setting can make the loop grow; stability verified at 44.1/48/88.2/96/176.4/192 kHz.

## Phase 6 — Physical material model

The twelve materials as parameter transformations, with full user override of every underlying value.

Exit criteria: every material is stable at every extreme of the other controls; switching material
does not click; overriding a parameter after selecting a material sticks.

## Phase 7 — Body resonator

Modal bank with 1/2/4/8/16 modes, seven body presets, inactive modes genuinely skipped.

Exit criteria: all poles inside the unit circle for every setting; measured CPU scales with *active*
mode count; body presets are recognisable and musically distinct.

## Phase 8 — Polyphonic voice architecture

Voice, VoiceManager, stealing, release tails, sleeping, per-voice smoothing. Layers A/B with
independent tuning, material and stereo position; Layer B genuinely bypassed when off.

Exit criteria: 64 voices without dropouts on the target machine; voice stealing produces no audible
click; a released voice measurably stops consuming CPU; Layer B off costs nothing.

## Phase 9 — Modulation matrix

Sources, destinations, routing table, per-destination smoothing time constants.

Exit criteria: every destination reachable from every source; no zipper noise anywhere; audio-rate
destinations stay in range under maximum depth; adding a destination requires no source changes.

## Phase 10 — Nonlinearity and oversampling

Nine nonlinear modes, 2×/4×/8× oversampling applied only to nonlinear sections, DC blocking, latency
reporting.

Exit criteria: measured aliasing below threshold at each factor; THD matches the reference for each
mode; no DC offset after asymmetric shaping; reported latency correct.

## Phase 11 — Effects rack

Filter, saturation, distortion, delay, chorus, phaser, flanger, reverb, final clipper. Feed loop with
its safety limiter.

Exit criteria: every effect bypasses bit-transparently; no clicks on parameter changes; reverb decays
to true silence; the feed loop cannot be made to explode by any parameter combination.

## Phase 12 — Preset system

PresetManager, factory bank across all thirteen categories, at least 50 presets each demonstrating a
distinct capability, seeded Randomise/Mutate with parameter locking and four modes.

Exit criteria: every preset loads and round-trips; randomisation with a fixed seed is reproducible;
no preset produces clipping or instability.

## Phase 13 — Professional UI

LookAndFeel, all panels, resonator/decay/spectrum visualisers, macro controls, meters, tooltips and
accessibility.

Exit criteria: no repaint storms under automation; editor open/close leaks nothing; every control has
a readable label, a real-unit readout and a tooltip; UI never touches the audio thread.

## Phase 14 — Testing hardening

Complete the coverage table in TESTING.md, add reference-render regression tests, add the allocation
detector for `processBlock`.

## Phase 15 — Profiling

The full matrix from the brief: 1/8/16/32/64 voices × effects on/off × oversampling
off/2×/4×/8×. Results and the resulting optimisation decisions recorded in PERFORMANCE.md.

## Phase 16 — Optimisation and sound design

Optimise only what was measured. Then iterate on the sound: listen, adjust ranges and macro mappings,
refine presets.

---

## Deferred, deliberately

- **AUv3** — needs full Xcode; not available in this environment.
- **Non-12-TET tuning tables (Scala import)** — the tuning layer is built to accommodate it
  (fractional MIDI notes throughout, no hard-coded 12), but it is not Phase 1–16 work.
- **MPE note expression** — the voice architecture keeps per-note state so it can be added; the
  brief lists it as "where practical".
- **Windows build** — portable by construction, unvalidated in practice.
