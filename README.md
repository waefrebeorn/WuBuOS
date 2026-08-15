# WuBuOS — the BODY of the WuBu AGI

**ZealOS kernel · Win98 shell · Styx/9P namespace · 20+ hardware drivers ·
Arch containers · WuBuNOS compiler — one hosted binary that runs on Linux,
and a measured-boot chain that runs on metal.**

WuBuOS is the **Body** of the WuBu AGI — the operating system that protects,
hosts, and acts. It is one of three repositories that form the complete system:

| Repo | Role | LOC |
|------|------|-----|
| **wubuos** (this repo) | THE BODY — kernel, GUI, drivers, runtime | 472,955 |
| **wubuwizard** | THE BRAIN — inference engine, training, KV cache | 218,100 |
| **wubunos** | THE COMPILER — HolyC JIT, 11 ISA backends | 14,115 |
| **TOTAL** | **Three repos, one AGI** | **705,170** |

- **Code lives on GitHub** — [`waefrebeorn/WuBuOS`](https://github.com/waefrebeorn/WuBuOS)
- **Models + datasets live on HuggingFace** — the [`WaefreBeorn` org](https://huggingface.co/WaefreBeorn)
- **Unified vision** — [VISION.md](VISION.md) explains the three-repo architecture

## What WuBuOS is

WuBuOS is the operating system layer. It provides:

- **ZealOS kernel** — memory, tasking, VBE framebuffer, FAT32/TXFS/AHCI, interrupt controller, PS/2
- **Win98/XP shell** — window manager, desktop, startmenu, explorer, terminal (DOS-box windows)
- **Styx/9P namespace** — a real filesystem namespace backed by `.wubu` containers (9P2000)
- **Hardware drivers** — 20+ GPU, NVMe, network, HDA, battery, SD, USB, thermal (Steam Deck + laptop IDs)
- **VSL (Virtual Syscall Layer)** — multi-OS dispatch: Linux, Windows NT (ReactOS), macOS
- **Container runtime** — Arch Linux containers via bwrap, Proton/Wine for Windows games
- **Measured boot** — WuBuFW UEFI firmware (no EDK2), TPM PCR4 attestation, chainloader
- **DOS compatibility** — in-process 8086 interpreter + INT 21h/10h/16h (22/22 tests)

The Brain (wubuwizard) and Compiler (wubunos) link into WuBuOS as submodules:
- `src/brain/` → wubuwizard
- `src/compiler/` → wubunos

Together they form a complete AGI system: the Body hosts, the Brain thinks, the Compiler builds.

## Quick start

```bash
make all                 # full build (kernel jit compiler runtime tools gui
                         #   bridge apps worldsim metal audio shell bear)
make hosted              # hosted binary (runs on Linux)
./src/hosted/wubu --screenshot /tmp/screenshot.ppm

make test                # all 414 test targets
make test_agi_metal      # the measured-boot/AGI gate (the root of trust)
build_iso.sh             # bootable ISO (kernel + limine)
qemu-test.sh             # boot it under QEMU
```

Details: [docs/BUILDING.md](docs/BUILDING.md). The submodule
`src/runtime/container/wubucontainer` needs `git submodule update --init`.

## The measured boot chain (why this is an AGI body, not a toy OS)

```
WuBuFW (src/firmware — UEFI from scratch, no EDK2)
  └─ measures the kernel → PCR4 + AuthentiCode (TPM)
  └─ chainloader reads KERNEL.ELF → SHA-256 → attestation handoff
  └─ ExitBootServices → crt0 → kernel_main
  └─ AGI supervisor (wubu_agi_kernel) with the root-of-trust gate LIVE
     (verified: make test_agi_metal = PASS)
```

The kernel carries the AGI organs: the 5+1 rollback recovery, the hive port
(the AGI's memory, kernel-side), attestation, and the HX human-model family.
Full spine: [docs/BOOT_CHAIN.md](docs/BOOT_CHAIN.md).

## Repo layout

```
src/
  kernel/    memory, tasking, VBE, FAT32, AHCI, interrupt, PS/2, recovery, AGI organs
  firmware/  WuBuFW: UEFI from scratch, TPM, secureboot, the chainloader (wubufw.fd)
  compiler/  HolyC lexer, parser, codegen, PTX backend
  jit/       WuBuNOS JIT: x86-64, ARM64, RISC-V 64, WASM backends, regalloc, minic
  audio/     DAW, Furnace (30+ chips), TinySoundFont, AI plugins
  hosted/    DRM/KMS, Vulkan, X11, WSL2, macOS AVF
  runtime/   Styx/9P, VSL, containers, Arch, network, DOS emulator, syscall manifest
  gui/       Win98 WM, desktop, startmenu, explorer, terminal
  bear/      RL training, Vulkan/CUDA, n-pole physics
  brain/     wubuwizard integration (encoders, optimizers, attention, diffusion)
  apps/      Editor, canvas, codec, freedoom, calc, control
  bridge/    DOS flip, syscall bridge
  shell/     Unified GUI shell
  worldsim/  GAAD, terrain, entity, physics
  framework/ WuBuFX application framework
  tools/     iso9660, screenshot, weight-check, test harnesses
tools/
  bench/              benchmarks
  dev/                developer utilities
  isa-test/           ISA-level test suites
  kvctools/           KV cache tools
  peephole_superopt/  peephole superoptimizer
  probe/              hardware probes
  research/           research scripts
  wubu_game_probe/    game probe tooling
docs/
  adr/                architecture decision records
  compendium/         institutional ledger (01-reference regenerates via `make docs`)
  reference/          reference documentation
  research/           research outputs (7hop-drivers: 203 Kevin-Bacon docs)
  summaries/          module summaries
  wiki/               wiki pages
  x86/                x86 architecture notes
vault/                archives (accomplishments, phases, planning)
```

## Status (verified 2026-08-15)

| Metric | Value |
|---|---|
| C source files | 2,463 |
| C header files | 1,006 |
| Total C LOC | 472,955 |
| Test targets | 414 (`make test` — critical/high/medium tiers) |
| Measured-boot AGI gate | `make test_agi_metal` = **PASS** |
| Build | `make all` / `make hosted` exit 0 |
| WuBuNOS backends | 11 ISA targets (x86-64, ARM64, RISC-V 64, WASM, PTX, …) |
| WuBu compliance | `_GNU_SOURCE` eliminated — replaced with `WUBU_HOSTED` + `wubu_gnu_compat.h` |

**Honest — not "all done"**: the remaining VSL syscalls, the bare-metal
context-switch no-ops, and the stub-phrase spots are tracked openly in
`BATTLESHIP_GAPS.md`, the `REACTOS_NT_SYSCALL_STUDY.md` audit, and the
compendium ledger.

## Docs

- [VISION.md](VISION.md) — the unified three-repo architecture (must-read)
- [docs/BOOT_CHAIN.md](docs/BOOT_CHAIN.md) — the measured-boot spine
- [docs/BUILDING.md](docs/BUILDING.md) — build, test, ship (ISO/USB/QEMU)
- [docs/MODULES.md](docs/MODULES.md) — full annotated module table (auto-generated)
- [OS_BIBLE.md](OS_BIBLE.md) — the design bible (vision, architecture, principles)
- [STATE.md](STATE.md) — current state, verified on date
- [BATTLESHIP.md](BATTLESHIP.md) — the gap board (v23)
- [docs/compendium/](docs/compendium/) — the institutional ledger

## License

[Waefrebeorn Umbrella License v3.0](LICENSE) — source-available, not OSI/FSF
approved.