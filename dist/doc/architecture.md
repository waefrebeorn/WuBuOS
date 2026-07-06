# WuBuOS — Architecture & Roadmap

> **LOCKED** — This document reflects ground truth from the full stub+form gap hunt and name parity audit.

## Vision

WuBuOS is NOT the kernel. It is a **GUI shell + container runtime** that wraps the ZealOS kernel, following the Inferno OS `emu` pattern:

- **ZealOS IS the kernel** — ring-0, single-user, HolyC JIT, boots on real hardware (154K LOC)
- **WuBuOS IS the shell** — Win98 desktop, Styx/9P namespace, .wubu container execution
- **The hosted binary IS the product** — `wubu` runs on Linux/Windows/macOS as a regular executable
- **Triple-platform** — Linux (native DRM/KMS), Windows (WSL2), macOS (Apple Virtualization)
- **Arch rip** — Arch Linux base for containers, ripping through Linux drivers for SteamOS/Proton

## Name Parity

`src/kernel/zealos_parity.h` provides 1:1 name mapping from ZealOS PascalCase → WuBuOS snake_case.

Current: **64/96 core functions mapped (64%)**. Target: 100%.

Roadmap: extract ZealOS function names from `grep -rn 'U0 \|I64 \|Bool ' ZealOS/src/Kernel/*.ZC`, compute diff against our names, add aliases.

## Roadmap — Tiered by ROI

### TIER 1: CRITICAL — Fill the Hollow Citadel (highest ROI)

| Component | What | Why |
|-----------|------|-----|
| input.c | Real keyboard/mouse queue + event dispatch | Unblocks GUI input |
| tasking.c | Real timer tick + context switch | Unblocks bare-metal |
| holyc_codegen | Function calls, struct layout, string literals | Unblocks REPL |
| wubu_math.h | Pure C math implementation | Unblocks full C self-containment |

### TIER 2: CRITICAL — Delete Dead Code / Replace Third-Party

| Component | What | Why |
|-----------|------|-----|
| Display | X11 → DRM/KMS (wubu_display.c written) | Zero X11 dep on Linux |
| Math | libm → pure C math (wubu_math.h) | Zero libm dep |
| Naming | NanoShellOS naming → WuBuOS naming in wm_nano/* | Naming consistency |
| DRM | libdrm → direct ioctl | Zero libdrm dep |
| GBM | libgbm → custom GBM | Zero libgbm dep |
| JIT | MIR c2m → self-contained JIT | Zero MIR subprocess dep |

### TIER 3: HIGH — Container Polish

| Component | What | Why |
|-----------|------|-----|
| 9P Dispatch | Per-container 9P Styx dispatch | Socket exists, no walk/read |
| cgroups | cgroup/setrlimit enforcement | Config stored but never applied |

### TIER 4: HIGH — App Wiring

| Component | What | Why |
|-----------|------|-----|
| REPL | Text rendering (bitmap font) | Black rect only currently |
| Notepad | Real implementation | Pure stub |

### TIER 5: MEDIUM — Integration

| Component | What | Dependencies |
|-----------|------|--------------|
| HolyC REPL | Compiles + executes in-process | Depends on codegen |
| GUI Events | Dispatches events to ZealOS apps | Depends on input, tasking |
| 9P Namespace | Per-container 9P namespace wired | Depends on 9P dispatch |
| SteamOS Container | SteamOS container launches | Depends on app wiring, DRM/KMS |
| Bare-metal Boot | | Depends on REPL, GUI events |
| Integration Test | | Depends on REPL, GUI events |

### TIER 6: LOW — Distribution

| Component | What | Status |
|-----------|------|--------|
| WSL2 Distribution | WuBuOS as WSL2 distribution | Scripts written, needs testing |
| Arch Rootfs Builder | | Scripts written, needs testing |
| macOS .app Bundle | wubu_macos.m written, needs Mac testing | |

## The Real Gaps (verified against source code)

See `docs/risk_register.md` for the full cell-by-cell audit.

Key metrics:
- 134 C/H files total
- ~28K source LOC + 7.7K test LOC
- 511+ tests passing
- 171 (void) suppression casts across the codebase
- 42 TODO/FIXTURE markers in source
- 6 test suites with hollow stubs (VSL, Proton, exec_elf, exec_pe, vsl_init, vsl_run)
- Name parity: 64/96 (64%)
- Third-party deps remaining: libX11 (hosted.c), libm (WorldSim/tests), libdrm (wubu_display.c), libgbm (wubu_display.c)

## Platform Paths

### Linux (primary development)
- `make hosted` → `./src/hosted/wubu` runs as X11 app
- DRM/KMS via wubu_display.c for production
- Container GPU passthrough via bind mounts (working)

### Windows (WSL2)
- `wsl --install WuBuOS` → imports our Arch rootfs
- GPU via /dev/dxg (Mesa d3d12)
- Same wubu binary, same containers

### macOS (Apple Virtualization)
- `build-macos.sh` → cross-compiles aarch64 binary
- `wubu_macos.m` → ObjC launcher via Virtualization.framework
- GPU via VirtIO GPU
- Same wubu binary, same containers