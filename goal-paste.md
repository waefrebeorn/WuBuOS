# 🎯 WuBuOS Session — Next Kickoff (v23 — 2026-07-08)

**Baseline:** 747+ tests GREEN / 64+ targets. `GATE_EXIT=0` across all tiers.

## What was DONE this session
- **JIT x86-64 Encoder (wubu_x86.c)**: All 33 `return 0;` stubs replaced with real byte-count return values. Every `wx86_*()` encoder function now reports the number of bytes emitted (form==function). New test `"encoder: return values match bytes emitted"` proves 14 instruction types return correct lengths. (The return-0 stubs were a pure form≠function gap — the encoding worked but the API contract (int return = byte count) was lying.)
- **UX Stream E bundled wallpaper**: Full wallpaper decode+load chain in `dosgui_wm.c` → `wubu_wallpaper_default_path()`, committed in prior session as `28e700b` (feat(ux)).
- **Desktop Stream 3 context menu** (`dosgui_wm_ctxmenu.c`): already implemented from prior session (`033554f`). `ctx_action_sort_by_name()`, `ctx_action_create_shortcut()`, `ctx_action_view_desktop()`, `ctx_action_refresh()` — all real C, not stubs.
- **Documentation fixes**: wallpaper test bench + `run_high_gui` target patch in Makefile.

## Standing orders (immutable)
- C11 only · opaque structs · minimal includes · **no god headers** · every module self-contained
- Every edited function does real work or is marked TODO. No stubs / scaffolding / "for later".
- **"Rewriting from scratch in C" = the point of the project**
- Stop not allowed. Blocked → alternate paths. Tests must pass after changes.

## Scoreboard
- **~367 sprint REAL_GAPs remaining** (form≠function, triple-DA filtered). The JIT encoder was ~33 gaps — now 0 in that file.
- **Top remaining gap files** (actual empty-body / return-0-only stubs, re-scanned):
  - `bear_cudnn.c` — 12 empty `{}` CUDA/cuBLAS wrappers (no GPU in env)
  - `vsl_syscall_net.c` — 58 void casts (unused syscall params)
  - `wubu_metal.c` — 16 return-0 + 15 void casts
  - `wubu_vulkan.c` — 12 return-0
  - `interrupt.c` — 17 return-0 + 22 void casts
  - `vsl_syscall_fileio.c` — 46 void casts
  - `vsl_syscall_proc.c` — 37 void casts

## HIGHEST PRIORITY
**Pick one gap file and close it.** The JIT encoder was a good model: small file (469 lines), mechanical fix (return byte count), with test coverage added. The next best targets are:

- **A 🔥**: `vsl_syscall_net.c` or `vsl_syscall_fileio.c` or `vsl_syscall_proc.c` — ~40-60 void casts each, unused register params in 6-register syscall convention. Each void-cast elimination is writing the actual syscall semantics.
- **B**: `bear_cudnn.c` — 12 empty CUDA wrappers. Each needs real cuBLAS/cuDNN call or `#error` + alternate CPU path.
- **C**: `wubu_metal.c` (70 gap points, 1508 lines) — monolithic split + stub closure.
- **D**: Prestige v23: update slate + vault old phases.

## Commands
```bash
cd /home/wubu/.hermes/profiles/mind-palace/home/myseed
make clean && make runtime && make hosted   # build gate
make test                                   # full gate (64+ targets)
```

## Key docs
- `screenshots/README.md` — media catalog
- `STATE.md` — triple DA phase table
- `slate.md` — v23 active work surface