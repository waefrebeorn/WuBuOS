# Self-hosted, self-booting: the independence ladder

A body that needs a host is a process, not a machine. This page defines what
"independence" means in measurable steps, records what WuBuOS has **today**, and
names the three gaps that stand between the current tree and a system that
rebuilds and reboots itself with no host in the loop.

Nothing here is a claim. Each row says how it is checked.

## What exists today (verified in-tree)

| Piece | Where | Evidence |
|---|---|---|
| 512-byte BIOS bootsector | `src/kernel/boot.S` → `boot.bin` | `make boot` prints "Bootsector built (512 bytes)" |
| Floppy image + real boot | `src/kernel/disk.img` | `make qemu` → `qemu-system-x86_64 -fda disk.img -serial stdio -m 128` |
| Freestanding kernel | `kernel.elf` | linked without libc; `metal_main` entry |
| C11 compiler | `wubunos/holyd*.c` | "self-hosting battery" in `holyd_test.c` |
| Own FAT32 / 9P / namespace | `src/kernel/fat32*.c`, `src/runtime/styx_*` | in test_all |
| Own C library | `src/kernel/libc.c` (+ `__*_chk` shims) | linked into kernel.elf |

**What does not exist:** an in-tree UEFI loader. `grep -r efi_main` finds only
PE-parsing code for Wine/Proton, not a firmware entry point. Boot today is
BIOS/MBR only.

## The ladder

| Level | Name | Definition | Status |
|---|---|---|---|
| **L0** | Hosted-verified | Host `gcc` builds the tree; tests run on Linux | **current default** |
| **L1** | Bare-metal boot | Kernel runs on hardware/QEMU with no host OS underneath | **exists** (BIOS, `make qemu`) |
| **L2** | Self-describing boot | Firmware/loader is ours: reads the kernel image through our own storage driver, measured chain | **partial** — MBR sector reads only; no UEFI |
| **L3** | Self-hosted toolchain | HolyD compiles a kernel-sized program on-target, and compiles itself (stage2 == stage3) | **partial** — battery is compiler tests, not a kernel build |
| **L4** | On-device rebuild | WuBuOS userland builds `kernel.elf` from source and writes a bootable image | **absent** |
| **L5** | Independence | Two off-network rebuilds are bit-identical; rollback slot; no host toolchain or libc | **absent** |

L1 without L4 is a machine that can *run* but not *reproduce*. That is the exact
difference between an OS and a body.

## Acceptance tests (one per level, no prose credit)

1. **L1 audit — `make boot-audit`**
   `readelf -d kernel.elf` shows no `NEEDED` entries; `nm -u kernel.elf` is
   empty except the documented `__*_chk` shims. Fails if any host libc symbol
   leaks in. *This check does not exist yet; it is the cheapest of the set.*

2. **L2 UEFI loader — `make efi && make qemu-efi`**
   A C11 `efi_main` (no EDK2) that opens the boot volume through the firmware's
   Simple File System protocol, loads `kernel.elf`, calls `ExitBootServices`,
   and jumps. Pass criteria: serial trace shows the loader's own line before the
   kernel's first line, and `kernel.elf` is read from a FAT32 image the loader
   mounted itself. *Absent today.*

3. **L3 self-compile — `make holyd-stage3`**
   HolyD compiled by HolyD produces a binary whose `.text` hash equals the
   host-built HolyD's (or a documented, justified delta). *Partial: the battery
   exists, the stage-2/stage-3 hash comparison does not.*

4. **L4 on-device rebuild — `make selfrebuild` inside WuBuOS**
   Boot WuBuOS, run HolyD over `src/kernel/*.c` plus the linker script, produce
   `kernel.elf` on the target, write `disk.img`, reboot into it. Pass criteria:
   the second boot's serial trace reports a build hash computed *on target*, and
   the image boots. *Absent.*

5. **L5 reproducibility — `make repro-check`**
   Two builds from the same source snapshot with the network off and different
   absolute paths produce identical `kernel.elf` hashes (or the delta is
   explained and bounded). Rollback image kept. *Absent.*

## Why this is the AGI requirement, not a nice-to-have

The decision plane now prices every delegated choice (`wubu_decision`). A body
that cannot rebuild itself cannot re-price its own decisions after a change: the
toolchain becomes the hidden dependency that no threshold can see. Self-rebuild
is what makes the system's cost model *true* — you can only claim a build is
cheap if the machine can actually do it.

It is also the difference between "runs on Linux with a compatibility layer" and
"is the computer". The user's rule is explicit: third-party runs on the AGI
kernel, never the reverse. L4 is that rule made executable.

## Next three steps, in order

1. **`make boot-audit`** (an hour): prove L1 is genuinely freestanding. Any
   host-libc symbol found is a real dependency bug worth fixing.
2. **UEFI loader in C11** (a session): the firmware path that does not exist.
   Start with the Simple File System protocol and a fixed kernel path, skip
   graphics and ACPI handoff initially.
3. **HolyD stage-3 hash** (a session): turn the existing self-hosting battery
   into a bootstrap-fixed-point test. This is the gate that unlocks L4, because
   on-device rebuild is meaningless if the compiler's output depends on which
   compiler built it.

## Honest status line

WuBuOS boots on bare metal today (BIOS), and its compiler can compile the
constructs the kernel needs. It cannot yet build itself on itself, and there is
no UEFI entry point in the tree. L4/L5 are the frontier, and they are measurable.
