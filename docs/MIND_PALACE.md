# The WuBuOS Mind Palace

A walk through the operating system, from the outside in. Every number and path
here was read off the tree on **2026-11-11**; `test_docs` fails the build if a
path in this file stops existing.

Read it in order. Each room states **what it is for** first and what is inside
it second. If a room's purpose is not clear in two sentences, that is a defect
in this document, not in you.

---

## The one-paragraph answer

WuBuOS is the **body** of an AGI system: an operating system that boots on real
metal through a UEFI firmware it builds itself, runs a ZealOS-derived kernel,
and then hosts a desktop, a Windows/Linux compatibility layer, and the AGI's
brain and compiler as submodules. It is not a distribution of Linux — it is a
small OS that *contains* Linux (and Windows, and macOS) as guests.

**Three repositories, one system:**

| repo | role | what it is |
|---|---|---|
| **WuBuOS** | the body | this tree — kernel, firmware, GUI, runtime, drivers |
| **wubuwizard** | the brain | inference, training, KV cache (`src/brain/`) |
| **WuBuNOS** | the compiler | HolyC lexer/parser/codegen, ISA backends (`src/compiler/`) |

---

## The walk

### Gate 0 — the front door

**For:** turning the source into something that runs.

- `Makefile` includes `mk/vars.mk`, `mk/objects.mk`, `mk/targets.mk`,
  `mk/pattern-rules.mk`, `mk/tests.mk` — the build is split by concern.
- `make all` builds everything; `make hosted` builds the binary that runs on
  Linux today; `make test` runs the gate.
- `docs/BUILDING.md` — build, test, ship.

**Enter here:** `Makefile`, `mk/`.

---

### Gate 1 — the firmware (WuBuFW)

**For:** getting from power-on to a measured kernel handoff, without EDK2.

A UEFI firmware written from scratch in C11: PCI, ACPI, AHCI, NVMe, XHCI, GOP,
TPM 2.0, SHA-256, RSA, Secure Boot. It measures the kernel into PCR4 and
attests it before the kernel runs.

**Inside:** 58 files, 9,665 LOC.

**Enter here:** `src/firmware/fw_main.c` (the entry), `fw_tpm.c`,
`fw_secureboot.c`, `fw_agi.c` (the attestation handoff).

**Proved by:** `make test_agi_metal` — the measured-boot gate.

**Read next:** `docs/BOOT_CHAIN.md`.

---

### Gate 2 — the kernel

**For:** memory, tasks, interrupts, filesystems, and drivers — the parts that
cannot be guests.

ZealOS-derived: ring-0, VBE framebuffer, FAT32, TXFS, AHCI, NVMe, PS/2, interrupt
controller, and the recovery/rollback machinery.

**Inside:** 58 files, 79,714 LOC — the second-largest tree after `brain`.

**Enter here:** `src/kernel/metal_main.c` (the entry), `memory.c`, `fat32.c`.

**Proved by:** the kernel tier of `make test`.

---

### Gate 3 — the runtime

**For:** the things an OS does *around* the kernel — the namespace, the syscall
layer, containers, networking, and every compatibility subsystem.

This is where most of the recent work lives.

**Inside:** 58 files, 90,047 LOC.

| subsystem | where | what it is |
|---|---|---|
| **VSL** — Virtual Syscall Layer | `src/runtime/vsl/` (49 files) | multi-OS syscall dispatch: Linux, Windows NT, macOS |
| **OCI / containers** | `src/runtime/oci/` (12 files) | Arch containers via bwrap |
| **9P / Styx** | `src/runtime/styx*.c` | the `/n` namespace |
| **capability authority** | `src/runtime/wubu_cap/` | NT-style object capabilities |
| **package management** | `wubu_pkg*.c`, `wubu_pkgbuild.c`, `wubu_aur.c` | pacman/makepkg/AUR analogues |
| **graphics translation** | `src/spells/graphics/render/vkd3d/` | the D3D12→Vulkan spell |
| **accessibility** | `wubu_atspi*.c`, `wubu_dbus.c` | AT-SPI2 over a D-Bus marshaller |
| **document/FS formats** | `wubu_doldoc.c`, `wubu_redsea.c`, `wubu_vga.c` | DolDoc, RedSea, raw VGA |

**Proved by:** the medium tier of `make test` (the largest tier).

---

### Gate 4 — the GUI

**For:** the Win98/XP shell — window manager, desktop, start menu, explorer,
terminal.

**Inside:** 58 files, 68,722 LOC.

**Enter here:** `src/gui/wm.c` (the window manager), `src/gui/shell/desktop.c`
(the desktop), `src/gui/shell/startmenu.c`, `src/gui/shell/taskbar_tasks.c`.

**The surfaces:** classic Win98 theme, a modern dark theme, and the newer
overlays — `wm_rim`, `wm_slime`, the taskbar.

**Proved by:** `test_wm` and the GUI tier.

---

### Gate 5 — the apps

**For:** the programs that ship with the shell.

**Inside:** 58 files, 104,435 LOC — editor, canvas, codec, calculator, control
panel, DOS window, games.

**Enter here:** `src/apps/`.

---

### Gate 6 — the guests (brain + compiler)

**For:** the AGI's mind and its language, hosted as submodules.

- `src/brain/` → **wubuwizard** (202,653 LOC) — encoders, optimizers, attention,
  diffusion, training.
- `src/compiler/` → **WuBuNOS** (71,101 LOC) — the HolyC compiler and its ISA
  backends.

Both are separate repositories, wired in as submodules.

---

### The spell book — reusable modules

**For:** the unit of reuse. A **spell** is a self-contained module filed in a
**book** (a broad need) under a **genre** (a category). Casting one is
fail-closed and idempotent.

```
src/spells/<book>/<genre>/<spell>/<spell>.spell   <- the manifest
                                    <sources>     <- the code
```

**Inside:** 58 files, 10,430 LOC — two spells: **vkd3d** (the from-scratch
D3D12→Vulkan translation layer) and **swarm** (the peer-to-peer aggregation
ledger, filed under `networking/aggregation`).

**Enter here:** `src/spells/README.md` (the doctrine), `wubu_spell.h` (the ABI),
`wubu_spellbook.c` (the registry).

**Cast one:** `make spells` (list), `make spellcheck` (validate), or
`./src/runtime/wubu_spell cast vkd3d`.

**Proved by:** `test_spellbook` — which scans the real tree, so a spell filed
wrong fails the build.

---

## The five numbers that matter

| | |
|---|---|
| C/H files in `src/` | **5,067** |
| Lines of C | **750,001** |
| Test targets | **512** |
| Spells | **2** — vkd3d, swarm |
| Gate | `make test` — 0 errors, 0 segfaults |

---

## Adding a room

1. Decide the room's **purpose** in one sentence. If you cannot, it is not a
   room yet.
2. Put it in the layer that owns that purpose — do not create a second owner.
3. Give it a test that asserts the behaviour you claim, not a restatement of the
   implementation.
4. If it is reusable, make it a **spell** and file it in the book.
5. Add it to this document, and keep the numbers honest.

## Reading rules for every document in this tree

- **Function and goal first.** The first three lines say what the thing is for.
- **No orphan claims.** If a document names a file, the file exists.
- **No aspiration presented as fact.** Planned work is marked as planned.
- **Stale is worse than short.** A short accurate document beats a long
  flattering one.
