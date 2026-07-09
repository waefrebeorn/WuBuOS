# WuBuOS Mind Palace Index

## Core Architecture
- [BATTLESHIP.md](BATTLESHIP.md) — ~400 REAL_GAPs v22: ~40 code-level (Part 1) + ~370 parity marathons (Part 2) + plumber deep-dive (Part 3) + DA audit (Part 4)
- [README.md](README.md) — Project overview, architecture, quick start (honest v22)
- [STATE.md](STATE.md) — Current state, progress, vaulted accomplishments (v22)
- [OS_BIBLE.md](OS_BIBLE.md) — Complete OS specification

## Gallery
- [screenshots/](screenshots/) — Live screenshots of the WuBuOS desktop, apps, and themes

## Audit & Analysis
- [WUBUOS_DISTRO_ROADMAP.md](WUBUOS_DISTRO_ROADMAP.md) — Distro build tier map (DA-gap-aligned, extended 2026-07-08)
- [goal-paste.md](goal-paste.md) — Current campaign goal paste
- [slate.md](slate.md) — Active work surface (v26)
- [REACTOS_NT_SYSCALL_STUDY.md](REACTOS_NT_SYSCALL_STUDY.md) — ReactOS NT syscall mapping for VSL bridge (297 mapped, 0 implemented → EPIC E1)
- [DESKTOP_FIXUP_PLAN.md](DESKTOP_FIXUP_PLAN.md) — Desktop fixup (ReactOS-learned), Streams 1-4
- [vault/](vault/) — Vaulted accomplishments (ACCOMPLISHMENTS_2026-07-08.md)

## Roadmap & Planning
- [BATTLESHIP.md](BATTLESHIP.md) — Active REAL_GAPs only (roadmap source)
- [WUBUOS_DISTRO_ROADMAP.md](WUBUOS_DISTRO_ROADMAP.md) — Tier-based component breakdown

## Implementation References
- [Makefile](Makefile) — Build system, test targets
- Gap-scanner: `~/.hermes/profiles/mind-palace/skills/software-development/wubuos-battleship-gaps/scripts/find_real_gaps.py`

## Source Directories
- [src/kernel/](src/kernel/) — Memory, tasking, VBE, FAT32, AHCI, interrupt, PS/2
- [src/compiler/](src/compiler/) — HolyC lexer, parser, codegen, PTX backend
- [src/audio/](src/audio/) — DAW, Furnace (30+ chips), TinySoundFont, AI plugins
- [src/hosted/](src/hosted/) — DRM/KMS, Vulkan, X11, WSL2, macOS AVF
- [src/runtime/](src/runtime/) — Styx/9P, VSL, containers, Arch, network, snapshot
- [src/gui/](src/gui/) — Win98 WM, desktop, startmenu, explorer, terminal
- [src/bear/](src/bear/) — RL training, Vulkan/CUDA, n-pole physics
- [src/apps/](src/apps/) — Editor, canvas, codec, freedoom, calc, control
- [src/tools/](src/tools/) — ISO9660, screenshot, weight_check, demo_record
- [src/bridge/](src/bridge/) — DOS flip, syscall bridge
- [src/shell/](src/shell/) — Unified GUI shell
- [src/worldsim/](src/worldsim/) — GAAD, terrain, entity, physics

## Test Targets (make test_XXX) — 64 targets, 747+ assertions GREEN
| Target | Description |
|--------|-------------|
| test_jit | JIT compiler (82) |
| test_holyc | HolyC compiler (84) |
| test_network | Network stack (139) |
| test_snapshot | Snapshot/restore (132) |
| test_archd | Arch daemon (16) |
| test_holyd | HolyC DOS daemon (31) |
| test_oci | OCI runtime (10) |
| test_proton | PE exec (32) |
| test_proton2 | Proton + GameScope (14) |
| test_bottles | .wubu bottles (16) |
| test_dosgui_wm | Window manager (16) |
| test_dosgui_explorer | File explorer (74) |
| test_dosgui_term | Terminal (PTY, tabs, ANSI) |
| test_styx | 9P protocol (29) |
| test_styxfs | 9P file server (11) |
| test_memory | Kernel heap (15+) |
| test_tasking | Task scheduler (10) |
| test_fat32 | FAT32 filesystem (20) |
| test_txfs | TXFS (25) |
| test_ahci | AHCI disk (16) |
| test_audio | Audio engine (14) |
| test_vsl | VSL syscalls (52) |
| test_bridge | Syscall bridge (25) |
| test_bridge_flip | DOS flip (13) |
| test_holyc_ptx | PTX backend (31) |
| test_wallpaper | Desktop wallpaper (18) |
| … | (full gate = 64 targets) |

## Key Skills (Hermes)
- `systems-programming` — OS dev, kernel, JIT, GUI, daemons, C pitfalls
- `wubuos-battleship-gaps` — BATTLESHIP gap analysis (v22 honest ~400)
- `wubuos-architecture` — WuBuOS architecture + gap-scanner methodology
- `wubuos-masterpiece-architecture` — Arch NT + Proton + HolyC DOS
- `wubuos-zealos-parity` — ZealOS→WuBuOS 1:1 name mapping
- `wubuos-container-isolation` — cgroups v2 + seccomp-bpf
- `wubuos-jit-self-hosted` — x86-64 encoder, disassembler, minic compiler
