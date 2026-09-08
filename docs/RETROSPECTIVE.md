# Retrospective — Sprint 4

## What went well?

- Kept the first 13 factory indices stable so existing leak/state tests stayed valid
- Scrollable library was required once the bank grew — shipped with the expansion
- Host program API + MIDI PC share one apply path (Init-then-apply)

## What went poorly?

Favorites deferred; aftertouch still open.

## Continue doing

- Prefer message-thread preset apply via `AsyncUpdater` for MIDI-driven changes

## Next

Sprint 5 release hardening — freeze features, ship checklist, notarization when secrets exist.
