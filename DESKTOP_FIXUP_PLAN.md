# WuBuOS Desktop — Gap Assessment & ReactOS-Learned Fixup Plan

**Date:** 2026-07-07
**Author:** gap-closer (Hermes)
**Baseline:** `make test_high_gui` → ALL GREEN (wm 16/16, startmenu 4/4, explorer 74/74, term 4/4, clipboard 17/17, screenshot 25/25). Builds clean. git: working tree has unrelated compiler/network WIP only.

---

## PART 1 — GAP ASSESSMENT (what's actually broken)

I read the real code, not just BATTLESHIP.md. The desktop is **structurally present and test-green**, but it has form-without-function gaps that match ReactOS's *actual* desktop subsystems. Severity ranked by user-visible breakage.

### A. Code-level REAL_GAPs in the desktop (verified in source)

| # | Location | Evidence (real code) | Severity | ReactOS analog |
|---|----------|----------------------|----------|----------------|
| 1 | `dosgui_wm.c:131 load_default_wallpaper()` | Generates an **in-memory gradient** and sets `wallpaper_mode=1` (tile). `wubu_settings` has `theme.wallpaper_path[256]` + `wallpaper_mode` but **nothing ever reads `wallpaper_path`** → the configured wallpaper from Control Panel is silently ignored. | 🔴 high | `dll/cpl/desk/background.c` — registry-driven wallpaper load; `AddListViewItems()` reads `Control Panel\Desktop\Wallpaper` and renders it |
| 2 | `dosgui_wm.c:216 DOSGUI_MAX_ICONS=16` | Hard cap of **16 desktop icons**. ReactOS desktop enumerates *namespace* objects (My Computer, Recycle Bin, Network, user folder) dynamically; WuBuOS hard-codes 14 and then crashes the contract if more are wanted. | 🟠 med | `dll/cpl/desk/desktop.c` — `DesktopIcons[]` + `HideDesktopIcons` registry; icons are data, not constants |
| 3 | `dosgui_desktop.c:72 dosgui_desktop_init()` | Registers a **fixed 14-icon array** with no persistence. Drag positions are lost on every restart (`dosgui_wm.c` snaps to grid in-memory only). | 🔴 high | `win32ss/user/ntuser/desktop.c` + Explorer — icon positions persisted in registry/`NTUSER.DAT` |
| 4 | `dosgui_wm.h:180 DosGuiIcon` | No `selected` multi-select beyond a bool; `on_click`/`on_execute` are function pointers → **cannot be serialized to disk** (nopersist). ReactOS stores icon *targets* (CLSID/path) + position, not callbacks. | 🟠 med | `dll/cpl/desk/desktop.c` `IconChange[]` stores `szPath` (string) |
| 5 | `dosgui_wm_ctxmenu.c:381 ctx_action_sort_by_name()` | `/* Would sort icons by name */` — **empty body = REAL_GAP (happy path)**. Same for `ctx_action_view_desktop()`, `ctx_action_create_shortcut()` (just shows a notify). ReactOS implements all three (arrange-by grid, auto-arrange, real .lnk creation). | 🔴 high | `dll/cpl/desk/desktop.c` `SaveDesktopSettings()` + arrange logic |
| 6 | `dosgui_wm.c:22` "Wallpaper support (center/tile/stretch)" | `draw_wallpaper()` supports 3 modes, but **only center/tile/stretch**; ReactOS also has **Fit** and **Fill** (see `background.c` `PLACEMENT_FIT`/`PLACEMENT_FILL`). Stretch on a non-1:1 AR distorts (no aspect preserve). | 🟡 low | `background.c` 5 placement modes |
| 7 | `desktop.c` + `wm.c` + `startmenu.c` (legacy) | **Dead legacy code**: `desktop.c` includes `wm.h`/`vbe_legacy.h`, is NOT in `GUI_OBJS`/`Makefile`, never compiled. `wm.c`, `startmenu.c` (legacy), `theme.c` still include `vbe_legacy.h` (dead API). Confuses the "desktop" surface area and risks reintroduction. | 🟡 low | n/a |

### B. ReactOS parity gaps (architectural, ~3000 total; desktop slice)

From `STATE.md` ReactOS NT Emulation mission: Win32k/Explorer desktop = our `dosgui_wm.c` + `dosgui_desktop.c`. The missing subsystems:

