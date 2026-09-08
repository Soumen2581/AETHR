# AETHR — Final Polish Audit

Date: 2026-09-08  
Repo: https://github.com/Soumen2581/AETHR  
Scope: finish quality (sound, UI, reliability, docs, CI) — not feature expansion.

## Verdict (baseline)

The instrument is already substantial: 13 engines on two DSP cores, APVTS-backed UI,
Catch2 suite (~78 tests), macOS/Windows CI + release packaging. Branding is cleanly
**AETHR / ixmuk**. Final polish is blocked by a few **fake or incomplete product
promises**, DSP/host contract bugs, and stale documentation — not by missing engines.

---

## P0 — Must fix

| ID | Area | Issue | Location |
|----|------|-------|----------|
| P0-1 | Presets | Factory presets other than Init only patch a few params; prior state leaks | `Source/Presets/PresetManager.h` |
| P0-2 | UI | SAVE button does nothing useful (undo txn + pulse only) | `PluginEditor.cpp` |
| P0-3 | DSP/host | `getTailLengthSeconds()` uses `min(release, decay)` — truncates offline bounce | `PluginProcessor.cpp` |
| P0-4 | Realtime | Arp `MidiBuffer::addEvent` can grow past prepare-time capacity | `Arpeggiator.h`, `PluginProcessor.cpp` |
| P0-5 | Engines | Mid-note engine switch leaves dormant core frozen → click / double attack | `Voice.cpp` |
| P0-6 | Docs | `TESTING.md` / ROADMAP still claim 28 tests, no Windows, pluginval 10 | `docs/TESTING.md`, etc. |

## P1 — Important

| ID | Area | Issue |
|----|------|-------|
| P1-1 | UI | Tooltips missing on combos, toggles, header actions, arp, engines |
| P1-2 | UI | `fx.phaser.feedback` and `exciter.seed` have DSP but weak/no controls |
| P1-3 | UX | Sync on still shows free Rate/Time as equally active |
| P1-4 | UX | No Performance / Advanced density mode |
| P1-5 | Presets | Thin factory bank (13); browser has no categories |
| P1-6 | DSP | Delay time jumps without crossfade; filter coeffs every sample |
| P1-7 | FX | Comb filter only updates left delay line |
| P1-8 | CI | Release workflow skips pluginval |
| P1-9 | State | Preset name/index not restored with host state |
| P1-10 | MIDI | No CC1 / aftertouch / program change (document or add minimal CC) |
| P1-11 | Docs | README risk of overselling engine physics depth vs two cores |

## P2 — Polish

| ID | Issue |
|----|-------|
| P2-1 | Wordmark hardcodes `"AETHR"` instead of `branding::productName` |
| P2-2 | Full-editor 36 Hz `repaint()` |
| P2-3 | Layout via magic child indices in `resized()` |
| P2-4 | Accessibility: custom hit zones without handlers |
| P2-5 | Modulation matrix is read-only overview |
| P2-6 | Visualizers are stylized, not audio scopes |
| P2-7 | Unsigned / un-notarized macOS release zips (Gatekeeper) |
| P2-8 | Chorus + phaser share one LFO phase |

## Already solid

- Realtime processBlock contract (no heap/UI/logging on audio path) for the main render
- Parameter ID permanence + `versionHint`
- Engine strip / arp strip APVTS wiring
- INIT / RAND / MUTATE / UNDO / REDO
- CI matrix macOS + Windows, artefact verify, pluginval on CI macOS
- Pitch accuracy + NaN/stability tests

## Fix order for this pass

1. P0-1 … P0-6  
2. P1-1, P1-2, P1-3, P1-4 (simple Advanced), P1-7, P1-8, docs  
3. Expand factory presets with init-then-apply + better default  
4. Build + ctest; do not claim unverified items  

## Completed in this pass (2026-09-08)

| ID | Fix |
|----|-----|
| P0-1 | `applyFactory` always `init()` then delta |
| P0-2 | SAVE writes user `.aethr` presets via FileChooser |
| P0-3 | `getTailLengthSeconds` uses max(decay, release, delay, reverb) |
| P0-4 | Arp MIDI buffer sized to 64 KiB at prepare |
| P0-5 | Mid-note engine-family switch retriggers cores |
| P0-6 | TESTING / BUILD / README / RISKS aligned with reality |
| P1-1 | Tooltips on combos, toggles, header actions, lab |
| P1-2 | Exciter Seed + Phaser Feedback controls |
| P1-3 | Sync gates free Rate/Time vs Div |
| P1-4 | ADV toggle: Performance vs Engineering layout |
| P1-7 | Stereo comb filter uses L/R buffers |
| P1-8 | Release workflow runs pluginval on macOS |
| P2-1 | Wordmark uses `branding::productName` |
| Presets | Stronger Init default; musical randomize ranges |
| Tests | Factory leak regression; suite **79/79** green |

## Out of scope this pass (explicit)

- Full ≥50 factory bank redesign — **done Sprint 4**  
- Saturation oversampling (document limitation instead of fake claim)  
- Codesign / notarization secrets — **runbook ready Sprint 5; secrets still external**  
- Rewriting all engines as unique physics cores  
- Docker “deploy” (native plugin; DAW install is not containerized)

## Agile close-out (2026-09-08)

Sprints 1–5 complete. Product at **0.95.0 feature freeze**. See [`RELEASE.md`](RELEASE.md) and
[`AGILE_PRODUCT_BACKLOG.md`](AGILE_PRODUCT_BACKLOG.md). Local suite **85/85**.
