# WuBuOS — Architecture & Roadmap (v7 — Post Triple DA + Name Parity)

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

## Roadmap

### Phase A: Fill the Hollow Citadel (highest ROI)

| Cell | What | Why |
|------|------|-----|
| 300 | input.c: real keyboard/mouse queue + event dispatch | Unblocks Cell 202 (GUI input) |
| 303 | tasking.c: real timer tick + context switch | Unblocks Cell 206 (bare-metal) |
| 311 | holyc_codegen: function calls, struct layout, string literals | Unblocks Cell 201 (REPL) |
| 381 | libm → pure C math (wubu_math.h implementation) | Unblocks full C self-containment |

### Phase B: Delete Dead Code / Replace Third-Party

| Cell | What | Why |
|------|------|-----|
| 380 | X11 → DRM/KMS (wubu_display.c written) | Zero X11 dep on Linux |
| 381 | libm → pure C math (wubu_math.h) | Zero libm dep |
| 382 | NanoShellOS naming → WuBuOS naming in wm_nano/* | Naming consistency |
| 388 | libdrm → direct ioctl | Zero libdrm dep |
| 389 | libgbm → custom GBM | Zero libgbm dep |
| 391 | MIR c2m → self-contained JIT | Zero MIR subprocess dep |

### Phase C: Container Polish

| Cell | What | Why |
|------|------|-----|
| 350 | Per-container 9P Styx dispatch | Socket exists, no walk/read |
| 353 | cgroup/setrlimit enforcement | Config stored but never applied |

### Phase D: App Wiring

| Cell | What | Why |
|------|------|-----|
| 361 | REPL: text rendering (bitmap font) | Black rect only currently |
| 362 | Notepad: real implementation | Pure stub |

### Phase E: Integration

| Cell | What | Why |
|------|------|-----|
| 201 | HolyC REPL compiles + executes in-process | Depends on 311 |
| 202 | GUI dispatches events to ZealOS apps | Depends on 300, 303 |
| 204 | Per-container 9P namespace wired | Depends on 350 |
| 205 | SteamOS container launches | Depends on D, 380 |
| 206 | Bare-metal boot | Depends on 201, 202 |
| 207 | Integration test | Depends on 201, 202 |

### Phase F: Distribution

| Cell | What | Why |
|------|------|-----|
| 384 | WuBuOS as WSL2 distribution | Scripts written, needs testing |
| 386 | Arch rootfs builder | Scripts written, needs testing |
| 390 | macOS .app bundle | wubu_macos.m written, needs Mac testing |

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
