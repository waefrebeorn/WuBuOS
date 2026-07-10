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
| `apps__wubu_codec.c` | **FUTURE REBUILD** — media codec API (`wubu_codec.h`) is referenced by `wubu_apps2_test.c` but needs ffmpeg (`libavcodec`, absent). Restore + wire once ffmpeg dev headers present. |
| `apps__wubu_freedoom.c` | referenced only by `wubu_arch_test.c` (not compiled) |
| `apps__doom.c` | referenced only by `wubu_arch_test.c` / `wubu_ct_bwrap.c` (not compiled) |
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
