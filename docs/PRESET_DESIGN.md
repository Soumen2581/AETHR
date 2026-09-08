# AETHR — Preset design

## Purpose

A factory bank is documentation that makes sound. Each preset must **demonstrate a distinct
capability of the engine** — a producer should be able to work out what a control does by comparing
two presets. A bank of 50 variations on one patch is worthless regardless of how good that patch is.

Rules:

1. No preset exists without a reason that can be stated in one sentence.
2. No preset clips. Peak output stays at or below −6 dBFS so the user has headroom to play.
3. No preset relies on the final limiter to sound acceptable. The limiter is a safety device, not a
   mix tool.
4. Every preset is playable across at least two octaves. A patch that only works on one note is a
   sound effect, and belongs in the FX category if it ships at all.
5. Every preset has a stable decay: holding a chord must not build up into feedback.

## Categories

Thirteen categories, with the capability each is meant to expose. Target ≥ 50 presets total.

| Category | Demonstrates | Target |
| --- | --- | --- |
| Plucked | Accurate pitch tracking, excitation position, short-to-medium \(T_{60}\), string materials | 6 |
| Metallic | Dispersion and inharmonicity; steel and metal materials | 5 |
| Bells | Strong dispersion plus a modal body; long decay with fast-decaying upper partials | 4 |
| Percussion | Very short decay, high damping, noise-burst and click excitation, drum-like bodies | 5 |
| Organic | Wood and nylon materials, subtle chaos for note-to-note variation, gentle body resonance | 4 |
| Glass | High brightness, narrow-bandwidth body modes, crystalline mode ratios | 4 |
| Drones | Near-unity loop gain held safely, slow modulation, layer detuning | 4 |
| Dark | Low-pass loop filtering, low registers, saturation, minimal high-partial content | 4 |
| Experimental | Unusual mode ratios, extreme dispersion, unstable-but-bounded feed loop | 4 |
| Psychedelic | Deep modulation, phaser and delay interaction, chaos-driven drift | 4 |
| Bass | Clean low-register tracking, controlled decay, tight body, no muddiness | 4 |
| Atmosphere | Long reverb, slow evolving modulation, layered resonators | 4 |
| FX | Non-pitched or intentionally chaotic material; risers, impacts, textures | 4 |

## Format and compatibility

A preset is the APVTS tree serialised to XML plus a metadata header (name, category, author, engine
version). It travels the same code path as host state, so a preset can never contain something the
host cannot restore.

Forward and backward compatibility:

- Unknown parameters in a loaded preset are **ignored**, not treated as an error.
- Parameters absent from a preset keep their **defaults**.

Together these mean a 1.1 preset loads in 1.0 with the new features simply inactive, and a 1.0 preset
loads in 1.1 with new parameters at sensible defaults. This is only true while the ID contract in
`ARCHITECTURE.md` §10 is honoured.

Factory presets are compiled in as binary data (so they cannot be deleted or corrupted); user presets
live in the standard per-user application data directory.

## Macro controls

Eight macros, each mapping to several underlying parameters along a musically designed curve. They
exist so a producer can get somewhere interesting without knowing what a dispersion allpass is, and so
that a preset can be varied expressively without opening the full panel.

| Macro | Drives |
| --- | --- |
| MATERIAL | Damping, dispersion, brightness, feedback, body character, nonlinear amount |
| ATTACK | Excitation burst duration, attack shape, excitation brightness, transient intensity |
| DECAY | \(T_{60}\), loss, air absorption, body decay |
| BRIGHTNESS | Loop-filter cutoff, excitation colour, tilt, high-partial loss |
| BODY | Body mix, active mode count, mode bandwidth, body decay |
| CHAOS | Excitation randomisation, pitch drift, feedback and damping modulation, nonlinear instability |
| SPACE | Reverb size/mix, delay mix, stereo width, spread |
| DRIVE | Nonlinear drive, saturation tone, output compensation |

Design constraints for the mappings:

- A macro at its default position must leave the sound at the preset's intended state, so macros are
  offsets, not absolute values.
- No macro position may produce instability, clipping, or silence. The endpoints are useful, not
  merely extreme.
- **Output compensation** is built into DRIVE and BODY: increasing drive must not simply increase
  loudness, or the user perceives "louder" as "better".

## Randomise and Mutate

Uniform randomisation of every parameter produces noise, not sounds. Instead each parameter carries a
distribution in a curated table: which values are musically plausible, and how likely.

Four modes:

| Mode | Behaviour |
| --- | --- |
| Safe | Narrow deviation from the current patch. Always musical, never surprising. |
| Musical | Moderate deviation within known-good regions of the space. The default. |
| Experimental | Wide deviation; inharmonic and unusual settings become likely. |
| Chaotic | Very wide, including deliberately unstable-but-bounded regions. Still cannot produce NaN or runaway feedback — the guards in `DSP_NOTES.md` §8 are not bypassed by any mode. |

Operations:

- **Randomise** — draw fresh values for all unlocked parameters.
- **Mutate** — perturb unlocked parameters around their current values, so a promising patch can be
  explored rather than replaced.
- **Lock** — exclude selected parameters or whole sections. The intended workflow is to lock pitch and
  decay, then mutate excitation, body and effects.

**Reproducibility** (brief §37): the generator is explicitly seeded, and the seed is stored with the
result. Re-applying a seed reproduces a patch exactly. No use of time-seeded or global RNG anywhere in
the preset system, and the same applies to the excitation PRNG so that a "random" patch also renders
identically.
