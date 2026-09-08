# AETHR — Release guide

**Milestone:** 0.95 Beta (feature freeze) → 1.0 Release  
**Current CMake version:** must match `project(AETHR VERSION …)` in `CMakeLists.txt`

This document is the release operating procedure. Feature work that is not on the
accepted-residuals list is frozen for the 0.95 line unless it is a P0 bugfix.

---

## Feature freeze (0.95)

### In scope until 1.0

- P0 / P1 bug fixes that break sound, state, CI, or install
- Documentation honesty
- Codesign / notarization when Apple/Windows secrets are available
- Test coverage for existing contracts

### Explicitly deferred past 0.95

| Item | Why |
|------|-----|
| Favorites | P3 workflow polish |
| Aftertouch | P3 MIDI |
| Saturation oversampling | Accepted residual R5 |
| Editor magic-index layout rewrite | TD-1; risk without product gain |
| BodyResonator coeff profiling | Profile-gated |
| Unique physics core per engine | Docs already honest about two cores |

---

## Release candidate checklist

Run this before tagging `vX.Y.Z` (tag **must** equal CMake `VERSION`).

### Local

- [ ] `./Tools/ci-static-check.sh` passes
- [ ] `cmake --preset ci && cmake --build --preset ci --parallel`
- [ ] `ctest --preset ci --output-on-failure` — all green
- [ ] `./Tools/ci-verify-artefacts.sh build/ci/Aethr_artefacts/RelWithDebInfo macOS` (or Windows)
- [ ] Smoke: load Standalone, play notes, step factory presets, SAVE a `.aethr`, INIT
- [ ] Smoke: toggle ADV, open Library, Esc dismisses, MIDI PC changes preset (if keyboard available)
- [ ] `docs/RISKS.md` residuals still accurate (R5 aliasing, unsigned macOS)
- [ ] README / INSTALL version claims match this tag

### CI / GitHub

- [ ] Latest `master` CI green on **macOS and Windows**
- [ ] No open P0 issues against the freeze commit
- [ ] `CMakeLists.txt` `VERSION` already bumped to the tag you will push

### Tag and publish

```bash
# Example for 0.95.0 — VERSION in CMakeLists.txt must already be 0.95.0
git tag v0.95.0
git push origin v0.95.0
```

- [ ] Release workflow builds both platforms, runs tests, pluginval (macOS), uploads zips
- [ ] GitHub Release assets present: `AETHR-v…-macOS.zip`, `AETHR-v…-Windows.zip`
- [ ] Install from Release zip on a clean machine / second user account
- [ ] Gatekeeper: if unsigned, INSTALL.md quarantine notes still correct

### Optional (when secrets exist)

- [ ] Developer ID sign + notarize macOS zip (see [CI.md § Notarization](CI.md))
- [ ] Windows Authenticode (optional; document if skipped)

---

## Version policy

| Line | Meaning |
|------|---------|
| `0.95.x` | Feature-frozen beta; bugfix + packaging only |
| `1.0.0` | First public release after RC checklist + (ideally) notarized macOS |
| `1.x` | Parameter `versionHint` stays `1`; IDs never rename |

Never retag. Never force-push release tags. Bump patch for hotfix (`0.95.1`), minor only when freeze lifts.

---

## Accepted shipping residuals

Documented so they are not mistaken for accidental omissions:

1. **Hard/wavefold saturation can alias** (no oversampling yet) — prefer Soft default  
2. **macOS Release zips are ad-hoc / unsigned** until notarization secrets are configured  
3. **Aftertouch unmapped**; mod wheel (CC1) and program change are mapped  
4. **pluginval on Windows CI** not yet wired (macOS path is the gate)
