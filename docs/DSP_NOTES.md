# STRATA — DSP notes

Mathematical reference for the engine. Every formula here is either implemented in
`Source/Core`/`Source/DSP` or is the specification a later phase implements against. Where a
formula is already implemented, the test that pins it is named.

Notation: \(f_s\) sample rate (Hz), \(f_0\) fundamental (Hz), \(\omega = 2\pi f/f_s\) normalised
radian frequency, \(\omega_0 = 2\pi f_0/f_s\), \(D\) loop length in samples.

---

## 1. The loop, and why the naive delay length is wrong

The classic Karplus–Strong loop is a delay line of \(D\) samples with a loss filter in the feedback
path. Its resonances sit at integer multiples of \(f_s/D\), so the obvious tuning rule is

\[
D = \frac{f_s}{f_0}.
\]

This is wrong as soon as the loop contains anything other than the delay line, because **every
element in the loop contributes phase delay**. The resonance condition is that the *total* phase
delay around the loop equals one period at \(f_0\):

\[
\frac{f_s}{f_0} \;=\; D_{\text{int}} \;+\; D_{\text{ap}}(\omega_0) \;+\; D_{\text{lf}}(\omega_0) \;+\; D_{\text{disp}}(\omega_0)
\tag{1}
\]

where the terms are the integer delay line, the tuning allpass (§2), the loop/loss filter (§3) and
the dispersion chain (§5). Ignoring the last three terms produces an error of roughly one to two
samples. At \(f_0 = 4186\) Hz (C8) with \(f_s = 48\) kHz the period is only 11.5 samples, so a
1.5-sample error is **about 230 cents** — more than a whole tone. This single correction is the
difference between an instrument and a toy.

Because \(D_{\text{lf}}\) and \(D_{\text{disp}}\) vary slowly with \(\omega_0\), (1) is solved by
fixed-point iteration: evaluate the phase-delay terms at the current pitch estimate, subtract, and
re-evaluate. Two or three iterations reach well under 0.1 cent.

---

## 2. Fractional delay by first-order allpass

The tuning allpass is

\[
A(z) = \frac{c + z^{-1}}{1 + c\,z^{-1}}, \qquad c = \frac{1-\Delta}{1+\Delta}
\tag{2}
\]

for a target fractional delay \(\Delta\) samples. Its phase response is

\[
\theta(\omega) = -\omega + 2\arctan\!\left(\frac{c\sin\omega}{1 + c\cos\omega}\right)
\tag{3}
\]

and therefore its phase delay is

\[
D_{\text{ap}}(\omega) \;=\; -\frac{\theta(\omega)}{\omega} \;=\; 1 - \frac{2}{\omega}\arctan\!\left(\frac{c\sin\omega}{1 + c\cos\omega}\right).
\tag{4}
\]

Sanity checks, all satisfied by (4):

- \(c = 0 \Rightarrow A(z) = z^{-1} \Rightarrow D_{\text{ap}} = 1\) at every frequency.
- \(c = 1 \Rightarrow A(z) = 1 \Rightarrow D_{\text{ap}} = 0\) at every frequency.
- \(\omega \to 0\): \(\arctan(x)\to x\) gives \(D_{\text{ap}} \to 1 - 2c/(1+c) = \Delta\), which is the
  design goal.

**Why an allpass and not interpolation.** \(|A(e^{j\omega})| = 1\) exactly. Inside a feedback loop
that matters enormously: the decay time is then set *only* by the loss filter, so "decay" and
"tuning" are independent controls. Linear interpolation, by contrast, is a lowpass whose attenuation
depends on \(\Delta\) — with it, decay time becomes a function of where a note's fractional part
happens to land, and adjacent semitones audibly differ in sustain.

**Conditioning.** \(A\) is stable for \(|c| < 1\), i.e. \(\Delta > 0\). The coefficient becomes
ill-conditioned as \(\Delta \to 0\) (\(c \to 1\), pole and zero collide on the unit circle). We
therefore constrain \(\Delta \in [0.5, 1.5]\), giving \(c \in [-0.2, \tfrac{1}{3}]\), and absorb the
integer part into the delay line. Within that window the allpass is numerically benign and its
frequency-dependent phase error is small.

