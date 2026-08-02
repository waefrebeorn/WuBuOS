# Hosted OS-Abstraction Audit (gap K2)

Audit date: 2026-08-02. Scope: any Linux-only syscall (mmap, pthread,
ioctl, readlink, gettimeofday, open/read/write, select) inside the
PORTABLE core (`src/kernel/*.c`, non-test) vs the HOSTED layer
(`src/hosted/`).

## Result: the core is clean

A full grep of `src/kernel/` for `mmap(`, `pthread_`, `ioctl(`,
`readlink`, `gettimeofday`, and bare `open(`/`read(`/`select(` finds
**zero real syscall uses** in the portable core. The matches are
module names (`wubu_memmap`), FS APIs (`fat32_open`), or the
`wubu_metal*` hosted layer.

The kernel core is genuinely freestanding C11: it talks to the machine
through `inb`/`outb`/MMIO (`wubu_serial`, `wubu_apic`, `ahci`, `ps2`),
the heap (`memory.c`), and its own abstractions.

## Where the platform-specifics live (by design)

The hosted layer (`src/hosted/`) is the PARITY scaffold, not the OS:
- `wubu_metal_drm.c` — Linux DRM/KMS (`/dev/dri`, ioctls)
- `wubu_metal_evdev.c` — Linux input (`/dev/input`, evdev ioctls)
- `wubu_metal_x11.c` — X11 (XCB/Xlib sockets)
- `hosted_wayland_shm.c`, `wubu_gbm.c`, `wubu_display.c` — display plumbing
- `wubu_metal.c` — the metal-backend dispatch

These are already isolated under `src/hosted/` behind the
`WUBU_METAL_*` backend switches. Windows/macOS legs (K3/K4) will swap
this directory, not touch the kernel.

## Rules this audit confirms

1. A syscall that appears in `src/kernel/*.c` is a BUG (the freestanding
   rule). The check build enforces `-ffreestanding -nostdlib`.
2. The hosted layer is replaceable per platform; its interface to the
   kernel is the `wubu_metal.h` dispatch, nothing more.
3. The `test_*.c` files are allowed hosted calls (they run on the host
   by definition) but must never leak into the kernel objects.

_Evidence: the grep above; the kernel.elf links with -nostdlib and
contains no libc/syscall references beyond the hand-rolled libc.c._