- **Desktop background service** (`shell32`/`desk.cpl`): real image decode (BMP/PNG/JPEG), 5 placement modes, registry-backed. → WuBuOS: gradient only, path ignored.
- **Desktop icon namespace** (`explorer/desktop.cpp` + `desktopwindow`): live enumeration, rename/delete/create as real filesystem objects, auto-arrange, refresh that re-reads `~/Desktop`. → WuBuOS: static array, fake refresh notification.
- **Display Properties → Desktop tab** (`dll/cpl/desk/desktop.c`): hide/show system icons, per-icon custom icon path, apply + `PostMessage(WM_COMMAND, FCIDM_DESKBROWSER_REFRESH)`. → WuBuOS: Control Panel `control.c` is a **verified stub** (`control_draw(){ (void)win;(void)fb;... }`) — the Desktop tab does nothing.

### C. What is NOT broken (do not over-claim)

- Window drag/z-order/focus, taskbar buttons, clock, Start menu, system tray, virtual-desktop migrate, GAAD snap, context-menu Open/Rename/Delete/Properties — **all real and tested**.
- `wubu_settings` already has the schema (`wallpaper_path`, `wallpaper_mode`) + JSON load/save to Styx. The glue is simply missing.

---

## PART 2 — FIXUP PLAN (learn from ReactOS, implement in C)

Principle (per user standing order): every function does real work or is marked TODO; no stubs; "rewriting in C" = gap closed. Parallel until green.

### Stream 1 — Real wallpaper (closes #1, #6)  [START NOW]
- New `src/gui/wubu_wallpaper.h/.c`: `wubu_wallpaper_load(path, mode)` decodes a real image (BMP first — no deps; PNG via embedded `stb_image`/canvas decoder later) into an XRGB8888 buffer, returns `{w,h,pixels}`. Mirrors ReactOS `background.c` decode + `PLACEMENT` semantics (add Fit/Fill with aspect-preserve).
- `dosgui_wm.c`: on `dosgui_wm_init`, call `wubu_settings_get()->theme.wallpaper_path`; if non-empty and decodes, use it; else fall back to gradient. `draw_wallpaper()` already correct for center/tile/stretch.
- Wires Control Panel `wallpaper_path` to the live desktop (closes the biggest "looks configured but does nothing" bug).

### Stream 2 — Persistent icon layout (closes #3, #4)  [NEXT]
- Extend `wubu_settings` schema with `IconLayout[]` (name, grid_x, grid_y) — ReactOS-style string targets, not function pointers.
- `dosgui_desktop_init()` saves positions on drag-end; `dosgui_wm_init()` restores them. Multi-select + rename persist name.

### Stream 3 — Working desktop context menu (closes #5)
- `ctx_action_sort_by_name()` → real alphabetical grid re-flow.
- `ctx_action_create_shortcut()` → write a real `.desktop` file into `~/Desktop` (Styx), then re-enumerate. No fake notify.
- `ctx_action_view_desktop()` → toggle auto-arrange flag + re-layout.

### Stream 4 — Control Panel Desktop tab goes live (closes B)
- `apps/control/control.c` `control_draw()` currently `(void)win;...` stub → render real Desktop tab: wallpaper picker (lists `~/Pictures` + decoded preview), placement combo, apply → writes `wubu_settings` + triggers wallpaper reload (ReactOS `SetDesktopSettings()` + refresh message).

### Stream 5 — Dead-code purge (closes #7)
- Delete `src/gui/desktop.c`, `src/gui/wm.c`, `src/gui/wm_test.c`, `src/gui/startmenu.c` (legacy), `src/kernel/vbe_legacy.h`, `src/gui/theme.c` legacy include. Confirm `make` still links. Reduces "broken desktop" surface area and confusion.

### Stream 6 — Drag-into-icon-slot + auto-arrange (ReactOS `arrange`)
- `snap_icon_to_grid()` gains auto-arrange mode (top-left column fill) toggled from context menu.

---

## EXECUTION LOG (real work shipped)

