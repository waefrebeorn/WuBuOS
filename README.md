# 🌱 WuBuOS

**ZealOS kernel · Win98 shell · Styx/9P namespace · Hosted binary**

A GUI shell + container runtime wrapping ZealOS kernel — runs as a Linux binary (hosted) or standalone (bare-metal via ZealOS boot).

![WuBuOS Win98 Desktop](docs/screenshot.png)

## Architecture

```
Layer 5: .wubu Containers  — SteamOS, Brave, HolyC apps
Layer 4: Container Runtime — fork/exec, 9P namespace per container
Layer 3: Win98 GUI Shell   — WM, Start menu, taskbar, 98.css theme
Layer 2: Platform Layer    — Linux X11/Wayland, Windows Win32, bare ZealOS
Layer 1: ZealOS Kernel     — ring-0, HolyC JIT, RedSea FS (already boots on metal)
```

## Status

| Component | Tests | What's Real |
|-----------|-------|-------------|
| Kernel stubs (mem, task, vbe, fat32, ahci, txfs) | 109 ✅ | Struct inits, no real HW |
| JIT mmap stub | 20 ✅ | a+b/a*b only |
| HolyC compiler skeleton | 41 ✅ | Lex/parse, no real codegen |
| .wubu container + VSL + Proton | 108 ✅ | API surface, no process creation |
| Styx/9P2000 + StyxFS | 40 ✅ | **Real** message serialization |
| GUI (WM + dbuf + start menu + theme) | 56 ✅ | **Real** pixel rendering |
| DOS flip bridge (Ctrl+Alt+T) | 13 ✅ | **Real** X11 key → mode switch |
| Hosted binary | 8 ✅ | Opens X11 window |
| Package manager + compilers | 15 ✅ | Registry, no real installation |
| **Total: 30 C files** | **462/462 ✅** | **~11K real LOC** |

## ⚠️ Hard-Dive Reality

WuBuOS has 462 passing tests, but they verify **API signatures**, not **runtime behavior**. The battleship has been reset with 8 real behavioral cells (200-207). See `docs/risk_register.md` for details.

## Quick Start

```bash
# Build everything
make all

# Run tests (462)
make test

# Build the hosted binary
make hosted

# Run WuBuOS as a Linux app
./src/hosted/wubu            # X11 window
./src/hosted/wubu -h         # Headless (Styx server)
./src/hosted/wubu -w 800 600 # Custom size
```

## The Inferno Pattern

WuBuOS follows the Inferno OS `emu` model:
- **Runs hosted** — launch it as a Linux binary (like Inferno's emu)
- **Runs bare-metal** — boot ZealOS, WuBuOS is the Win98 shell
- **Everything is a file** — Styx/9P namespace
- **Everything is a container** — .wubu format (host fork/exec)
- **ZealOS kernel inside** — HolyC JIT, RedSea FS, DolDoc

## Project Layout

```
src/
├── kernel/     # Memory, tasking, VBE, input, IRQ, FAT32, AHCI, TXFS (stubs)
├── jit/        # JIT runtime (mmap stub)
├── compiler/   # HolyC compiler skeleton (lexer, parse, codegen)
├── runtime/    # .wubu, host-linux (was VSL), Proton, Styx/9P2000, StyxFS, pkg
├── gui/        # WM, taskbar, desktop, theme, double-buffer, start menu
├── bridge/     # DOS flip, clipboard, IPC, VBE↔WorldSim
├── hosted/     # Inferno emu-style X11 launcher (the product)
├── apps/       # HolyC REPL, Notepad
└── tools/      # ISO 9660 builder, screenshot generator
```

## Build Targets

```bash
make all           # All layers
make test          # 462/462 tests
make hosted        # Hosted binary (./src/hosted/wubu)
make test_styx     # Styx/9P2000 (29 tests)
make test_hosted   # Hosted mode (8 tests)
make test_wm       # Window manager (26 tests)
make test_startmenu # Start menu (13 tests)
make test_styxfs   # StyxFS namespace (11 tests)
make test_apps     # Package manager + VSL + Proton + compilers (15 tests)
make clean         # Clean artifacts
```

## Reference

- [Inferno OS](https://github.com/inferno-os/inferno-os) — Styx/9P, Dis bytecode, hosted emu (767K LOC)
- [ZealOS](https://github.com/Zeal-Operating-System/ZealOS) — Modernized TempleOS (154K LOC, boots on metal)
- [NanoShellOS](https://github.com/GenericProgrammer1234/NanoShellOS) — x86-64 hobby OS (GUI reference)
- [holyc-lang](https://github.com/tsoding/holyc-lang) — HolyC compiler in C
