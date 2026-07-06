# WuBuOS Mind Palace Index

## Core Architecture
- [BATTLESHIP.md](BATTLESHIP.md) — ~3000 REAL_GAP audit (triple DA verified)
- [README.md](README.md) — Project overview, architecture, quick start
- [STATE.md](STATE.md) — Current state, progress, vaulted accomplishments
- [OS_BIBLE.md](OS_BIBLE.md) — Complete OS specification

## Audit & Analysis
- [WUBUOS_DISTRO_ROADMAP.md](WUBUOS_DISTRO_ROADMAP.md) — Distro build tier map (DA-gap-aligned)
- [goal-paste.md](goal-paste.md) — Current campaign goal paste
- [slate.md](slate.md) — Active work surface
- [REACTOS_NT_SYSCALL_STUDY.md](REACTOS_NT_SYSCALL_STUDY.md) — ReactOS NT syscall mapping for VSL bridge

## Roadmap & Planning
- [BATTLESHIP.md](BATTLESHIP.md) — Active REAL_GAPs only (roadmap source)
- [WUBUOS_DISTRO_ROADMAP.md](WUBUOS_DISTRO_ROADMAP.md) — Tier-based component breakdown

## Implementation References
- [Makefile](Makefile) — Build system, test targets
- [CARTPOLE8_MASTER_RULES.h](CARTPOLE8_MASTER_RULES.h) — Physics verification rules

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

## Test Targets (make test_XXX)

| Target | Description |
|--------|-------------|
| test_jit | JIT compiler (82 assertions) |
| test_holyc | HolyC compiler (84 assertions) |
| test_network | Network stack (139 assertions) |
| test_snapshot | Snapshot/restore (132 assertions) |
| test_archd | Arch daemon (16 assertions) |
| test_holyd | HolyC DOS daemon (31 assertions) |
| test_daemon_panel | Desktop-daemon bridge (21 assertions) |
| test_oci | OCI runtime (10 assertions) |
| test_proton | PE exec (32 assertions) |
| test_proton2 | Proton + GameScope (14 assertions) |
| test_anticheat | Anti-cheat research (14 assertions) |
| test_bottles | .wubu bottles (16 assertions) |
| test_dosgui_wm | Window manager (16 assertions) |
| test_dosgui_explorer | File explorer (74 tests) |
| test_dosgui_term | Terminal (PTY, tabs, ANSI) |
| test_styx | 9P protocol (29 assertions) |
| test_styxfs | 9P file server (11 assertions) |
| test_memory | Kernel heap (15+ assertions) |
| test_tasking | Task scheduler (10 assertions) |
| test_fat32 | FAT32 filesystem (20 assertions) |
| test_txfs | TXFS (25 assertions) |
| test_ahci | AHCI disk (16 assertions) |
| test_audio | Audio engine (14 assertions) |
| test_gc | Garbage collector (10 assertions) |
| test_vsl | VSL syscalls (52 assertions) |
| test_bridge | Syscall bridge (25 assertions) |
| test_bridge_flip | DOS flip (13 assertions) |
| test_holyc_ptx | PTX backend (31 assertions) |
| test_wubu | Full integration (47 assertions) |
| test_hosted | Hosted binary |
| test_host_exec | Host execution |
| test_metal | Metal platform |
| test_apps | Apps (15 assertions) |
| test_apps2 | Apps v2 (16 assertions) |
| test_gamelib | Game library |
| test_pkgmgr | Package manager |
| test_deploy | Deploy |
| test_screenshot | Screenshot (9 assertions) |
| test_gui_screenshot | GUI screenshot (11 assertions) |
| test_mime | MIME |
| test_trash | Trash |

## Key Skills (Hermes)
- `systems-programming` — OS dev, kernel, JIT, GUI, daemons, C pitfalls
- `wubuos-masterpiece-architecture` — Arch NT + Proton + HolyC DOS
- `wubuos-zealos-parity` — ZealOS→WuBuOS 1:1 name mapping
- `wubuos-container-isolation` — cgroups v2 + seccomp-bpf
- `wubuos-jit-self-hosted` — x86-64 encoder, disassembler, minic compiler
- `wubuos-wayland-migration` — Wayland client migration
- `wubuos-fable-porting` — Mythos Fable windowing agent port