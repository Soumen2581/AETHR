# AETHR — Agile Product Backlog

Living backlog for https://github.com/Soumen2581/AETHR  
Last updated: 2026-09-08  
Milestone target: **AETHR 0.9 — Development Complete** → 0.95 Beta → 1.0 Release

Status legend: `TODO` · `IN PROGRESS` · `BLOCKED` · `DONE`

---

## Product vision

> A premium physical-resonance instrument that combines physically inspired synthesis,
> experimental sound design, and an exceptionally polished tactile interface.

---

## Current sprint

| Field | Value |
|-------|--------|
| **Sprint** | 2 |
| **Name** | AETHR Visual System |
| **Goal** | Premium visual honesty and efficiency without a UI rewrite — scoped repaints, truthful Resonance Core labels, category preset browser, basic accessibility |
| **Dates** | 2026-09-08 → 2026-09-15 |
| **Branch policy** | Prefer `fix/*` / `feature/*`; small focused commits on `master` only when risk is low and CI is green |

### Sprint 2 stories

| ID | Story | Priority | Status |
|----|-------|----------|--------|
| S2-1 | Scoped UI repaints (stop full-editor every tick) | P2 | DONE |
| S2-2 | Honest Resonance Core / matrix labeling | P2 | DONE |
| S2-3 | Preset browser grouped by category | P2 | DONE |
| S2-4 | Accessibility titles + Esc dismiss browser | P2 | DONE |
| S2-5 | Engine strip tooltips (name + family) | P2 | DONE |

### Sprint 1 stories (complete)

| ID | Story | Priority | Status |
|----|-------|----------|--------|
| S1-1 | Synced delay reflected in `getTailLengthSeconds` | P1 | DONE |
| S1-2 | Click-free delay time changes (crossfade) | P1 | DONE |
| S1-3 | Preset identity round-trips with host state | P1 | DONE |
| S1-4 | MIDI CC1 contract (implement or document) | P1 | DONE |
| S1-5 | Risk register truth (R10 / residuals) | P1 | DONE |
| S1-6 | Confirm polish-pass CI green on master | P0 | DONE |

---

## Epics

### EPIC A — Foundation

| ID | Item | P | Status | Notes |
|----|------|---|--------|-------|
| A-1 | Architecture / ownership cleanup (editor child-index layout) | P2 | TODO | `PluginEditor.cpp` magic indices |
| A-2 | Build reliability (Werror CI, MSVC) | P1 | DONE | Windows float-equal + voice heap |
| A-3 | Documentation accuracy | P1 | DONE | TESTING/README/BUILD/RISKS partial |
| A-4 | Technical debt register | P2 | DONE | `docs/TECHNICAL_DEBT.md` |
| A-5 | Stale RISKS R10 (Windows never built) | P1 | DONE | Sprint 1 |

### EPIC B — DSP Quality

| ID | Item | P | Status | Notes |
|----|------|---|--------|-------|
| B-1 | Tail length uses max not min | P0 | DONE | Polish |
| B-2 | Synced delay in tail length | P1 | DONE | Sprint 1 |
| B-3 | Delay time crossfade | P1 | DONE | Sprint 1 |
| B-4 | Filter coeffs per-block not per-sample | P2 | TODO | CPU |
| B-5 | Saturation oversampling | P2 | TODO | Accepted residual R5 |
| B-6 | Separate chorus/phaser LFO phase | P2 | TODO | |
| B-7 | Stereo comb filter | P1 | DONE | Polish |
| B-8 | Arp MidiBuffer capacity | P0 | DONE | 64 KiB |

### EPIC C — Engine Quality

| ID | Item | P | Status | Notes |
|----|------|---|--------|-------|
| C-1 | Mid-note engine-family retrigger | P0 | DONE | `Voice.cpp` |
| C-2 | Held-note switch regression test (no reset) | P1 | TODO | Extend EngineTests |
| C-3 | Per-engine character audit (13 engines) | P2 | TODO | Sprint 3 |
| C-4 | Physics depth honesty in docs | P1 | DONE | README + ENGINE_REFERENCE |

### EPIC D — UI/UX

| ID | Item | P | Status | Notes |
|----|------|---|--------|-------|
| D-1 | Dead SAVE → real user presets | P0 | DONE | |
| D-2 | Tooltips on non-knobs | P1 | DONE | |
| D-3 | Sync enable gating | P1 | DONE | |
| D-4 | ADV Performance / Engineering | P1 | DONE | |
| D-5 | Phaser feedback + seed controls | P1 | DONE | |
| D-6 | Scoped 36 Hz repaint | P2 | DONE | Sprint 2 |
| D-7 | Resonance Core telemetry viz | P2 | DONE | Honest STATUS ART labels |
| D-8 | Accessibility handlers | P2 | DONE | Titles + Esc browser |
| D-14 | Category preset browser | P2 | DONE | Sprint 2 |

