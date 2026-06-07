# 🌱 WuBuOS

**Hybrid TempleOS/ZealOS C-Port with Win98/XP Classic GUI**

A personal, hackable "seed" OS you fully own.

## Architecture

```
Layer 4: Apps        — Notepad, Paint, Explorer, HolyC REPL, WorldSim
Layer 3: GUI Shell   — Win98 WM, Start menu, taskbar, 98.css theme
Layer 2: Bridge      — DOS flip (Ctrl+Alt+T), shared FS, clipboard, IPC
Layer 1: Core        — ZealOS C-port + JIT (MIR/mmap) + WorldSim engine
```

## Status

| Layer | Component | LOC | Tests |
|-------|-----------|-----|-------|
| Core | Kernel (mem, task, vbe, input, irq, fat32) | ~3,200 | 49 ✅ |
| Core | JIT (mmap + MIR backends) | ~700 | 20 ✅ |
| Core | WorldSim (terrain, ECS, physics, render, sim) | 934 | 18 ✅ |
| GUI | WM + taskbar + desktop + theme | ~600 | — |
| GUI | NanoShellOS widget fork (28 files) | ~13,600 | — |
| Bridge | Mode switch + clipboard + IPC | ~200 | — |
| Apps | REPL + Notepad | ~200 | — |
| **Total** | **97 source files** | **27,220** | **277/277 ✅** |

## Build

```bash
make all          # Build all layers
make test         # Run all tests (277/277)
make test_vsl      # Run VSL tests (46/46)
make test_bridge   # Run VBE↔WorldSim bridge tests (25/25)
make worldsim     # Build WorldSim only
make test_worldsim # Run WorldSim tests (18/18)
make clean        # Clean build artifacts
```

## Project Layout

```
src/
├── kernel/     # Memory, tasking, VBE, input, interrupts, FAT32
├── jit/        # JIT runtime (jit.h API, mmap + MIR backends)
├── gui/        # Window manager, taskbar, desktop, Win98 theme
│   └── wm_nano/  # NanoShellOS widget reference (28 files)
├── bridge/     # DOS flip, clipboard, IPC, VBE↔WorldSim bridge
├── apps/       # HolyC REPL, Notepad
└── worldsim/   # Terrain, ECS, physics, render, simulation
```

## Constraints

- Pure C11 kernel, ring-0, single-user
- <100K LOC budget
- Self-documenting (like TempleOS)
- No dependency bloat (MIR as only JIT dep, -lm only)
- Public domain core + MIT additions

## Vision

The TempleOS soul — divine simplicity, JIT-everything, single-user ring-0 —  
wrapped in a faithful Win98/XP classic desktop.  
Flip between them with a hotkey, like dropping to DOS in Windows 9x.

Your local AI tools, custom engines, and fuzzing harnesses  
live inside a simple, controllable, retro-modern OS you fully own.
