# WuBu Naming & Organization Map

Purpose: one file that answers "what prefix means what," so a new module isn't
another `zx`. Verified against the working tree 2026-11-06.

## Submodules: the duplicate-source problem (measured 2026-11-25)

`tools/tree_names.py` surveys every submodule for the failures that are objective:
basename collisions across directories, files that break their directory's dominant prefix,
acronym-only names, and sources with no matching header. Run: `python3 tools/tree_names.py`
(no arguments = every submodule).

What it found, and the reason it matters more than any rename:

**The same language runtime exists THREE times, and the copies have drifted.**

| copy | .c files | state |
|---|---:|---|
| `src/gui/wet/externals/wua/src` | 9 | newest of the two vendored copies |
| `src/gui/vws/externals/wet/externals/wua/src` | 9 | **stale**: 7 files byte-identical, `wb_lv_eval.c` 8 lines behind, `wb_lv_host.c` 16 lines behind |
| `src/lua/www/~WuBu~/src/lv` | 12 | richest variant (`string`, `stdlib`, `value` files the others lack) |

That is `vws` vendoring `wet` which vendors `wua`, plus a third checkout in `lua/www`. It
violates the "never leave a second copy" law at the repository scale, and the divergence
proves it is not academic: a fix applied in `wet` is invisible to `vws`, and whichever copy
a component links decides which bugs it has.

**Resolved by revision, not by rename.** Each copy carries its own git checkout, so the
divergence had an exact cause and an exact fix:

| checkout | was | now |
|---|---|---|
| `src/gui/wet` (original) | `d86dd19` | `d86dd19` |
| `src/gui/vws/externals/wet` | `a7eff0d` | **`d86dd19`** |
| `src/gui/wet/externals/wua` | `bf39a26` | `bf39a26` |
| `src/lua/www/~WuBu~/src/lv` | `bf39a26` | `bf39a26` |
| `src/gui/vws/externals/wet/externals/wua` | `912f802` | **`bf39a26`** |

vws was a stale fork of its own toolkit: a wet fix was invisible to it, and `bf39a26`'s
`wb_lv_call_multi` was missing. Bumped in the vws repo (`4d9295d`) and verified by building
and running `test_vws_designer` against the new checkout -- not by comparing file counts.

`make check_vendor` (`tools/vendor_drift.py`) now holds the line: it pairs every nested
checkout with its original by repository, reports disagreement as DRIFT, and exits non-zero.
Two flaws were fixed in it during the first hour of its life, both worth remembering:
it stopped descending at the first checkout, so it reported "no drift" on a tree with
exactly the drift it was written to find; and it called unanimous agreement "ambiguous".
A gate that cannot see its own subject is the most expensive kind of lie.

Still true and not fixed by pinning: three checkouts of one runtime is still three
checkouts. Making `vws` consume the parent tree's `wet` instead of vendoring it is the
structural fix, and it needs vws's build rewritten, not a bump.

Two smaller findings from the same survey:

- `src/brain/tools/` and `src/brain/vault/tools/` carry duplicated tools (`compare_weights_two_paths.c`,
  `dump_gate_raw.c`, `dump_moe_split.c`, `ssm_layer0_dump.c`, plus a dated `vault/tools/20260518_233049/`).
  A `vault/` of archived copies inside a live tree is a second copy with a date on it.
