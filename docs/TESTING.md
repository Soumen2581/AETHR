# STRATA — Testing

## Methodology

Three layers, each catching what the others cannot:

1. **Offline unit and integration tests** (Catch2, `Tests/`) — deterministic, fast, run on every
   build. These own correctness of the maths, the guards, the parameter contract and the processor's
   host-facing behaviour.
2. **Host validation** (pluginval, auval) — catches lifecycle, threading and state bugs that only
   appear when a real host drives the plugin in ways we would not think to.
3. **DAW testing by hand** — catches the things neither of the above models: how it sounds, whether
   automation feels right, whether CPU is acceptable in a real project.

The rule for every phase: **build, run tests, read the warnings, inspect behaviour, profile if
relevant, fix, commit**. A phase is not finished while any of those is outstanding.

## Running

```bash
ctest --preset dev                                  # everything
./build/dev/Tests/StrataTests                       # direct, full output
./build/dev/Tests/StrataTests "[pitch]"             # by tag
./build/dev/Tests/StrataTests --list-tests
```

Tags in use: `[math]`, `[pitch]`, `[gain]`, `[decay]`, `[guards]`, `[stability]`, `[parameters]`,
`[state]`, `[processor]`, `[dsp]`, `[buses]`, `[ui]`, `[robustness]`.

## Current status

**28 tests, all passing.** Total runtime 0.44 s — fast enough that there is never a reason to skip
them.

### What is covered as of Phase 1

| Area | Tests |
| --- | --- |
| Pitch maths | MIDI→Hz against the 12-TET reference table for A0…C8, agreement within 0.01 cent; Hz↔note round-trip over 12–120 in quarter-semitone steps; cent/semitone ratio consistency |
| Gain maths | dB↔gain round-trip from −90 to +12 dB; exact zero at and below the floor (a fader at minimum must be true silence, not −100 dB of hiss) |
| Decay maths | \(g \leftrightarrow T_{60}\) exact inversion across 4 sample rates × 4 loop lengths × 4 decay times; **direct iteration of the loop** confirming the amplitude reaches −60 dB within 0.1 dB; degenerate inputs (zero/negative/absurd) fail safe instead of producing infinities |
| Numerical guards | NaN/±inf substitution; denormal flushing; state sanitising including magnitude clamping; block-level detection, repair and peak measurement with poisoned samples present; loop-gain clamping including NaN and >1 inputs; coefficient validation |
| Parameters | every parameter is ranged, uniquely identified, lowercase and dot-namespaced, and has a display name; defaults inside range; normalised↔real conversions are true inverses; all published IDs resolve |
| State | full save/restore into a **fresh instance** with six non-default values; rejection of empty, garbage and foreign-plugin state without disturbing current values |
| Host integration | bus layouts (mono and stereo accepted, input or >2 channels refused); reported capabilities; editor create/destroy cycles |
| Rendering | finite, silent output at 44.1/48/88.2/96/176.4/192 kHz; stability across block sizes 32…2048; **a block larger than promised** handled without allocation or invalid output; double-precision path; repeated sample-rate and block-size changes |

### Deliberate exact-equality assertions

The test sources compile with `-Wno-float-equal` (applied per file, see
`cmake/StrataWarnings.cmake`). This is intentional: the guards promise to substitute *exactly* zero
and the gain floor promises *exact* silence. Asserting "approximately zero" would let a real bug
through — a guard that returned `1e-30` instead of `0` would pass an approximate check while still
failing to stop a denormal.

## Host validation status

| Validator | Level | Result |
| --- | --- | --- |
| pluginval 1.0.4 | strictness **10** (maximum) | **SUCCESS** |
| auval (macOS) | default | **AU VALIDATION SUCCEEDED** |

pluginval at level 10 covers plugin scanning, cold and warm open, editor construction and automation,
parameter thread safety, background-thread state save/restore, bus enumeration and reconfiguration,
and parameter fuzzing. auval additionally exercises render calls at 11 025–192 000 Hz, block sizes
64–4096, the too-large-block failure path, parameter scheduling (immediate and ramped) and MIDI.

If a validation failure appears, fix the cause. Suppressing a genuine pluginval failure is how
plugins end up crashing in one host and not another.

## Planned coverage by phase

Tests are written **with** each phase, not after it.

| Phase | Tests added |
| --- | --- |
| 2–3 Resonator, pitch | **Cents-error measurement of rendered audio**: render each note A0–C8 plus intermediate semitones, estimate \(f_0\) by parabolic interpolation of the FFT peak refined with autocorrelation, assert < ±1 cent. Repeated at all six sample rates. Separate cases document the intended pitch shift when dispersion is engaged. |
| 4 Excitation | Determinism: identical seed ⇒ bit-identical output. Burst windowing produces no click (no discontinuity above a threshold in the first samples). |
| 5 Loop filter | Stability sweep: every filter type × cutoff × resonance × decay, at all sample rates, asserting no NaN and no growth over a long render. Measured \(T_{60}\) matches the requested value within tolerance. |
| 6–7 Material, body | Mode bank stability (pole radius < 1 for all settings); inactive modes cost nothing (timing assertion); material macros stay inside safe parameter ranges. |
| 8 Voices | Polyphony under stress; voice stealing produces no discontinuity above a click threshold; released voices actually sleep (CPU assertion); note-on/off ordering, sustain pedal, all-notes-off. |
| 9 Modulation | Matrix routing correctness; smoothing produces no step discontinuities; audio-rate destinations stay in range. |
| 10 Nonlinearity | **Aliasing measurement**: render a sine at a frequency whose harmonics fold, FFT, assert energy at the expected alias frequencies is below a threshold at each oversampling factor. THD measurement against reference. DC offset after asymmetric shaping is below threshold. |
| 11 Effects | Per-effect bypass is bit-transparent; delay time changes do not click; reverb tail decays to silence; final limiter never produces samples outside range. |
| 12 Presets | Every factory preset loads, produces finite audio, and round-trips. Seeded randomisation is reproducible. |
| 13 UI | Editor open/close under automation; no repaint storms (frame-time assertion). |

## Known gaps

- **No Windows build has been attempted.** Everything above is macOS/arm64.
- **No automated audio-quality regression yet.** Once the engine renders audio, reference renders will
  be committed for a small set of patches and compared with a tolerance, which is why
  `-ffp-contract=off` is set project-wide.
- **Realtime-safety violations are not yet detected automatically.** The rules in
  `docs/ARCHITECTURE.md` §9 are enforced by review. A hardened allocator hook that fails the tests on
  any allocation inside `processBlock` is planned for Phase 15.