**Cost.** One multiply-add pair and one state variable per sample.

**When Lagrange is used instead.** The allpass has state. When the coefficient changes quickly —
portamento, deep vibrato, chaos-driven drift — the stored state no longer corresponds to the new
coefficient and a discontinuity is audible. For those cases the engine switches to 3rd-order
Lagrange interpolation, which is stateless with respect to the fraction:

\[
y[n] = \sum_{k=0}^{3} h_k\, x[n-k], \qquad
h_k = \prod_{\substack{m=0 \\ m \neq k}}^{3} \frac{\Delta - m}{k - m}.
\]

Its magnitude response is not exactly unity, so the loss compensation of §4 is applied with the
interpolator's measured gain at \(f_0\) folded in.

---

## 3. Loop filter (loss filter)

Two forms are used.

**One-pole lowpass**, the workhorse:

\[
H(z) = \frac{1-a}{1 - a\,z^{-1}}, \qquad 0 \le a < 1
\tag{5}
\]

normalised to unity gain at DC. Magnitude and phase delay:

\[
|H(e^{j\omega})| = \frac{1-a}{\sqrt{1 - 2a\cos\omega + a^2}}, \qquad
D_{\text{lf}}(\omega) = \frac{1}{\omega}\arctan\!\left(\frac{a\sin\omega}{1 - a\cos\omega}\right).
\tag{6}
\]

\(a = 0\) gives a pure gain with zero phase delay, as expected.

**Two-point average**, the original Karplus–Strong loss filter:

\[
H(z) = \tfrac{1}{2}\left(1 + z^{-1}\right), \qquad |H(e^{j\omega})| = \cos(\omega/2), \qquad D_{\text{lf}} = 0.5 \ \text{samples exactly}.
\]

Its phase delay is exactly half a sample at all frequencies (it is linear phase), which makes it the
cheapest option to compensate for — at the cost of a fixed, quite dark tone.

**Stability inside the loop.** Whatever the selected response — including the resonant lowpass and
tilt options in the brief — the requirement is \(g\,|H(e^{j\omega})| < 1\) for all \(\omega\). A
resonant filter has passband gain above unity near its cutoff, so the loop gain must be scaled by
\(1/\max_\omega |H|\) before it is used. This is why the loop gain is always passed through
`guards::clampLoopGain` (maximum 0.9995) rather than being trusted.

---

## 4. Decay: specified as \(T_{60}\), derived as a gain

A loop that multiplies by \(g\) once per \(D\) samples completes \(f_s/D\) round trips per second.
Requiring 60 dB of attenuation after \(T_{60}\) seconds gives

\[
g^{\,T_{60} f_s / D} = 10^{-3}
\qquad\Longrightarrow\qquad
\boxed{\;g = 10^{-3D/(T_{60} f_s)}\;}
\tag{7}
\]

Implemented as `strata::math::decayTimeToLoopGain`, with `loopGainToDecayTime` as its inverse.
Both are pinned by *"Loop gain and T60 decay time are exact inverses"* and, more importantly, by
*"Loop gain produces the requested 60 dB decay when iterated"*, which iterates the loop directly and
checks the amplitude lands at −60 dB within 0.1 dB.

**Why this and not a raw feedback knob.** \(g\) in (7) depends on \(D\), i.e. on pitch. A fixed
feedback coefficient decays faster for high notes, because a short loop iterates more often per
second. Specifying the *time* and deriving the coefficient per note makes decay perceptually
consistent across the keyboard, and turns "damping" and "brightness" into controls over *which*
partials die first rather than over how long the note lasts.

**Folding in the filter's loss.** The loss filter also attenuates at \(f_0\), so to actually achieve
\(T_{60}\) the gain applied must be

\[
g_{\text{applied}} = \min\!\left(g_{\max},\; \frac{10^{-3D/(T_{60} f_s)}}{|H(e^{j\omega_0})|}\right).
\tag{8}
\]

