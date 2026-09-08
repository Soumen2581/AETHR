# AETHR — DSP architecture

This is the multi-engine contract. Equations for the shipped STRING core live in
[`DSP_NOTES.md`](DSP_NOTES.md). Per-engine behaviour lives in [`ENGINE_REFERENCE.md`](ENGINE_REFERENCE.md).

## Signal path

```
MIDI
  → Arpeggiator (optional; pass-through when off)
  → VoiceEngine (64 preallocated voices, steal / sleep)
      → Voice (note lifecycle)
          → KarplusStringEngine  (string / pluck / bowed / tube / waveguide / granular)
          → ModalEngine          (bell / plate / membrane / cavity / modal / spectral)
          → Hybrid               (KS into modal, mix = engine.control.a)
  → sum
  → FxRack (filter, saturation, delay, chorus, phaser, chamber)
  → DC block + output gain
```

`engine.type` selects the sounding core. Indices are never reused. STRING remains
index 0 and is the only engine that must meet the ±1 cent pitch contract.

Shared `resonator.*` / `exciter.*` / `body.*` IDs stay. Per-engine performance
character lives on `engine.control.a`–`d`. The arpeggiator is `arp.*`.
`versionHint` remains 1.

## Realtime rules

- Allocate in `prepare()` only.
- One settings snapshot per block (`engine::Settings`).
- No virtual `processSample()` on the hot path. New engines are added as concrete
  members of `Voice` (or a `std::variant` sized at prepare), dispatched once per block.
- Visualization data is published through atomics / snapshots. The editor never reads
  mutable DSP objects.

## Parameter IDs

Existing `resonator.*`, `exciter.*`, `body.*` IDs stay as the shared physical-model
controls. Engine character is `engine.control.a`–`d`. Arpeggiator is `arp.*`.
`versionHint` remains 1.

## Pitch contract

STRING (index 0) must stay within ±1 cent A0–C8 in clean configurations. Other engines
are character models and are only required to be finite and audible.
