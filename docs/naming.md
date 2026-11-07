# WuBu Naming & Organization Map

Purpose: one file that answers "what prefix means what," so a new module isn't
another `zx`. Verified against the working tree 2026-11-06.

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

## Stale names to kill (small, safe renames)

- `wubd_holyc_agi_test`, `wubd_holyd_test` in src/runtime — a leftover "holyc"
  name violates the ONLY-HOLYD rule. Rename to `wubd_holyd_*`.
- `holyd_bf_stub.c` — unexplained; either drop the "+ stub" framing or document
  what it actually is.
