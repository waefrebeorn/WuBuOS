# ADR-002: Opaque Structs at Every Module Seam

- **Status:** accepted
- **Date:** 2026-08-05

## Context

WuBuOS exposes raw struct layouts in headers (`wubu_theme.h`,
`dosgui_wm_internal.h`), creating massive recompile cascades when a
field is added. 67% of header changes trigger full kernel recompiles.

## Decision

1. Public headers expose only opaque handles (`DosGuiWindow *`,
   `wubu_theme_t *`) with accessor functions
2. Struct definitions move to `<module>_internal.h` (same pattern as
   `dosgui_wm_internal.h`)
3. `dosgui_window_chrome.h` is the single public header for window
   chrome — apps include only this, not `dosgui_wm_internal.h`

## Consequences

- Adding a window field only recompiles the WM, not every app
- ABI stability for plugin-style app loading
- Compile times drop (fewer headers in every TU's include closure)

## Related

- `docs/adr/ADR-001-centralized-window-chrome.md`
- research/066-theme-j-planning-ux.md (Theme J2: opaque struct seams)
- src/apps/calc/calc.c — uses only `dosgui_window_chrome.h`
- src/apps/comfy/comfy.c — uses only `dosgui_window_chrome.h`
- src/apps/cmd/cmd.c — uses only `dosgui_window_chrome.h`
- src/apps/bonzi/bonzi.c — partially migrated
