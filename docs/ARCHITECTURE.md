# AETHR — Architecture

**Product:** AETHR — Physical Resonance Engine
**Type:** polyphonic virtual instrument (VST3 / AU / Standalone)
**Status:** Multi-engine platform scaffolding. STRING (Karplus–Strong) is the shipped core.
**Document owner:** DSP + architecture
**Last verified against toolchain:** 17 August 2026

---

## 1. Repository assessment

The repository was empty when this work started. Findings:

| Item | Finding |
| --- | --- |
| Git | Initialised, branch `master`, **zero commits**, no remotes |
| Existing source | None |
| Existing build system | None |
| Existing JUCE checkout | None anywhere on the machine |

There is no legacy code to preserve or migrate, so the layout below is chosen freely rather than
inherited. The user has shipped JUCE plugins before (`AstralRig.vst3`, `PsytranceVST.vst3` are
installed in `~/Library/Audio/Plug-Ins`), and an earlier project on the Desktop used JUCE 8.0.2 with
`FetchContent`. That prior project is *not* a dependency of this one; it was inspected only to
confirm local conventions (CMake + FetchContent, `COPY_PLUGIN_AFTER_BUILD`, centralised branding).

### 1.1 Toolchain, verified empirically

| Component | Version | Notes |
| --- | --- | --- |
| OS | macOS 26.5.2 (build 25F84) | |
| CPU | arm64 (Apple Silicon) | |
| Compiler | Apple clang 21.0.0 | C++23 accepted |
| CMake | 4.3.2 (Homebrew) | CMake 4 rejects `cmake_minimum_required` below 3.5 |
| Generator | Ninja 1.13.2 | |
| Xcode | **Command Line Tools only** — no full Xcode, `xcodebuild` unavailable | see §1.3 |
| macOS SDK | via `xcrun`, CLT instance | |
| pluginval | Not installed; v1.0.4 is current | fetched by `Tools/fetch-pluginval.sh` |

### 1.2 Dependency decisions

| Dependency | Version | Why |
| --- | --- | --- |
| JUCE | **9.0.1**, pinned tag | Latest stable. Verified to exist and to build here. Never a moving branch: a `master` pin would make builds irreproducible. |
| Catch2 | **v3.9.1**, pinned tag | Latest v3. Chosen over GoogleTest for header-light test authoring, `SECTION`-based fixtures, and first-class CTest integration via `catch_discover_tests`. |
| pluginval | **v1.0.4** | Tracktion's validator, the de-facto standard for plugin robustness testing. |

No other third-party dependencies. JUCE bundles the VST3 SDK, so nothing further is needed for the
plugin formats we ship.

JUCE is reused from `External/JUCE` when present (`FETCHCONTENT_SOURCE_DIR_JUCE` is set
automatically) so that wiping `build/` does not trigger a 117 MB re-download. The pinned tag remains
the source of truth for a clean checkout.

### 1.3 Consequence of having no full Xcode — measured

`xcodebuild` is unavailable. The AU question was answered empirically rather than assumed:

- **AU v2 builds and validates with Command Line Tools alone.** JUCE's AU wrapper needs only the
  `AudioUnit` and `CoreAudioKit` frameworks, which the CLT SDK provides; `juceaide` also builds and
  runs. `auval` turns out to ship with macOS itself (`/usr/bin/auval`), not with Xcode, so AU can be
  validated here too — and it reports **AU VALIDATION SUCCEEDED**.
- **AUv3 is not buildable**, because it requires an app-extension target only Xcode can produce. It is
  out of scope, and recorded as deferred in `docs/ROADMAP.md`.

AU therefore stays enabled by default rather than being dropped. A second, unrelated macOS issue was
found and fixed during Phase 1 — build output inherits `com.apple.quarantine` from the launching
process, after which dyld refuses to load the plugin despite a valid signature. The build now clears
it automatically. Both findings are documented in `docs/BUILD.md`.

---

## 2. JUCE 9.0.1 verification notes

Every API this architecture depends on was checked against the real headers in `External/JUCE`, not
from memory. Results:

