# 🎯 WuBuOS Session — Next Kickoff (v24 — 2026-07-08)

**Baseline:** 747+ tests GREEN / 64+ targets. `GATE_EXIT=0` across all tiers.

## What was DONE v23→v24 (2 commits)
```
0a4b442  feat(jit): wubu_x86.c returns real byte counts (33 stubs closed)
01f0eec  feat(pkgmgr): 4 TODO gaps closed — resolve_deps + clean_cache +
         autoremove + verify_installed
```

- **JIT x86-64 Encoder** (`wubu_x86.c`): 33 `return 0;` stubs replaced with real byte-count return values. Every `wx86_*()` now returns bytes emitted (form==function). Validated by new 14-instruction test proving correct lengths for mov64/mov32/mov/add/ret/jmp/jcc/call/push/shl/cqo/neg/idiv/sub_rsp.

- **Package Manager** (`wubu_pkgmgr.c`): 4 TODO/void-cast functions eliminated:
  - `resolve_deps` — Kahn's topological sort via SQLite (transitive closure, cycle detection)
  - `clean_cache` — directory scan + mtime comparison + unlink
  - `autoremove` — SQL orphan detection (auto_installed=1 ∧ no reverse dep)
  - `verify_installed` — SHA256 file checksum verification

**Coincidental fix:** duplicate `int count = 0;` in `wubu_pkgmgr_get_stats()` found & fixed.

**Commits since groundwork:** 2. 11 files, 301+88 lines changed. All tier gates green.

## Standing orders (immutable)
- C11 only · opaque structs · minimal includes · no god headers · every module self-contained
- Every edited function does real work or is marked TODO. No stubs / scaffolding / "for later".
- "Rewriting from scratch in C" = the point of the project → anything under that = REAL_GAP.
- Stop not allowed. Blocked → alternate paths. Tests must pass after changes.

## Scoreboard
- **~363 sprint REAL_GAPs remaining** (was ~400). Triple DA, form≠function filtered.
- **0 remaining TODO/FIXME comments** in the entire `src/` tree.
- **5 parity epics** (SteamOS / Ubuntu-Arch / TempleOS / ZealOS / ReactOS) — marathons, not sprint.

## HIGHEST PRIORITY — next gap to close

The low-hanging form≠function fruit is gone. Remaining gaps are in large files with partial implementations. Pick one:

### Option A 🔥: VSL syscall void-cast files
Each has 40-58 `(void)d; (void)e; (void)f;` on the 4th-6th register params. These are syscalls with <6 real params in the 6-register convention. Each void cast represents a real syscall that should be doing something with those params or documenting why they're unused. 3 files, ~140 total:
- `vsl_syscall_net.c` — 58 void casts (socket/ns/security syscalls)
- `vsl_syscall_fileio.c` — 46 void casts
- `vsl_syscall_proc.c` — 37 void casts

### Option B: Monolith split of ≥800-line files
Top candidates: `wubu_metal.c` (1508), `hosted.c` (1320), `interrupt.c` (1153), `fat32.c` (1060), `wubu_archd.c` (1055), `wubu_proton.c` (1053), `wubu_vulkan.c` (990), `wubu_canvas.c` (1325).

### Option C: Bear RL compute backend
`bear_cudnn.c` has ~12 empty `{}` CUDA wrappers with CPU-fallback already written (the file is 1141 lines, mostly real). Remaining gaps are edge cases in the fallback paths.

### Option D: Sprint board scattershot
Pick any file from BATTLESHIP.md's top-20 and close 5-10 gap points.

## Commands
```bash
cd /home/wubu/.hermes/profiles/mind-palace/home/myseed
make clean && make runtime && make hosted && make test
make test_pkgmgr   # 11/11
make test_jit      # 76/76
```

## Key docs
- `slate.md` — v24 active work surface
- `BATTLESHIP.md` — ~1562 REAL_GAP audit (v20)
- `.hermes/plans/2026-07-07_doc-media-roadmap-overhaul.md` — last session plan

## Skills
- `wubuos-test-suite` — gate discipline, gap-hunting, orthogonal-failure triage
- `wubuos-monolithic-split` — proven split pattern
- `wubuos-architecture` — build system, triple-place Makefile pattern