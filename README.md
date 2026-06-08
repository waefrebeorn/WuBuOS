# 🌱 WuBuOS

**Hybrid TempleOS/ZealOS C-Port with Win98/XP Classic GUI + Styx/9P Namespace**

A personal, hackable "seed" OS you fully own — runs as a Linux binary (hosted) or standalone (bare-metal ISO).

![WuBuOS Win98 Desktop](docs/screenshot.png)

## Architecture

```
Layer 5: Runtime     — .wubu container format, Styx/9P namespace, VSL, Proton
Layer 4: Apps        — Notepad, Paint, Explorer, HolyC REPL, WorldSim, LLM Chat
Layer 3: GUI Shell   — Win98 WM, Start menu, taskbar, 98.css theme
Layer 2: Bridge      — DOS flip (Ctrl+Alt+T), VBE↔WorldSim, clipboard, IPC
Layer 1: Core        — ZealOS C-port + JIT (MIR/mmap) + WorldSim + FAT32 + Styx/9P
```

## Status

| Component | Tests | Phase |
|-----------|-------|-------|
| Kernel (mem, task, vbe, fat32, ahci, txfs) | 109 ✅ | 1-2 |
| JIT (mmap + MIR + HolyC) | 61 ✅ | 1-2 |
| WorldSim (terrain, ECS, physics, render, sim) | 18 ✅ | 1-2 |
| .wubu container + VSL + Proton | 108 ✅ | 1-2 |
| ISO 9660 bootable | 20 ✅ | 1-2 |
| GUI (WM + double-buffer) | 17 ✅ | 1-2 |
| WM test suite + Win98 theme | 26 ✅ | 2 |
| Start menu (Win98 classic) | 13 ✅ | 2 |
| VBE↔WorldSim bridge | 25 ✅ | 1-2 |
| DOS flip bridge (Ctrl+Alt+T) | 13 ✅ | 2-3 |
| **Styx/9P2000 protocol** | **29 ✅** | **3a** |
| **Hosted binary (X11/headless)** | **8 ✅** | **3b** |
| **Total: 118+ files** | **436/436 ✅** | **31 cells done** |

## Quick Start

```bash
# Build everything
make all

# Run tests (384)
make test

# Build the hosted binary (172KB blob)
make hosted

# Run WuBuOS as a Linux app
./src/hosted/wubu            # X11 window
./src/hosted/wubu -h         # Headless (Styx server)
./src/hosted/wubu -w 800 600 # Custom size
```

That's it. No VM, no Docker, no nested OS. WuBuOS runs as a regular Linux binary.

## The Blob OS Concept

WuBuOS is designed as a **self-containerized OS**:

- **Runs hosted** — launch it as a Linux binary (like Inferno OS's emu)
- **Runs bare-metal** — boot the ISO on real hardware
- **Everything is a file** — Styx/9P namespace (everything = file server)
- **Everything is a container** — .wubu format (universal blob)
- **Self-hosted apps** — HolyC JIT compiler built in C11
- **Linux app compat** — VSL (Virtualization Substrate Layer) runs Linux binaries
- **Windows app compat** — Proton translation layer runs PE binaries
- **Inferno OS protocol** — Styx/9P for namespace-based everything

## Project Layout

```
src/
├── kernel/     # Memory, tasking, VBE, input, IRQ, FAT32, AHCI, TXFS
├── jit/        # JIT runtime (mmap + MIR + HolyC)
├── compiler/   # HolyC compiler (lexer, parse, codegen)
├── runtime/    # .wubu, VSL, Proton, Styx/9P2000
├── gui/        # WM, taskbar, desktop, Win98 theme, double-buffer
│   └── wm_nano/  # NanoShellOS widget fork (28 files)
├── bridge/     # DOS flip, clipboard, IPC, VBE↔WorldSim
├── hosted/     # Inferno emu-style X11 launcher (the blob)
├── apps/       # HolyC REPL, Notepad
├── worldsim/   # Terrain, ECS, physics, render, simulation
└── tools/      # ISO 9660 builder, screenshot generator
```

## Reference

- [Inferno OS](https://github.com/inferno-os/inferno-os) — Styx/9P, Dis bytecode, hosted emu
- [ZealOS](https://github.com/Zeal-Operating-System/ZealOS) — Modernized TempleOS fork
- [NanoShellOS](https://github.com/GenericProgrammer1234/NanoShellOS) — x86-64 hobby OS
- [holyc-lang](https://github.com/tsoding/holyc-lang) — HolyC compiler in C

## Build Targets

```bash
make all           # All layers
make test          # 436/436 tests
make hosted        # Hosted binary (./src/hosted/wubu)
make test_styx     # Styx/9P2000 (29 tests)
make test_hosted   # Hosted mode (8 tests)
make test_worldsim # WorldSim (18 tests)
make test_vsl      # VSL (46 tests)
make test_proton   # Proton (24 tests)
make clean         # Clean artifacts
```
