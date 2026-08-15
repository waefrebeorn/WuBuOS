# Testing

Hosted test suite for WuBuOS. All tests compile as user-space binaries with
WuBu compliance flags and run on the host — no hardware boot required.

## Build flags

Every test target uses the WuBu compliance toolchain:

```
-D_POSIX_C_SOURCE=200809L    # POSIX 2008 baseline (no _GNU_SOURCE)
-DWUBU_HOSTED                 # hosted runtime personality
-include wubu_gnu_compat.h    # portable shim (replaces _GNU_SOURCE deps)
```

These three flags let kernel sources compile cleanly on any C11 host without
pulling in GNU extensions. Recent work eliminated all `_GNU_SOURCE` dependency
from the repo — `wubu_gnu_compat.h` now supplies the handful of POSIX-level
shims needed.

## Layout

```
mk/tests.mk            ← 414 test targets, organized into tiers
src/kernel/test/       ← per-module selftests (wubu_<mod>_selftest.c)
src/kernel/wubu_test_stubs.c  ← test-only stubs for excluded symbols
build/testobj/         ← cached kernel .o (built once, reused per test)
```

## Running tests

```bash
# One specific test:
make test_jit
make test_fat32
make test_hw_audio

# A tier (groups of related tests):
make test_critical_runtime    # 8 targets  (runtime core)
make test_critical_kernel     # 9 targets  (kernel/metal)
make test_high_bridge         # 3 targets  (syscall bridge)
make test_high_gui            # 18 targets (hosted GUI)
make test_high_bear           # 26 targets (Bear RL/JIT/compiler)
make test_medium_other        # 37 targets (apps/audio/tools/other)

# Hardware driver family (216 test_hw_* targets):
make test_all

# Full suite — all tiers sequentially:
make test
```

## Tier breakdown

`mk/tests.mk` defines 414 test targets organized into tiers:

### test_critical_runtime (8 targets)
Runtime core: containers, network, OCI, snapshots, VSL, HolyD, HolyC AGI, Proton, spawn.

```
test_network  test_snapshot  test_vsl  test_holyd  test_holyc_agi
test_proton   test_proton2   test_spawn
```
Prerequisite: `runtime`

### test_critical_kernel (9 targets)
Kernel/metal: filesystems, storage, display, decompressors.

```
test_fat32  test_txfs  test_ahci  test_drm_direct
test_zlib   test_zip   test_lzx   test_cab  test_dram_hedge
```

### test_high_bridge (3 targets)
Syscall bridge and DOS flip layer.

```
test_bridge  test_bridge_flip  test_syscall
```
Prerequisite: `runtime`

### test_high_gui (18 targets)
Hosted GUI: window manager, desktop, startmenu, explorer, terminal,
clipboard, compositor, shell, wallpaper, control panel, calculator.

```
test_synth  test_wubu_sound  test_dosgui_cp_sound  test_hwdetect
test_colonel  test_dosgui_wm  test_dosgui_ui  test_dosgui_dos_window
test_dosgui_startmenu  test_dosgui_explorer  test_dosgui_term
test_clipboard  test_screenshot  test_compositor  test_shell
test_wallpaper  test_control  test_calc
```
Prerequisites: `gui`, `runtime`

### test_high_bear (26 targets)
Bear RL / JIT / compiler: codegen, register allocation, loop analysis,
branch profiling, PGO, ARM64 encoder, minic, memory, tasking, HolyC, PTX.

```
test_jit  test_jit_regalloc  test_jit_remat  test_jit_branch
test_jit_type  test_jit_loop  test_jit_branch_profile
test_jit_subsystem_integration  test_jit_pgo_byte  test_jit_loop_consume
test_jit_deep_opt  test_jit_fuzzer  test_jit_supremacy  test_jit_torture
test_arm64_enc  test_minic_cg  test_jit_regression  test_peephole_superopt
test_memory  test_tasking  test_input  test_holyc  test_hedge
test_holyc_ptx  test_battery
```

### test_medium_other (37 targets)
Apps, audio, tools, WorldSim, namespace, deployment, math, package manager.

```
test_worldsim  test_audio  test_apps  test_apps2  test_wubu
test_host_exec  test_gaad  test_iso  test_weights  test_gc
test_txfs  test_dbuf  test_styx  test_styxfs  test_anticheat
test_bottles  test_ns_bridge  test_ns_snap  test_ns_pkg  test_ns_kernel
test_ns_9p  test_ns_dram  test_deploy  test_math  test_pkgmgr
test_gamelib  test_mime  test_trash  test_cap  test_txn
test_cmd  test_dos_emu_smoke  test_manifest  test_bytropix_verifier
```
Prerequisites: `runtime`, `gui`

### test_hw_* (216 targets)
Per-hardware-driver selftests. Each `test_hw_<driver>` compiles the
selftest + driver source + `wubu_test_stubs.c` against cached kernel objects.

Examples: `test_hw_audio`, `test_hw_nvidia_turing`, `test_hw_wifi7`,
`test_hw_xhci`, `test_hw_bt`, `test_hw_bcache`, `test_hw_nvme_gen5`,
`test_hw_usb4`, `test_hw_vulkan14`.

### Additional targets

```
test_vsl_cpm         # VSL CP/M syscall personality
test_vsl_macclassic  # VSL Mac Classic syscall personality
test_all             # All 216 test_hw_* targets
test                 # Full suite: all tiers + VSL personalities
```

## How it works

1. **Cached kernel objects** — `mk/tests.mk` precompiles verified kernel
   modules into `build/testobj/*.o` once. Only changed sources recompile.

2. **Per-test link** — each `test_hw_<mod>` target compiles the selftest,
   the module under test, and `wubu_test_stubs.c`, then links against the
   cached objects. The module's own `.o` is excluded from the cache to avoid
   double-definition.

3. **Tier grouping** — phony targets (`test_critical_runtime`, etc.) depend
   on their member tests, so `make test_critical_runtime` runs exactly that
   tier. The top-level `test` target runs all tiers sequentially.

## Stub routing

`src/kernel/wubu_test_stubs.c` provides test-only symbols (e.g.
`wubu_rtc_driver_for`, `wubu_hid_driver_for`) that route a vendor/chip string
to its driver name. These are **stubs** for test determinism — the real
routing happens in the kernel's hw_detect layer. If a test does a routing
CHECK you must extend the corresponding stub to mirror the real logic.