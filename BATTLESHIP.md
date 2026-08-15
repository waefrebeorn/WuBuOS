# WuBuOS BATTLESHIP — REAL_GAP Board (v23, 2026-08-15)

```
╔════════════════════════════════════════════════════════════════════════╗
║  W U B U O S   B A T T L E S H I P   (v23, 2026-08-15)               ║
║  2463 C files · 1006 H files · 472,955 LOC · 414 test targets       ║
╚════════════════════════════════════════════════════════════════════════╝
```

> **Honesty discipline:** every gap listed here is verified by reading the actual
> code, not by scanner handoff. Stale counts from prior sessions are noted and
> corrected.

---

## Repo facts (verified 2026-08-15)
- **2463 `.c` / 1006 `.h` / 472,955 LOC** (tracked `src/`).
- **414 `make test_*` targets** across all tiers.
- `make all` / `make hosted` exit 0.
- WuBu compliance: **`_GNU_SOURCE` eliminated** — replaced with `WUBU_HOSTED` guard macro and `wubu_gnu_compat.h` / `wubu_ftw.h` native definitions.

## Recent sessions (2026-08-15)
1. **Bug #9/#10 fix** — minic JIT multiplicative `rax_is_const` flag not cleared after non-XRA add/sub, causing `(x+1)*(x-1)` to return wrong result. Fixed in `jit_minic_expr.c`.
2. **Build fix wave** — restored missing test files, fixed link lines for `test_minic_cg`, `test_drivers`, `holyc`, `test_jit_regression`.
3. **`_GNU_SOURCE` elimination** — 215 source files converted to `WUBU_HOSTED`. Created `wubu_gnu_compat.h` (CPU_*, CLONE_NEW*) and `wubu_ftw.h` (FTW_*, DT_*). Build uses `-D_POSIX_C_SOURCE=200809L` exclusively.
4. **Repository reorganization** — removed 1300+ stale build artifacts, organized `tools/` into subdirs (`bench/`, `dev/`, `isa-test/`, `research/`), moved `research/` → `docs/research/7hop-drivers/`, moved `docs/archive/` → `vault/`.

---

## PART 1 — CODE-LEVEL REAL_GAPs

### A. Live `system()` calls in production code

| # | File:line | Context | Status |
|---|-----------|---------|--------|
| 1 | `src/kernel/wubu_drv_install.c:238` | `system(mkdir_cmd)` — driver install | legitimate (install script) |
| 2 | `src/kernel/wubu_drv_install.c:264` | `system(clone_cmd)` — git clone | legitimate (install script) |
| 3 | `src/kernel/wubu_drv_install.c:524` | `system(cmd)` — modprobe | legitimate (install script) |
| 4 | `src/kernel/wubu_drv_install.c:570` | `system(cmd)` — driver load | legitimate (install script) |

> Remaining `system()` calls in `src/` are exclusively in test/demo files (`demo_capture.c`, `test_*.c`, `jit_supremacy_test.c`, `jit_torture_test.c`). These are test harnesses, not production code — they compile only for the test target and don't ship.

### B. Stub-phrase functions (verified by reading)

| # | File | Function | Status |
|---|------|----------|--------|
| 1 | `src/kernel/tasking.c` | `task_preempt_enable/disable` — arch preemption ctrl | open |
| 2 | `src/runtime/wubu_holyd_session.c` | `s->compiler = NULL` placeholder | open |
| 3 | `src/bear/bear_cudnn.c` | cuBLAS handle stubs (non-CUDA path) | open |
| 4 | `src/runtime/wubu_pkgmgr.c` | `repo_update` TODO fetch remote | open |
| 5 | `src/compiler/holyc_ptx.c` | PTX matmul TODO comment | open |
| 6 | `src/gui/wubu_compositor_standalone.c` | draw-quad TODO + wl_shm/xdg TODO | open |
| 7 | `src/gui/wubu_compositor.c` | 9P server thread TODO | open |
| 8 | `src/runtime/wubu_bottles.c` | winetricks TODO | open |

> **Code-level total: ~8 open gaps** (down from ~40 in v22). Most historical stubs were closed in prior sessions.

---

## PART 2 — PARITY EPICS (marathons)

### EPIC E1 — ReactOS NT Emulation (297 syscalls)
- **Status:** 20+ NT syscall module files (`vsl_nt_*.c`), 29 dispatch registrations,
  ~610 handler functions (many are stubs). Honest count of fully-implemented
  syscalls is difficult — the dispatch table has 29 entries, the bridge header
  defines many more ordinal slots. This is a marathon, not a sprint.