| API | Status in JUCE 9.0.1 | Consequence |
| --- | --- | --- |
| `juce_add_plugin` options (`IS_SYNTH`, `FORMATS`, `VST3_CATEGORIES`, `AU_MAIN_TYPE`, `COPY_PLUGIN_AFTER_BUILD`, …) | Unchanged | none |
| `juce_add_plugin` target shape | Creates a **`STATIC` library** plus one target per format (`JUCEUtils.cmake:2277`) | The test binary links the shared-code target directly, so JUCE compiles once |
| `AudioProcessorValueTreeState` | Unchanged, including `ParameterLayout`, `getRawParameterValue`, `copyState`, `replaceState` | none |
| `AudioParameterFloat/Int/Choice` | Attribute-builder constructors current; the older label/lambda constructors are now `[[deprecated]]` | Use `AudioParameter*Attributes` exclusively |
| Parameter classes' location | **Moved** to the new `juce_audio_processors_headless` module | No code change: `juce_audio_processors` includes it and depends on it, so `juce::juce_audio_utils` still transitively provides everything |
| `juce::dsp` | Intact: `Oversampling` (with `FilterType`), `DelayLine` (`None`/`Linear`/`Lagrange3rd`/`Thiran`), `StateVariableTPTFilter`, `FirstOrderTPTFilter`, `ProcessorChain`, `FFT`, `SIMDRegister`, `LookupTable` | Our DSP plan is unaffected |
| `SynthesiserVoice` / `Synthesiser` / `MPESynthesiser` | Unchanged | Voice layer plan stands (though we use a custom voice manager, §7) |
| `LookAndFeel_V4` | Present | UI plan stands |
| `Component` / `Graphics` painting | Unchanged | none |
| `Font(float)` | Still present, but `FontOptions` is the modern path | All our code uses `FontOptions` |
| `juce::ignoreUnused` | Still exists, **relocated** to `juce_core/maths/juce_MathsFunctions.h` | We prefer `[[maybe_unused]]` anyway |
| `Drawable` | **No longer inherits `Component`** (JUCE 9 breaking change) | Vector art must be drawn via `Drawable::draw*`, not parented as a child component. Affects Phase 13. |
| SVG loading | `Drawable::createFromSVG(const XmlElement&)` **removed**; SVG now parsed by lunasvg | Use `createFromSVGFile`/`createFromSVGString` if we ship vector assets |
| New modules | `juce_animation`, `juce_javascript`, `juce_audio_processors_headless` | `juce_animation` is a candidate for Phase 13 meter/indicator easing; not linked yet |
| C compiler requirement | **JUCE 9 requires `C` in the project `LANGUAGES`** and fails configuration otherwise (`JUCE/CMakeLists.txt:39`) | Our `project()` call declares `LANGUAGES C CXX` — do not remove |
| zlib/png/jpeg/flac | Now compiled as C, symbols no longer namespaced | Only matters if we ever link external copies; we do not |

JUCE 9.0.1 is fully usable for this project. No fallback to 8.0.15 is required.

### 2.1 C++23

`CMAKE_CXX_STANDARD 23`, standard required, extensions off. JUCE modules only *require*
`cxx_std_17` (an interface minimum, so a higher project standard is fine). Features we intend to
rely on: `std::numbers`, designated initialisers, `constexpr` maths, `[[nodiscard]]`/`[[likely]]`,
`std::span`, `std::bit_cast`, `if consteval`. Two cautions recorded during assessment:

- Apple clang's libc++ is behind on some C++23 *library* features (notably `std::print`/`<format>`
  coverage and `std::mdspan`). The DSP layer does not need them. If a specific facility is missing we
  work around it locally rather than lowering the project standard, and note it here.
- `-ffp-contract=off` is set project-wide. Fused multiply-add reassociation would otherwise make
  offline render comparisons non-reproducible between compilers, which undermines the regression
  tests in `docs/TESTING.md`. Fast-math is **never** enabled: the NaN and denormal guards depend on
  IEEE-754 semantics being honoured.

---