The clamp is what makes "infinite" sustain safe: as \(T_{60} \to \infty\) the requested gain
approaches \(1/|H|\), and the clamp holds it just below unity.

**Frequency-dependent decay.** The \(T_{60}\) of the partial at \(\omega\) is

\[
T_{60}(\omega) = \frac{-3D}{f_s \log_{10}\!\big(g\,|H(e^{j\omega})|\big)},
\]

which is the mathematical content of "damping" and "brightness": they shape \(|H|\), and hence the
ratio of high-partial decay to fundamental decay. This is also the model for **air absorption**,
which in a real string is a mild frequency-dependent loss — implemented as a gentle one-pole tilt
whose effect grows with the note's length.

---

## 5. Dispersion and stiffness

An ideal string is non-dispersive: all partials travel at the same speed and the spectrum is
harmonic. A real string has bending stiffness, so higher partials travel faster and the spectrum is
stretched — this is what makes a piano's upper partials sharp and gives bells and glass their
character.

Modelled as a cascade of \(M\) first-order allpasses in the loop:

\[
A_{\text{disp}}(z) = \left(\frac{\eta + z^{-1}}{1 + \eta\,z^{-1}}\right)^{M}, \qquad |\eta| < 1
\tag{9}
\]

with total phase delay \(M \cdot D_{\text{ap}}(\omega)\big|_{c=\eta}\) from (4). Because that delay
*decreases* with frequency for \(\eta > 0\), the partials are progressively stretched, which is
exactly the desired inharmonicity. The parameter mapping:

- **stiffness** → \(\eta\) magnitude (how strongly partials are displaced),
- **dispersion** → \(M\) (how far up the spectrum the effect reaches),
- **inharmonicity** → signed \(\eta\), allowing stretched or compressed spectra,
- **mode spread** → an additional per-partial offset applied in the body resonator (§6).

Stability is unconditional for \(|\eta| < 1\), which is enforced by clamping. Note that engaging
dispersion **changes the perceived pitch by design**: (1) keeps the *fundamental* on target, but the
partials are deliberately no longer integer multiples of it. This is documented behaviour, not
error, and the pitch tests exclude it (see `docs/TESTING.md`).

---

## 6. Body resonator

A parallel bank of second-order modal resonators, each modelling one resonance of an instrument
body:

\[
H_k(z) = \frac{g_k\,(1 - r_k^2)}{1 - 2 r_k \cos(\omega_k)\,z^{-1} + r_k^2\,z^{-2}}
\tag{10}
\]

implemented as the difference equation

\[
y_k[n] = 2 r_k \cos(\omega_k)\, y_k[n-1] - r_k^2\, y_k[n-2] + g_k (1-r_k^2)\, x[n].
\]

Pole radius from either bandwidth or decay time:

\[
r_k = e^{-\pi B_k / f_s} \quad (B_k = \text{−3 dB bandwidth in Hz}), \qquad
r_k = 10^{-3/(T_{60,k} f_s)} \quad (\text{decay form}).
\]

Poles lie at \(r_k e^{\pm j\omega_k}\), so \(r_k < 1\) guarantees stability; the \((1-r_k^2)\)
factor keeps the peak gain roughly constant as the bandwidth changes, so sweeping "body decay" does
not also sweep loudness.

Each mode carries `{ frequency ratio, gain, decay, bandwidth }`. Mode counts of 1/2/4/8/16 are
selectable, and **inactive modes are skipped entirely** rather than being computed with zero gain —
with 64 voices the difference between 16 evaluated modes and 2 is the difference between shipping and
not.

---

## 7. Nonlinearity and oversampling

The nonlinear stage sits after the resonator (and optionally inside the feed loop). A memoryless
saturator \(y = f(x)\) generates harmonics; any harmonic above \(f_s/2\) folds back as aliasing,
which is the single most common reason a physical-modelling instrument sounds cheap.

Rule of thumb: a soft saturator's audible harmonic content extends to roughly the 8th harmonic, so
content up to \(8 f_0\) must fit below Nyquist. Oversampling by \(L\) raises the effective Nyquist
to \(L f_s/2\). Hence 2× is adequate for gentle saturation, 4× for hard clipping and wavefolding, and
8× only for the most extreme wavefold and chaotic modes.

