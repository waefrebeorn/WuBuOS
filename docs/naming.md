# WuBu Naming & Organization Map

Purpose: one file that answers "what prefix means what," so a new module isn't
another `zx`. Verified against the working tree 2026-11-06.

## Reorganization queue (measured 2026-11-25)

Ordered by confusion caused per unit of risk. A batch lands only with: `git mv`, path
tokens rewritten, **`.o` names in `mk/` rewritten** (a `.c` sweep misses them and the build
says `No rule to make target`), build, and the tests that compile those units. File renames
and SYMBOL renames are separate changes -- dragging symbol names through every caller is
how a tidy becomes a regression.

| # | Now | Purpose it should announce | Refs | Risk |
|---|---|---|---:|---|
| 1 | `control_panel.c` + `cp_{text,hardware,display,network,theme,sound}.c` | one prefix: `control_panel.c` + `control_panel_*.c` | 181 | medium -- two naming schemes in one subsystem, 7 files |
| 2 | `gfx/gui_{surface,blend,shape,blur}.c` vs top-level `gui_blit.c` | one layer name for the primitive drawing layer (`gfx_*`), and `gui_blit` joined to it | ~30 | medium -- touches the render path |
| 3 | ~~`daemon_panel.c` -> `service_panel.c`~~ | **REJECTED after reading it**: it bridges *daemons* (archd, holyd) into the tray and a window; `service_mgr.c` manages *services*. `daemon_panel` is honest and `service_panel` would assert a relationship that does not exist | 7 | -- |
| 4 | `big_picture.c` / `big_picture_games.c` | **undocumented acronym** -- read it, then name it for what it does | ~15 | low, but needs a purpose first |
| 5 | `dos_window.c` vs `wm_window.c` | `dos_window` is the guest-app window body; `wm_window` is the WM's window object. Two "windows" in one layer | ~40 | high -- core windowing |
| 6 | **TWO files named `wubu_bonzi.{c,h}`** -- `src/gui/` is the buddy sprite blitter, `src/kernel/` is a different module; includes use the SAME token `wubu_bonzi.h` in both layers | `src/gui/buddy_sprite.*` for the blitter. **Renaming needs path-qualified edits only** (`$(GUI)/wubu_bonzi.c`, `../wubu_bonzi.h` vs the kernel's plain include), because a bare-token sweep cannot tell the two files apart | 13 | **medium** -- a plain sweep would cross the layers silently |

Rule for #4: a name that needs a comment to explain the acronym is already broken. Read the
file first, then name it -- do not invent an expansion.

#4 is done: `bpm.*` -> `big_picture.*` (Big Picture Mode). #3 is REJECTED -- the queue was
written from names alone, and opening the file showed the rename would have made the
relationship false. A queue entry is a question, not an instruction: read the file before
you rename it, including when you wrote the queue yourself.

## Renamed: the two "desktop" files (2026-11-25)

`src/gui/shell/desktop.c` and `src/gui/shell/wm_desktop.c` were two different things with
the same word in the name, so every reader had to open both to know which one to edit:

| was | is | what it actually owns |
|---|---|---|
| `desktop.c` / `desktop.h` | `shell_session.c` / `shell_session.h` | shell lifecycle (`desktop_init/_shutdown/_render`) and the ONE app-launch path (`launch_app`, `desktop_launch_program/_exec/_icon`) |
| `wm_desktop.c` | `desktop_icons.c` | the icon grid: snapping, auto-arrange, sort, refresh, `~/.desktop` shortcut creation |

Rename mechanics that matter (a regex rename corrupted 9 files once): the `git mv` and the
caller edits landed in ONE change, path tokens only (`wm_desktop.c`, `shell/desktop.h`,
`shell/desktop.c`, `./desktop.h`, `"desktop.h"`), plus the `.o` names in `mk/`. Verified by
building and by running the tests that compile those units -- never by grep alone.

Still split-worthy: `shell_session.c` carries both bootstrap and launching. When it next
grows, `launch_app` + the three `desktop_launch_*` move to `app_launch.c`.

## The #1 collision: `ns`

Three unrelated things currently share "ns":
- `src/kernel/wubu_ns.{c,h}` — **NVMe / block-DM namespaces** (wubu_ns_nvme/
  wubu_ns_namespace/wubu_ns_multipath). Kernel driver-flags surface. UNRELATED to /n.
- `/n` namespace + `src/runtime/wubu_ns_bridge_internal.h`, `wubu_ns_fs.c` — the
  **Plan 9-style 9P filesystem** the module/PE loader reads from (`g_ns_root`, the
  `build_path` "resolve /n/ and keep subdirs" rule). This is what the loader uses.
- `styxfs_server.c` / `styxfs_client` — the **9P transport** the /n server speaks.

Do not add a fourth meaning. Conventions going forward:
- `/n` filesystem code := `nsfs_` (was wubu_ns_fs) — the thing tests link.
- 9P transport := `styx_`.
- block namespace := keep `wubu_ns` (it is NVMe-specific; rename only if a clear
  owner emerges).

## Loader / module stack (the `zx` family — rename forward to `mod_`)

The `zx` koan is overloaded and signals nothing. Prefer, on each NEW edit, the
`mod_` family (do NOT mass-rename — a regex rename already corrupted 9 files):
- `mod_run`  = the one-call runner primitive (was wubu_zx_run).
- `mod_reg`  = the module registry: load/dedup-by-SHA256/refcount/unload
  (was wubu_zxreg).
- `mod_load` = namespace loader: read a PE by name from /n (was wubu_zxload).
- `mod_raw_pe` = the PE loader contract (wubu_pe): parse/map/reloc/IAT/TLS/entry.
- `modsys`   = the symbol registry the IAT resolves against (wubu_modsym).

## Prefix governance (which prefix means which layer)

| prefix | layer | example |
|---|---|---|
| `wubu_`  | OS-identity / kernel owner (sparing use) | wubu_bonzi, wubu_agi_kernel |
| `mod_`   | module/PE loader stack (kernel) | mod_run, mod_reg, mod_load |
| `pe_`    | the PE loader contract (KEEP, unambiguous) | (inside mod_raw_pe) |
| `nsfs_`  | the /n filesystem glue | read a module by name |
| `styx_`  | 9P transport (server + client) | styxfs_server |
| `sess_`  | P3 session stack (session boot, autostart) | sess_boot, sess_autostart |
| `bus_`   | message bus | bus_client |
| `vs_`    | the VSL syscall layer | vs_dispatch |
| `nt_`    | the NT personality | nt_syscall_dispatch |
| `holyd_` | the compiler (wubunos; NEVER holyc) | holyd_codegen_stmt |

Rule: a bare first token on a NEW file must name its layer, not "wubu." `wubu_` is
the identity prefix, not a default.

## Test naming

`test-<module>` is one logical gate. The aggregate `make test-module` runs the 8
module/PE/self-build targets as ONE gate — the honest "owned & tested" inventory.
A file in `src/kernel/{vendor raft}` that is NOT in any test-module member is
undeclared debt, not progress.

## Build artifacts

`.o` / `.d` are already gitignored (`*.o`, `*.d` in .gitignore) and untracked
(verified 0 tracked). They pollute disk, not the repo. Do not `git add` them.

## Resolved (kept for the record)

- `wubd_holyc_agi_test` -> `wubd_holyd_agi_test` (src/runtime).  The leftover
  "holyc" name violated the ONLY-HOLYD rule.  DONE -- `wubd_holyc_*` now has
  zero references anywhere.
- `holyd_bf_stub.c` -- an unexplained "+ stub" framing, referenced by no build
  in either repo.  Removed (wubunos cf88a36).

## Odd first tokens, audited 2026-11-10

Every short first token was checked against "does it name its layer?".  All are
meaningful domain names, not noise:

| token | what it is | where |
|---|---|---|
| `lfm2` | the LFM2 language-model architecture | `src/brain/include` |
| `crnn` | convolutional-recurrent net, the photo tool's recogniser | `src/apps/wubuoffice/tools` |
| `oci`  | Open Container Initiative runtime descriptors | `src/runtime/oci` |
| `fw`   | firmware | `src/firmware` |
| `train` | the model-training tools | `src/apps/wubuoffice/tools` |
| `run`  | runner / test scripts | `src/brain/tools`, `src/tools` |

None need renaming.  The rule is that a first token names the CONCERN the file
implements; a bare `wubu_` would say nothing, and these all say something.
