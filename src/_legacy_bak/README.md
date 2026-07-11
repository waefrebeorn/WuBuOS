# src/_legacy_bak/ — Excluded / Dead-Code Graveyard

This directory holds source files that are **NOT part of the WuBuOS build
graph** (i.e. no `Makefile` target compiles or links them). They are kept for
reference / archaeology, not built.

## Why these are excluded

Files land here for one of three reasons:

1. **Stale / superseded duplicate** — an older or alternate copy of a file that
   *is* in the build graph under a `subdir/` path. The built copy wins.
2. **Needs unavailable deps** — requires libraries not installed in this build
   env (Wayland, wlroots, Vulkan headers, ML frameworks, etc.) or a feature
   that is intentionally disabled.
3. **Orphan / never-wired** — referenced by another `.c` via `#include` or a
   forward-decl but never actually compiled by any `Makefile` rule.
4. **Legacy scratch / demo** — one-off tools, demos, or worldsim experiments
   with no build target.

## Inventory

### Superseded / stale duplicates (option (1))
| File (here) | Superseded by (in build) |
|---|---|
| `apps__calc.c` | `apps/calc/calc.c` |
| `apps__calculator.c` | `apps/calc/calc.c` (+ `apps/calc/calc.h`) |
| `apps__control.c` | `apps/control/control.c` |
| `apps__notepad.c` *(legacy)* | `apps/notepad/notepad.c` |
| `apps__repl.c` *(legacy)* | `apps/repl/repl.c` |
| `apps__explorer.c` | `gui/dosgui_explorer.c` |
| `apps__terminal.c` | `gui/dosgui_term.c` |
| `apps__wubu_canvas.c` | `apps/canvas/canvas.c` |

### Needs unavailable deps (option (2)) — status
| File (here) | Missing dep / note | Status |
|---|---|---|
| `gui__wubu_compositor.c` | wlroots (present) + `gbm.h`/`pixman.h` (need `libgbm-dev`/`libpixman-1-dev`, apt blocked — no sudo) | **FUTURE REBUILD** — `wubu_compositor.h` API is still in the build graph with NO impl; restore this once dev headers installable. |
| `gui__wubu_compositor_standalone.c` | `<wayland-server-core.h>`, gbm, vulkan, pixman (gbm/pixman headers absent) | **FUTURE REBUILD** — alternate Vulkan-path compositor; secondary to `wubu_compositor.c`. |
| `hosted__wubu_display.c` | ~~needs xf86drm/linux headers~~ | **RESTORED** (commit d1f6877) → `src/hosted/wubu_display.c`; exercised by `test_drm_direct`. Removed from this dir. |
| `jit__jit_mir.c` | MIR backend; included by `jit.c` but never wired | dead — leave. |
| `bear__wubu_math.c` | only referenced by `worldsim/` + `audio/` tests (those dirs not built) | dead — leave. |
| `runtime__vsl__vsl_syscall_table.c` | syscall dispatch table | **DEAD** — superseded: `vsl_syscall.c` already defines `vsl_syscall_table[]` inline (line 18). Do not restore. |
| `runtime__container__wubucontainer__src__handlers__shToElf__stub.c` | nested stub inside the container handler tree; untracked, never compiled | dead — leave. |

### Orphan / scratch / demo (option (3)+(4))
| File (here) | Note |
|---|---|
| `apps__wubu_codec.c` | **RESTORED** (commit fdac678) → `src/apps/wubu_codec.c`; ffmpeg-free at compile time (shells out to ffmpeg binary). Exercised by `test_apps2` (19/19). Removed from this dir. |
| `apps__wubu_freedoom.c` | **RESTORED** (commit on fixup/organize-exclusions) → `src/apps/wubu_freedoom.c`; defines the `wubu_doom_*` launcher API that `wubu_arch_test.c` exercises. `test_arch` PASSES. Removed from this dir. |
| `apps__doom.c` | raycaster demo; NOT the `wubu_doom_*` API (that lives in wubu_freedoom.c). Referenced nowhere in build. Leave. |
| `tools__dosgui_screenshot.c` | standalone screenshot tool, no Makefile target |
| `tools__wubu_demo_record.c` | demo recorder, no target |
| `tools__wubu_demo_screenshot.c` | demo screenshot, no target |
| `tools__wubu_x11_recorder.c` | X11 recorder (needs X11), no target |
| `worldsim__entity.c` | worldsim sim, no Makefile target |
| `worldsim__physics.c` | worldsim sim, no target |
| `worldsim__render.c` | worldsim sim, no target |
| `worldsim__sim.c` | worldsim sim, no target |
| `worldsim__terrain.c` | worldsim sim, no target |
| `worldsim__test_worldsim.c` | worldsim test, no target |

### Originally-legacy (present before this cleanup)
| File (here) | Note |
|---|---|
| `desktop.c`, `startmenu.c`, `theme.c`, `wm.c` | old desktop/WM sources |
| `wm_test.c` | old WM test |
| `vbe_legacy.h` | old VBE header |

## How to revive a file

If a file here should re-enter the build:

1. Move it back to its proper `src/<dir>/` location (or `git mv` from history).
2. Add the `.o` to the relevant `*_OBJS` variable **and**, if a test recipe
   compiles the parent `.c` directly, add the `.c` there too (see
   `wubuos-monolithic-split` skill — "Dedup-aware blanket Makefile wiring").
3. Resolve any missing-dependency guards.

## Pre-existing broken tests (fixed at recipe level; remaining is test-source)

- `test_dosgui_apps` — **FIXED & PASSING (42/42)**. Recipe rot corrected (paths, includes,
  restored `wubu_freedoom.c`). Exposed the 7 app state structs (CalcState, NotepadState,
  PaintState, TaskManagerState, RegeditState, FileManagerState, ControlState) in their
  public headers (was opaque → test reached into internals). Implemented a per-instance
  Paint API (`paint_create/destroy/add_shape/save_undo/undo/set_tool/launch`) + `PaintTool`
  enum + `PAINT_TOOL_*` aliases. Added the full WM submodule set + container/styx/json
  modules to the recipe (B22 discipline: any recipe with `dosgui_wm.c` needs the whole set).
  Removed stub `dosgui_wm_create` from the test so the real WM provides non-NULL windows.
- `test_arch` — **FIXED** (restored `wubu_freedoom.c`, which defines the `wubu_doom_*` API).
- `test_drm_direct` — **FIXED** (restored `wubu_display.c` + `wubu_gbm.c`; `-I/usr/include/drm`).
- `test_apps2` — **FIXED** (restored `wubu_codec.c`; ffmpeg-free at compile time).
- `test_container_registry` — **COMPILES** (removed `h.handler`/`h2.handler` accesses to the
  non-existent `handler` member; added `wubu_container_registered_count/name` accessors so the
  opaque engine is no longer poked directly). Still **fails at runtime** because
  `wubu_container_init()` requires the real WuBuContainer submodule + Node/Bun runtime, which
  is absent in this sandbox — an environment limitation, not a code bug.