## 3. Product concept

AETHR is a physical-modelling instrument built on a generalised, extended Karplus–Strong
resonator. It is not a delay-with-feedback effect: the resonator is one stage in a chain designed
so that plucked strings, struck metal, glass, tuned percussion, drones and deliberately unstable
textures are all reachable from the same engine.

The design commitment that separates it from a toy: **the loop's tuning, its decay and its timbre
are independently controllable**, because the pitch is corrected for the phase delay of every
element inside the loop (§6.2), and the decay is derived from a target T60 rather than from a raw
feedback number (§6.3).

---

## 4. Signal flow

```
                     ┌──────────────────────── per voice ────────────────────────┐
MIDI ─▶ Voice Manager ─▶ Exciter ─▶ Karplus Resonator ─▶ Body Resonator ─▶ Voice Nonlinearity ─▶ Pan
                     │      ▲             ▲    ▲                                                  │
                     │      │             │    └── Feed Loop (bounded, saturated)                 │
                     │      └── Modulation Matrix ──────────────────────────────────────────┐     │
                     └───────────────────────────────────────────────────────────────────────┼─────┘
                                                                                             │
   Layer A / Layer B mix ◀───────────────────────────────────────────────────────────────────┘
            │
            ▼
   Filter ─▶ Saturation ─▶ Distortion ─▶ Delay ─▶ Chorus/Phaser/Flanger ─▶ Reverb ─▶ Final Clipper ─▶ Output
            ▲                                                                            ▲
            └──────────────────── global Modulation Matrix ───────────────────────────────┘
```

Two rules make this modular rather than a monolith:

1. Every block implements the same informal contract — `prepare(spec)`, `reset()`,
   `process(context)` — so blocks can be reordered or bypassed without touching their neighbours.
2. Modulation is *pulled* by destinations from the matrix, never *pushed* by sources into named
   fields (§8). Adding a destination therefore does not require editing any modulation source.

The order above is the starting point. §9 of the original brief permits revision if profiling
justifies it; any change must be recorded here with its measurement.

---

## 5. Directory layout

```
CMakeLists.txt              Top-level build: branding, options, dependency pinning, plugin target
CMakePresets.json           dev / debug / release / release-native presets
cmake/AethrWarnings.cmake  Per-file strict warning policy (see §11.2)
External/JUCE/              Pinned JUCE checkout (git-ignored, reused by FetchContent)
Source/
  PluginProcessor.{h,cpp}   AudioProcessor: parameters, state, bus layouts, output stage
  PluginEditor.{h,cpp}      Chassis editor, engine taxonomy, visualisers
  Core/
    Branding.h              Product identity, injected from CMake
    AudioMath.h             Pitch/gain/decay maths, JUCE-free and unit-tested
    RealtimeGuards.h        NaN, denormal, feedback-ceiling and coefficient guards
    TempoSync.h             Host tempo → synced rate/time
  Parameters/
    ParameterIDs.h          Stable IDs only — the host-facing contract
    ParameterLayout.{h,cpp} Ranges, names, groups, display formatting
  DSP/                      Shared primitives: KarplusResonator, Exciter, BodyResonator,
                            FractionalDelay, LoopFilter, Dispersion, FxRack, Modulation
  Engine/
    EngineType.h            Catalogue and `engine.type` indices
    EngineSettings.h        Per-block snapshot
    KarplusStringEngine     Shipped STRING core
    Voice / VoiceEngine     MIDI lifecycle and 64-voice pool
  UI/                       Custom controls, engine strip, visualisers, preset browser
  Presets/                  Factory bank, randomize/mutate
Tests/                      Catch2 suite, linked against the shared-code target
docs/                       ARCHITECTURE, DSP_ARCHITECTURE, ENGINE_REFERENCE, PHYSICAL_MODELS, …
```

`Source/Core` deliberately has no JUCE dependency beyond what it declares, so the maths and safety
layers are testable in isolation and reusable.

The multi-engine contract, fallback rules and next-engine gate are in
[`DSP_ARCHITECTURE.md`](DSP_ARCHITECTURE.md) and [`ENGINE_REFERENCE.md`](ENGINE_REFERENCE.md).

