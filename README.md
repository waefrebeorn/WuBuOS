# WuBuOS — ZealOS Kernel + Win98 Shell + Styx/9P + Arch Containers

```
╔════════════════════════════════════════════════════════════════╗
║                                                        ║
║     🌱  W U B U O S                                 ║
║                                                        ║
║     ZealOS kernel · Win98 shell · Styx/9P namespace   ║
║                                                        ║
║     73 C files · ~15K real LOC · 747+ tests green    ║
║     ~400 sprint REAL_GAPs · 5 parity epics · 64 targets  ║
║                                                        ║
║     Hosted ─── ZealOS ─── 9P ─── GUI ─── Containers ║
║                                                        ║
╚════════════════════════════════════════════════════════════════╝
```

## Architecture

**WuBuOS = ZealOS kernel + Win98 shell + Styx/9P + Arch containers**

- **Hosted binary**: ZealOS kernel runs in-process, Wayland compositor, Win98 WM/desktop/startmenu
- **Audio engine**: src/audio/ — 30+ chip emulations, Furnace tracker, SF2 synth, DAW, AI plugins
- **Styx/9P**: Real filesystem namespace backed by .wubu containers (9P2000)
- **Arch containers**: Fork+exec into Arch Linux rootfs (bwrap isolation, no syscall emulation)
- **HolyC JIT**: Self-hosted x86-64 encoder, disassembler, register allocator, minic compiler
- **Bear RL**: PPO training with Vulkan compute pipelines (policy, GAE, N-pole step, MMA matmul)
- **Desktop**: Win98-style wallpaper (real BMP decode + 5 placement modes), icons, taskbar, systray

## Quick Start

```bash
# Build everything
make all

# Build hosted binary (runs on Linux)
make hosted

# Run hosted with screenshot
./src/hosted/wubu --screenshot /tmp/screenshot.ppm

# Tests by tier
make test_critical_runtime   # OCI, Network, Snapshots, VSL, HolyD, Proton
make test_critical_kernel    # FAT32, TXFS, AHCI, DRM
make test_high_bridge        # syscall bridge, DOS flip
make test_high_gui           # WM, StartMenu, Explorer, Terminal, Clipboard, Wallpaper
make test_high_bear          # JIT, Memory, Tasking, HolyC, PTX
make test_medium_other       # WorldSim, Audio, Containers, etc.
make test                    # all 64 targets, 747+ assertions
```

## Status

| Metric | Value |
|--------|-------|
| **Sprint REAL_GAPs** | ~400 (Triple DA, form≠function filtered) |
| **Parity epics** | 5 (SteamOS / Ubuntu-Arch / TempleOS / ZealOS / ReactOS) |
| **Tests** | 747+ green across 64 targets |
| **LOC** | ~15K real (was 123K inflated) |
| **ZealOS name parity** | 96/96 |
| **Desktop wallpaper** | ✅ real decode (2026-07-07) |

## The Mission

WuBuOS merges TempleOS (HolyC/JIT), ReactOS (NT syscall emulation), SteamOS
(Proton/gamescope), Arch/Ubuntu (pacman/systemd), and Win98 (shell) into one
hosted binary. The VSL (Virtual Syscall Layer) is the bridge: NT → Linux →
Styx/9P → ZealOS → HolyC JIT.

**Discipline**: opaque structs, minimal includes, C11 only, no god headers, every
module self-contained. "Rewriting from scratch in C" = closing a gap.

## License
WuBuOS — MIT License (ZealOS kernel under its own license)
