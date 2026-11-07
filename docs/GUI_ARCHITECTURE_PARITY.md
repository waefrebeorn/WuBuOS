# WuBuOS GUI Architecture — Mint / Win11 Parity Reorg & Relabel Map

Date: 2026-11-07. Repo: `/home/wubu/wubuos`, branch `main`, HEAD `f60b08d`.

Directive: *"reorganization and relabeling to fix the architecture GUI — it needs to be
on parity with Linux Mint and Windows 11."* This doc is the grounded map (from the real
include graph + the two committed source studies), the architecture finding, and the
staged execution order for the rename + split. It does NOT invent a new stack.

## The architecture finding (read this first)

The parity architecture **already exists in the tree as an orphaned line**:

| Line | Files | Who calls it | State |
|------|-------|--------------|-------|
| **LIVE host** | `dosgui_wm.c` + `dosgui_wm_{window,input,layout,window_state}.c` + `dosgui_wm_render.c` + `dosgui_window_chrome.c` | every app (`notepad`, `calc`, `control`, `taskmgr`, `explorer`, `edr_dash`, …), `hosted.c`, the syscall bridge | Immediate-mode single-framebuffer. **This is what a user sees.** |
| **SHADOW** | `wubu_compositor.c` (+`wubu_compositor_standalone.c`) | only its own test | Retained-mode compositor (win11 DWM/MIL model) |
| **SHADOW** | `wubu_wm.c`, `wubu_wm_{render,desktop,input}.c` | only `wubu_wm_test.c` | WM-over-compositor split (Cinnamon Muffin model) |

So "reorganize + relabel to Mint/Win11 parity" is **reconciling these two lines** — adopt
the clean retained-compositor/WM split into the live render path — not building a new
stack. The studies (q.v.) already specify the target: `cinnamon_study.md` (§/n mapping,
P1 compositor / P2 WM / P3 bus / P4 panel / P5 theme+xlet) and
`windows11_wm_study.md` (DWM off-screen surfaces + retained tree + Snap; `/n/compositor/`,
`/n/wm/`).

## Module → parity-layer map (from the real include graph)

### Compositor / WM — the two lines to unify
- **LIVE host (rename later, keep working first):** `dosgui_wm.c`
  `dosgui_wm_window.c` `dosgui_wm_input.c` `dosgui_wm_layout.c`
  `dosgui_wm_window_state.c` `dosgui_wm_render.c` `dosgui_window_chrome.c`
  `wm.h` `dosgui_wm_internal.h` `dosgui_wm.h`
- **SHADOW retained line (the parity target):** `wubu_compositor.c`
  `wubu_compositor_standalone.c` + `wubu_wm.c` `wubu_wm_render.c` `wubu_wm_desktop.c`
  `wubu_wm_input.c` + `gui_dbuf.c` (double-buffer)

### Shell (the Cinnamon/LayoutManager analog)
- **Start menu:** `dosgui_startmenu.c` + `_db` `_search` `_tree` `_power`
- **Taskbar / panel:** `dosgui_wm_taskbar.c` `dosgui_wm_systray.c`
  `dosgui_wm_tray_world.c` `dosgui_wm_clock.c` + `dosgui_daemon_panel.c`
  `dosgui_service_mgr.c`
- **Desktop:** `dosgui_wm_desktop.c` `dosgui_wm_icons.c` `dosgui_wm_icon_glyphs.c`
  `dosgui_wm_ctxmenu.c` `dosgui_wm_ctxmenu_engine.c` `dosgui_wm_holyc_term.c`
  `dosgui_desktop.c`
- **Context menus:** `dosgui_wm_ctxmenu.c` `dosgui_wm_ctxmenu_engine.c`

### Shell apps (Win11/Cinnamon app-parity)
- **File manager:** `dosgui_explorer*.c` (11 files)
- **Control panel:** `dosgui_controlpanel.c` + `dosgui_cp_{display,hardware,network,sound,theme}.c`
- **Terminal:** `dosgui_term*.c` (5 files)
- **Window chrome / wallpaper / notify / trash / clipboard / welcome:**
  `dosgui_window_chrome.c` `wubu_wallpaper.c` `wubu_notify.c` `wubu_trash.c`
  `wubu_clipboard*.c` `wubu_welcome.c`