### EPIC E2 — SteamOS Parity (~30 subsystems)
| Subsystem | Status |
|-----------|--------|
| Steam Input | ✅ IMPLEMENTED (wubu_steaminput, 7/7 tests) |
| Pressure Vessel | ✅ IMPLEMENTED (wubu_pressure_vessel, 5/5 tests) |
| Game mode | ✅ IMPLEMENTED (wubu_gamemode, 4/4 tests) |
| EC control | ✅ IMPLEMENTED (wubu_ec_control, 5/5 tests) |
| Sniper runtime | ✅ IMPLEMENTED (wubu_steamrt, 5/5 tests) |
| CEF/Chromium UI shell | not implemented |
| Steam Networking (P2P) | not implemented |
| gamescope | not implemented |
| Steam Cloud sync | not implemented |

### EPIC E3 — Ubuntu/Arch Parity (~20 subsystems)
- Arch daemon: 16/16 tests (package backend, not service manager)
- NetworkManager, Polkit, D-Bus, PipeWire, CUPS, AppArmor: not implemented

### EPIC E4 — TempleOS Parity (~15 subsystems)
- HolyC JIT: 11 ISA backends (x86-64, ARM64, RISC-V, MIPS, 68k, 8086, 6502, Z80, 8051, AVR, PTX)
- Doc/DolDoc, RedSea FS, Ring-0 direct: not implemented
- Hardware driver registry: ✅ IMPLEMENTED (wubu_drv + 9 driver classes)

### EPIC E5 — ZealOS Parity (~8 subsystems)
- Identity-mapped memory, VGA direct, PC speaker: not implemented

> **Parity total: ~200+ marathon work items across all epics.**

---

## PART 3 — WUBU COMPLIANCE

The user's directive: **WuBu compliance means WE define the feature surface, not glibc's `_GNU_SOURCE` macro.**

| Item | Status |
|------|--------|
| `_GNU_SOURCE` in source files | ✅ 0 occurrences |
| `wubu_gnu_compat.h` (CPU_*, CLONE_NEW*) | ✅ force-included in all test builds |
| `wubu_ftw.h` (FTW_*, DT_*) | ✅ included post-system-header where needed |
| `WUBU_HOSTED` guard macro | ✅ replaces `_GNU_SOURCE` in 215 files |
| Build flags | ✅ `-D_POSIX_C_SOURCE=200809L` exclusively |
| Third-party code (`reference/`, `os-studies/`) | exempt (not WuBu-owned) |

---

## PART 4 — REPOSITORY STRUCTURE (current)

```
src/
  kernel/    ZealOS kernel (memory, tasking, VBE, FAT32, AHCI, interrupt, drivers)
  firmware/  WuBuFW UEFI (TPM, secureboot, chainloader)
  compiler/  WuBuNOS HolyC compiler (11 ISA backends, MIR optimizer)
  jit/       x86-64 encoder, regalloc, minic expression compiler
  runtime/   Styx/9P, VSL, containers, network, DOS emulator, archd, holyd
  gui/       Win98 WM, desktop, startmenu, explorer, terminal, theme
  apps/      Editor, canvas, codec, calc, notepad, cmd, music, todo, notes
  audio/     DAW, Furnace tracker, TinySoundFont, AI plugins
  bear/      RL training (PPO, Vulkan/CUDA compute)
  bridge/    Syscall bridge, DOS flip
  worldsim/  GAAD world state, physics, terrain
  hosted/    DRM/KMS, Vulkan, Metal, main entry
  shell/     Unified GUI shell
tools/
  bench/     Performance benchmarks (dram_hedge, regalloc, supremacy)
  dev/       Dev tooling (scanners, generators, linters)
  isa-test/  ISA driver tests (6502, AVR, MIPS, MIR)
  research/  Recursive self-improvement, probe scripts
  probe/     Hardware probe demos
docs/
  research/  7-hop Kevin-Bacon driver convergence docs
  adr/       Architecture Decision Records
  compendium/ Institutional ledger (philosophy, reference, architecture, roadmap)
vault/       Accomplishments, planning docs, archives
```

---

## REPRODUCIBLE NUMBERS (run these yourself)
```bash
cd /home/wubu/wubuos
find src/ -name '*.c' | wc -l          # 2463
find src/ -name '*.h' | wc -l          # 1006
find src/ -name '*.c' -o -name '*.h' | xargs wc -l | tail -1  # 472,955
grep -c '^test_' mk/tests.mk           # 414
grep -rl "_GNU_SOURCE" src/ --include="*.c" --include="*.h" | wc -l  # 0
make test_high_bear 2>&1 | grep RESULT  # RESULT: PASS
```
