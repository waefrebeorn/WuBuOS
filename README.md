# WuBuOS — ZealOS Kernel + Win98 Shell + Styx/9P + Arch Containers

```
╔════════════════════════════════════════════════════════════════════════╗
║     🌱  W U B U O S                                                       ║
║     ZealOS kernel · Win98 shell · Styx/9P namespace · Arch containers    ║
║     461 C files · ~104K LOC · 90 test targets green                     ║
║     ~40 code REAL_GAPs + ~370 parity marathons (Triple DA) · 64 targets  ║
╚══════════════════════════════════════════════════════════════════════════╝
```

## Architecture

**WuBuOS = ZealOS kernel + Win98 shell + Styx/9P + Arch containers**

- **Hosted binary**: ZealOS kernel runs in-process, Wayland compositor, Win98 WM/desktop/startmenu
- **Audio engine**: src/audio/ — 30+ chip emulations, Furnace tracker, SF2 synth, DAW, AI plugins
- **Styx/9P**: Real filesystem namespace backed by .wubu containers (9P2000)
- **Arch containers**: Fork+exec into Arch Linux rootfs (bwrap isolation, no syscall emulation)
- **HolyC JIT**: Self-hosted x86-64 encoder, disassembler, register allocator, minic compiler
- **16-bit DOS compatibility**: Real 8086 interpreter + INT 21h/10h/16h DOS layer — runs .COM/.EXE in-process, captures a text screen + RGBA frame for the Desktop "compatible window" (22/22 regression tests)
- **Bear RL**: PPO training with Vulkan compute pipelines
- **Desktop**: Win98-style wallpaper (real BMP decode + 5 placement modes), icons, taskbar, systray

## Quick Start

```bash
make all                 # full build
make hosted              # hosted binary (runs on Linux)
./src/hosted/wubu --screenshot /tmp/screenshot.ppm

make test                # all 90 test targets, 747+ assertions
make test_dos_emu        # 16-bit DOS emulator (22 regression tests)
make test_dos_emu_smoke  # minimal DOS .COM run
python3 ~/.hermes/profiles/mind-palace/skills/software-development/wubuos-battleship-gaps/scripts/find_real_gaps.py src   # honest gap scan
```

## Status (honest, v22 — 2026-07-08)

| Metric | Value |
|--------|-------|
| **Code-level REAL_GAPs** | ~40 (verified: 10 `system()` + 23 stub-phrase + 6 bare-metal no-op) |
| **Parity marathons** | ~370 (ReactOS NT 297 + SteamOS ~30 + Ubuntu/Arch ~20 + TempleOS ~15 + ZealOS ~8) |
| **Tests** | 747+ green across 64 targets |
| **LOC** | ~15K real |
| **ZealOS name parity** | 96/96 |
| **Baseline stub class** | CLOSED (0 empty bodies, 0 const-only-no-syscall in `src/`) |

> **Honesty note**: BATTLESHIP v21's "~-400 sprint gaps" was not reproducible — the
> scanner was broken. v22 partitions the ~400 into ~40 verifiable code gaps + ~370
> parity marathons (per the rule "rewriting from scratch in C = REAL_GAP, including
> ReactOS gaps to WuBuOS"). See `BATTLESHIP.md` Part 1/2/3/4.

### Recent additions (post-v22, 2026-07-15 → 2026-07-19)
- **16-bit DOS compatibility shim** (`src/runtime/wubu_dos_emu*`): a real 8086
  interpreter + INT 21h/10h/16h DOS layer that runs `.COM`/`.EXE` in-process and
  captures a text screen + RGBA frame. 22/22 regression tests green; wired into
  `make test_dos_emu` / `make test_dos_emu_smoke`. (2026-07-19: fixed an execution
  hang in `step()` + wired two dead API stubs — `wubu_dos_emu_step`,
  `wubu_dos_emu_peek16`.)
- **Monolith dissolution campaign** (2026-07-15 → 2026-07-18): large modules
  (`wubu_editor.c`, `wubu_canvas.c`, `styx.c`, `wubu_proton.c`, `wubu_clipboard.c`,
  `dosgui_explorer.c`, startmenu, etc.) split into opaque C11 leaf modules with
  minimal includes; Win98 Terminal engine wired to the real `cmd.c`; DOS Box window
  engine + bare-metal boot/interrupt PIC/IRQ split. Repo grew to 461 `.c` / ~104K LOC.

## Screenshots

![Desktop (Win98 Classic theme)](screenshots/wubuos-desktop-2026-07-07.png)

*WuBuOS booting in headless mode — 1024×768 Win98 desktop with My Computer,
HolyC REPL, Recycle Bin, Control Panel icons, and taskbar clock.*

## The Mission

WuBuOS merges TempleOS (HolyC/JIT), ReactOS (NT syscall emulation), SteamOS
(Proton/gamescope), Arch/Ubuntu (pacman/systemd), and Win98 (shell) into one
hosted binary. The VSL (Virtual Syscall Layer) is the bridge: NT → Linux →
Styx/9P → ZealOS → HolyC JIT.

**Discipline**: opaque structs, minimal includes, C11 only. "Rewriting from scratch
in C" = closing a gap — including every ReactOS-NT syscall and every missing
SteamOS/Ubuntu/TempleOS/ZealOS subsystem, which are reclassified as REAL_GAP
marathons.

## License
WuBuOS — MIT License (ZealOS kernel under its own license)
