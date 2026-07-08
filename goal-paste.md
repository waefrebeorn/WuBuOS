# 🎯 WuBuOS Session — Next Kickoff (v25 — 2026-07-08)

**Baseline:** 747+ tests GREEN / 64+ targets. `GATE_EXIT=0` across all tiers.

## What was DONE v24→v25 (1 commit)
```
52715a3  feat(bear,libc,anticheat): 5 REAL_GAPs closed — bear checkpoints
         + vsprintf + proton config
```

- **Bear RL Checkpoints** (`bear_nn.c`): `bear_checkpoint_save/load` — binary serialization format (magic 'WUBR', version header, layer weights+biases, GRU 9-param block, logstd). Validates struct dims on load for round-trip safety. ~220 LOC added.

- **Bear Trainer** (`bear_ppo.c`): `bear_trainer_save/load` — scalar state (iteration, best_return, total_steps, PPOConfig) + sidecar policy checkpoint (`.policy` file). ~85 LOC added.

- **freestanding libc** (`libc.c`): `vsprintf` — real printf-style formatted output supporting %d/%i, %u, %x/%X, %s, %c, %p, %l/%ll, %0N width, %%. Downstream `sprintf` now formats real data instead of returning 0. ~157 LOC added.

- **Anticheat** (`wubu_anticheat.c`): `wubu_anticheat_proton_config` — stores custom config into mutable `recommended_configs[]` table, queryable by Proton/bottles downstream. ~18 LOC added.

**Total: 5 gaps closed.** ~464 lines added, 13 removed. Gate GREEN.

## Standing orders (immutable)
- C11 only · opaque structs · minimal includes · no god headers · every module self-contained
- Every edited function does real work or is marked TODO. No stubs / scaffolding / "for later".
- "Rewriting from scratch in C" = the point of the project → anything under that = REAL_GAP.
- Stop not allowed. Blocked → alternate paths. Tests must pass after changes.

## Scoreboard
- **~358 sprint REAL_GAPs remaining** (was ~363). Triple DA, form≠function filtered.
- **0 remaining TODO/FIXME comments** in the entire `src/` tree.
- **5 parity epics** (SteamOS / Ubuntu-Arch / TempleOS / ZealOS / ReactOS) — marathons, not sprint.
- **12 commits** since v22 ground. Gate always green.

## HIGHEST PRIORITY — next gap to close

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

### Option E 🆕: styxfs dir stubs
`styxfs_closedir` + `styxfs_readdir_r` — 2 remaining void-cast stubs in the 9P filesystem. Requires wiring real directory iteration (opendir handles exist, need closedir/readdir_r delegates).

## Commands
```bash
cd /home/wubu/.hermes/profiles/mind-palace/home/myseed
make clean && make runtime && make hosted && make test
```

## Key docs
- `slate.md` — v25 active work surface
- `BATTLESHIP.md` — ~1562 REAL_GAP audit (v20)
- `goal-paste.md` — this doc (v25)

## Skills
- `wubuos-test-suite` — gate discipline, gap-hunting, orthogonal-failure triage
- `wubuos-monolithic-split` — proven split pattern
- `wubuos-architecture` — build system, triple-place Makefile pattern, VSL void-cast conventions