---

## 6. DSP architecture

The mathematics is derived in `docs/DSP_NOTES.md`; this section records the *decisions* and why they
were taken.

### 6.1 Fractional delay: choice and rationale

The resonator's tuning resolution is the fractional part of a delay line. Four candidates were
evaluated against pitch accuracy, transient quality, CPU cost and stability *inside a feedback
loop* — the last criterion is what eliminates the obvious choice.

| Method | Magnitude response | Consequence in a feedback loop |
| --- | --- | --- |
| Linear | Lowpass; attenuation depends on the fractional part | Decay time becomes a function of which fraction a note lands on. Two adjacent semitones can differ audibly in sustain. Rejected as the primary method. |
| Lagrange 3rd order | Much flatter, still not unity | Residual, pitch-dependent loss; ~4 multiplies/sample; needs 4 taps. Good for *modulated* delay. |
| Thiran / first-order allpass | **Exactly unity magnitude** | Decay is controlled solely by the loop filter, which is exactly the separation of concerns we want. Cost: frequency-dependent phase (mild dispersion — often musically desirable) and a state variable that must be handled carefully when the coefficient changes. |
| Higher-order allpass | Unity | More accurate phase, more state, more retune cost. Not justified yet. |

**Decision.** Primary path: integer delay line + **first-order allpass** for the fractional part,
with the fractional target constrained to \([0.5, 1.5]\) samples where the allpass coefficient
\(c=(1-\Delta)/(1+\Delta)\) is well-conditioned. Alternative path: **Lagrange 3rd order**, selected
automatically when pitch is being modulated quickly (glide, deep vibrato, chaos-driven drift), where
a changing allpass coefficient would otherwise produce a discontinuity in stored state.

This is a *measured* decision, not a preference: Phase 3 benchmarks both paths for cents error
across A0–C8 and for CPU cost, and the results are recorded in `docs/PERFORMANCE.md`. If Lagrange
proves within tolerance on decay consistency, the switching logic can be simplified.

### 6.2 Pitch accuracy: the loop is longer than the delay line

Naively `D = fs / f0` is wrong, because the loop filter, the tuning allpass and the dispersion
chain each contribute their own phase delay. The requirement is that the **total** loop delay at
the fundamental equals \(f_s/f_0\):

\[
\frac{f_s}{f_0} \;=\; D_{\text{int}} \;+\; D_{\text{ap}}(\omega_0) \;+\; D_{\text{lf}}(\omega_0) \;+\; D_{\text{disp}}(\omega_0)
\]

with \(\omega_0 = 2\pi f_0/f_s\). Closed forms for each term are derived in `docs/DSP_NOTES.md`.
Because \(D_{\text{lf}}\) and \(D_{\text{disp}}\) depend only weakly on \(\omega_0\), the system is
solved by two or three fixed-point iterations at note-on and whenever pitch moves materially —
cheap, and convergent.

Target: **< ±1 cent** across A0–C8 with dispersion and nonlinearity neutral. Deviations are expected
and documented once dispersion or chaos is engaged, because those features change the partial
structure by design.

### 6.3 Decay: specified as T60, not as "feedback"

A raw feedback knob makes decay depend on pitch: a short loop iterates more often per second, so the
same coefficient decays faster at high notes. Instead the user sets a decay **time**, and the loop
gain is derived per note:

\[
g \;=\; 10^{-3 D / (T_{60} f_s)}
\]

(`aethr::math::decayTimeToLoopGain`, unit-tested against a direct iteration of the loop). Decay is
then perceptually consistent across the keyboard, and "damping"/"brightness" become genuinely
independent controls that shape *which* partials die first rather than how long the note lasts.

### 6.4 Stability strategy

Three independent layers, because one is not enough:

1. **Coefficient clamping** — `guards::clampLoopGain` keeps the loop gain strictly below unity
   (0.9995 max). A nonlinearity or a resonant loop filter can add passband gain, so exact unity is
   already divergent in practice.
