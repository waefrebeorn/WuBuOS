# ADR-003: Unified Design Tokens (CLI + GUI)

- **Status:** accepted
- **Date:** 2026-08-05

## Context

CLI tools (gen_text, wubu_cli, api_server) and GUI apps (calc, repl,
edr_dash, comfy, etc.) used inconsistent color values and layout metrics.
Research 066-J3 (Theme J, rank 3) identified this as the highest-leverage
UX cohesion action: a single token file shared across both layers.

## Decision

Create `wubu_tokens.h` with:
- Color tokens (WuBu green #00C853, blue, grays, semantic colors)
- Layout tokens (border width=2, title bar height=24, padding)
- ANSI escape helpers (CLI)
- `wubu_token_color()` resolver (maps logical name → hex/ANSI)

Both wubuwizard (`include/wubu_tokens.h`) and wubunos (`src/gui/wubu_tokens.h`)
maintain copies kept in sync via the token contract.

## Consequences

- `wubu_banner.h` now uses `WUBU_TOKEN_GREEN_ANSI` for the green accent
- `dosgui_window_chrome.c` button text uses `WUBU_COLOR_ACCENT_GREEN`
- `dosgui_wm_layout.c` border width unified to 2 (matching `WUBU_TOKEN_BORDER_WIDTH`)
- Adding a new color requires editing ONE token file, not hunting for hex values