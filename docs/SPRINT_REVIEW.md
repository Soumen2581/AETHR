# Sprint Review — Sprint 5

**Sprint:** AETHR Release Hardening  
**Date:** 2026-09-08  
**Goal:** Feature-freeze at 0.95, ship an RC procedure, document notarization without inventing secrets.

## Completed

| Story | Result |
|-------|--------|
| S5-1 Feature freeze | `CMakeLists.txt` → **VERSION 0.95.0** |
| S5-2 RC checklist | `docs/RELEASE.md` |
| S5-3 Notarization runbook | Secrets table + steps in `docs/CI.md` |
| S5-4 Delay zipper test | `Tests/FxRackTests.cpp` |
| S5-5 Preflight | `Tools/release-preflight.sh` |

## Test results

Local: **85/85** via `ctest --preset ci`.

## Next

Tag `v0.95.0` when `master` CI is green and manual smoke items in `RELEASE.md` are checked. Then packaging / notarization when Apple secrets exist → **1.0**.
