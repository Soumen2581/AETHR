# AETHR — Engine reference

`engine.type` choice indices are stable. Every catalogue entry now dispatches to a
distinct core or character. STRING remains the pitch-accurate default (index 0).

| Index | Name | Family | Core | Character knobs (A–D) |
|------:|------|--------|------|------------------------|
| 0 | String | Physical | Karplus–Strong | Tension, Position, Loss, Pickup |
| 1 | Pluck | Physical | Karplus–Strong | Pluck, Position, Tension, Pickup |
| 2 | Bowed | Physical | Karplus–Strong + friction | Pressure, Speed, Friction, Position |
| 3 | Bell | Physical | Modal bank | Strike, Inharmonic, Partial, Bloom |
| 4 | Plate | Physical | Modal bank | Size, Stiff, Hit, Spread |
| 5 | Membrane | Physical | Modal bank | Tension, Size, Hit, Damp |
| 6 | Tube | Physical | Karplus–Strong (inverted R) | Length, Breath, Open, Turbulence |
| 7 | Cavity | Physical | Modal bank | Volume, Opening, Air, Coupling |
| 8 | Waveguide | Synthetic | Dual KS + stiffness | Scatter, Dispersion, Node, Feedback |
| 9 | Modal | Synthetic | Modal bank | Modes, Stretch, Q, Tilt |
| 10 | Granular | Experimental | KS with retriggered grains | Size, Density, Pitch, Spray |
| 11 | Spectral | Synthetic | Dense modal bank | Partials, Stretch, Freeze, Diffuse |
| 12 | Hybrid | Experimental | KS into modal | Balance, Couple, Cross, Limit |

Shared resonator knobs (decay, damping, bright, feedback, stiffness) still apply to
every engine. The four engine knobs in the Resonator panel rename with the selected
engine — they are the “how to play this model” controls.

## Sound-quality notes (Sprint 3)

- Mid-note engine-family changes re-trigger the active core (`Voice::applySettings`) —
  verified without `processor.reset()` in Catch2.
- FX filter SVF coefficients update once per block (or when settings change), not every sample.
- Chorus and phaser use independent LFO phases so wet Motion FX do not lock together.
- Factory / Init saturation defaults to **Soft**. Hard clip and wavefold remain available
  and stay finite under high drive, but may alias (no oversampling yet — see RISKS R5).

## STRING

**Class:** `aethr::engine::KarplusStringEngine`  
**IDs:** `resonator.*`, `exciter.*`, `body.*`, `layer.b.*`, `engine.control.*`  
**Tests:** `[pitch]`, `[resonator]`, `[exciter]`, `[engine][acceptance]`

Exciter → dual phase-compensated KS loops → optional Layer B → 16-mode body. Loop gain
is T60-derived then scaled by `resonator.feedback`. Interpolation default is allpass.
Pitch target: < ±1 cent A0–C8 in clean configurations.

## Modal family (Bell, Plate, Membrane, Cavity, Modal, Spectral)

**Class:** `aethr::engine::ModalEngine`

The body bank is the resonator, not a colouration after a string. Mix is forced to 1.
Inharmonicity comes from `engine.control.b` plus `resonator.stiffness`. Do not require
±1 cent pitch on these engines.

## Arpeggiator

**IDs:** `arp.enable`, `arp.mode`, `arp.division`, `arp.octaves`, `arp.gate`, `arp.swing`,
`arp.latch`, `arp.pattern`  
**Tests:** `[engine][arp]`

Tempo-synced, sample-accurate. Modes: Up, Down, Up-Down, Played, Random, Chord.
`arp.pattern` is a 16-bit gate (bit 0 = step 1). Default all steps on. When disabled,
incoming MIDI is passed through unchanged.

## Adding an engine

1. Write the mathematical model, stability risks, CPU cost and tests in this file.
2. Implement the engine in `Source/Engine/` with `prepare()`-only allocation.
3. Prefer a new parameter namespace for model-specific physics. Shared `engine.control.*`
   knobs are for performance character, not a substitute for a real PDE.
4. Dispatch from `Voice`. Keep STRING as default index 0.
5. Never reuse an `engine.type` index for a different model.
