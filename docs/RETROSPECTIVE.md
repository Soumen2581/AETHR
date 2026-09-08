# Retrospective — Sprint 1

## What went well?

- Prior polish pass cleared true P0s, so Sprint 1 could focus on host contracts
- Small increments (tail → delay → state → MIDI → docs) stayed reviewable
- Regression tests for factory leak and preset index caught state issues early

## What went poorly?

- Catch2 discovery can lag a single rebuild; reconfigure needed after new `TEST_CASE`
- Full-editor 36 Hz repaint still untouched (UI CPU debt remains)

## What slowed development?

- Large prior prompt surface area; Agile docs first was the right gate

## What caused rework?

- Tail length fixed once in polish with free delay times; Sprint 1 had to align sync resolution

## What should change?

- Always `cmake --preset ci` after adding tests before claiming suite counts
- Prefer `fix/*` branches for multi-file DSP when CI is already busy on master

## Stop doing

- Shipping docs that claim unverified platform status (R10 lesson)

## Continue doing

- Init-then-apply for presets; atomic MIDI → settings snapshots (not APVTS writes from CC)

## Automate

- Optional: CI step that greps RISKS for “never been compiled”
