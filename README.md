# WuBuOS — ZealOS Kernel + Win98 Shell + Styx/9P + Arch Containers

```
╔═══════════════════════════════════════════════════════╗
║                                                        ║
║     🌱  W U B U O S                                 ║
║                                                        ║
║     ZealOS kernel · Win98 shell · Styx/9P namespace  ║
║                                                        ║
║     73 C files · ~15K real LOC · 747+ tests green   ║
║     ~3000 REAL_GAPs · 67% ZealOS parity · 85% VSL stubs ║
║                                                        ║
║     Hosted ─── ZealOS ─── 9P ─── GUI ─── Containers ║
║                                                        ║
╚═══════════════════════════════════════════════════════╝
```

## Architecture

**WuBuOS = ZealOS kernel + Win98 shell + Styx/9P + Arch containers**

- **Hosted binary**: ZealOS kernel runs in-process, Wayland compositor, Win98 WM/desktop/startmenu
- **Audio engine**: src/audio/ — 30+ chip emulations, Furnace tracker, SF2 synth, DAW, AI plugins, MIDI/USB/HID
- **Styx/9P**: Real filesystem namespace backed by .wubu containers (9P2000)
- **Arch containers**: Fork+exec into Arch Linux rootfs (bwrap isolation, no syscall emulation)
- **HolyC JIT**: Self-hosted x86-64 encoder, disassembler, register allocator, minic compiler
- **Bear RL**: PPO training with Vulkan compute pipelines (policy, GAE, N-pole step, MMA matmul)

## Quick Start

```bash
# Build everything
make all

# Build hosted binary (runs on Linux)
make hosted

# Run hosted with screenshot
./src/hosted/wubu --screenshot /tmp/screenshot.ppm

# Run tests by tier (recommended)
make test_critical_runtime   # CRITICAL: OCI, Network, Snapshots, VSL, HolyD, Proton
make test_critical_kernel    # CRITICAL: FAT32, TXFS, AHCI, DRM
make test_high_bridge        # HIGH: syscall bridge, DOS flip
make test_high_gui           # HIGH: WM, StartMenu, Explorer, Terminal, Clipboard
make test_high_bear          # HIGH: JIT, Memory, Tasking, HolyC, PTX
make test_medium_other       # MEDIUM/LOW: WorldSim, Audio, Containers, etc.

# Run all tiers
make test

# Individual test targets
make test_holyc        # 84/84 passing
make test_styxfs       # 11/11 passing
make test_vsl          # 55/55 passing
make test_network      # 139/139 passing
make test_snapshot     # 132/132 passing
make test_holyd        # 33/33 passing
```

## Project Structure

```
src/
├── kernel/          # Memory, task, VBE, FAT32, AHCI, interrupt, zealos_parity, ps2
├── compiler/        # HolyC lexer/parser/codegen (310-313)
├── hosted/          # Wayland compositor, DRM/KMS, X11 fallback
├── audio/           # 30+ chips, Furnace, SF2, DAW, AI plugins, MIDI
├── runtime/         # Styx/9P, VSL, containers, Arch, RAM disk, network, snapshot
├── gui/             # Win98 WM, desktop, startmenu, explorer, terminal, clipboard
├── apps/            # Editor, canvas, codec, terminal, calc, control, freedoom
├── bridge/          # DOS flip Ctrl+Alt+T (206-207)
├── worldsim/        # GAAD (393), terrain, entity, physics
├── bear/            # RL training, Vulkan/CUDA, n-pole physics
└── tools/           # ISO9660, screenshot, weight_check
```

## Status

| Metric | Value |
|--------|-------|
| **REAL_GAPs** | ~3000 (Triple DA verified) |
| **Tests** | 747+ green across 58 targets |
| **LOC** | ~15K real (was 123K inflated) |
| **ZealOS parity** | 67% (96/96 name parity) |
| **VSL stubs** | 85% |

## The Mission

WuBuOS is an **alternate reality OS** where TempleOS won — it merges:
- **ReactOS** NT syscall emulation (297 syscalls → VSL)
- **ZealOS/TempleOS** HolyC JIT + ring-0 philosophy
- **SteamOS** Proton/Wine + gamescope + Pressure Vessel
- **Arch/Ubuntu** systemd + package management + networking
- **Win98/2000/XP** classic shell via Win98 WM

The VSL (Virtual Syscall Layer) is the bridge — NT syscalls → Linux syscalls → Styx/9P → ZealOS kernel → TempleOS HolyC JIT.

## License

WuBuOS — MIT License (ZealOS kernel under its own license)

## Screenshots

| Screenshot | Description |
|------------|-------------|
| `wubuos_screenshot.png` | Hosted binary rendering (Win98 theme, 1024×768) |
| `wubuos_demo.mp4` | 4-second demo video showing desktop, start menu, app launch |

### Theme Variants

Run with different themes:
```bash
./src/hosted/wubu --theme win98   --screenshot /tmp/win98.ppm   # Win98 Classic (default)
./src/hosted/wubu --theme xp      --screenshot /tmp/xp.ppm      # XP Luna Blue
./src/hosted/wubu --theme xp-media --screenshot /tmp/xp-media.ppm # XP Media Center Orange
./src/hosted/wubu --theme wubu    --screenshot /tmp/wubu.ppm    # WuBu Custom Green
```