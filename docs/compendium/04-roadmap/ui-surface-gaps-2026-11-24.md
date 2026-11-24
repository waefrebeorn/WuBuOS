# UI surface gaps — audit 2026-11-24

Scope: what the desktop *does* versus what it *looks like it does*. Every claim
here came from reading the binding code or running a target; the "re-verified"
section lists earlier suspicions that turned out to be fine, so the next session
does not chase them again.

Method: for each launcher, check that `on_draw` / `on_key` / `on_mouse` are all
assigned and that the renderer's geometry has a matching hit-test. A painted
control with no hit-test is a screenshot, not a control.

## Progress

| What | Where | Verification |
|---|---|---|
| Ctrl was `MOD_ALT` in the terminal and Explorer (every Ctrl shortcut dead, Alt fired instead) | `src/gui/term.c`, `src/gui/explorer_input.c` | `test_term` 12, `test_explorer` 115, both with negative controls asserting 0x04 is not Ctrl. Commit `c7f2402f` |
| Calculator's 30 painted buttons unclickable | `src/apps/calc/calc.c` | `test_calc` 10: clicks the cell labelled "7" and reads 7 back; out-of-grid click is a no-op. Commit `a05405af` |
| Registry Editor was a blank window (launcher bound nothing, draw was a `(void)` stub); values had no owner; `expanded_keys[]` was `int` holding cast pointers | `src/apps/regedit/regedit.c` | `test_regedit` 4, plus `build/regedit_frame.ppm` inspected: tree, expansion, values pane, matching selection. Commit `91a1169c` |
| Control Panel categories unclickable and `active_tab` never read (nine tabs promised, one static page delivered) | `src/apps/control/control.c` | `test_control` 31/31 including 11 interaction assertions; `build/control_frame.ppm` inspected. Commit `b1b65ace` |
| Win+Shift+Arrow swallowed by the Win+Arrow snap block (the shortcut could never fire) | `src/gui/wm_input.c` | `test_wm` with a RED-then-GREEN proof: fails on the pre-fix ordering, passes after. Commit `dda69b76` |
| **Win+E / Win+I / Win+Q did not exist** — the three most-used Windows shell shortcuts, so a user's first instincts did nothing | `src/gui/wm_input.c` | `test_wm` drives the dispatcher with spies for the launchers, plus negative controls (`Win+Shift+E`, `Win+Ctrl+I` must not fire) |
| Two desktop orbs at ~5.4 % CPU each for 3 h (every desktop start left one behind: `setsid()`) | `tools/wubu_buddy_orb_demo.c` | `flock` single-instance guard; second start exits 0 with "already running". Commit `4fcb1974` |
| The orb's affect model ran **15× too fast** and scanned sensors 9× per frame (poll called inside the per-blob loop with a hardcoded `0.5f` dt) | `tools/wubu_buddy_orb_demo.c`, `src/runtime/wubu_buddy_emotion.c` | steady-state CPU 6 % → 1 % at 30 fps (9 % → 1 % at 240); mood now eases (`energy 0.25→0.22`, `warmth 0.31→0.43`). Commit `7b13fa74` |

## Open items

Each carries the owner file, the acceptance test that would close it, and the
evidence behind the claim.

1. **Registry hive is in-memory only.** `regedit_seed_default_hive()` builds
   HKLM\Software\WuBuOS + HKCU\Wallpaper on every launch; edits do not survive a
   restart. Owner: `src/apps/regedit/regedit.c` plus a JSON load/save pair that
   uses the same pattern as `wubu_settings_io.c`. Acceptance: a test that writes
   a value, destroys the state, reloads, and reads it back. Priority: medium
   (the editor is now honest about being a viewer, but it advertises an editor).
2. **Metal leg has no window manager.** `grep -rn 'wm_init\|desktop_init'
   src/kernel/` is empty: the booted kernel brings up the supervisor, VBE and the
   mascot, but every window, taskbar and panel lives in the hosted binary.
   Priority: high for the product, large in scope — this is an architecture
   project, not a gap to patch in an afternoon.
3. **Three app sources have no test recipe:** `src/apps/comfy/comfy.c` (390 loc),
   `src/apps/bonzi/bonzi.c` (236), `src/apps/tandem/tandem.c` (186), plus the
   thin adapter `src/apps/app_explorer.c` (84). All are built into `APP_OBJS`.
   Acceptance: one behavioural test each (state round-trip + a draw that paints
   something), wired into `test_high_gui`.
4. **Dead config keys, not dead settings.** `font_size`, `font_family` and
   `cursor_size` are serialized by `wubu_settings_io.c` and consumed by nothing;
   no UI writes them either, so a user editing `settings.json` sees no effect.
   Either wire them (a global text scale exists via `vbe_set_text_scale`) or drop
   the keys. Priority: low, cleanup.
5. **Dead getters.** `wubu_settings_get_font_scale()` and
   `wubu_settings_high_contrast()` have no callers outside the module because the
   apply path reads the struct directly (`wubu_theme_set_high_contrast`). Dead
   API, not a dead setting — do not file these as user-facing lies.

## Claims re-verified (do not re-file these)

- **DOS Box is not a blank window.** `launch_dos_box()` never binds a callback
  itself; it returns `dos_window_spawn(proc, 1)`, which binds all three. A
  per-launcher grep reads as "draw only, no input" and is wrong.
- **Taskbar, systray, notification centre, quick settings and the Start menu all
  have hit-tests.** `taskbar_modern_handle()`, `wm_systray`'s "ONE owner" tray
  hit-test, `quick_settings_handle_click()` and `startmenu_handle_click()` exist
  and are called from `wm_handle_mouse()`. The absence of a `_click` function in
  `taskbar_tasks.c` / `startmenu_search.c` means those are data modules, not dead
  surfaces.
- **Explorer's context menu is dispatched.** `explorer_input.c` maps the click to
  `ex_ctx_row_at()` and switches to `explorer_open_entry()` / copy / paste / etc.
- **`canvas_standalone.c` is not an orphan copy.** It is a standalone entry
  point built by its own target, not a duplicate engine.

## Open questions

- Does anything expect `control_set_tab()` to accept 9 tabs? The header promised
  nine; the renderer only ever had eight categories. The setter now refuses
  anything outside 0..7 rather than letting the renderer index past its table. If
  a caller needed tab 8, it was already drawing nothing.
