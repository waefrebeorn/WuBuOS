# WuBuOS — Battleship v7 (Triple DA + Name Parity + Third-Party Audit)

**Methodology**: Behavioral verification + stub hunt + name parity audit + third-party dep scan
**Current state**: 34 C files, ~134 C/H files, ~28K source LOC, 511+ tests
**Name parity**: 64/64 core functions mapped in zealos_parity.h

## Resolved Cells

| Cell | Description | Evidence |
|------|-------------|----------|
| 200 | ZealOS kernel in-process + Win98 GUI shell | hosted.c:194 init, hosted_test.c 14 behavioral |
| 203 | Fork+exec for .wubu containers | wubu_host_exec.c:212 fork+chroot+execv+mount, test 15 behavioral |
| 310 | HolyC codegen: ternary, AND, OR, IF, WHILE, FOR | holyc_codegen.c label backpatching, 71/71 eval tests |
| 380 | wubu_display.c — DRM/KMS + X11 dual backend | wubu_display.c: probe+drm_init+evdev |
| 383 | Container bind mounts applied | wubu_host_exec.c:231 mount(MS_BIND) loop |
| 390 | macOS Apple Virtualization launcher | wubu_macos.m: VZVirtualMachine + VZLinuxBootLoader |

## Name Parity Status

| Subsystem | ZealOS funcs | WuBuOS funcs | Parity | Missing |
|-----------|-------------|--------------|--------|---------|
| Memory | 17 | 13 | 76% | 4 (BlkPool*, Mem32Dev*, Mem64Dev*, ReAlloc) |
| Task | 21 | 12 | 57% | 9 (FocusNext, FocusPrev, Validate, KillAll, Idle, SetPriority, GetPriority, CtxSwitch_Save, CtxSwitch_Restore, DerivedValsUpdate, DeathWait) |
| FAT32 | 18 | 10 | 56% | 8 (FileWritePtr, FileTruncate, AllocClus, AllocContigClus, FreeAllClus, FileFindFreeDir, CDate2Dos, Dos2CDate) |
| VBE | 4 | 2 | 50% | 2 (RawPutS, RawPutCharAttr) |
| Input | 8 | 2 | 25% | 6 (QueuePriKey, QueueMouse, GetPriKey, GetMouse, SetMouseBounds, MouseSpeed) |
| Interrupt | 6 | 2 | 33% | 4 (UnRegister, Disable, Enable, Ack) |
| Styx/9P | 14 msg types | 14 msg types | 100% | 0 |
| JIT | 8 | 6 | 75% | 2 (DisAsm, MIRCompile) |
| **TOTAL** | **96** | **61** | **64%** | **35** |

## Third-Party Dependencies → C Replacement Roadmap

