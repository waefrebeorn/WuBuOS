# WuBuOS — ZealOS Kernel + Win98 Shell + Styx/9P + Arch Containers

```
╔══════════════════════════════════════════════════════════════════════╗
║     🌱  W U B U O S                                                       ║
║     ZealOS kernel · Win98 shell · Styx/9P namespace · Arch containers    ║
║     468 C files · 214 H files · ~105K LOC · 91 test targets             ║
║     (verified 2026-07-19 from `git ls-files` + `make` targets)           ║
╚══════════════════════════════════════════════════════════════════════════╝
```

## What WuBuOS is

**WuBuOS = ZealOS kernel + Win98 shell + Styx/9P + Arch containers**, built as a
single hosted binary that runs on Linux. It merges five lineage studies into one
OS-scale C11 codebase:

- **ZealOS** — the hosted kernel (memory, tasking, VBE, FAT32, AHCI, interrupt, PS/2)
- **Win98/XP shell** — WM, desktop, startmenu, explorer, terminal (DOS-box windows)
- **Styx/9P** — a real filesystem namespace backed by `.wubu` containers (9P2000)
- **Arch containers** — fork+exec into an Arch Linux rootfs (bwrap isolation)
- **HolyC JIT** — self-hosted x86-64 encoder, disassembler, register allocator, minic compiler

Plus three engines that make it an "OS-scale" project rather than a GUI shell:

- **16-bit DOS compatibility** — a real 8086 interpreter + INT 21h/10h/16h DOS layer
  that runs `.COM`/`.EXE` in-process and captures a text screen + RGBA frame for the
  Desktop "compatible window" (`src/runtime/wubu_dos_emu*`, 22/22 regression tests).
- **VSL (Virtual Syscall Layer)** — the bridge: NT → Linux → Styx/9P → ZealOS →
  HolyC JIT. Dispatched through a single machine-readable manifest
  (`src/runtime/wubu_manifest/`) that also generates the Styx9P op enum + HolyC FFI.
- **Bear RL** — PPO training with Vulkan compute pipelines (`src/bear/`).

## Repository layout

```
src/
  kernel/    memory, tasking, VBE, FAT32, AHCI, interrupt, PS/2
  compiler/  HolyC lexer, parser, codegen, PTX backend
  audio/     DAW, Furnace (30+ chips), TinySoundFont, AI plugins
  hosted/    DRM/KMS, Vulkan, X11, WSL2, macOS AVF
  runtime/   Styx/9P, VSL, containers, Arch, network, snapshot,
             16-bit DOS emulator, unified syscall manifest
  gui/       Win98 WM, desktop, startmenu, explorer, terminal
  bear/      RL training, Vulkan/CUDA, n-pole physics
  apps/      Editor, canvas, codec, freedoom, calc, control
  tools/     ISO9660, screenshot, weight_check, demo_record
  bridge/    DOS flip, syscall bridge
  shell/     Unified GUI shell
  worldsim/  GAAD, terrain, entity, physics
```

> **Module hygiene (2026-07-15 → 07-19):** large monoliths were dissolved into
> opaque C11 leaf modules with minimal includes — `wubu_editor.c` → `src/apps/editor/`,
> `wubu_canvas.c` → `src/apps/app_canvas*` + `wubu_canvas_*`, `styx.c` →
> `src/runtime/styx_*`, `wubu_clipboard.c` → `src/gui/wubu_clipboard*`,
> `wubu_proton.c` → `src/runtime/wubu_proton*` + `wubu_proton_dxvk*`, etc.
> See `docs/MONOLITH_DISSOLUTION.md` for the full old→new map. Docs that still name
> the old monolith files are out of date.

## Quick Start

```bash
make all                 # full build
make hosted              # hosted binary (runs on Linux)
./src/hosted/wubu --screenshot /tmp/screenshot.ppm

make test                # all 91 test targets
make test_dos_emu        # 16-bit DOS emulator (22 regression tests)
make test_dos_emu_smoke  # minimal DOS .COM run
make test_manifest       # unified syscall manifest (15 checks)
make test_vsl            # VSL syscalls (87 checks)

# Honest gap scan
python3 scripts/wubu_manifest_gen.py --help   # manifest codegen entry point
```

## Status (verified 2026-07-19)

| Metric | Value | Source |
|--------|-------|--------|
| C source files | 468 | `git ls-files 'src/**/*.c'` |
| Header files | 214 | `git ls-files 'src/**/*.h'` |
| Lines of code (src, tracked) | ~105,459 | `git ls-files … \| xargs wc -l` |
| Test targets (`make test_*`) | 91 | `grep -c '^test_.*:' Makefile` |
| Build | `make runtime` exits 0, clean under `-O2` | verified |
| Repo location | `/home/wubu/wubuos` | canonical |

### Known open work (not "all done")
- **E1 ReactOS NT**: 88 / 297 syscalls transliterated (209 remain).
- **Stub-phrase no-ops** still present: `tasking.c`×2, `wubu_anticheat.c`×2,
  `bear_cudnn.c`×3, `wubu_screenshot.c`, `wubu_pkgmgr.c`, `oci_http_client.c`,
  `holyc_ptx.c`, `wubu_compositor_standalone.c`×2, `wubu_compositor.c`, `wubu_bottles.c`.
- **Bare-metal no-ops**: `tasking.c`×3 (context-switch).

> Historical note: an earlier audit (`BATTLESHIP.md` v22, 2026-07-08) described the
> project as "~40 code gaps + ~370 parity marathons, 64 targets, ~15K LOC". Those
> figures are stale — the repo has since grown to 468 `.c` / ~105K LOC / 91 targets
> through the monolith-dissolution campaign. This README reflects the 2026-07-19
> verified state.

## The Mission

WuBuOS merges TempleOS (HolyC/JIT), ReactOS (NT syscall emulation), SteamOS
(Proton/gamescope), Arch/Ubuntu (pacman/systemd), and Win98 (shell) into one
hosted binary. The VSL is the bridge: NT → Linux → Styx/9P → ZealOS → HolyC JIT.

**Discipline**: opaque structs, minimal includes, C11 only. "Rewriting from scratch
in C" = closing a gap — including every ReactOS-NT syscall and every missing
SteamOS/Ubuntu/TempleOS/ZealOS subsystem, which are reclassified as REAL_GAP
marathons.

## Security note

The `src/runtime/container/wubucontainer` tree is a git **submodule** (WuBuContainer).
On 2026-07-19 its `shToElf/stub.c` self-extracting shell-ELF dropper was removed
(commit `8f480e0` in that repo; parent pin updated). It contains no WuBuOS code.

## License

WuBuOS — MIT License (ZealOS kernel under its own license).
