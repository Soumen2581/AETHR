# Retrospective — Sprint 2

## What went well?

- Kept scope to honesty + efficiency; no UI rewrite
- Category browser is a small high-value workflow win
- Scoped repaint is measurable against TD-2

## What went poorly?

- `setTooltip` requires `SettableTooltipClient` — caught at compile time

## What should change?

- Prefer inheriting `SettableTooltipClient` when adding tooltips to custom components

## Continue doing

- Label decorative visuals as status art rather than implying analyzers

## Next

Sprint 3 sound quality; leave layout magic indices (TD-1) unless it blocks work.
