# Sprint Review — Sprint 1

**Sprint:** AETHR Foundation Hardening  
**Date:** 2026-09-08  
**Goal:** Close remaining host/DSP contract holes so offline bounce, delay automation, session recall, and risk docs are trustworthy.

## Completed

| Story | Result |
|-------|--------|
| S1-1 Synced tail length | `getTailLengthSeconds` uses `resolvedDelaySeconds` |
| S1-2 Click-free delay | ~20 ms read crossfade on time jumps in `FxRack` |
| S1-3 Preset identity in state | `aethrFactoryPresetIndex` property + editor restore |
| S1-4 MIDI CC1 contract | CC1 → brightness (live); documented in INSTALL |
| S1-5 Risk register truth | R10 updated for Windows CI |
| Agile OS docs | `AGILE_PRODUCT_BACKLOG.md`, `TECHNICAL_DEBT.md` |

## Not completed (defer)

- Held-note engine-switch test without `reset()` (C-2)
- Filter per-block coeffs (B-4)
- Scoped UI repaint (D-6)

## Bugs discovered

None blocking. Polish CI run for `f55ea55` was still in progress at review time — verify before calling Sprint 1 fully closed on CI.

## Technical debt

Updated `docs/TECHNICAL_DEBT.md` — TD-3, TD-7, TD-11, TD-12, TD-13 addressed this sprint.

## Test results

Local: `ctest --preset ci` — **80 tests** expected after rediscovery; preset index + factory leak tests pass. Full suite re-run green after Sprint 1 changes.

## Next sprint recommendation

**Sprint 2 — AETHR Visual System:** scoped repaints, Resonance Core telemetry honesty, a11y, preset browser categories (no giant rewrite).
