# Display portability — the Swiss-army-knife rule

Researched 2026-10-19 from the designs that actually solved this: Linux
`simpledrm`/`simplefb`, multiboot2's framebuffer tag, UEFI GOP, coreboot's
framebuffer info, QEMU ramfb, and the Bochs VBE/dispi interface.

## The one rule

**Never program the card if something already handed you a framebuffer.**
simpledrm exists because every firmware (UEFI, BIOS, U-Boot, coreboot, ARM
ATF) already scans out *something* at boot. The OS's job is to take that
address + format and draw into it. Programming modes is the fallback, not the
default — and it is exactly the path that depends on whether the platform ran
its own option ROM.

Our QEMU failure is a textbook case: dispi accepts `xres=1024 yres=768
enable=0x41` and echoes it back, painting an LFB at either `0xC0000000` or
`0xFD000000` changes nothing, while a SeaBIOS-booted payload painting
`0xFD000000` shows up instantly. The card is "configured" in its registers and
not scanning out.

## Discovery order (most portable first)

| # | source | where it exists | gives us |
|---|---|---|---|
| 1 | **ramfb** via fw_cfg `etc/ramfb` | any QEMU with `-device ramfb` (x86, ARM, riscv) | we supply a RAM buffer; QEMU scans it, no card registers at all |
| 2 | **UEFI GOP** handoff | all UEFI machines, real hardware | linear FB address, w/h/pitch, pixel format |
| 3 | **multiboot2** framebuffer tag (type 8) | GRUB, QEMU `-kernel`, any multiboot2 loader | same fields as GOP |
| 4 | **coreboot framebuffer info** | coreboot payloads | same |
| 5 | **VBE 2.0 / Bochs dispi** | BIOS/QEMU with option ROM run | mode registers + LFB |
| 6 | **virtio-gpu / DRM** | modern QEMU and hardware | command buffers, real acceleration later |

WuBuOS currently implements 2 and 5 (GOP handoff + dispi), and now 1 as well
(`src/kernel/ramfb.c`: probe fw_cfg signature at 0x510, walk the file directory
at selector 0x19, find `etc/ramfb`, write `{addr, fourcc, flags, w, h, stride}`
big-endian). The kernel prefers ramfb and falls back to the GOP path.

## Why each one matters

- **ramfb** is the only path that needs zero card knowledge. If QEMU displays a
  1024x768 buffer we hand it, the whole VGA option-ROM question disappears.
- **GOP** is the real-hardware path: every UEFI machine has it, and the address
  it gives is the one the firmware is already scanning out.
- **multiboot2 tag 8** is the same data from a non-UEFI loader, and is the
  cheapest way to boot our kernel under SeaBIOS/GRUB where mode setting is
  known-good.
- **VBE/dispi** stays as a last resort because it is the only option on legacy
  BIOS machines without GOP.

## Implementation traps found so far

- A **3 MB static BSS buffer** pushed the kernel image past `0x400000` and into
  the heap start, breaking boot. The ramfb buffer now lives at a fixed free
  physical address (`0x5000000`, above the 4..68 MB heap window).
- `%llx` does not exist in `klog_printf`; format with `%x` + `unsigned long`.
- Handoff fields must be validated by **version**, not by "limine says ok" —
  `g_limine_ok` is truthy without Limine and silently skips the handoff.
- Anything above the 1 GB identity window (both `0xC0000000` and `0xFD000000`)
  must be mapped through the VMM before the first write or the kernel faults
  silently.
- QEMU monitor `screendump` under `-display none`, and VNC when the guest never
  exits the firmware shell, both show a **640x480 dummy surface**. Use
  `tools/qemu_vnc_shot.py` with the shell actually exited (`printf 'exit\n'`
  into `-serial stdio`) or the measurement is meaningless.

## Status

ramfb is implemented and logs `ramfb configured 1024x768 at 0x5000000` and
`ramfb backend active, painted 1024x768` under `-vga none -device ramfb`.
The VNC surface still reports 640x480 in those runs, so the final hop — the
display picking up the buffer — is **not yet verified**. Next checks: confirm
the QEMU version supports ramfb on x86 (it needs fw_cfg DMA-capable builds),
and try the DMA write path instead of the legacy port write, since some QEMU
versions only accept ramfb configuration through `fw_cfg` DMA.
