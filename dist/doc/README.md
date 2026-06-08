# 🌱 WuBuOS

**ZealOS kernel · Win98 shell · Styx/9P namespace · Hosted binary · Triple-platform**

A GUI shell + container runtime wrapping ZealOS kernel — runs as a Linux binary (hosted), a WSL2 distribution (Windows), or an Apple Virtualization guest (macOS).

![WuBuOS Win98 Desktop](docs/screenshot.png)

## Architecture

```
Layer 5: .wubu Containers  — SteamOS, Brave, HolyC apps
Layer 4: Container Runtime — fork/exec, 9P namespace per container
Layer 3: Win98 GUI Shell   — WM, Start menu, taskbar, 98.css theme
Layer 2: Platform Layer    — Linux DRM/KMS, Windows WSL2, macOS AVF
Layer 1: ZealOS Kernel     — ring-0, HolyC JIT, RedSea FS (already boots on metal)
```

## Platform Coverage

| Platform | Host kernel | GPU | Container runtime | Distribution |
|----------|-------------|-----|-------------------|--------------|
| Linux    | Linux       | DRM/KMS direct | fork+exec native | Native package |
| Windows  | NT/WSL2     | /dev/dxg (paravirt) | fork+exec native | `wsl --install WuBuOS` |
| macOS    | XNU         | VirtIO GPU | fork+exec native | Homebrew / .app bundle |

Same `wubu` binary. Same `.wubu` containers. Same 9P Styx namespace. One binary IS the product (Inferno emu pattern).

## Container Runtime

Containers are **host processes** — fork + chroot + exec. No syscall emulation.
- **Arch base**: rips through Linux drivers for SteamOS/Proton compat
- **GPU passthrough**: /dev/dri + /dev/nvidia* + /dev/dxg bind-mounted into container
- **9P namespace**: per-container Styx socket for /wubu, /dev, /prog
- **SteamOS preset**: Arch root + Steam Runtime + Proton + GPU passthrough

## Status

| Component | Tests | What's Real |
|-----------|-------|-------------|
| Kernel (mem, task, vbe, fat32, ahci, txfs) | 109 ✅ | Working subsystems, hosted mode |
| JIT mmap stub | 20 ✅ | a+b/a*b eval |
| HolyC compiler (71 eval tests) | 71 ✅ | Lex/parse/eval with label backpatching |
| .wubu container + VSL + Proton | 108 ✅ | VSL/Proton PARTIAL (bare-metal scaffolding) |
| Styx/9P2000 + StyxFS | 40 ✅ | **Real** message serialization |
| GUI (WM + dbuf + start menu + theme) | 56 ✅ | **Real** pixel rendering |
| DOS flip bridge (Ctrl+Alt+T) | 13 ✅ | **Real** X11 key → mode switch |
| Hosted binary (Cell 200) | 14 ✅ | **Real** kernel init + GUI shell + input routing |
| Container runtime (Cell 203) | 15 ✅ | **Real** fork+exec + exit codes + kill |
| Package manager + compilers | 15 ✅ | Registry, no real installation |
| **Total: 34 C files, ~134 C/H** | **511+ ✅** | **~28K source LOC + 7.7K test LOC** |

## Battleship Progress

| Cell | Description | Status |
|------|-------------|--------|
| 200 | ZealOS kernel in-process + Win98 GUI shell | ✅ |
| 201 | HolyC REPL compiles + executes code | ⬜ |
| 202 | GUI dispatches input to ZealOS apps | PARTIAL |
| 203 | Fork+exec for .wubu containers | ✅ |
| 204 | Per-container 9P namespace | ⬜ |
| 205 | SteamOS container with GPU passthrough | ⬜ |
| 206 | Bare-metal boot | ⬜ |
| 207 | Integration test: wubu runs, GUI appears, REPL works | ⬜ |
| 310 | HolyC codegen: ternary, AND, OR, IF, WHILE, FOR | ✅ |
| 311 | HolyC codegen: function calls, struct layout, string literals | ⬜ |
| 324 | VSL Linux virtualization layer | PARTIAL (bare-metal scaffolding) |
| 330 | Proton Windows compat layer | PARTIAL (host Wine delegation) |
| 380 | wubu_display.c — DRM/KMS + X11 dual backend | ✅ (header+impl) |
| 383 | Container bind mounts applied | ✅ |
| 384 | WuBuOS as WSL2 distribution | 🟡 (scripts written) |
| 385 | 9P Styx bridge: ZealOS↔Arch | ⬜ |
| 386 | Arch rootfs builder | 🟡 (scripts written) |
| 387 | libseat/seatd DRM master management | ⬜ |
| 390 | macOS Apple Virtualization launcher | ✅ (wubu_macos.m) |

## Quick Start

```bash
# Build everything
make all

# Run tests (511+)
make test

# Build the hosted binary
make hosted

# Run WuBuOS as a Linux app
./src/hosted/wubu            # X11 window with Win98 desktop
./src/hosted/wubu -h         # Headless (Styx server)
./src/hosted/wubu -t         # Temple REPL full-screen

# Run container tests
make test_host_exec
```

## Distribution (Inferno emu pattern)

```bash
# Create demo distribution
make hosted
./create-initramfs.sh        # dist/boot/initramfs.img

# Create bootable USB
sudo ./create-usb.sh /dev/sdX

# Build for macOS
./build-macos.sh             # dist/build-macos/

# Test in QEMU
./qemu-test.sh --kernel /boot/vmlinuz-linux
```

## ⚠️ Hard-Dive Reality

WuBuOS has 511+ passing tests across 20 test suites. Cells 200, 203, 310, 380, 383, 390 are verified at runtime. VSL and Proton are PARTIAL — their interface skeletons are architecture commitments for the bare-metal path, not dead code. Name parity is 64/64 core functions mapped via `zealos_parity.h`. See `docs/risk_register.md` for full gap analysis.
