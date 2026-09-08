# Retrospective — Sprint 5

## What went well?

- Freeze documented as a product rule, not only a version bump
- Delay crossfade got a dedicated regression instead of relying on memory
- Notarization path documented without committing secrets or fake CI that would fail

## What went poorly?

CI on GitHub must still go green for the freeze commit after push; notarization remains blocked on secrets.

## Continue doing

- Keep parameter IDs and `versionHint` frozen through 1.x
- Prefer preflight script before every tag

## Next

Manual RC smoke → tag `v0.95.0` → 1.0 when signed/notarized (or explicitly ship unsigned with INSTALL notes).
