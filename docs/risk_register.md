# WuBuOS — Battleship / Risk Register (v3 — Post Cell 200+203)

**Methodology**: Behavioral verification. ✅ means feature works at RUNTIME, not just compiles.
**Current state**: 32 C files, ~13K real LOC, 468+ tests (14 behavioral hosted, 15 behavioral container)
**Architecture**: WuBuOS = GUI shell + container runtime. ZealOS IS the kernel.
**Container runtime**: Arch base → host fork+chroot+exec → 9P namespace per container → GPU passthrough

## Real Gaps (Cells 200-207)

| Cell | Description | Depends | Status |
|------|-------------|---------|--------|
| 200 | ZealOS kernel runs in-process inside `wubu` hosted binary | — | ✅ |
| 201 | HolyC REPL compiles and executes code in-process | 200 | ⬜ |
| 202 | Win98 GUI dispatches real input events to ZealOS apps | 200 | ⚠️ PARTIAL |
| 203 | Fork+exec for Linux .wubu containers (host delegation) | — | ✅ |
| 204 | Per-container 9P namespace mount (Styx socket isolation) | 203 | ⬜ |
| 205 | SteamOS container launches with GPU passthrough | 203 | ⬜ |
| 206 | Bare-metal: ZealOS boots → WuBuOS as Win98 shell | 200,201,202 | ⬜ |
| 207 | Integration test: `wubu` binary runs, GUI appears, REPL executes code | 200,201,202 | ⬜ |

## What Actually Works (runtime-verified)

- Styx/9P2000 message serialization (real bytes on wire)
- Win98 GUI pixel rendering (real X11 pixels)
- Ctrl+Alt+T DOS flip (real X11 key event)
- Double-buffered rendering with dirty rects
- **Cell 200**: Hosted binary inits kernel (mem+VBE+tasking), WM creates windows, desktop renders Win98 chrome, input routes from X11 to WM, start menu has entries, Styx namespace populated
- **Cell 203**: Real fork+exec container creation, exit code propagation, SIGTERM kill, state polling, SteamOS preset with GPU passthrough + env configuration, 9P bind mount config

## Container Architecture

- **Host delegation**: containers are host fork+chroot+exec processes (NOT syscall emulation)
- **Arch base**: minimal Arch Linux rootfs for SteamOS/driver compat
- **GPU passthrough**: /dev/dri + /dev/nvidia* bind-mounted into container
- **9P namespace**: per-container Styx socket for /wubu, /dev, /prog
- **SteamOS preset**: Arch root + Steam Runtime + Proton + GPU passthrough + DRM/KMS

## Systemic Risks

| # | Risk | Sev | Truth |
|---|------|-----|-------|
| S1 | VSL is dispatch table, not Linux compat | 🟡 | Replaced by wubu_host_exec.c (real fork+exec) |
| S2 | Proton cannot load PEs | 🔴 | Use host Wine via platform delegation. |
| S3 | Bare-metal ≠ rip Linux drivers | 🔴 | ZealOS boot + WuBuOS as HolyC shell. |
| S4 | SteamOS needs real Linux process execution | ✅ | Cell 203: host fork+exec. |
| S5 | LOC was inflated (claimed 41K, real 11K) | ✅ | Honest accounting done. |

## Old Battleship (001-110) — ARCHIVED
Tracked API signatures, not behavior. See vault/achievements.md for history.