- `src/audio/bigmac` and `src/apps/wubupad` collisions are `third_party/` and CMake build
  artefacts -- vendored code and generated files, correctly excluded from renaming.

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
| 5 | `dos_guest_view.c` vs `wm_window.c` | `dos_window` is the guest-app window body; `wm_window` is the WM's window object. Two "windows" in one layer | ~40 | high -- core windowing |
| 6 | **TWO files named `wubu_bonzi.{c,h}`** -- `src/gui/` is the buddy sprite blitter, `src/kernel/` is a different module; includes use the SAME token `wubu_bonzi.h` in both layers | `src/gui/buddy_sprite.*` for the blitter. **Renaming needs path-qualified edits only** (`$(GUI)/wubu_bonzi.c`, `../wubu_bonzi.h` vs the kernel's plain include), because a bare-token sweep cannot tell the two files apart | 13 | **medium** -- a plain sweep would cross the layers silently |

Rule for #4: a name that needs a comment to explain the acronym is already broken. Read the
file first, then name it -- do not invent an expansion.

#4 is done: `bpm.*` -> `big_picture.*` (Big Picture Mode). #3 is REJECTED -- the queue was
written from names alone, and opening the file showed the rename would have made the
relationship false. A queue entry is a question, not an instruction: read the file before
you rename it, including when you wrote the queue yourself.

## Renamed: the gfx layer (2026-11-25)

`src/gui/gfx/` held `gui_surface.c`, `gui_blend.c`, `gui_shape.c`, `gui_blur.c` -- the layer
said `gfx`, every file said `gui`, and the public header was `gui_gfx.h` inside a directory
already called `gfx`. One name per layer:

| was | is |
|---|---|
| `gfx/gui_{surface,blend,shape,blur}.c` | `gfx/gfx_{surface,blend,shape,blur}.c` |
| `gfx/gui_gfx.h` / `gui_gfx_internal.h` | `gfx/gfx.h` / `gfx/gfx_internal.h` |
| `gfx/gui_gfx_test.c` | `gfx/gfx_test.c` |

## Renamed: dos_window is a guest view (2026-11-25)

`src/gui/dos_window.c` sat beside `src/gui/wm_window.c` and read as another kind of window.
It is not: it blits a 16-bit DOS process's captured VGA framebuffer and forwards
keyboard/mouse into the guest through its control channel. `wm_window.c` is window lifecycle,
z-order, focus and virtual desktops. One word was doing two jobs.

| was | is |
|---|---|
| `dos_window.c` / `dos_window.h` | `dos_guest_view.c` / `dos_guest_view.h` |
| `dos_window_test.c` / `_stub.c` | `dos_guest_view_test.c` / `_stub.c` |
| target `test_dos_window` | `test_dos_guest_view` |

The function symbols (`dos_window_close`) are unchanged on purpose: a file rename and a
symbol rename are separate changes, and the symbol is still accurate.

The test files were caught by the rule written after the `test_cp_sound` failure -- when
renaming `X.c`, look for `X_test.c` -- which is why this batch needed one sweep instead of
two. Verified: `make hosted` rc=0, `test_dos_guest_view` ALL PASSED.

`gui_blit.c` stays at `src/gui/` on purpose: it is guest presentation, not a primitive, and
the layer split is the point of the rename.

Verified: `make hosted` rc=0, `test_gui_gfx` and `test_wm_shadow` rc=0, `test_wm` 0 failed.
The first verification run named two targets that do not exist (`test_gfx`, `test_shadow`)
and reported them as failures -- a reminder that a test name invented from a file name is
not a gate.

## Submodules, re-measured with the metric fixed (2026-11-25)

The first survey was unusable: it counted vendored SDKs and test programs as naming defects.
`bigmac` alone reported 735 "acronym hints" and 8 "collisions" that were all SDL2's
per-platform files, and `src/wemu/tests` reported 21 "off-prefix" files for names like
`exec_chain.c` and `fork_clone.c`, which are named for the scenario they exercise. A metric
that flags correct names teaches people to ignore it.

`tools/tree_names.py` now excludes `third_party/`, `externals/`, `vendor/`, `deps/`,
`vault/` (an archive of dated snapshots) and any `build*` directory, and it skips OFF-PREFIX
in `tools/` and any directory with "test" in its name. The honest result, own sources only:

| tree | .c | collisions | off-prefix | note |
|---|---|---|---|---|
| `src/compiler` | 208 | 1 | 0 | the collision is `compat/win32/sys/mount.h` vs `compat/win32/linux/mount.h` -- deliberate shim mirrors of platform paths |
| `src/brain` | 1465 | 1 -> 0 | 21 | its `tools/`/`vault/` "collisions" were the archive; the real one was `include/wubu_kv_runtime.c` |
| `src/wemu` | 94 | 0 | 0 | clean |
| `src/apps/wubupad` | 59 | 0 | 0 | its only "collision" was CMake compiler-id files under `build-audit/` and `build-asan/` |
| `src/apps/wubuoffice` | 534 | 21 | 11 | the largest real block; not yet read |
| `src/lua/www` | 312 | 0 | 0 | 279 acronym hints are its own `wb_*`/`lv_*` style, not a defect |
| `src/tools/wubugrep` | 9 | 0 | 1 | 0 |
| `src/kernel/WuBuKernel` | 11 | 0 | 0 | clean |
| `src/gui/vws`, `src/gui/wet` | 9, 11 | 0 | 0 | clean; vws now builds against `../wet` |
| `src/audio/bigmac` | 479 | 1 -> 0 | 0 | its own `wb_*` prefix is uniform |

Three defects found and fixed in the submodules themselves:

- **`src/brain/include/wubu_kv_runtime.c` was 0 bytes** -- a `.c` in a header directory, in no
  build file, no include, no test. It was the only 0-byte source file in the entire WuBuOS
  tree, so it was debris, not a placeholder being filled in. Deleted (brain `e94ecbc`).
- **`src/audio/bigmac/tools/wb_fm_g2.c` shared a name with the build module** and was not the
  module: 305 lines with its own `main()` and printf timing output, i.e. a standalone
  benchmark. Renamed `wb_fm_g2_bench.c`; the Makefile only ever built `src/wb_fm_g2.c`
  (bigmac `e7746cc`).
- The CMake `build-audit/` and `build-asan/` compiler-id files are build output that the walk
  was counting as sources.

Still open, measured and not guessed: `wubuoffice` (21 collisions / 11 off-prefix), `brain`
src/ (21 off-prefix: `lfm2_*`, `quantized_*`, `safetensors_*` against a 96.3% `wubu_`
dominant prefix in 562 files -- a real style split, and renaming it means renaming inside
another repo's model families, which needs a behavioural check first, not a sweep).

## Rename mechanics (learned by breaking it, 2026-11-25)

**Never sweep historical prose.** The 2026-11-25 renames also ran through `vault/` and
`os-studies/`, which record what the tree looked like AT THE TIME ("`dosgui_wm.c` -> new split"
"studied `dosgui_wm_desktop.c`"). The sweep rewrote those sentences into names that never
existed in that era -- and in one case into a path that has never existed at all
(`src/gui/wubu_desktop_icons.c`; the real file is `src/gui/shell/desktop_icons.c`). A study
document is EVIDENCE of a past state, so rewriting its paths falsifies the evidence. Those
edits were reverted; keep archive/study trees out of every path sweep, and sweep code +
build files + the docs that describe the CURRENT tree only.



A rename is `git mv` + path tokens + **the `.o` names in `mk/`** + build + the tests that
compile those units. Every omission below produced a real failure on this tree:

- **A sweep must be bounded to a LAYER, not a token.** `wubu_bonzi.h` existed in
  `src/gui`, `src/kernel` and `src/brain` (a submodule). A global replace crossed all
  three; `make hosted` still returned 0 because none of them are in the hosted build, so
  the damage was latent -- it would have surfaced as a broken kernel build much later.
- **A substring does not know where a name begins.** Replacing `bpm.` renamed the
  identifier `g_bpm.`; replacing `cp_sound.c` rewrote the recipe's
  `test_cp_sound.c` into `test_control_panel_sound.c` while the file kept its old name --
  one red from the build and one from the docs gate (the file no longer appeared in any
  target). Before a sweep, count the occurrences of `<name>.` that are NOT `.c/.h/.o`
  paths; if any exist, the sweep needs path-qualified patterns.
- **When a sweep touches `X.c`, look for `test_X.c`, `X_test.c` and anything ending in
  `X.c`** before calling the batch done. Finish the rename rather than reverting one side:
  a half-renamed pair is exactly what produces two reds.
- **File names and symbol names move in separate changes.** `holyc_eval.c` still exports
  `wubu_holyc_eval`; that is deliberate, not an oversight.
- **Verify by building and running, never by grep.** Each of the failures above passed
  grep and only the compiler or the gate caught it.

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