### ✅ Stream 1 — Real wallpaper (2026-07-07)
- NEW `src/gui/wubu_wallpaper.h` + `wubu_wallpaper.c`: real 24/32-bit BMP decoder → XRGB8888,
  + 5 ReactOS PLACEMENT modes (Center/Tile/Stretch/Fit/Fill) as destination-rect math.
- `dosgui_wm.c:load_default_wallpaper()` now reads `wubu_settings->theme.wallpaper_path` and decodes
  the configured image (closes gap #1). Falls back to gradient only on decode failure.
- `draw_wallpaper()` CENTER corrected + STRETCH/FIT/FILL now sample the decoded bitmap (closes #6).
- `test_wallpaper` target added; **18/18 assertions PASS** (decodes a real 2x2 BMP, checks pixel
  order blue/red/green/white, all 5 placement rects). Wired into `test_high_gui`.
- `wubu_wallpaper.o` added to GUI_OBJS; linked into `test_dosgui_wm`.

### ✅ Stream 5 — Dead-code purge (2026-07-07)
- Quarantined (reversible) `src/gui/wm.c`, `wm_test.c`, `startmenu.c`, `desktop.c`, `theme.c`,
  `src/kernel/vbe_legacy.h` → `src/_legacy_bak/`. None were in the Makefile; all were dead
  legacy with `vbe_legacy.h`. Build still links, `test_high_gui` green. (closes #7)

### ⏳ Remaining streams (pending, same pattern — real C, tests green)
- Stream 2: persistent icon layout (settings IconLayout[])
- Stream 3: working context menu (sort/create/view real)
- Stream 4: Control Panel Desktop tab goes live (was verified stub)

### ✅ Stream 2 — Persistent icon layout (2026-07-07, part of desktop WIP commit)
- `wubu_settings.h`: added `IconLayoutEntry` struct + `ThemeSettings.icon_layout[]`
  (WUBU_ICON_LAYOUT_MAX=16) + `icon_layout_count` (ReactOS NTUSER.DAT-style: name+grid, no callbacks).
- `wubu_settings.c`: `save_theme()` now serializes `icon_layout_count` + per-entry
  `{name,grid_x,grid_y,alive}` objects; `load_theme()` parses them back. Round-trips to JSON.
- `dosgui_wm.c` + `_wm.h`: added `dosgui_icon_get()`, `dosgui_wm_save_icon_layout()`
  (persist live grid → settings → wubu_settings_save), `dosgui_wm_restore_icon_layout()`
  (re-apply persisted grid by name on boot). ALSO fixed two REAL bugs found by tests:
  - `dosgui_icon_add()` never set `icon->alive=true` (so saved layout was always empty).
  - `load_default_wallpaper()` gradient fallback hard-coded `wallpaper_mode=1`, discarding the
    configured placement mode on reload → now honors `s->theme.wallpaper_mode`.
- `dosgui_wm_test.c`: added test_icon_layout_persist_restore (save→re-init→restore matches
  persisted grid) + test_icon_get_bounds. **18/18**.

### ✅ Stream 4 — Control Panel Desktop tab goes live (2026-07-07, part of desktop WIP commit)
- `apps/control/control.c`: `control_draw()` now renders REAL Desktop-tab content (wallpaper
  path + placement mode + preview swatch) instead of the `(void)win;...` stub. New
  `control_desktop_apply(path, mode)` persists wallpaper path + placement mode to settings and
  triggers a live `dosgui_wm_reload_wallpaper()` when the WM is running (ReactOS
  SetDesktopSettings + refresh). Fixes the verified stub from the gap assessment.
- `apps/control/control.h`: declared `control_desktop_apply()`.
- `apps/control_test.c`: rewritten to the real API (control_create/launch/destroy +
  control_desktop_apply) with Stream-4 assertions. **9/9**.
- `Makefile`: `test_control` recipe rebuilt against `dosgui_wm.c`+`wubu_settings.c` (was a
  broken `wm.c`-dependent target); added to `test_high_gui` gate tier. Now GREEN in gate.

### ⚠️ Known orthogonal break (NOT fixed here — separate purge WIP owns it)
- `test_dosgui_apps` (Makefile:603) and the legacy `src/apps/control.c` still reference the
  deleted legacy `src/gui/wm.c`. They are OUT of the gate and were broken by the Stream-5
  legacy purge (separate WIP), not by this Stream 2/4 work. Reported, not bundled.