2. **State sanitising** — `guards::sanitiseState` is applied at the point where a value enters
   recursive storage: non-finite becomes zero, denormals are flushed, magnitude is clamped to a
   ceiling so a briefly unstable loop *decays* rather than escalating.
3. **Output backstop** — `guards::sanitiseBlock` in the output stage guarantees the host never sees
   NaN or infinity, whatever happens upstream.

This ordering matters: catching NaN only at the output would leave the filter state permanently
poisoned. The guard belongs where the value is produced.

---

## 7. Voice architecture

A custom voice manager rather than `juce::Synthesiser`, because we need:

- per-voice release tails that outlive note-off by seconds (long resonances),
- amplitude-threshold **voice sleeping** so a released voice stops costing CPU,
- steal-with-fade rather than steal-with-cut, to avoid clicks,
- per-voice modulation state driven from the matrix.

Design:

- Fixed pool sized to the maximum polyphony option (64); voices are never allocated during playback.
- `voice.polyphony` selects the *active* count. It is marked non-automatable because changing it
  resizes the active set — a message-thread operation.
- Allocation is O(1) amortised via free/active intrusive lists.
- Stealing policy: prefer sleeping voices, then the quietest released voice, then the oldest held
  voice; the stolen voice fades over a short ramp before restarting.
- A voice sleeps when its output stays below the silence threshold (−120 dB, `math::silenceGain`) for
  a full block *and* its resonator energy estimate is below the same threshold.

---

## 8. Modulation architecture

Hard-wiring `if (lfo1Target == pitch)` does not scale to the destination list in the brief. Instead:

- **Sources** (LFO 1/2, Envelope 1/2, Random, Velocity, Aftertouch, Mod Wheel, Key Track) each
  publish one normalised value per block, and optionally a per-sample buffer when the destination
  needs audio-rate resolution.
- **Destinations** are declared in a table of `{ id, base parameter, modulation range, smoothing
  time }`.
- The **matrix** is a fixed-capacity array of `{ sourceIndex, destinationIndex, depth, curve }`
  slots, evaluated once per block into a dense array of destination offsets.
- DSP blocks read `modulated(destination)` — they never know which source drove them.

Adding a destination is therefore one table entry plus one read, with no change to any source, and
no allocation at any point. Depth is applied in the destination's own units so that "50 % of LFO 1 to
damping" means the same thing regardless of the LFO's shape.

---

## 9. Threading model and the realtime contract

Three threads, with a one-way data discipline:

| Thread | May do | Must never do |
| --- | --- | --- |
| Audio | Read parameter atomics, run DSP, publish measurement atomics | Allocate, lock, log, touch files, call into UI, call the host synchronously |
| Message | Build parameters, serialise state, manage presets, resize voice pool, paint | Block waiting on the audio thread |
| Timer (UI) | Poll measurement atomics at 30 Hz, repaint dirty regions | Read DSP state that is not atomic |

Mechanisms:

- Parameters flow message → audio through `AudioProcessorValueTreeState` raw atomic pointers, cached
  once in the constructor so no lookup by name ever happens on the audio thread.
- Measurements flow audio → UI through `std::atomic` scalars with relaxed ordering (a stale meter
  frame is harmless; a lock is not).
- Anything that must change size (voice pool, scratch buffers, oversamplers) is sized in
  `prepareToPlay`. `processBlock` treats an over-long block as an error to survive, not to allocate
  for: it degrades gracefully and asserts in debug.

The output stage already demonstrates the pattern: the gain ramp is materialised into a scratch
buffer allocated in `prepareToPlay`, and the over-long-block branch falls back to a constant gain
rather than resizing.

---

## 10. Parameter and state contract

- IDs live alone in `Source/Parameters/ParameterIDs.h`, are lowercase and dot-namespaced, and are
  **never** renamed or reused. Retiring a parameter retires its ID permanently.
- `versionHint` is 1 for the whole 1.x series. VST3 derives stable parameter hashes from it, so
  changing it silently invalidates users' automation.
