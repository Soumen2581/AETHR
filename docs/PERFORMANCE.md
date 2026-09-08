# AETHR — Performance

## Policy

Optimise only after measuring, and never at the expense of audio correctness. The priority order from
the brief is fixed:

1. audio correctness → 2. pitch accuracy → 3. numerical stability → 4. absence of artefacts →
5. musical usefulness → 6. CPU efficiency → 7. UI polish

Profiling happens at **every phase boundary**, not once at the end, so a regression can be attributed
to the phase that introduced it.

## Reference machine

| | |
| --- | --- |
| CPU | Apple M4, 10 cores |
| RAM | 16 GB |
| OS | macOS 26.5.2 |
| Compiler | Apple clang 21.0.0 |
| Build | `dev` preset (RelWithDebInfo, `-O2 -g`), arm64 |

Shipping performance figures must come from a `release` or `release-native` build (LTO enabled);
`dev` numbers are indicative only.

## Measured — Phase 1

The engine does not exist yet, so these are baselines for the harness, not DSP figures.

| Metric | Value |
| --- | --- |
| Clean configure (JUCE reused from `External/`) | ~8 s |
| Clean build, all targets + tests | ~2 min |
| Incremental build after touching one source file | ~3 s |
| Test suite (28 tests) | 0.44 s |
| pluginval, strictness 10 | ~16 s |
| auval | < 1 s |
| VST3 binary | 13 MB (arm64, `-g`) |
| AU binary | 12 MB |
| Standalone binary | 12 MB |

The binaries are large because RelWithDebInfo embeds debug info; a stripped `release` build will be a
fraction of this. Worth watching, but not worth acting on yet.

## Profiling matrix — Phase 15

To be filled in with measurements. Each cell records CPU as a percentage of one core at 48 kHz with a
256-sample block, measured over a sustained render, and the same figure at 96 kHz.

| Voices | FX off, OS off | FX off, OS 2× | FX off, OS 4× | FX off, OS 8× | FX on, OS off | FX on, OS 2× | FX on, OS 4× | FX on, OS 8× |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 1 | | | | | | | | |
| 8 | | | | | | | | |
| 16 | | | | | | | | |
| 32 | | | | | | | | |
| 64 | | | | | | | | |

Additional measurements to take at the same time:

- **Idle cost** — all voices released and asleep. Should be indistinguishable from bypass; if not,
  voice sleeping is not working.
- **Body-mode scaling** — 1, 2, 4, 8, 16 active modes. Cost must scale with *active* modes; a flat
  curve means inactive modes are being computed.
- **Layer B off versus on** — off must cost nothing measurable.
- **Worst-case block** — 64 simultaneous note-ons in one block, which is the spike that causes
  dropouts in real projects even when the average is comfortable.
- **UI open versus closed** — the editor must not measurably affect audio-thread cost.

## Benchmarks scheduled for Phase 3

The fractional-delay decision in `ARCHITECTURE.md` §6.1 is provisional until measured. The benchmark
compares, for allpass and 3rd-order Lagrange:

| Criterion | How measured |
| --- | --- |
| Pitch accuracy | Cents error from rendered audio, A0–C8 plus intermediate semitones, six sample rates |
| Decay consistency | Spread of measured \(T_{60}\) across adjacent semitones at a fixed decay setting — this is where linear interpolation fails badly |
| Transient quality | Rise time and overshoot of the first period after excitation |
| CPU | Cost per voice-sample, isolated from the rest of the chain |
| Modulation behaviour | Artefacts under fast pitch modulation, which is the allpass's known weakness |

The result and the resulting default are recorded here. Choosing the more expensive method without a
measurement that justifies it is not acceptable, and neither is choosing the cheaper one.

## Optimisation techniques, in the order they will be applied

1. **Do less work** — voice sleeping, inactive-module bypass, skipping inactive body modes,
   coefficient caching keyed on parameter change rather than recomputed per sample.
2. **Improve memory behaviour** — delay buffers sized to powers of two so wrapping is a mask rather
   than a branch; per-voice state kept contiguous; scratch buffers reused.
3. **Vectorise** — `juce::dsp::SIMDRegister` (NEON on this machine) for the body-mode bank and the
   per-voice output stage, which are the natural candidates because they apply the same operation
   across independent state.
4. **Reduce precision selectively** — `float` where analysis shows it is safe; never in pitch or
   decay coefficient derivation (`DSP_NOTES.md` §8).

Explicitly rejected: `-ffast-math`. The NaN and denormal guards depend on IEEE-754 semantics that it
is permitted to break, and it would make reference-render regression tests non-reproducible.