| Dependency | Used By | C Replacement | Cell | Priority |
|------------|---------|---------------|------|----------|
| libX11 (-lX11) | hosted.c | DRM/KMS (wubu_display.c) | 380 | ✅ Done |
| libm (-lm) | WorldSim, VBE tests | Pure C (wubu_math.h) | 381 | 🟡 Medium |
| libdrm | wubu_display.c | Direct ioctl (no libdrm) | 388 | ⬜ Low |
| libgbm | wubu_display.c | Custom GBM (wubu_gbm.h) | 389 | ⬜ Low |
| libxcb | libX11 dep | Remove when X11 removed | — | ⬜ Low |
| libbsd | libX11 dep | Remove when X11 removed | — | ⬜ Low |
| libmd | libX11 dep | Remove when X11 removed | — | ⬜ Low |
| MIR (c2m) | jit_mir.c | Self-contained JIT (no c2m subprocess) | 391 | 🟡 Medium |
| NanoShellOS naming | wm_nano/* | Rename to WuBuOS naming | 382 | ⬜ Low |

## Active Gap Cells (v7)

### Layer 1: Kernel — Hollow Stubs

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 300 | input.c: 52 lines, only init/shutdown, no event dispatch | 🔴 | input.c:13-52 |
| 301 | interrupt.c: 42 lines, only init/shutdown/register, no IDT | 🔴 | interrupt.c:16-42 |
| 303 | tasking.c: no timer tick, no preemption, cooperative only | 🔴 | tasking.c:11-240 |
| 304 | fat32.c: dir entry update on close is TODO | 🟡 | fat32.c:845 |
| 305 | Name parity: 35 ZealOS functions still unmapped | 🟡 | zealos_parity.h |
| 306 | wubu_math.h: header only, no implementation | 🟡 | kernel/wubu_math.h |

### Layer 2: Compiler — Codegen Gaps

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 311 | holyc_codegen: no function calls with args, no struct layout, no string literals | 🔴 | holyc_codegen.c:527 |
| 312 | holyc_codegen: break/continue emit placeholders (no jump target) | 🟡 | holyc_codegen.c:755 |
| 313 | holyc_codegen: assignment stores to stack slot (TODO) | 🟡 | holyc_codegen.c:494 |

### Layer 3: VSL — PARTIAL (bare-metal scaffolding)

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 324 | VSL: 712 lines, 54 (void) casts, syscall stubs — PARTIAL | 🟡 PARTIAL | wubu_vsl.c entire |

### Layer 4: Proton — PARTIAL

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 330 | Proton: 406 lines, 0 PE load ops — host Wine delegation | 🟡 PARTIAL | wubu_proton.c |

### Layer 5: wubu_exec — Dispatches To Air

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 340 | exec_linux_elf: validates magic but delegates to VSL stub | 🔴 | wubu_exec.c:124 |
| 341 | exec_win_pe: (void) suppresses all params | 🔴 | wubu_exec.c:130 |
| 343 | vsl_init: just sets a bool | 🟡 | wubu_exec.c:76 |
| 344 | vsl_run: returns 0 unconditionally | 🟡 | wubu_exec.c:103 |

### Layer 6: Container Gaps

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 350 | Per-container 9P Styx dispatch (socket exists, no walk/read) | 🟡 | wubu_host_exec.c:130 |
| 353 | cgroup/setrlimit enforcement (stored but never applied) | 🟡 | wubu_host_exec.c:211 |

### Layer 7: Third-Party → C Replacements

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 381 | libm → pure C math (wubu_math.h written, needs impl) | 🟡 | kernel/wubu_math.h |
| 382 | NanoShellOS naming → WuBuOS naming in wm_nano/* | ⬜ | gui/wm_nano/* |
| 388 | libdrm → direct ioctl | ⬜ | wubu_display.c |
| 389 | libgbm → custom GBM | ⬜ | wubu_display.c |
| 391 | MIR c2m → self-contained JIT | 🟡 | jit_mir.c |

### Layer 8: App Wiring

| Cell | Description | Severity | Source |
|------|-------------|----------|--------|
| 361 | REPL: renders black rect, no text output | 🟡 | repl.c:30 |
| 362 | Notepad: stub (compiles but no real implementation) | 🔴 | apps/notepad.c |

## Summary

| Category | Count |
|----------|-------|
| 🔴 CRITICAL (REAL GAP) | 8 |
| 🟡 HIGH (PARTIAL / NAMING / THIRD-PARTY) | 12 |
| ⬜ LOW | 4 |
| ✅ RESOLVED | 6 |
| **TOTAL ACTIVE** | **24** |

## Priority Order

1. **Cell 300** — input.c event queue (unblocks 202)
2. **Cell 303** — tasking timer tick + preemption (unblocks 206)
3. **Cell 311** — codegen function calls/structs (unblocks 201)
4. **Cell 381** — pure C math (wubu_math.h impl)
5. **Cell 340/341** — route exec through wubu_host_exec
6. **Cell 305** — name parity: map remaining 35 functions
7. **Cell 388/389** — libdrm/libgbm → direct ioctl
8. **Cell 391** — MIR → self-contained JIT
