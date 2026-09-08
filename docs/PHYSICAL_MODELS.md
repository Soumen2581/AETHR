# AETHR — Physical models

Materials and engines are **parameter mappings and DSP topologies**, not samples.

## Shipped: STRING (Karplus–Strong waveguide)

A fractional delay, a selectable loop filter, optional dispersion allpasses, and a
T60-derived loop gain form a 1D waveguide. Stereo comes from decorrelated excitation
into two identical-length loops. A modal `BodyResonator` colours the result; it is not
the primary resonator.

Pitch is the total loop phase delay, not `fs / f0`. See [`DSP_NOTES.md`](DSP_NOTES.md).

## Materials

`material.type` writes damping, brightness, decay, stiffness, body and exciter starting
points. The user can override every underlying control afterwards. Morphing between
materials is a later increment: interpolate the mapped parameters continuously, do not
switch presets.

## Planned models

| Engine | Intended model | Must not be |
|--------|----------------|-------------|
| PLUCK | Pluck/pickup position, tension, nonlinear displacement | STRING with a different exciter |
| BOWED | Stable friction / stick-slip | Sustained noise into KS |
| BELL | Inharmonic modal bank as the *primary* resonator | Body preset on a string |
| PLATE | 2D modal / plate modes | Stiffness on a 1D loop |
| MEMBRANE | Tensioned circular modes | Drum body preset |
| TUBE | Bore waveguide, open/closed | Reverb |
| CAVITY | Helmholtz / body cavity with coupling | Chamber FX |
| WAVEGUIDE | Bidirectional delay network, scattering | Single KS loop |
| MODAL | General modal engine, variable mode count | BodyResonator reused as-is |
| GRANULAR | Grains as *excitation* into a resonator | Generic granular sampler |
| SPECTRAL | Spectral/modal hybrid | Generic FFT effect |
| HYBRID | Two engines, parallel / serial / coupled | Dual KS layers |

Each model is documented with equations, stability bounds and a test plan in
[`ENGINE_REFERENCE.md`](ENGINE_REFERENCE.md) *before* it is implemented.
