# Monolith Dissolution — Old → New Module Map

**Written 2026-07-19.** This document records the large-module dissolution campaign
that ran 2026-07-15 → 2026-07-19. It exists so that older docs
(`BATTLESHIP.md` v22, `OS_BIBLE.md`, `REACTOS_NT_SYSCALL_STUDY.md`, `wiki/`,
`BATTLESHIP_GAPS.md`) — which still name the *old* monolith filenames — can be
reconciled against the current tree.

**Verified current tree (2026-07-19):** 468 `.c` / 214 `.h` / ~105,459 LOC /
**91 `make test_*` targets**. Repo root: `/home/wubu/wubuos`.

## Why this map exists
The older docs were correct at the time, but the source tree was subsequently
split into opaque C11 leaf modules (minimal includes, no god headers, self-contained
per-concern `.c` files). Any doc that references the *old* filenames below is now
pointing at a deleted file and should be updated to the new location.

## Map (verified against `src/`)

| Old monolith (deleted) | New module(s) — verified present |
|------------------------|----------------------------------|
| `src/apps/wubu_editor.c` | `src/apps/editor/` (`editor.c`, `editor_bookmark.c`, `editor_find.c`, `editor_macro.c`, `editor_selection.c`, `editor_undo.c`) |
| `src/apps/wubu_canvas.c` | `src/apps/app_canvas.c` + `src/apps/wubu_canvas_*.c` (`_blend`, `_draw`, `_filter`, `_io`, `_io_ppm`, `_layers`, `_plugin`, `_transform`, `_undo`), `wubu_canvas.h`, `wubu_canvas_internal.h` |
| `src/runtime/styx.c` | `src/runtime/styx_*.c` (`styx_enc`, `styx_fid`, `styx_names`, `styx_parse`, `styx_serve`) + `src/runtime/styxfs_*.c` (`styxfs_callbacks`, `styxfs_host`, `styxfs_path`, `styxfs_posix`, `styxfs_server`, `styxfs_util`, `styxfs_vfs`) |
| `src/gui/wubu_clipboard.c` | `src/gui/wubu_clipboard.c` + `wubu_clipboard_mime.c`, `wubu_clipboard_wl.c`, `wubu_clipboard_internal.h` |
| `src/gui/dosgui_explorer.c` | `src/gui/dosgui_explorer*.c` (`_drives`, `_format`, `_fs`, `_fsops`, `_info`, `_input`, `_ops`, `_preview`, `_render`, `_tree`, `_zip`) |
| `src/gui/startmenu.c` | `src/gui/dosgui_startmenu*.c` (`_db`, `_power`, `_search`, `_tree`) + `src/gui/wubu_gamelib_startmenu.c` |
| `src/runtime/wubu_proton.c` | `src/runtime/wubu_proton*.c` + `wubu_proton2*.c` (`wubu_proton2`, `_device`, `_gamescope`, `_launch`) |
| `src/runtime/wubu_dxvk.c` | `src/runtime/wubu_proton_dxvk.c` + `src/runtime/wubu_dxvk_conf.c` |
| `src/runtime/wubu_exec.c` (partial) | `src/runtime/wubu_exec_*.c` (`_format`, `_wasm`, `_macho`, `_dos`, `_container`) |
| `src/runtime/wubu_container.c` (partial) | `src/runtime/wubu_container_test.c` + container lifecycle/serialize/io/flatpak/ops modules; `src/runtime/wubu_bottles_*.c` |
| `src/kernel/...` interrupt PIC/IRQ | split into dedicated interrupt/PIC/IRQ-routing modules; bare-metal boot separated |

## Discipline applied (per project rules)
- **Opaque structs** — modules expose a public API header + an `_internal.h` for
  cross-module prototypes only; no function bodies in headers.
- **Minimal includes** — each `.c` includes only what it needs.
- **No god headers** — no single header that pulls in the whole tree.
- **Self-contained** — each leaf module compiles independently and links via the
  `RT_OBJS` list in the `Makefile`.

## Open work still tracked (NOT dissolved — still no-ops)
These functions remain stub-phrase / bare-metal no-ops and are the current sprint
board (distinct from the dissolved monoliths above):
- `src/kernel/tasking.c` ×2 regions (ctx-switch no-ops) + 2 stub phrases
- `src/runtime/wubu_anticheat.c` ×2
- `src/bear/bear_cudnn.c` ×3
- `src/gui/wubu_screenshot.c`, `src/gui/wubu_pkgmgr.c`, `src/gui/wubu_compositor.c`,
  `src/gui/wubu_compositor_standalone.c` ×2
- `src/runtime/oci_http_client.c`, `src/compiler/holyc_ptx.c`
- `src/runtime/wubu_bottles.c`

## E1 ReactOS NT status (separate epic)
88 / 297 NT syscalls transliterated into `src/runtime/vsl_syscall_nt.c` (decomposed
into `vsl_nt_{atoms,io,job,proc,sync,registry}.c`). 209 remain.
