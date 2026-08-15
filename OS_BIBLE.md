# WuBuOS Design Bible

**Version:** 2.0 (2026-08-15)
**Status:** Living Document
**Scale:** 2463 C files · 1006 H files · 472,955 LOC · 414 test targets

---

## Table of Contents

1. [Vision & Philosophy](#1-vision--philosophy)
2. [Architecture Overview](#2-architecture-overview)
3. [Kernel Layer (ZealOS-based)](#3-kernel-layer-zealos-based)
4. [WuBuNOS Compiler (11 ISA Backends)](#4-wubunos-compiler)
5. [Hosted Runtime (Inferno emu pattern)](#5-hosted-runtime)
6. [GUI Shell (Win98/XP Classic)](#6-gui-shell)
7. [Namespace & Styx/9P](#7-namespace--styx9p)
8. [Container Runtime](#8-container-runtime)
9. [Virtual Syscall Layer (VSL)](#9-virtual-syscall-layer)
10. [Bear RL](#10-bear-rl)
11. [WuBu Compliance](#11-wubu-compliance)
12. [Build System](#12-build-system)
13. [Testing Strategy](#13-testing-strategy)
14. [Repository Structure](#14-repository-structure)

---

## 1. Vision & Philosophy

### Core Mission
WuBuOS is the **BODY of the WuBu AGI** — a GUI shell + container runtime wrapping the ZealOS kernel. The Brain (`wubuwizard`) learns; the Body protects, hosts, and acts; **WuBuNOS** is the compiler that targets every ISA.

### Design Principles

| Principle | Description |
|-----------|-------------|
| **Single Binary** | One hosted binary (`src/hosted/wubu`) runs on Linux, WSL2, macOS |
| **Inferno emu Pattern** | Host OS abstraction layer; kernel runs in-process |
| **ZealOS IS the Kernel** | Ring-0, single-user, HolyC JIT, boots on metal |
| **WuBuOS IS the Shell** | Win98/XP desktop, Styx namespace, .wubu containers |
| **WuBu Compliance** | Own the feature surface — no `_GNU_SOURCE`, no glibc feature macros |
| **C11 Portability** | Maximum portability, no C++ dependencies, `-std=c11` |
| **Theme Engine** | Switchable themes (Ctrl+T): Win98, XP Luna, WuBu Green |
| **Honesty Over Inflation** | Real, verified LOC (472,955), not inflated; no fake features |

### Non-Goals
- Not a Linux distribution (we're a shell on top)
- Not a Windows compatibility layer (we host via Proton/Wine/VSL)
- Not a microkernel (ZealOS is monolithic)
- Not POSIX-compliant (WuBu compliance replaces POSIX feature macros)

---

## 2. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        USER SPACE                               │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                  wubu (hosted binary)                    │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐ │   │
│  │  │   GUI SHELL │ │  CONTAINER  │ │   HOLYC VM          │ │   │
│  │  │  (Win98/XP) │ │  RUNTIME    │ │   (JIT + 11 backends)│ │   │
│  │  ├─────────────┤ ├─────────────┤ ├─────────────────────┤ │   │
│  │  │ • Desktop   │ │ • Bubblewrap│ │ • Lexer/Parser       │ │   │
│  │  │ • WM        │ │ • Profiles  │ │ • C Transpiler       │ │   │
│  │  │ • StartMenu │ │ • GPU Pass  │ │ • JIT Compiler       │ │   │
│  │  │ • Taskbar   │ │ • DXVK/VKD3D│ │ • 11 ISA backends    │ │   │
│  │  │ • Theme Eng │ │ • Steam Lib │ │ • Syscall Bridge     │ │   │
│  │  └──────┬──────┘ └──────┬──────┘ └──────────┬──────────┘ │   │
│  │         │               │                    │            │   │
│  │         └───────────────┼────────────────────┘            │   │
│  │                         ▼                                 │   │
│  │              ┌─────────────────────────────────────┐      │   │
│  │              │    STYX/9P NAMESPACE                 │      │   │
│  │              │  /wubu /dev /prog /net /n            │      │   │
│  │              └───────────┬─────────────┘              │      │   │
│  │                          │                            │      │   │
│  └──────────────────────────┼────────────────────────────┘      │
└─────────────────────────────┼───────────────────────────────────┘
                              │
                    ┌─────────┴─────────┐
                    │   ZEALOS KERNEL   │
                    │  (in-process)     │
                    ├───────────────────┤
                    │ • VBE Framebuffer │
                    │ • Tasking/Sched   │
                    │ • Memory Manager  │
                    │ • Interrupt/ISR   │
                    │ • HolyC JIT       │
                    │ • Driver Registry │
                    └───────────────────┘
```

---

## 3. Kernel Layer (ZealOS-based)

### 3.1 Memory Manager (`src/kernel/memory.c`)
- Buddy allocator with red-zone canaries
- Page frame allocator (4K pages)
- Kernel heap with `mem_debug_dump()` introspection

### 3.2 Tasking/Scheduler (`src/kernel/tasking.c`)
- Round-robin with priority inheritance
- Process/thread model maps to ZealOS tasks

### 3.3 VBE Framebuffer (`src/kernel/vbe.c`)
- 64-glyph 8x16 font baked into binary
- Gradient, circle, shade, clip primitives
- Double-buffered SHM for Wayland surface

### 3.4 Interrupt/ISR (`src/kernel/interrupt.c`)
- LAPIC, IOAPIC, MSI/MSI-X, TSC deadline
- SYSCALL/SYSRET fast path
- PIC cascade legacy support

### 3.5 Filesystems
- **FAT32** (`src/kernel/fat32.c`): 25/25 tests, LFN support
- **TXFS** (`src/kernel/txfs.c`): WAL transactional FS, 25/25 tests
- **AHCI** (`src/kernel/ahci.c`): SATA with simulator, 16/16 tests

### 3.6 Driver Registry (`src/kernel/wubu_drv.c`)
- Linux-style device/driver model with ID tables
- NVMe, network, HDA, GPU, battery, SD, USB-class, thermal drivers
- Steam Deck + laptop PCI IDs

---

## 4. WuBuNOS Compiler

The compiler (`src/compiler/`) is WuBuOS's from-scratch C11 toolchain. Brand name: **WuBuNOS**.

### ISA Driver Space (11 backends)
| Backend | Type | Status |
|---------|------|--------|
| x86-64 | Native JIT | ✅ |
| ARM64 | Native JIT | ✅ |
| RISC-V | Interpreter | ✅ |
| MIPS | Interpreter | ✅ |
| 68k | Interpreter | ✅ |
| 8086 | Interpreter | ✅ |
| 6502 | Interpreter | ✅ |
| Z80 | Interpreter | ✅ |
| 8051 | Interpreter | ✅ |
| AVR | Interpreter | ✅ |
| PTX (NVIDIA) | Native JIT | ✅ |

### Optimizer (7 passes)
- Constant folding, strength reduction, DCE, LICM, loop unroll, combine, CSE
- Linear-scan SSA register allocator
- x86 peephole post-codegen pass

### Minic JIT (`src/jit/jit_minic*.c`)
- Expression compiler with XRA (extended register allocator)
- Supports arithmetic, bitwise, shift, compare, logical operators
- 68/68 regression tests

---

## 5. Hosted Runtime

### 5.1 Entry Point (`src/hosted/hosted.c`)
- Wayland client (registry, compositor, shell, SHM, seat)
- Headless mode for CI — no compositor needed
- DRM/KMS direct (atomic modesetting, plane composition)

### 5.2 Metal Abstraction (`src/hosted/wubu_metal.c`)
- Audio backends: ALSA, PulseAudio, PipeWire (dlopen)
- GAAD mode selection (Golden Angle Area Decomposition)

### 5.3 WuBuFW UEFI Firmware (`src/firmware/`)
- UEFI from scratch, no EDK2
- TPM measured boot → PCR4 + AuthentiCode
- Chainloader reads KERNEL.ELF → SHA-256 → attestation handoff

---

## 6. GUI Shell

### Window Manager (`src/gui/dosgui_wm.c`)
- Win98/XP chrome, GAAD snap, virtual desktops
- Theme engine: Win98 Classic, XP Luna Blue, WuBu Green
- Taskbar, startmenu, explorer, terminal

### Apps (20 registered)
- Editor, canvas, calculator, notepad, cmd, music, todo, notes
- Control panel with hardware/display/network/sound/theme applets
- Big Picture Mode (gamepad-first shell)

---

## 7. Namespace & Styx/9P

- 9P2000.L protocol in-kernel
- Mount points: `/wubu`, `/dev`, `/prog`, `/net`, `/n`
- The `/n` control plane: EC, SteamInput, world state, NT registry
- Container isolation via namespace chroot

---

## 8. Container Runtime

- **Bubblewrap** isolation with OCI registry support
- **Proton/Wine** integration for Windows games
- **Arch daemon** — pacman wrapper, AUR build/search, signing, hooks
- **Pressure Vessel** — Steam Linux Runtime container preset
- **DOS emulator** — in-process 8086 interpreter (22/22 tests)

---

## 9. Virtual Syscall Layer (VSL)

Multi-OS syscall dispatch from a single entry point:

| Personality | Syscalls | Status |
|-------------|----------|--------|
| Linux x86-64 ABI | ~50+ handlers | ✅ |
| Windows NT (ReactOS) | 148/297 transliterated | partial |
| macOS XNU | 52 BSD + 13 Mach + IPC | partial |

---

## 10. Bear RL

- PPO training with Vulkan/CUDA compute pipelines
- N-pole cartpole physics (TWO implementations)
- Muon optimizer, amoeba body mutation
- SafeTensors bridge for HF model loading

---

## 11. WuBu Compliance

The user's directive: **WuBu compliance means WE define the feature surface.**

| Item | Implementation |
|------|---------------|
| CPU affinity macros | `wubu_gnu_compat.h` → `CPU_ZERO`/`CPU_SET`/`CPU_ISSET`/`CPU_CLR`/`CPU_COUNT` |
| Clone namespace flags | `wubu_gnu_compat.h` → `CLONE_NEWNS`/`CLONE_NEWPID`/etc |
| nftw flags | `wubu_ftw.h` → `FTW_DEPTH`/`FTW_PHYS`/`FTW_DP` |
| dirent types | `wubu_ftw.h` → `DT_DIR`/`DT_REG`/etc |
| Build flags | `-D_POSIX_C_SOURCE=200809L` exclusively |
| Hosted leg guard | `WUBU_HOSTED` replaces `_GNU_SOURCE` |

---

## 12. Build System

```bash
make all                 # full build (kernel jit compiler runtime tools gui bridge apps worldsim metal audio shell bear hosted_objs)
make hosted              # hosted binary (runs on Linux)
make test                # all 414 test targets
make test_high_bear      # 26 core JIT + subsystem tests
make test_critical_kernel # kernel module tests (memory, tasking, FAT32, TXFS, AHCI)
make test_critical_runtime # runtime tests (VSL, Styx, containers, network)
make test_medium_other   # medium-priority tests (worldsim, audio, apps, etc)
make test_high_gui       # GUI tests (WM, desktop, startmenu, explorer)
make test_high_bridge    # bridge tests (VBE, syscall)
```

### Build flags
- `-std=c11 -D_POSIX_C_SOURCE=200809L` (hosted)
- `-std=c11 -DWUBU_HOSTED -include wubu_gnu_compat.h` (tests)
- `-std=c11 -DWUBU_NO_LIBM -ffreestanding -nostdlib` (kernel/metal)

---

## 13. Testing Strategy

- **414 test targets** in `mk/tests.mk`
- Tiered organization: critical_kernel, critical_runtime, high_bear, high_gui, high_bridge, medium_other, hw_*
- Each test compiles and runs in isolation
- WuBu compliance: no `_GNU_SOURCE` in any test recipe

---

## 14. Repository Structure

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
  bench/     Performance benchmarks
  dev/       Dev tooling (scanners, generators, linters)
  isa-test/  ISA driver tests
  research/  Recursive self-improvement, probe scripts
docs/
  research/  7-hop Kevin-Bacon driver convergence docs
  adr/       Architecture Decision Records
  compendium/ Institutional ledger
vault/       Accomplishments, planning docs, archives
```