- **DOS compat window:** `dosgui_dos_window.c`

### Theme & a11y & ui-automation
- `wubu_theme.c` (+ `wubu_theme.h`) · `wubu_a11y.c` · `wubu_ui.c`

### Services / settings / mime
- `wubu_settings*.c` · `wubu_mime*.c` · `wubu_json.c` · `wubu_ladder.c`

### Apps engines (keep under `apps/`)
- `dosgui_bpm*.c` `dosgui_era_apps.c` `wubu_bonzi.c` `wubu_proton*.c`
  `wubu_pkgmgr*.c` `wubu_gamelib*.c` `wubu_sound.c` `wubu_waveosc.c`
  `wubu_screenshot*.c` `wubu_deploy*.c` `wubu_session*.c`

Full per-file layer tagging was generated from the tree at:
`~/.hermes/cache/scratch/gui_include_graph.json` (regenerate, don't trust stale).

## Proposed relabel (prefix scheme, from both studies' vocabulary)

| Layer | New prefix | Live host | Shadow to merge |
|-------|-----------|-----------|-----------------|
| Compositor (owns the frame) | `wubu_comp_*` | `dosgui_wm_render.c` (blit loop → frame owner) | `wubu_compositor.c` |
| Window manager (stack/focus/move) | `wubu_wm_*` | `dosgui_wm.c` `dosgui_wm_window.c` `dosgui_wm_input.c` | `wubu_wm*.c` |
| Shell / chrome | `wubu_shell_*` | `dosgui_window_chrome.c` `dosgui_wm_taskbar.c` `dosgui_wm_desktop.c` … | — |
| Theme | `wubu_theme_*` (already `wubu_theme_`) | — | — |

> **Decision gate (awaiting the architect):** prefix scheme breadth (`wubu_comp_*`/`wubu_wm_*`,
> or keep `dosgui_` but re-purpose). This is the one input that changes which files move.
> Recommended: **heavy-first Option A** — adopt the retained split into the live render path
> (frame owned by the compositor, WM stacked over it, per DWM/MIL + Muffin), then relabel.

## /n control plane (from both studies — no file moves needed to land this)

- `/n/compositor/` — `w/{id}/surface`, `tree`, `frame`, `thumb/{w}/{scale}`, `snap/{layouts,assist,groups}`
- `/n/wm/` — `window/{id}/{geometry,visibility,z}`, `desktops/<i>/{create,close,rename,wallpaper,switch}`, `taskbar/{align,pins}`
- `/n/ui/` — `panel/<id>/content{left,center,right}`, `layout/`, `theme/`, `xlet/<uuid>/`, `settings/`

## Staged execution order (each verifiable, each committed peer-matched)

1. **[DONE] Survey + map** — this doc + `gui_include_graph.json`. No files moved.
2. **Adopt the retained split into the live path** (the structural fix, not a rename):
   make `dosgui_wm_render()` the sole compositor frame-owner; move window geometry into a
   retained set (Win11 #2/#8) — re-verify `make test_high_gui` + `desktop_shot` green.
3. **Relabel** — `git mv` + symbol rename per the approved prefix scheme, scoped/idempotent
   (the rename discipline in `wubuos-architecture` §P3: bound patterns, no tree-wide bare sed).
4. **/n control plane** — bind the real surfaces (Cinnamon/Win11 rows above) as tested C11
   publishers, mirroring `wubu_ns_win32` / the `nt_registry_styx_export` swap-callback pattern.
5. **Triple-DA** — fresh `make clean && make all && make test` + filesystem sweep + re-scan
   of the "frame owned by one compositor" invariant before reporting done.

## Standing rules honored

- Survey before dispatching / mass-rename (done above; ~60 GUI files mapped from the real graph).
- No irreversible rename fired blind at a green tree on an unconfirmed prefix scheme.
- Deliver working artifacts + `echo $?`; every phased step re-verified fresh.
- Commit + push peer-matched; leave `wubucontainer` submodule drift untouched.
