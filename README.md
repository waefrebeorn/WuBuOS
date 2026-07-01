# WuBuOS — ZealOS Kernel + Win98 Shell + Styx/9P + Arch Containers

```
╔═══════════════════════════════════════════════════════╗
║                                                        ║
║     🌱  W U B U O S                                 ║
║                                                        ║
║     ZealOS kernel · Win98 shell · Styx/9P namespace  ║
║                                                        ║
║     73 C files · ~15K real LOC · 747+ tests green   ║
║     1562 REAL_GAPs · 67% ZealOS parity · 85% VSL stubs ║
║                                                        ║
║     Hosted ─── ZealOS ─── 9P ─── GUI ─── Containers ║
║                                                        ║
╚═══════════════════════════════════════════════════════╝
```

## Architecture

**WuBuOS = ZealOS kernel + Win98 shell + Styx/9P + Arch containers**

- **Hosted binary**: ZealOS kernel runs in-process, Wayland compositor, Win98 WM/desktop/startmenu
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

# Run tests
make test_holyc        # 84/84 passing
make test_styxfs       # 11/11 passing
make test_vsl          # 55/55 passing
make test_network      # 139/139 passing
make test_snapshot     # 132/132 passing
make test_holyd        # 31/30 passing
```

## Project Structure

```
src/
├── kernel/          # Memory, task, VBE, FAT32, AHCI, interrupt, zealos_parity, ps2
├── compiler/        # HolyC lexer/parser/codegen (310-313)
├── hosted/          # Wayland compositor, DRM/KMS, ALSA, X11 fallback
├── runtime/         # Styx/9P, VSL, containers, Arch, RAM disk, network, snapshot
├── gui/             # Win98 WM, desktop, startmenu, explorer, terminal, clipboard
├── apps/            # Editor, canvas, codec, terminal, calc, control, freedoom
├── bridge/          # DOS flip Ctrl+Alt+T (206-207)
├── worldsim/        # GAAD (393), terrain, entity, physics
├── bear/            # RL training, Vulkan compute (policy, GAE, N-pole, MMA)
└── tools/           # ISO9660, screenshot, weight_check
```

## Status

| Metric | Value |
|--------|-------|
| **REAL_GAPs** | 1562 (Triple DA verified) |
| **Tests** | 747+ green across 47 targets |
| **LOC** | ~15K real (was 41K inflated) |
| **ZealOS parity** | 67% (96/96 name parity) |
| **VSL stubs** | 85% |

## License

WuBuOS — MIT License (ZealOS kernel under its own license)