Consequences for the design:

- Only the nonlinear section is oversampled, never the whole chain. Oversampling a linear filter buys
  nothing and costs a great deal.
- `juce::dsp::Oversampling` (verified present in JUCE 9.0.1, with both
  `filterHalfBandFIREquiripple` and polyphase IIR designs) provides the anti-imaging/anti-aliasing
  filters. The FIR equiripple design is linear phase, which matters when the dry and wet paths are
  mixed.
- Latency is reported to the host via `setLatencySamples` using
  `Oversampling::getLatencyInSamples()`, otherwise oversampling silently smears timing.
- A **DC blocker** follows every asymmetric nonlinearity, because asymmetric transfer functions
  produce a DC offset that would otherwise accumulate in the feedback loop and eat headroom:

\[
y[n] = x[n] - x[n-1] + R\,y[n-1], \qquad R = 1 - \frac{2\pi f_c}{f_s},
\]

with \(f_c\) around 5–20 Hz.

---

## 8. Numerical safety

Three properties are enforced, in this order, because catching a NaN at the output is too late — by
then the recursive state is permanently poisoned.

1. **Coefficients are validated before use.** `guards::isValidCoefficient` in debug,
   `guards::clampLoopGain` always.
2. **Values entering recursive state are sanitised.** `guards::sanitiseState` replaces non-finite
   values with zero, flushes denormals, and clamps magnitude to a ceiling so an unstable loop *decays*
   rather than reaching infinity.
3. **The output is a backstop.** `guards::sanitiseBlock` in the output stage.

Denormals: `juce::ScopedNoDenormals` covers the block, but state that persists *between* blocks is
not covered by it, hence the explicit flush. A decaying resonator tail is exactly the case that
generates denormals — amplitudes fall smoothly towards zero and can spend a long time in the
denormal range, where arithmetic can be orders of magnitude slower.

Floating-point configuration: `-ffp-contract=off` project-wide, fast-math never enabled. The guards
above depend on IEEE-754 semantics (NaN comparison behaviour, denormal representation) that
fast-math is explicitly permitted to break.

**Precision.** Pitch and decay maths is done in `double`. At 20 Hz, one cent is about 0.012 Hz; a
`float` mantissa of 24 bits does not reliably preserve that once a delay length and then a filter
coefficient are derived from it. Audio sample data stays in `float` unless the host requests double
precision, which the processor supports.

---

## 9. Excitation

The exciter must be **deterministic when asked**, so a preset reproduces exactly (brief §3, §37).
All noise sources therefore draw from an explicitly seeded PRNG owned by the voice, never from a
global or time-seeded generator. The seed is part of the preset when "deterministic" is enabled and
is re-drawn per note otherwise.

Spectral shaping:

- **White** noise: flat. **Pink**: −3 dB/octave, via a cascade of one-pole filters fitted to the
  ideal \(1/f\) slope.
- **Burst duration** windows the excitation with a raised-cosine window rather than a rectangular
  one; a rectangular gate has a \(\text{sinc}\) spectrum whose sidelobes produce an audible click
  distinct from the intended transient.
- **Excitation position** (a plucking point) is modelled as a comb: exciting at fractional position
  \(\beta\) along the string cancels partials at multiples of \(1/\beta\), which is the physically
  correct way to get "plucked near the bridge" brightness rather than merely EQing the burst.

---

## References

The standard literature this implementation follows:

- Karplus & Strong, *Digital Synthesis of Plucked-String and Drum Timbres*, CMJ 1983.
- Jaffe & Smith, *Extensions of the Karplus–Strong Plucked-String Algorithm*, CMJ 1983 — source of
  the loop-filter phase-delay compensation and the allpass tuning approach.
- Smith, *Physical Audio Signal Processing* — dispersion allpass chains, modal synthesis, and the
  stability analysis reproduced above.
- Välimäki et al., *Discrete-Time Modelling of Musical Instruments*, Rep. Prog. Phys. 2006.
