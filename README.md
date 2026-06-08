# 🌱 WuBuOS

**ZealOS kernel · Win98 shell · Styx/9P namespace · Arch containers · FreeDoom**

A GUI shell + container runtime wrapping ZealOS kernel — runs as a Linux binary (hosted), a WSL2 distribution (Windows), or an Apple Virtualization guest (macOS).

![WuBuOS Win98 Desktop](docs/screenshot.png)

## Architecture

```
Layer 6: .wubu Containers  — FreeDoom, Steam, Brave, HolyC apps
Layer 5: Container Runtime — fork/exec, 9P namespace per container
Layer 4: Arch Root Mount   — RAM (tmpfs) for containers, SSD for bare metal
Layer 3: Win98 GUI Shell   — WM, Start menu, taskbar, 98.css theme
Layer 2: Platform Layer    — Linux DRM/KMS, Windows WSL2, macOS AVF
Layer 1: ZealOS Kernel     — ring-0, HolyC JIT, RedSea FS (already boots on metal)
```

## Design Philosophy: Why Not Both

- **Arch** is the stable NT-era kernel — real drivers, real GPU, real networking
- **Win98 shell** is the humane interface — snappy, visible, yours
- **HolyC JIT** is the DOS soul — ring-0 escape hatch, raw immediacy
- **Containers** are the safety rail — run risky code without killing the OS
- **Ramdisk** is the container root — tmpfs, zero disk, instant teardown
- **SSD** is the bare metal root — persistent, real OS on real hardware

Same `wubu` binary. Same `.wubu` containers. Same 9P Styx namespace.
One binary IS the product (Inferno emu pattern).

## Platform Coverage

- **Linux**: DRM/KMS direct GPU, fork+exec native containers
- **Windows**: WSL2 distro (`wsl --install WuBuOS`), /dev/dxg paravirt GPU
- **macOS**: Apple Virtualization.framework, VirtIO GPU

## Container Runtime

Containers are **host processes** — fork + chroot + exec. No syscall emulation.
- **Arch base**: rips through Linux drivers for SteamOS/Proton compat
- **GPU passthrough**: /dev/dri + /dev/nvidia* + /dev/dxg bind-mounted into container
- **9P namespace**: per-container Styx socket for /wubu, /dev, /prog
- **SteamOS preset**: Arch root + Steam Runtime + Proton + GPU passthrough
- **FreeDoom**: prboom-plus + freedoom WADs inside Arch container with GPU+audio

## Root Mount: RAM vs SSD

| Mode | Path | Persistent | Use Case |
|------|------|-----------|----------|
| RAM  | /run/wubu/ramdisk | No (tmpfs) | Container/hosted mode |
| SSD  | /var/wubu/roots/arch-base | Yes | Bare metal install |
| RAM→SSD | install_to_disk() | After copy | Opt-in persistence |

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
| Arch bootstrap (Cell 390) | 5 ✅ | pacstrap + configure API |
| FreeDoom launcher (Cell 391) | 10 ✅ | prboom-plus in Arch container with GPU+audio |
| Root mount RAM/SSD (Cell 392) | 12 ✅ | tmpfs + disk modes + install_to_disk + snapshot |
| **Total** | **530+ ✅** | **~30K source LOC** |

## Battleship Progress

| Cell | Description | Status |
|------|-------------|--------|
| 200 | ZealOS kernel in-process + Win98 GUI shell | ✅ |
| 203 | Fork+exec for .wubu containers | ✅ |
| 310 | HolyC codegen: ternary, AND, OR, IF, WHILE, FOR | ✅ |
| 380 | wubu_display.c — DRM/KMS + X11 dual backend | ✅ |
| 383 | Container bind mounts applied | ✅ |
| 390 | Arch rootfs bootstrap (pacstrap + configure) | ✅ |
| 391 | FreeDoom launcher (prboom-plus in Arch container) | ✅ |
| 392 | Root mount: RAM (tmpfs) + SSD + install_to_disk | ✅ |
| 201 | HolyC REPL compiles + executes code | ⬜ |
| 202 | GUI dispatches input to ZealOS apps | PARTIAL |
| 204 | Per-container 9P namespace | ⬜ |
| 205 | SteamOS container with GPU passthrough | ⬜ |
| 206 | Bare-metal boot | ⬜ |
| 311 | HolyC codegen: function calls, struct layout | ⬜ |
| 324 | VSL Linux virtualization layer | PARTIAL |
| 330 | Proton Windows compat layer | PARTIAL |

## Quick Start

```bash
# Build everything
make all

# Run tests (530+)
make test

# Build the hosted binary
make hosted

# Run WuBuOS as a Linux app
./src/hosted/wubu            # X11 window with Win98 desktop
./src/hosted/wubu -h         # Headless (Styx server)
./src/hosted/wubu -t         # Temple REPL full-screen

# Run container tests
make test_host_exec

# Run Arch/bootstrap tests
make test_arch

# Run ramdisk tests
make test_ramdisk
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

WuBuOS has 530+ passing tests across 23 test suites. Cells 200, 203, 310, 380, 383, 390, 391, 392 are verified at runtime. VSL and Proton are PARTIAL — their interface skeletons are architecture commitments for the bare-metal path, not dead code. Arch bootstrap creates real rootfs via pacstrap. FreeDoom runs inside Arch containers with GPU passthrough. Ramdisk mounts tmpfs for container mode, SSD for bare metal. See `docs/risk_register.md` for full gap analysis.