### EPIC E — Presets

| ID | Item | P | Status | Notes |
|----|------|---|--------|-------|
| E-1 | Factory init-then-apply | P0 | DONE | + regression test |
| E-2 | Stronger Init default | P1 | DONE | |
| E-3 | Musical randomize ranges | P1 | DONE | |
| E-4 | Preset name/index in host state | P1 | DONE | Sprint 1 |
| E-5 | Category browser / ≥50 bank | P2 | TODO | Sprint 4 |
| E-6 | Favorites | P3 | TODO | |

### EPIC F — Performance

| ID | Item | P | Status | Notes |
|----|------|---|--------|-------|
| F-1 | Voice pool on heap (Windows stack) | P0 | DONE | |
| F-2 | BodyResonator coeff rebuild cost | P2 | TODO | Profile first |
| F-3 | UI full repaint cost | P2 | TODO | |

### EPIC G — MIDI / Performance

| ID | Item | P | Status | Notes |
|----|------|---|--------|-------|
| G-1 | Notes / velocity / sustain / pitch bend | P1 | DONE | Basic |
| G-2 | CC1 mod wheel | P1 | DONE | Brightness; INSTALL docs |
| G-3 | Aftertouch / program change | P3 | TODO | |
| G-4 | Arp / sequencer verification | P1 | PARTIAL | Tests exist; UX polish later |

### EPIC H — Testing

| ID | Item | P | Status | Notes |
|----|------|---|--------|-------|
| H-1 | Catch2 suite ~79 tests | P1 | DONE | CI |
| H-2 | Factory leak test | P1 | DONE | |
| H-3 | Held engine-switch test | P1 | TODO | |
| H-4 | Delay automation zipper test | P2 | TODO | With B-3 |
| H-5 | pluginval in CI + release | P1 | DONE | |

### EPIC I — CI/CD

| ID | Item | P | Status | Notes |
|----|------|---|--------|-------|
| I-1 | macOS + Windows matrix | P0 | DONE | |
| I-2 | Manual CI vs push concurrency | P1 | DONE | |
| I-3 | Release version gate + artefacts | P1 | DONE | |
| I-4 | Codesign / notarization | P2 | TODO | Needs secrets |

### EPIC J — Release

| ID | Item | P | Status | Notes |
|----|------|---|--------|-------|
| J-1 | INSTALL / CI docs | P1 | DONE | |
| J-2 | Final polish audit | P1 | DONE | |
| J-3 | Feature freeze at 0.95 | — | TODO | Milestone |
| J-4 | Release candidate checklist | P2 | TODO | |

---

## User stories (active)

### S1-1 — Synced tail length
**AS A** producer bouncing offline  
**I WANT** AETHR’s reported tail to match synced delay settings  
**SO THAT** my render is not truncated.

**Acceptance**
- [ ] `getTailLengthSeconds` uses the same resolved delay seconds as the audio path
- [ ] Existing tests still pass
- [ ] No parameter ID changes

### S1-2 — Click-free delay
**AS A** sound designer automating delay time  
**I WANT** delay time changes without zipper noise  
**SO THAT** automation is usable in a mix.

**Acceptance**
- [ ] Time changes crossfade (or equivalent) in `FxRack`
- [ ] No realtime heap in process path
- [ ] Suite green

### S1-3 — Preset identity in state
**AS A** user recalling a DAW project  
**I WANT** the preset name to match what I saved  
**SO THAT** the session looks coherent.

**Acceptance**
- [ ] Preset index or name survives `get/setStateInformation`
- [ ] Params still round-trip
- [ ] Suite green

### S1-4 — MIDI CC1 contract
**AS A** keyboard player  
**I WANT** clear mod-wheel behaviour  
**SO THAT** I am not surprised in a live set.

**Acceptance**
- [ ] Either CC1 maps to a documented destination **or** docs state unsupported
- [ ] No fake UI for unsupported CCs

### S1-5 — Risk register truth
**AS A** maintainer  
**I WANT** RISKS.md to match CI reality  
**SO THAT** we do not ship false confidence.

**Acceptance**
- [ ] R10 corrected
- [ ] Aliasing + unsigned macOS listed as accepted residuals

---

## Prioritization rule

Never work P3 while P0 exists. Prefer **RISK × IMPACT**. Protect parameter IDs and realtime safety.

## Next sprints (planned)

| Sprint | Goal |
|--------|------|
| 2 | AETHR Visual System |
| 3 | AETHR Sound Quality |
| 4 | AETHR Workflow |
| 5 | AETHR Release Hardening (feature freeze) |
