# WuBuOS Mind Palace Index

> **Verified 2026-07-19:** 468 `.c` / 214 `.h` / ~105K LOC / **91 test targets**.
> The older v22 docs (BATTLESHIP.md, this index's historical blocks) say
> "~40 + ~370 gaps, 64 targets, ~15K LOC" — those are stale; see
> `docs/MONOLITH_DISSOLUTION.md`. This index points to current material; the
> study/reference trees under `os-studies/`, `reference/`, and `wiki/` are
> intentionally historical analyses and may carry older figures.

## Core Architecture
- [BATTLESHIP.md](BATTLESHIP.md) — REAL_GAP board (v22 lineage; numbers stale, see note)
- [README.md](README.md) — Project overview, architecture, quick start (verified 2026-07-19)
- [STATE.md](STATE.md) — Current state, progress (verified 2026-07-19)
- [OS_BIBLE.md](OS_BIBLE.md) — Complete OS specification
- [docs/MONOLITH_DISSOLUTION.md](docs/MONOLITH_DISSOLUTION.md) — old monolith → new module map

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
- [src/runtime/](src/runtime/) — Styx/9P, VSL, containers, Arch, network, snapshot, **16-bit DOS emulator** (`wubu_dos_emu*`)
- [src/gui/](src/gui/) — Win98 WM, desktop, startmenu, explorer, terminal
- [src/bear/](src/bear/) — RL training, Vulkan/CUDA, n-pole physics
- [src/apps/](src/apps/) — Editor, canvas, codec, freedoom, calc, control
- [src/tools/](src/tools/) — ISO9660, screenshot, weight_check, demo_record
- [src/bridge/](src/bridge/) — DOS flip, syscall bridge
- [src/shell/](src/shell/) — Unified GUI shell
- [src/worldsim/](src/worldsim/) — GAAD, terrain, entity, physics

## Test Targets (make test_XXX) — 91 targets
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
| test_vsl | VSL syscalls (87) |
| test_bridge | Syscall bridge (25) |
| test_bridge_flip | DOS flip (13) |
| test_holyc_ptx | PTX backend (31) |
| test_wallpaper | Desktop wallpaper (18) |
| test_dos_emu | 16-bit DOS emulator (22) |
| test_dos_emu_smoke | Minimal DOS .COM run |
| test_manifest | Unified syscall manifest (15) |
| … | (full gate = 91 targets) |

## Key Skills (Hermes)
- `systems-programming` — OS dev, kernel, JIT, GUI, daemons, C pitfalls
- `wubuos-battleship-gaps` — BATTLESHIP gap analysis (v22 honest ~400)
- `wubuos-architecture` — WuBuOS architecture + gap-scanner methodology
- `wubuos-masterpiece-architecture` — Arch NT + Proton + HolyC DOS
- `wubuos-zealos-parity` — ZealOS→WuBuOS 1:1 name mapping
- `wubuos-container-isolation` — cgroups v2 + seccomp-bpf
- `wubuos-jit-self-hosted` — x86-64 encoder, disassembler, minic compiler
