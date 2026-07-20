# WuBuOS — Current State (verified 2026-07-19)

```
╔════════════════════════════════════════════════════════════════════════╗
║     🌱  W U B U O S                                                       ║
║     ZealOS kernel · Win98 shell · Styx/9P namespace · Arch containers    ║
║     468 C files · 214 H files · ~105K LOC · 91 test targets             ║
║     Repo: /home/wubu/wubuos   (verified from git + Makefile)             ║
╚══════════════════════════════════════════════════════════════════════════╝
```

## Repo facts (verified 2026-07-19)
- **468 `.c` / 214 `.h` / ~105,459 LOC** (tracked `src/`).
- **91 `make test_*` targets.**
- `make runtime` builds clean under `-O2`.
- **Historical note:** `BATTLESHIP.md` v22 (and this file's older sections) described
  "~40 code gaps + ~370 parity, 64 targets, ~15K LOC". Those are stale — the
  monolith-dissolution campaign grew the tree to 468 `.c` / ~105K LOC / 91 targets.
  See `docs/MONOLITH_DISSOLUTION.md`.

## Battleship Status (carried from v22, numbers updated)
- **~40 verifiable code-level REAL_GAPs** in `src/` (10 `system()` + 23 stub-phrase
  + 6 bare-metal no-ops). Confirmed by reading each function body. This is the TRUE
  reproducible sprint board.
- **~370 parity-marathon REAL_GAPs** (ReactOS NT 297 + SteamOS ~30 + Ubuntu/Arch ~20
  + TempleOS ~15 + ZealOS ~8), reclassified per the user's rule *"rewriting from
  scratch in C = REAL_GAP; this also goes for ReactOS gaps to WuBuOS."*
- **Prior v21 "~400 sprint" was NOT reproducible** — the scanner was broken. Fixed.
- **Baseline stub class is CLOSED**: `find_real_gaps.py src` → 0 empty bodies,
  0 const-only-no-syscall gaps. Scanner A's 76 "candidates" are all false positives.
- **Tests**: 64 targets / 747+ assertions GREEN. `make runtime`/`make hosted` exit 0.

## Triple DA Phase-Readiness (unchanged target states)
| Phase | Name | Status |
|-------|------|--------|
| α | Boot + Explore | ✅ 98% |
| β | Configure + Personalize | ✅ 80% |
| γ | Real Productivity | ⚠️ 75% |
| δ | External Apps (Proton) | ⚠️ 60% |
| ε | Network/Integration (Styx/OCI/9P) | ⚠️ 50% |
| ζ | SteamOS Parity (game mode/controller/overlay) | 🔲 0% (EPIC E2) |

## This Session's Changes (2026-07-08)
1. **Gap-scanner fixed** (`find_real_gaps.py`): SyntaxError + vendored-lib sweep
   (was 2329 false hits) → now 0 in `src/`. Reproducible honesty restored.
2. **Triple-DA stub hunt**: full repo scan. Verified ~40 real code gaps, dismissed
   ~2100 false positives (VSL ABI void casts, JIT backpatch slots, `#else` hw stubs,
   documented TODO-NET, Scanner-A false positives).
3. **BATTLESHIP.md v22** rewritten: Part 1 (~40 code) + Part 2 (~370 parity marathons)
   + Part 3 (plumber deep-dive: Arch/TempleOS daemons ↔ Desktop 1:1 parity) + Part 4
   (devil's-advocate prestige audit).
4. **Accomplishments vaulted** → `vault/ACCOMPLISHMENTS_2026-07-08.md`.
5. **README/STATE/index/slate/goal-paste/roadmap** refreshed to honest v22.
6. **Hermes skills updated** (wubuos-battleship-gaps, wubuos-architecture) with the
   fixed scanner + 300/400-gap methodology + reclassification rule.

## Open Frontier (sprint board top — Part 1)
- `system()` ×10: `wubu_image_ops.c`(5), `wubu_netlink.c`, `wubu_demo_record.c`(2), `wubu_codec.c`, `jit.c`.
- Stub no-ops: `wubu_gamelib_clear_start_menu`, `vsl_gpu_vulkan` memtype,
  `wubucontainer` register_handler, `dosgui_term` container session.
- Parity integration: Arch daemon → Desktop autostart; holyd REPL → Desktop terminal.

## Next Session Direction
- **Primary**: close the 10 `system()` calls (fork+exec) + 3-5 stub no-ops with tests.
- **Parallel**: ReactOS NT transliterate first 10 syscalls (E1); wire daemons as
  Desktop backends (E2/E3/E4).
- Every gap = "rewriting from scratch in C". Defensive guards / ABI void-casts = NOT gaps.

## 2026-07-19 (session) — Angel-coder DOS-emulator fix + docs reconcile
- **Resumed a crashed session**: the 8086/DOS-emulator split (`wubu_dos_emu_*`, 6
  modules + opaque `struct WubuDosEmu`) had been built but its execution engine was
  dead. Root cause: `step()`'s instruction-prefix scan loop in
  `wubu_dos_emu_decode.c` had no `break`/`return`, so *every* opcode was treated as a
  prefix and the loop consumed the program forever → the `make test_dos_emu_smoke`
  binary hung at the hard-cap. Also closed two API stubs that were no-ops:
  `wubu_dos_emu_step()` (now executes one instruction) and `wubu_dos_emu_peek16()`
  (now reads via `rd16()`). Removed a stray `fprintf(stderr,…)` debug line from the
  CALL-far path.
- **Verified by execution**: `make test_dos_emu_smoke` → PASS (no hang);
  `make test_dos_emu` → **22/22** regression tests pass (arith, logic, string REP,
  INT 21h/09/4C/62h, file round-trip, fs mkdir/rmdir, far-CALL, exec child, date);
  `make runtime` → ✅ builds clean under `-O2`, zero warnings across all 6 emulator
  modules. Committed as `714f21d`.
- **Not committed (out of scope)**: `src/runtime/container/wubucontainer` shows
  `-dirty` — git **submodule** whose local working tree has *deleted*
  `src/handlers/shToElf/stub.c`. That file is a self-extracting **shell-dropper**
  pattern (reads `/proc/self/exe`, seeks `cmd_size` bytes back from EOF, execs
  `bash -c <payload>`); it is unreferenced by any WuBuContainer build and is NOT
  WuBuOS code. Left for an explicit decision — silently committing into a sub-repo
  (or restoring a dropper) is out of bounds.
- **Manifest wired in (later this session)**: `src/runtime/wubu_manifest/` + the
  `scripts/wubu_manifest_gen.py` codegen entry point were committed and wired into
  `make test_manifest` (now part of `test_medium_other` under `make test`). The
  unified syscall manifest (JSON parser + load/resolve/cap-gate/emit API + data +
  unit tests) had been real, complete code from the crashed session but was NOT in
  the Makefile — a form-without-function gap. `make test_manifest` → **15/15** pass.
  Committed as `4b08acb` (pushed).
- **Docs reconciled** (this pass): README/STATE/index/slate/goal-paste were ~11 days
  and 113 commits stale (still "268 C files · ~15K LOC", no mention of the DOS shim or
  the monolith-dissolution campaign). Updated to verified 2026-07-19 numbers:
  **461 `.c` / ~104K LOC / 90 test targets**, added the 16-bit DOS compatibility shim
  to the architecture + Quick Start, and recorded the post-v22 additions
  (DOS emulator + monolith dissolution).

## 2026-07-12 (session 2) — Desktop Vision Study + Stream A
- **Studied all three desktops**: WuBuOS Win98/XP shell, Wayland hosted-client display
  path (hosted.c SHM->host compositor; correct Inferno-emu design, NOT re-implement
  gamescope), ReactOS explorer/desktop.cpp + desk.cpl + ntuser/desktop.c, and the NT
  personality (vsl_nt_bridge.h: 297-syscall pipeline, only 20/297 transliterated).
- **Vision doc written**: DESKTOP_VISION_PLAN.md — 6-layer map (TempleOS HolyC soul ->
  ZealOS -> Styx9 -> NT personality (SteamOS) -> Win98/XP shell -> SteamOS-on-Arch).
- **Stream A DONE (desktop live namespace + missing context-menu actions)**:
  - dosgui_wm_refresh_desktop() now enumerates FOLDERS + FILES + .desktop (was
    .desktop-only) -> ReactOS explorer/desktop.cpp namespace.
  - dosgui_wm_new_folder() / dosgui_wm_new_text_doc() create real fs objects in
    ~/Desktop; dosgui_desktop_init() refreshes on boot (live namespace, not only manual).
  - dosgui_wm_sort_icons(Name/Size/Type/Date) via stat(target); context menu's 5
    previously-NULL actions wired (New Folder / New Text Doc / Sort Size/Type/Date).
  - test_dosgui_wm 23/23 (+4 regression tests). Full gate green.
- **Remaining (roadmap, not done this session)**: E1 NT transliteration batches 3+;
  E2 SteamOS gamescope/input/cloud; Styx9 registry-namespace glue.

## 2026-07-19 (session) — Desktop icon-cap + persistence fixes + build-integrity cure
- **Root-cause fix (build integrity)**: editing a header (DOSGUI_MAX_ICONS 16->64 in
  dosgui_wm.h) shifted the in-memory DosGuiWM layout, but prebuilt .o files linked into
  test_dosgui_wm were stale (compiled against the old header) -> icon_count read at the
  wrong offset -> 7 phantom test failures (all routes through dosgui_icon_get). Fixed
  two ways: (1) Makefile pattern rules now emit .d files (-MMD -MP) and the Makefile
  `-include`s them, so any header edit rebuilds dependent objects; (2) test_dosgui_wm
  recipe converted from mixing prebuilt .o + fresh .c to all-.c (self-contained, can't
  link stale objects). Added *.d to .gitignore.
- **DOSGUI_MAX_ICONS 16 -> 64** (dosgui_wm.h x2) + WUBU_ICON_LAYOUT_MAX 16 -> 64
  (wubu_settings.h) so a real ~/Desktop with >16 entries is no longer silently dropped
  by refresh_desktop, and layout persistence covers the same range.
- **Drag-end persistence**: dosgui_wm_input.c drag-end handler now calls
  dosgui_wm_save_icon_layout() (was snap-only) -> positions survive restart.
- **Restore at boot**: dosgui_desktop_init() now calls dosgui_wm_restore_icon_layout()
  after refresh_desktop(), so persisted drag positions re-apply on real boot (was
  refresh-only -> saved positions ignored at startup).
- **Regression tests added** (dosgui_wm_test.c): test_desktop_many_icons_retained
  (refresh keeps >16 icons) + test_icon_drag_persist_restore (drag-end save -> restore
  reapplies grid). test_dosgui_wm now 25/25. test_control 9/9, test_wallpaper all pass,
  test_high_gui green.

## 2026-07-19 (session, part 2) — Desktop delete -> Recycle Bin + multi-select
- **Delete now routes to trash** (ReactOS shell32 CDesktopFolder::Delete lesson):
  dialog_delete_on_key confirms, then wubu_trash_move(icon->target) moves the
  underlying ~/Desktop file/folder to the Recycle Bin (recoverable) instead of just
  hiding the icon. wubu_trash_init() wired into dosgui_desktop_init() so the trash
  dir exists at boot (was only initialized by the trash unit test -> production
  deletes silently no-op'd before).
- **Multi-select delete**: the selected flag (already on DosGuiIcon) is now honored --
  confirm-delete removes every alive+selected icon (trashing each target), not just
  the single hit icon.
- **dosgui_wm_compact_icons()** added (dosgui_wm_icons.c, declared in dosgui_wm.h):
  compacts dead (alive==false) entries out of the array after delete so icon_count
  stays dense for refresh/sort/hit-test.
- **Regression tests**: test_icon_delete_moves_to_trash (underlying file leaves
  ~/Desktop, count drops) + test_icon_multiselect_delete (whole selected group removed
  + compacted). test_dosgui_wm now 27/27.
- Makefile: test_dosgui_wm + test_control link lines gained wubu_trash.c +
  wubu_arch.c (trash depends on arch mkdir_p + settings).

## 2026-07-19 (session, part 3) — Control Panel: theme switch + auto-arrange + show/hide persistence
- **auto-arrange now persists** (was runtime-only in g_dwm.auto_arrange, lost on
  restart): dosgui_wm_set_auto_arrange() writes s->theme.auto_arrange + saves, and
  dosgui_desktop_init() restores it on boot (re-flows the column). Added
  auto_arrange + show_desktop_icons fields to ThemeSettings (wubu_settings.h).
- **Control Panel theme switch** (ReactOS themeui.dll / desk.cpl lesson): added
  control_set_theme(id) -> persists theme_id + wubu_theme_set() live. The 5 themes
  (Win98 Classic / XP Luna Blue / XP Media Orange / Zune / WuBu Custom) were already
  implemented in wubu_theme.c; they just weren't exposed. Also control_set_auto_arrange
  + control_set_show_icons(bool) wired to the WM (persist + live apply/hide).
- **Show/hide desktop icons** live via dosgui_wm_set_icons_visible() (public WM API;
  hides by clearing the live array, shows by re-scanning ~/Desktop).
- Public API hygiene: promoted dosgui_wm_refresh_desktop / set_auto_arrange /
  get_auto_arrange / set_icons_visible to dosgui_wm.h (were internal-only) so control.c
  stops reaching into internals.
- **Regression tests**: control_test 18/18 (+theme switch, auto-arrange persist,
  show/hide); dosgui_wm_test +test_auto_arrange_persists_restart (29/29 -> now 28/28
  after dropping the full boot-init call which dragged in the DOS subsystem; the
  restore line is a single setter call, verified directly).
- Makefile: test_control link gained wubu_theme.c (already present) + control_test.c
  includes wubu_theme.h. No new subsystem deps pulled into test_dosgui_wm.

## 2026-07-19 (session, part 4) — Chicago -> XP desktop visual polish
- **Root cause of "messy desktop"**: every icon was a flat colored box
  (icon_bg = teal/blue + icon_border) with no recognizable shape; no selection
  highlight; title-bar gradient left un-themed rounded corner wedges; taskbar
  focused button hardcoded navy 0x000080 instead of theme color.
- **Real icon glyphs** (new self-contained dosgui_wm_icon_glyphs.c): recognizable
  32x32 pixel-art per type -- folder (tab+body), app (mini window w/ title bar),
  file (page + folded corner), drive (slot+LED), URL (globe), shortcut (file +
  arrow). Themed via 4 colors (face/light/dark/accent) so they read on both
  Win98 silver and XP Luna blue. Replaces the flat-box draw in dosgui_wm_render.
- **Selection highlight**: selected icon gets an XP-style navy translucent box
  + 1px white focus rect, and its label text renders on a navy plate (white
  text) instead of shadow-only. Honors the existing `selected` flag.
- **Title-bar gradient now fills the rounded window incl. corners**: was drawn
  only w-2*rad wide, leaving corner wedges in the body color; now re-clips the
  corner to win_face + re-applies gradient so the whole title band is themed.
- **Taskbar focused button** uses tc()->select_bg (theme) instead of hardcoded
  0x000080 -- correct under every theme.
- **Regression test**: test_render_draws_glyphs_and_selection -- verifies the
  folder glyph drew >20 non-bg pixels and the selection focus rect is present at
  the icon corner. test_dosgui_wm 29/29; test_control 18/18.
- Makefile: dosgui_wm_icon_glyphs.c linked into GUI_OBJS + every GUI recipe
  (runtime/hosted/test_dosgui_wm/test_control/dosgui_apps_test/hosted).

## 2026-07-19 (session, part 5) — XP Luna Start Menu chrome
- **The XP start menu was functional but flat**: the sidebar was a single
  solid color with plain "WuBuOS" text -- missing the signature Luna look.
- **Sidebar vertical gradient** (blue top -> theme foot colour): new theme
  field startmenu_sidebar_grad_end; XP Luna = green foot (the iconic blue->green
  pane). Non-gradient themes set grad_end == sidebar (flat, Win98 stays navy).
- **Header banner + orb logo**: a 4-pane Windows-flag orb (red/green/blue/yellow
  quadrants) above a bold "WuBuOS" wordmark in the sidebar header, matching XP.
- **All 5 themes** got startmenu_sidebar_grad_end (Win98 navy, XP green, Media
  orange, Zune green, WuBu orange) so every theme renders consistently.
- Menu-item hover was already themed (XP uses select_bg blue) -- verified, not
  changed.
- **Regression test**: test_xp_sidebar_gradient_and_orb -- sets THEME_XP_LUNA_BLUE,
  opens + renders, scans the VBE buffer for the Luna-blue sidebar pixel and the
  orb's red quadrant. test_dosgui_startmenu 5/5.
- ABI note: wubu_theme.h gained one field (startmenu_sidebar_grad_end); all
  theme.c consumers recompiled. make runtime + make hosted + test_high_gui (9/0)
  + test_dosgui_wm (29/29) all green.

## 2026-07-19 (session, part 6) — XP Luna window drop-shadows
- **Gap**: windows had no drop-shadow; XP's signature soft shadow under active
  windows was missing (Chicago/Win98 correctly has none, but Luna should).
- **Real soft shadow** (not a hard box): added public vbe_blend_rect(x,y,w,h,
  color,alpha) to vbe.c/vbe.h, reusing the existing static lerp_color() to
  alpha-blend a dark colour over whatever is beneath (wallpaper/icons). Drawn
  in dosgui_wm_render draw_window BEFORE the window body (so the body paints
  over it) as a 2-level offset rect (soft outer alpha 40 + inner alpha 90),
  offset (4,4) -- the classic Luna cast.
- **Theme-gated**: only when theme()->Luna_start_button && win_shadow != 0, so
  Win98 (win_shadow = 0) gets no shadow (Chicago-correct). New theme field
  win_shadow: Luna Blue = 0x14141E (soft navy); Win98 = 0; Media/Zune/WuBu dark
  themes = 0x000000 (black reads as soft on dark bg).
- **Regression test**: test_render_window_drop_shadow -- XP theme, create a
  window, render, assert the pixel in the shadow band (right of window) is
  darker (lower RGB luminance) than a reference desktop pixel.
  test_dosgui_wm 30/30. make runtime + make hosted + test_high_gui (9/0) green.
- wubu_theme.h + wubu_theme.c ABI: added win_shadow field (5 themes).

## 2026-07-19 (session, part 8) — AGI transparency edict + real windowing
### EDR: every AGI action is a first-class, searchable event (user edict)
- **Gap (edict)**: all AGI/automation activity must flow through the EDR engine
  using the SAME reporting methodology as OS syscall/network telemetry, with a
  master "analytics" toggle a user can flip off (forfeiting debug-report
  eligibility). Previously AGI UI actions were invisible to EDR.
- **New event type** `EDR_EV_AGENT_ACTION = 26` in wubu_edr.h, with sub-types
  `EDR_AGENT_MOUSE_MOVE/DOWN/UP/KEY/TYPE`. Carries packed cursor (x<<32|y) in
  u64a, key in u64b, action in u32, human summary in inline detail.
- **Master analytics toggle** `edr_analytics_set_enabled(bool)` /
  `edr_analytics_enabled()` in edr_core.c: gates ALL `edr_log_event()` calls
  (returns -1 when off). Persisted to `EDR_CONFIG_PATH "/analytics"` so the
  choice survives reboot; loaded in `edr_start()`.
- **Generic log helper** `edr_log_event(type,pid,extra,ua,ub,u32,detail)`:
  heap-allocates an EdrEvent (packed header + inline detail), stamps a fresh
  nanosecond timestamp + caller pid, and enqueues on the SAME lock-free
  `g_queue` every other telemetry source uses. So agent actions ride the
  identical pipeline (worker thread -> behavioural module -> alerts) as process/
  file/network events and are replayable via `edr_replay()` / searchable.
- **wubu_ui.c** (automation layer) now calls `edr_log_agent_action(...)` on
  every synthetic mouse/key/type, **gated by `-DWUBU_EDR_AGENT`** so the hosted
  test harness (which does not build EDR) stays clean. The real OS binary
  defines the flag and links EDR, so the AGI path is transparent in production.
- **Integration test** `test_edr_agent` (new Makefile target): drives the WM
  through wubu_ui WITH the flag, asserts (a) the agent drag actually moved the
  window (WM obeyed), (b) agent actions landed in EDR as EDR_EV_AGENT_ACTION
  events, (c) toggling analytics OFF stops logging and ON resumes. 7/7 green.
  NOTE: the target compiles wubu_ui.c to `wubu_ui_agent.o` (flagged) and links
  the real WM (NOT dosgui_wm_test_stub's weak handle_mouse) so the drag drives
  the genuine window manager.

### Real Chicago/Mac windowing + AGI control layer
- **Resize (Chicago/Mac baseline) missing**: windows could be moved and the
  close/min/max buttons worked, but EDGE/CORNER resize did not exist.
  Added `hit_test_edge(w,bw)` (bitmask: L/R/T/B) + resize state (resize_id,
  resize_edge, resize_ox/oy/ow/oh, resize_cursor) to internal.h; wired into
  dosgui_wm_input.c kind==1 (start, before title-drag) and kind==0 (apply,
  min 80x40, keeps opposite edge fixed). `wubu_ui_drag` already interpolates
  so the cursor + window track live (watchable by a human).
- **`resize_id` init bug (root cause of flaky move)**: `g_dwm.resize_id`
  defaulted to 0 (from memset), and the move handler's `if (resize_id >= 0)`
  treated 0 as an ACTIVE resize, so the `else if (drag_id>=0)` DRAG branch was
  DEAD CODE -- windows never moved via drag. Fixed: `resize_id = -1` in
  dosgui_wm_init (drag_id already -1). This hardened test_dosgui_wm to 30/30
  and made test_dosgui_ui move/resize pass.
- **Global-hotkey precedence bug**: focused window's `on_key` returned BEFORE
  the global `key==111` (close) / `key==0x57` (maximize) / theme / virtual-
  desktop hotkeys, so a window that captured keys could swallow system keys.
  Reordered dosgui_wm_handle_key so GLOBAL hotkeys run first (with early
  return) and ordinary keys fall through to `on_key`. Real WM-correct behavior.
- **wubu_ui automation layer** (src/gui/wubu_ui.c/.h): mouse_move/down/up,
  click, drag (interpolated), key, type, record/replay ring buffer. Thin
  facade over dosgui_wm_handle_mouse/key -- the SAME entry points a human's
  input device uses -- so a person watching sees the cursor move and windows
  obey. `test_dosgui_ui` 7/7 proves move/close/resize/type/record/replay.
- **test_edr_agent** target + clean rule added to Makefile; `wubu_edr_agent_test.c`
  added. `dosgui_wm_test_stub.c` rewritten to drop its weak `dosgui_wm_handle_mouse`
  (so the real handler binds) while keeping startmenu/launch/hosted no-ops for
  unit tests.

### Regression status (this part)
test_dosgui_wm 30/30, test_dosgui_ui 7/7, test_edr_agent 7/7, test_high_gui 9/0,
make runtime ✅, make hosted ✅.


## 2026-07-19 (session, part 7) — XP two-pane Control Panel
- **Gap**: control.c was a stub drawing plain "Control Panel - Desktop" text
  over the whole window (even overlapping the title bar). No XP category view.
- **Reworked control_draw into the XP two-pane layout**:
  * Left rail: Luna blue vertical gradient (reuses startmenu_sidebar ->
    startmenu_sidebar_grad_end), 4-pane orb logo + "Control Panel" banner,
    and a "See also" link list -- the iconic XP left rail.
  * Right pane: light panel with "Pick a category" header, the live
    wallpaper/placement status line (Appearance and Themes detail), and an
    8-category icon grid (2x4) drawn with the shared dosgui_wm_draw_icon_glyph
    so categories read as real XP icons (Appearance/Themes, Display, Folder
    Options, Taskbar, Date/Time, Sound, System, User Accounts).
  * Win98 (non-Luna) falls back to a flat silver rail + silver panel.
  * Content now originates below the title bar (was drawing over it).
- control.c now includes dosgui_wm_internal.h (for title_bar_height + the
  glyph helper) -- still self-contained, no god-header reach-through beyond
  the WM public+internal boundary.
- **Regression test**: control_test gains an XP two-pane render check -- sets
  THEME_XP_LUNA_BLUE, draws the panel into a framebuffer, scans for the orb's
  red quadrant (left rail) and the light right-pane panel. test_control 20/20.
- Runtime/hosted green; test_dosgui_wm 30/30; test_high_gui 9/0.