- Three state categories are kept separate: **DSP state** (never serialised), **parameter state**
  (in the APVTS tree, saved by the host), and **UI state** (window size, active page — stored in the
  same tree under a distinct child node so it round-trips without polluting automation).
- `setStateInformation` validates the root node type and rejects foreign or malformed data outright
  rather than applying it partially. This is covered by tests.

---

## 11. Build system

### 11.1 Structure

`juce_add_plugin` produces the shared-code static library plus one target per format. The test
executable links that same static library, so:

- JUCE modules are compiled exactly once for the whole project,
- the code under test is bit-identical to the code that ships,
- there is no second, drifting copy of the source list.

### 11.2 Warning policy

JUCE attaches its module `.cpp` files as `INTERFACE` sources of the module targets, so they are
compiled **into** our target using **our** compile options. Applying `-Wall -Wextra -Wpedantic
-Wsign-conversion -Werror` at target level would therefore drown the build in warnings from
framework code we do not own.

Solution (`cmake/AethrWarnings.cmake`): JUCE's own curated set
(`juce::juce_recommended_warning_flags`) is linked at target level, and the aggressive set is applied
**per source file** to files we maintain. `AETHR_WARNINGS_AS_ERRORS` adds `-Werror` to that
per-file set and is ON in the `dev` and `debug` presets.

### 11.3 Presets

`dev` (RelWithDebInfo + `-Werror`) for daily work, `debug` for assertions and stepping, `release`
for a macOS universal binary with LTO, `release-native` for profiling a single architecture.

---

## 12. UI architecture

- One `LookAndFeel` subclass owns the palette, typography and control drawing; components never
  hard-code colours.
- The editor holds a fixed set of panels; parameters attach through
  `AudioProcessorValueTreeState::SliderAttachment` and friends so the UI never mutates DSP directly.
- A single timer at 30 Hz drives every animated element. Repaints are region-scoped — the Phase 1
  editor already repaints only the meter strip and the status line rather than the whole window.
- Visualisers (resonator, decay envelope, spectrum) render from small ring buffers published by the
  audio thread, downsampled to display resolution *before* publication so the paint routine stays
  cheap.
- Accessibility: every control gets a title, a value readout in real units and a tooltip;
  `setAccessible` defaults are respected rather than suppressed.

Phase 13 note: JUCE 9's `Drawable` is no longer a `Component`, so any vector art must be drawn in
`paint` rather than added as a child.

---

## 13. Preset architecture

- Presets are the APVTS tree serialised to XML, wrapped with metadata (name, category, author,
  engine version) — the same path as host state, so a preset can never contain something the host
  cannot restore.
- Factory presets are compiled in as binary data; user presets live in the standard per-user
  application data directory.
- Forward compatibility: unknown parameters in a loaded preset are ignored, and parameters absent
  from it keep their defaults. This lets 1.1 presets load in 1.0 without corrupting state.
- Randomise/Mutate operate on a curated table of per-parameter distributions with lockable
  parameters and a **seeded** generator, so a "random" patch can be reproduced exactly (brief §37).

---

## 14. Risks

Maintained in `docs/RISKS.md` with owner, likelihood, impact and mitigation. The four that shape the
architecture:

1. **Pitch error from in-loop phase** — mitigated by the compensation in §6.2 and by automated
   cents-error tests before any sound design work begins.
2. **Instability once nonlinearity is inside the loop** — mitigated by the three-layer guard
   strategy in §6.4 and by fuzz tests that sweep parameter combinations looking for NaN or runaway
   growth.
3. **CPU cost of 64 voices with body modes and oversampling** — mitigated by voice sleeping,
   inactive-mode bypass, coefficient caching, and by profiling at every phase boundary rather than
   at the end.
4. **AU packaging without full Xcode** — measured, not assumed; see `docs/BUILD.md`.

---

## 15. Roadmap

`docs/ROADMAP.md` holds the phase plan with explicit exit criteria. The rule for every phase:
build, run tests, read the warnings, inspect DSP behaviour, profile if relevant, fix, commit a
stable checkpoint — and only then continue.
