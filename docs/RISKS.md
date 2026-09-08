# AETHR — Risk register

Ordered by the product of likelihood and impact. Status is updated at each phase boundary.

---

## R1 — Pitch error caused by phase delay inside the loop

**Likelihood** high if unaddressed · **Impact** fatal to the product · **Status** mitigated by design,
to be verified in Phase 3

Setting the delay length to \(f_s/f_0\) and ignoring the phase delay of the loop filter, tuning
allpass and dispersion chain produces an error of one to two samples. At C8 with \(f_s = 48\) kHz the
period is 11.5 samples, so that is roughly 230 cents — the instrument would be unusable in the top
two octaves.

*Mitigation:* the compensation in `DSP_NOTES.md` §1–3, solved by fixed-point iteration, plus automated
cents-error measurement from rendered audio across A0–C8 at all six sample rates **before** any sound
design work starts. Target < ±1 cent.

---

## R2 — Instability once a nonlinearity sits inside the feedback loop

**Likelihood** high · **Impact** severe (NaN, blown speakers, host crash) · **Status** three-layer
guard in place and unit-tested

A saturator inside a loop can have effective gain above unity for some inputs, and a resonant loop
filter has passband gain above unity by construction. Either can turn a stable-looking configuration
divergent. One NaN permanently poisons recursive state.

*Mitigation:* coefficient clamping (`clampLoopGain`, maximum 0.9995), state sanitising at the point of
production (`sanitiseState`: non-finite → 0, denormals flushed, magnitude clamped so an unstable loop
*decays*), and an output backstop (`sanitiseBlock`). All three are unit-tested. Phase 5 and 11 add
parameter-sweep fuzzing that looks for NaN and for growth over long renders.

---

## R3 — CPU cost of the full engine at 64 voices

**Likelihood** medium-high · **Impact** high (unusable in a real project) · **Status** design
decisions taken, measurement deferred to Phase 15 by policy

64 voices × (resonator + up to 16 body modes + oversampled nonlinearity) is a large budget, and the
brief explicitly forbids trading audio correctness for CPU.

*Mitigation:* voice sleeping below the silence threshold; genuinely skipping inactive body modes
rather than multiplying by zero; bypassing inactive modules; caching coefficients and recomputing only
on change; oversampling only nonlinear sections; SIMD where measurement justifies it. Profiling
happens at every phase boundary, not once at the end, so a regression is attributed to the phase that
caused it.

---

## R4 — Zipper noise and clicks from parameter changes

**Likelihood** medium · **Impact** medium-high (sounds amateurish) · **Status** pattern established in
Phase 1

Every parameter that touches a gain, a delay length, a filter coefficient or a pan position can
produce a discontinuity.

*Mitigation:* per-parameter smoothing with time constants chosen per parameter — long enough to be
inaudible, short enough not to blunt intentional fast modulation. The output stage already
demonstrates the approach (ramp materialised into a `prepareToPlay`-allocated scratch buffer). Delay
time changes get crossfaded rather than smoothed, because interpolating a read pointer produces
pitch artefacts.

---

## R5 — Aliasing from the nonlinear stages

**Likelihood** medium · **Impact** high (the classic "cheap plugin" giveaway) · **Status** residual —
saturation currently runs at audio rate without oversampling

*Mitigation (planned):* oversample only the nonlinear sections, at a factor matched to the harmonic
content the mode generates (2× gentle, 4× hard clip and wavefold, 8× extreme); linear-phase FIR
anti-aliasing where dry/wet mixing occurs; report oversampling latency to the host; measure alias
energy by FFT and assert it below threshold at each factor.

*Current shipping behaviour:* Soft / tube / tape modes are preferred defaults; hard clip and
wavefold remain available and can alias at high drive. Documented rather than falsely claimed as
oversampled.

---

## R6 — macOS packaging and loading issues

**Likelihood** medium · **Impact** medium · **Status** two issues found and resolved in Phase 1

Three concrete problems already encountered:

1. **Quarantine inheritance.** Build output inherits `com.apple.quarantine` from the launching
   process, after which dyld refuses to load the plugin even though the signature verifies as valid.
   *Resolved:* post-build step clears attributes and re-signs ad-hoc, for the artefact and the
   installed copy. Documented in `BUILD.md`.
2. **Finder metadata in the build location.** The repository sits on the Desktop, a Finder- and
   iCloud-managed location, which re-attaches `com.apple.FinderInfo` to bundles and makes `codesign`
   refuse to sign them. *Mitigated:* the sanitise step retries, and reports only when the executable's
   own signature is actually invalid (FinderInfo breaks bundle verification but not loading — an AU
   carrying it still passes `auval`). *Recommendation recorded:* build outside Desktop/Documents,
   which was verified to resolve it completely, and is a prerequisite for notarisation.
3. **No full Xcode.** *Resolved for AU v2:* it builds and passes `auval` with Command Line Tools
   alone. AUv3 remains impossible here and is out of scope.

*Residual risk:* shipping requires Developer ID signing and notarisation, neither of which is set up.
That is release engineering, not architecture, but it must not be discovered late.

---

## R7 — Parameter ID or version-hint churn breaking users' sessions

**Likelihood** low-medium · **Impact** severe and irreversible for affected users · **Status**
contract documented and enforced by tests

Renaming an ID, reusing a retired one, or changing `versionHint` silently destroys automation and
saved state in every existing project.

*Mitigation:* IDs isolated in `ParameterIDs.h` with the rules stated in the file itself; `versionHint`
fixed at 1 for the 1.x series; tests assert uniqueness and the naming convention; state restoration
tested into a fresh instance. Adding a parameter is safe; changing one is a release-management
decision.

---

## R8 — Scope

**Likelihood** high · **Impact** medium · **Status** managed by the phase gates

The brief is very large: 12 excitation modes, 12 materials, 16 body modes, 9 nonlinear modes, 9
effects, 50+ presets, a full modulation matrix and an original UI.

*Mitigation:* the phase plan in `ROADMAP.md`, each with concrete exit criteria and a commit. The
architecture is deliberately built so breadth is additive — a new excitation mode, material or
modulation destination is a table entry, not a structural change. Features explicitly deferred are
listed at the end of the roadmap rather than left ambiguous.

---

## R9 — Apple clang C++23 library gaps

**Likelihood** low · **Impact** low · **Status** accepted

Apple clang's libc++ lags on some C++23 *library* features (`<format>`/`std::print` coverage,
`std::mdspan`). The DSP layer needs none of them.

*Mitigation:* if a facility is missing, work around it locally and note it in `ARCHITECTURE.md` §2.1
rather than lowering the whole project's standard. `-ffp-contract=off` and no fast-math are
non-negotiable and unrelated to the standard version.

---

## R10 — Windows portability debt

**Likelihood** medium · **Impact** low-medium · **Status** open

The build is portable by construction but has never been compiled on Windows.

*Mitigation:* MSVC warning flags already present; no Apple-specific CMake outside `if(APPLE)`; no
platform APIs used directly. Expect a short round of warning fixes on the first Windows build, and do
not schedule it as the last task before a release.
