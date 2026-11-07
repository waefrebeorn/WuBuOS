# Prestige Prompt — wubuos GUI / desktop (next session)

Date: 2026-11-07. Repo: `/home/wubu/wubuos`, branch `main`, HEAD `0da4251`
(peer-matched with `origin/main`, clean tree). Focus this session: **the desktop
and usability — rated 1/5.** The window-manager and object-model machinery are
tall; what the human sees and clicks is thin.

## What already exists (foundation — do NOT rebuild)

- **The WM/desktop render stack** — `src/gui/dosgui_*`: `dosgui_wm.c`,
  `dosgui_wm_desktop.c`, `dosgui_wm_render.c`, `dosgui_wm_window.c`,
  `dosgui_wm_input.c`, `dosgui_wm_taskbar.c`, `dosgui_wm_icons.c`,
  `dosgui_wm_icon_glyphs.c`, `dosgui_wm_systray.c`, `dosgui_wm_startmenu.c`,
  `dosgui_wm_ctxmenu*.c`, `dosgui_wm_clock.c`, `dosgui_window_chrome.c`,
  `dosgui_wm_holyc_term.c`. Plus apps: `dosgui_explorer*.c`,
  `dosgui_controlpanel.c`, `dosgui_daemon_panel.c`, `dosgui_era_apps.c`,
  `dosgui_service_mgr.c`, `src/apps/dosgui_apps.c`.
- **The authority object-model backbone** — `src/runtime/win32k/wubu_win32k.{c,h}`
  (station / desktop / class / window / GDI surfaces, all cap-backed revocable
  tokens) + `wubu_ns_win32` (`/n/win32/{registry,stations,windows,gdi}` control
  plane). Gates: `test_win32k` 49/49, `test_ns_win32` 16/16, `test_cap` 34/34.
  See the `wubuos-win32k-backbone` skill.
- **The hosted GUI gate** — `make test_high_gui` (mk/tests.mk line 81) + the
  `gui_shot` and `desktop_shot` targets (line 590-654): these render the REAL
  desktop through VBE + dosgui WM and drive desktop actions into PPM frames.

## The honest gap (why usability is 1/5)

1. **Many GUI tests are STUBS, not behavior.** `dosgui_wm_test_stub.c`,
   `dosgui_apps_test_stubs.c`, `dosgui_startmenu_test_stub.c`,
   `dosgui_wm_systray_test_stub.c`, `dosgui_dos_window_test_stub.c`,
   `dosgui_era_apps_test_stub.c`, `dosgui_explorer_test_stub.c` — wired into the
   high-tier gate but asserting nothing a user cares about. The desktop renders
   and the object model is cap-solid, but the click-through — launcher opens a
   window, explorer browses `/n/`, systray holds clocks, window move/resize/minimize
   actually work — is unverified.
2. **The F greeter app is still a future surface** (onboarding stream + sentinel
   done in `src/runtime/wubu_onboarding.*`; the visible first-boot screen is not).

## Next concrete step (do this first)

**Turn the start-menu usability stub into a real behavior gate.** Highest-leverage
on the 1/5 axis: the start menu is the human's entry point. Replace
`dosgui_startmenu_test_stub.c` with a real `dosgui_startmenu_test.c` that:
(1) registers a real dosgui app via the app registry, (2) simulates a start-menu
click on it, (3) asserts the app's window is created, mapped, and rendered through
`dosgui_wm_render()` — the "user clicked, something opened" moment. Wire it into
`test_high_gui` so every build proves it.

Then sweep the other stubs in the same pattern, in order:
- `dosgui_wm_systray_test_stub.c` -> real: clock mounts a systray icon, taskbar
  reflects windows.
- `dosgui_explorer_test_stub.c` -> real: explorer lists `/n/` entries.
- `dosgui_dos_window_test_stub.c` / `dosgui_wm_test_stub.c` -> real window
  move/resize/minimize assertions through `desktop_shot`-style PPM capture.
- `dosgui_apps_test_stubs.c` / `dosgui_era_apps_test_stub.c` -> real app engines.

## Also on the slate (secondary, pick when a stub sweep stalls)

- Bind `wubu_win32k_snapshot()` to a REAL Styx9P node under `/n/win32/*` (the
  P1b swap-callback pattern; currently snapshot-only, not an exported node).
- Build the F greeter app on the dosgui_* stack behind `wubu_onboarding`.
- Rules: dosgui recipes compile per-.c + separate run-line appended at EOF of
  `mk/tests.mk` (never the patch tool on recipe lines — splice only; verify with
  `cat -A` that commands start with real `^I`). Every new CM needs `-pthread`.

## Standing discipline

- Ship verified artifacts: real behavior gates, `echo $?` + the gate, not
  self-reported. A stub that "passes" is not work.
- Commit + push peer-matched (`local==origin==remote`); leave the unrelated
  `wubucontainer` submodule drift untouched.
- After the GUI usability tier is gated, re-measure (the desktop is the artifact,
  run `desktop_shot`) before planning more — the next order of magnitude is a
  human-felt result, not another module.
