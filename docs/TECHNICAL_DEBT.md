# AETHR — Technical Debt Register

Last updated: 2026-09-08

| ID | Description | Impact | Risk | Proposed solution | Priority | Status |
|----|-------------|--------|------|-------------------|----------|--------|
| TD-1 | `PluginEditor::resized` lays out via magic child indices | Fragile UI edits | Medium | Named members / panel-owned `resized` | P2 | Open |
| TD-2 | Full editor `repaint()` at 36 Hz | UI CPU | Medium | Dirty-region / component timers | P2 | Open |
| TD-3 | Delay time jumps without crossfade | Automation clicks | High | Crossfade reads in `FxRack` | P1 | Closed (Sprint 1) |
| TD-4 | SVF filter coeffs recomputed every sample | CPU | Medium | Block-rate coeff update | P2 | Open |
| TD-5 | Saturation without oversampling | Aliasing on hard modes | High | Oversample nonlinear path or soft-default | P2 | Accepted residual |
| TD-6 | Chorus + phaser share `lfoPhase` | Coupled motion | Low | Separate phases | P2 | Open |
| TD-7 | Preset index only in editor | Wrong name after DAW reload | Medium | Persist in state XML | P1 | Closed (Sprint 1) |
| TD-8 | Factory bank thin (13) | Weak first impression | Medium | Expand curated bank Sprint 4 | P2 | Open |
| TD-9 | Visualizers stylized not metered | Misleading if called “scopes” | Low | Honest labels or real telemetry | P2 | Open |
| TD-10 | Unsigned / un-notarized macOS zips | Gatekeeper friction | Medium | Secrets + notarize in release | P2 | Open |
| TD-11 | `RISKS.md` R10 stale (Windows never built) | Trust | Medium | Correct in Sprint 1 | P1 | Closed |
| TD-12 | `getTailLengthSeconds` ignores synced delay resolution | Truncated offline bounce | High | Use `resolvedDelaySeconds` | P1 | Closed (Sprint 1) |
| TD-13 | No MIDI CC1 / AT | Live expressiveness | Medium | Implement or document | P1 | Closed (Sprint 1) |
| TD-14 | Modulation matrix read-only | Looks interactive | Low | Relabel or wire Sprint 2 | P2 | Open |
| TD-15 | Two DSP cores for 13 “engines” | Marketing oversell | Medium | Docs honesty (done); deepen Sprint 3 | P2 | Mitigated in docs |

## Closed this cycle

| ID | Resolution |
|----|------------|
| Voice pool on stack (Windows CI segfault) | Heap `unique_ptr` pool |
| Dead SAVE button | User `.aethr` FileChooser save |
| Factory preset leak | `init()` then apply |
| Tail `min(release,decay)` | Conservative `max` (+ FX estimate) |
| Stereo comb L-only | Separate L/R write indices |
| Manual CI canceling push builds | Separate concurrency group |
