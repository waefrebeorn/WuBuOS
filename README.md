# WuBuOS — ZealOS Kernel + Win98 Shell + Styx/9P + Arch Containers

```
╔══════════════════════════════════════════════════════════════════════╗
║     🌱  W U B U O S                                                       ║
║     ZealOS kernel · Win98 shell · Styx/9P namespace · Arch containers    ║
║     613 C files · 138 test targets · measured-boot AGI gate              ║
║     (verified 2026-08-04 from `git ls-files` + `make` targets)           ║
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
make all                 # full build (kernel jit compiler runtime tools gui
                         #   bridge apps worldsim metal audio shell bear)
make hosted              # hosted binary (runs on Linux)
./src/hosted/wubu --screenshot /tmp/screenshot.ppm

make test                # all 138 test targets
make test_agi_metal      # the measured-boot/AGI gate (the root of trust)
make test_dos_emu        # 16-bit DOS emulator (22 regression tests)
make test_dos_emu_smoke  # minimal DOS .COM run
make test_manifest       # unified syscall manifest (15 checks)
make test_vsl            # VSL syscalls (87 checks)

build_iso.sh             # bootable ISO (kernel + limine)
qemu-test.sh             # boot it under QEMU
```

Docs: [BOOT_CHAIN.md](docs/BOOT_CHAIN.md) (the measured-boot spine),
[BUILDING.md](docs/BUILDING.md) (build + test + ship), the
[OS_BIBLE.md](OS_BIBLE.md) (design bible), `docs/compendium/` (institutional
ledger).

# Honest gap scan
python3 scripts/wubu_manifest_gen.py --help   # manifest codegen entry point
```

## Status (verified 2026-08-04)

| Metric | Value | Source |
|--------|-------|--------|
| C source files | 613 | `git ls-files 'src/**/*.c'` |
| Header files | 301 | `git ls-files 'src/**/*.h'` |
| Test targets (`make test_*`) | 138 | `grep -cE '^test_[a-z_0-9]*:' Makefile` |
| Build | `make all` / `make hosted` exit 0 | verified |
| Measured-boot AGI gate | `make test_agi_metal` = PASS | verified |
| Repo location | `/home/wubu/wubuos` | canonical |

### Known open work (not "all done")
- **E1 ReactOS NT**: 88 / 297 syscalls transliterated (209 remain).
- **Stub-phrase no-ops** still present: `tasking.c`×2, `wubu_anticheat.c`×2,
  `bear_cudnn.c`×3, `wubu_screenshot.c`, `wubu_pkgmgr.c`, `oci_http_client.c`,
  `holyc_ptx.c`, `wubu_compositor_standalone.c`×2, `wubu_compositor.c`, `wubu_bottles.c`.
- **Bare-metal no-ops**: `tasking.c`×3 (context-switch).

> Historical note: an earlier audit (`BATTLESHIP.md` v22, 2026-07-08) described the
> project as "~40 code gaps + ~370 parity marathons, 64 targets, ~15K LOC". Those
> figures are stale — the repo has since grown to 613 `.c` / 138 targets through
> the monolith-dissolution campaign and the kernel/firmware/GUI waves. This README
> reflects the 2026-08-04 verified state.

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

---

## License

This project is licensed under the **Waefrebeorn Umbrella License v3.0**.
See the [LICENSE](LICENSE) file for the full license text.

The Waefrebeorn Umbrella License is a custom source-available license.
It is not OSI-approved and not FSF-approved.

<!-- repodoc:BEGIN -->
## Module index (auto-generated 2026-08-04)

- **540 C modules** — full annotated table: [docs/MODULES.md](docs/MODULES.md)
- **1 test tools** (make targets `test_*`, e.g. `test_acpi, test_agi_kernel, test_agi_metal, test_ahci, test_ahcifat, test_anticheat, test_apps, test_apps2, test_arch, test_archd...`)
- **0 research docs** — full ledger: [research/INDEX.md](research/INDEX.md)

Regenerate with: `python3 tools/repodoc/repodoc.py . --readme`
<!-- repodoc:END -->
