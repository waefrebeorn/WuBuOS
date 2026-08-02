# Cross-Platform Build Config (gaps K3/K4)

The kernel is freestanding C11 (`-ffreestanding -nostdlib -fno-pie
-mcmodel=kernel`): the same object code builds on any host toolchain
that emits x86-64 ELF. The hosted layer (`src/hosted/`) is the only
platform-specific part and is swapped per OS.

## K3: Windows cross-build

- Toolchain: `x86_64-w64-mingw32-gcc` (any mingw-w64 build).
- The kernel objects compile with the SAME freestanding flags (the
  cross gcc handles `-mcmodel=kernel` + `-nostdlib`).
- The hosted Windows leg replaces `src/hosted/` with `src/hosted_win/`
  (Win32 API: CreateFile/ReadFile for the disk, DirectWrite/GDI for
  the framebuffer, the serial via CreateFile("COM1")). The kernel's
  dispatch boundary is `wubu_metal.h` -- the leg only implements that.
- Make invocation:
  `make CC=x86_64-w64-mingw32-gcc HOSTED_DIR=src/hosted_win kernel`
  (the variables are already overridable; the Windows leg files are a
  follow-up port, not a kernel change).

## K4: macOS cross-build

- Toolchain: `x86_64-apple-darwin-clang` (osxcross) or an Intel Mac.
- Same freestanding kernel build. The macOS leg is Metal/Quartz:
  `src/hosted_mac/` implements the `wubu_metal.h` dispatch with
  CoreGraphics for the framebuffer + IOKit HID.
- The kernel never touches Mach/BSD; the leg owns the platform.

## Parity doctrine (J7/K5)

- `make runtime tools` must VERIFY on each host; `parity.md` (generated
  by gen_docs) is the evidence matrix, not a promise.
- A kernel change that breaks a hosted leg is a REGRESSION: the check
  build must stay host-agnostic (the freestanding flags are the
  contract).

_Evidence: the kernel.elf links with -nostdlib on every host; the
hosted dispatch is the only platform seam (hosted-abstraction-audit.md)._
