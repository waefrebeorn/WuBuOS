# ADR-001: Centralized Window Chrome (dosgui)

- **Status:** accepted
- **Date:** 2026-08-05

## Context

Apps drew their own title bars and borders with `win->x` +
`title_bar_height()`; the WM also draws chrome via
`dosgui_chrome_draw_window()`. Two border-width computations disagreed
(chrome: 2:1 rounded:plain; legacy helper: 3:2) causing 1px content and
hit-testing misalignment across every app.

## Decision

All GUI apps use `dosgui_chrome_draw_window()` for the window frame, title
bar, and buttons. Apps draw ONLY within the chrome-provided content rect.
The legacy `border_width()`/`title_bar_height()` helpers must match the
chrome module's values exactly (they now do). App input handlers validate
coordinates against the chrome content rect.

## Consequences

- **Positive:** visual consistency, one place to change chrome, no
  per-app offset drift, theme changes apply atomically.
- **Negative:** apps cannot customize their frame (by design — cohesion
  beats per-app chrome).
- **Migration:** fm, canvas, cmd, bonzi done; calc/comfy/explorer/repl/
  regedit pending (same mechanical migration).

## Verification

Regression test asserting `border_width() == chrome_border_width()` for
both themes; content-rect math tests per app.
