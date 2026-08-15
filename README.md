# WuBuOS — the BODY of the WuBu AGI

**ZealOS kernel · Win98 shell · Styx/9P namespace · Arch containers · WuBuNOS
compiler — one hosted binary that runs on Linux, and a measured-boot chain that
runs on metal.** WuBuOS merges five lineage studies into one OS-scale C11
codebase. The Brain (`wubuwizard`) learns; the Body protects, hosts, and acts;
**WuBuNOS** is the compiler that targets every ISA — the HolyC JIT with 11
backends, the from-scratch C11 toolchain that compiles ON the kernel.

- **Code lives on GitHub** — [`waefrebeorn/WuBuOS`](https://github.com/waefrebeorn/WuBuOS)
- **Models + datasets live on HuggingFace** — the [`WaefreBeorn` org](https://huggingface.co/WaefreBeorn)
  (the WuBu-35M seed the Body hosts on metal).

## What WuBuOS is

- **ZealOS** — the hosted kernel (memory, tasking, VBE, FAT32, AHCI, interrupt, PS/2)
- **Win98/XP shell** — WM, desktop, startmenu, explorer, terminal (DOS-box windows)
- **Styx/9P** — a real filesystem namespace backed by `.wubu` containers (9P2000)
- **Arch containers** — fork+exec into an Arch Linux rootfs (bwrap isolation)
- **HolyC JIT** — self-hosted x86-64 encoder, disassembler, register allocator, minic compiler

Plus three engines that make it OS-scale:

- **16-bit DOS compatibility** — a real 8086 interpreter + INT 21h/10h/16h DOS
  layer that runs `.COM`/`.EXE` in-process (`src/runtime/wubu_dos_emu*`, 22/22 tests)
- **VSL (Virtual Syscall Layer)** — the bridge: NT → Linux → Styx/9P → ZealOS →
  HolyC JIT, dispatched through a single machine-readable manifest
- **Bear RL** — PPO training with Vulkan compute pipelines (`src/bear/`)

## Quick start

```bash
make all                 # full build (kernel jit compiler runtime tools gui
                         #   bridge apps worldsim metal audio shell bear)
make hosted              # hosted binary (runs on Linux)
./src/hosted/wubu --screenshot /tmp/screenshot.ppm

make test                # all 124 test targets
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
  audio/     DAW, Furnace (30+ chips), TinySoundFont, AI plugins
  hosted/    DRM/KMS, Vulkan, X11, WSL2, macOS AVF
  runtime/   Styx/9P, VSL, containers, Arch, network, DOS emulator, syscall manifest
  gui/       Win98 WM, desktop, startmenu, explorer, terminal
  bear/      RL training, Vulkan/CUDA, n-pole physics
  apps/      Editor, canvas, codec, freedoom, calc, control
  bridge/    DOS flip, syscall bridge
  shell/     Unified GUI shell
  worldsim/  GAAD, terrain, entity, physics
```

## Status (verified 2026-08-04)

| Metric | Value |
|---|---|
| C source files | 665 (`git ls-files 'src/**/*.c'`) |
| C header files | 306 (`git ls-files 'src/**/*.h'`) |
| Total C LOC | 187,689 (`find src -name '*.c' -o -name '*.h' | xargs wc -l`) |
| Test targets | 124 (`make test` — critical/high/medium tiers) |
| Measured-boot AGI gate | `make test_agi_metal` = **PASS** |
| Build | `make all` / `make hosted` exit 0 |
| E1 ReactOS NT | 88/297 syscalls transliterated (209 remain) |

**Honest — not "all done"**: the remaining VSL syscalls, the bare-metal
context-switch no-ops, and the stub-phrase spots are tracked openly in
`BATTLESHIP_GAPS.md`, the `REACTOS_NT_SYSCALL_STUDY.md` audit, and the
compendium ledger.

## Docs

- [docs/BOOT_CHAIN.md](docs/BOOT_CHAIN.md) — the measured-boot spine
- [docs/BUILDING.md](docs/BUILDING.md) — build, test, ship (ISO/USB/QEMU)
- [docs/MODULES.md](docs/MODULES.md) — full annotated module table (auto-generated)
- [OS_BIBLE.md](OS_BIBLE.md) — the design bible (vision, architecture, principles)
- [STATE.md](STATE.md) — current state, verified on date
- [AGI_OS.md](AGI_OS.md) — the AGI-OS architecture
- [BATTLESHIP.md](BATTLESHIP.md) + [BATTLESHIP_GAPS.md](BATTLESHIP_GAPS.md) — the gap board
- [docs/compendium/](docs/compendium/) — the institutional ledger (01-reference regenerates via `make docs`)

## License

[Waefrebeorn Umbrella License v3.0](LICENSE) — source-available, not OSI/FSF
approved.
