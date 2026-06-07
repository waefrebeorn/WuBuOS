# 🌱 WuBuOS

**Hybrid TempleOS/ZealOS C-Port with Win98/XP Classic GUI + Styx/9P Namespace**

A personal, hackable "seed" OS you fully own.  
Inferno OS Styx/9P protocol integrated for universal containerization.

## Architecture

```
Layer 5: Runtime     — .wubu container format, Styx/9P namespace, VSL, Proton
Layer 4: Apps        — Notepad, Paint, Explorer, HolyC REPL, WorldSim, LLM Chat
Layer 3: GUI Shell   — Win98 WM, Start menu, taskbar, 98.css theme
Layer 2: Bridge      — DOS flip (Ctrl+Alt+T), VBE↔WorldSim, clipboard, IPC
Layer 1: Core        — ZealOS C-port + JIT (MIR/mmap) + WorldSim + FAT32 + Styx/9P
```

## Status

| Component | Tests | Status |
|-----------|-------|--------|
| Kernel (mem, task, vbe, input, irq, fat32, ahci, txfs) | 109 ✅ | Phase 1-2 |
| JIT (mmap + MIR + HolyC) | 61 ✅ | Phase 1-2 |
| WorldSim (terrain, ECS, physics, render, sim) | 18 ✅ | Phase 1-2 |
| .wubu container + VSL + Proton | 108 ✅ | Phase 1-2 |
| ISO 9660 bootable + weights check | 28 ✅ | Phase 1-2 |
| GUI (WM + dbuf + wm_nano) | 17 ✅ | Phase 1-2 |
| Bridge (VBE↔WorldSim) | 25 ✅ | Phase 1-2 |
| **Styx/9P2000 protocol** | **29 ✅** | **Phase 3** |
| Window manager test suite + Win98 theme | ⬜ | Phase 3 |
| DOS flip bridge wiring | ⬜ | Phase 3 |
| Flatpak package manager | ⬜ | Phase 3 |
| Brave browser via VSL | ⬜ | Phase 3 |
| **Total: 112 source files** | **368/368 ✅** | |

## Build

```bash
make all           # Build all layers
make test          # Run all tests (368/368)
make test_styx     # Run Styx/9P2000 protocol tests (29/29)
make test_vsl      # Run VSL tests (46/46)
make test_bridge   # Run VBE↔WorldSim bridge tests (25/25)
make test_proton   # Run Proton Windows compat tests (24/24)
make worldsim      # Build WorldSim only
make test_worldsim # Run WorldSim tests (18/18)
make test_txfs     # Run transactional FS tests (25/25)
make test_dbuf     # Run double-buffered GUI tests (17/17)
make clean         # Clean build artifacts
```

## Project Layout

```
src/
├── kernel/     # Memory, tasking, VBE, input, interrupts, FAT32, AHCI, TXFS
├── jit/        # JIT runtime (mmap + MIR + HolyC backends)
├── compiler/   # HolyC compiler (lexer, parser, codegen)
├── runtime/    # WuBuOS runtime: .wubu, VSL, Proton, Styx/9P2000
├── gui/        # Window manager, taskbar, desktop, Win98 theme, dbuf
│   └── wm_nano/  # NanoShellOS widget reference (28 files)
├── bridge/     # DOS flip, clipboard, IPC, VBE↔WorldSim bridge
├── apps/       # HolyC REPL, Notepad
├── worldsim/   # Terrain, ECS, physics, render, simulation
└── tools/      # ISO 9660 builder, weight check tool
```

## Reference Repos (not committed)

- `inferno-os/` — Inferno OS (Styx/9P, Dis bytecode, hosted emu)
- `ZealOS/` — Original TempleOS fork with modern additions
- `NanoShellOS/` — x86-64 hobby OS with Win95-style GUI
- `holyc-lang/` — HolyC compiler in C (reference for our port)

## Constraints

- Pure C11 kernel, ring-0, single-user
- <100K LOC budget
- Self-documenting (like TempleOS)
- Public domain core + MIT additions

## Vision

The TempleOS soul — divine simplicity, JIT-everything, single-user ring-0 —  
wrapped in a faithful Win98/XP classic desktop with Inferno OS Styx/9P namespace.  

Linux runs as VSL (Virtualization Substrate Layer, like Proton) for driver/app compat.  
Inferno OS runs as hosted emu for portable containerization.  

Everything is a .wubu file. Everything is a Styx file server.  
Flip between GUI and Temple with a hotkey, like dropping to DOS in Windows 9x.
