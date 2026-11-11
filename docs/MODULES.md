# WuBuOS — Module Map

**Hand-checked 2026-11-11.** This file is no longer a generated catalogue. Its
old header promised re-generation through a repository-doc tool that was never
in the tree, and a 264-dead-reference audit proved the table was stale —
it omitted the runtime/VKD3D/spells work and named compiler files that were
rewritten months ago. A catalogue that is not regenerated is not a catalogue;
it is a fossil.

Every path below exists in the tree, and `test_docs` fails the build if one
stops existing. For a single module's purpose, read the module's own header —
that comment is the only description that cannot drift from the code.

## The layers (5,067 files, 750,001 LOC in `src/`)

| layer | files | LOC | what it is for |
|---|---|---|---|
| `src/firmware/` | 54 | 9,665 | **WuBuFW** — UEFI from scratch: PCI, ACPI, AHCI, NVMe, XHCI, GOP, TPM, Secure Boot. Entry: `src/firmware/fw_main.c`. |
| `src/kernel/` | 911 | 79,714 | memory, tasking, interrupts, FAT32/TXFS, drivers, recovery. Entry: `src/kernel/metal_main.c`. |
| `src/runtime/` | 534 | 90,047 | the namespace, VSL, containers, networking, all compatibility. Entry: `src/runtime/vsl/`. |
| `src/gui/` | 285 | 68,722 | the Win98/XP shell — WM, desktop, start menu, terminal. Entry: `src/gui/wm.c`. |
| `src/apps/` | 943 | 104,435 | the programs that ship with the shell. |
| `src/hosted/` | 40 | 7,271 | the binary that runs on Linux today. Entry: `src/hosted/hosted.c`. |
| `src/jit/` | 53 | 13,109 | native JIT: x86-64, ARM64, RISC-V, WASM; the JIT expression compiler. |
| `src/spells/` | 53 | 9,950 | the spell book — reusable third-party modules. Entry: `src/runtime/wubu_spellbook.c`. |
| `src/bear/` | 57 | 14,581 | RL training, Vulkan/CUDA. |
| `src/bridge/` | 10 | 2,423 | DOS flip, syscall bridge. |
| `src/audio/` | 10 | 2,890 | DAW, Furnace chips, AI plugins. |
| `src/tools/` | 22 | 6,120 | iso9660, screenshot, test harnesses. |
| `src/worldsim/` | 7 | 908 | GAAD, terrain, physics. |
| `src/shell/` | 7 | 789 | the unified GUI shell glue. |
| `src/framework/` | 6 | 682 | the WuBuFX app framework. |

## The guests (submodules — separate repositories)

| path | repo | files | LOC | what it is |
|---|---|---|---|---|
| `src/brain/` | **wubuwizard** | 1586 | 202,653 | inference, training, the KV cache. |
| `src/compiler/` | **WuBuNOS** | 253 | 71,101 | the HolyC compiler and its ISA backends. |
| `src/lua/` | — | 236 | 64,941 | embedded Lua/www tooling. |

## The subsystems that matter most right now

| subsystem | where | what it is |
|---|---|---|
| **VSL** — Virtual Syscall Layer | `src/runtime/vsl/` | multi-OS dispatch: Linux, Windows NT, macOS |
| **OCI / containers** | `src/runtime/oci/` | Arch containers via bwrap |
| **9P / Styx** | `src/runtime/styx*.c` | the `/n` namespace |
| **capability authority** | `src/runtime/wubu_cap/` | NT-style object capabilities |
| **spell book** | `src/spells/` | WuBu's DLL — reusable, gate-checked modules |

## The gates

| gate | what it proves |
|---|---|
| `make test` | 512 targets, 0 errors, 0 segfaults |
| `test_spellbook` | the spell book scans clean; a mis-filed spell fails the build |
| `test_docs` | every path the core docs name still exists |

**Read next:** [docs/MIND_PALACE.md](MIND_PALACE.md) — the verified walk through
the OS; [docs/SAGA.md](SAGA.md) — the project story from its commit log.
