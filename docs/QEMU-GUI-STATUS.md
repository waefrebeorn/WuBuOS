# QEMU display path — status and evidence (2026-10-19)

This file exists because the "GUI works" claim is the easiest one in the tree to
fake. It records what is measured, what is not, and the exact next experiments.

## The oracle

QEMU's monitor `screendump` under `-display none` returns a **640x480 dummy
surface** even when the guest has programmed 1024x768, so it cannot verify a
graphics path. Use the real framebuffer instead:

```
qemu-system-x86_64 ... -display vnc=127.0.0.1:7 ...
tools/qemu_vnc_shot.py 127.0.0.1 5907 /tmp/shot.ppm
```

`qemu_vnc_shot.py` speaks RFB 3.8 with no dependencies (security None, 32bpp
truecolour, raw encoding) and writes a P6 PPM. It was validated against a live
boot: it reported the same 640x480 two-colour frame as screendump, so both
agree today — the frame really is 640x480 and black.

## What is proven

| hop | evidence |
|---|---|
| firmware finds the VGA device | `[drv] bound GPU/GOP at 0:1.0` |
| BAR0 is the LFB | `bars: 0=C0000000/0x1000000 1=0/0x0 2=C1044000/0x1000` |
| dispi is on the legacy ports | MMIO window at BAR2+0x500 reads `0`; ports read `id=B0C5` |
| mode set is accepted | `readback xres=1024 yres=768 bpp=32 enable=65` (0x41 = enabled+LFB) |
| handoff v2 carries it | `[wubufw-loader] fb handoff: addr=C0000000 w=400 h=300` (hex: 1024x768) |
| kernel receives it | `handoff v2 fb=0xc0000000 w=1024 h=768 pitch=4096` |
| the 3 MB aperture is mapped | `GOP fb mapped (768 pages)` — 0xC0000000 is outside the low-1 GB identity window, so an unmapped write silently faults |
| kernel paints it | `GOP fb mapped (768 pages) + painted 1024x768` |

## What is NOT proven

**QEMU's scanout never leaves 640x480.** Every grab — with default VGA, with
`-vga std`, with `-device bochs-display`, from the firmware alone, and from the
running kernel — is 640x480 with two colours (black + a few 0xAA pixels).

So: registers say 1024x768, memory at 0xC0000000 is written, and the display
shows a 640x480 text surface. The gap is between the card's mode registers and
QEMU's scanout.

## Correction that cost several wrong conclusions

Direct `qemu-system-x86_64` runs that do **not** send `exit` to the WuBuFW shell
never boot the OS: the 640x480 black frame being measured was the **firmware's
own text shell**, not a failed graphics mode. `run-agi.sh` sends `exit` and boots
`build/agi-esp.img`. Any manual QEMU invocation must do the same:

```
{ sleep 2; printf 'exit\n'; sleep 40; } | qemu-system-x86_64 ... -serial stdio \
    -display vnc=127.0.0.1:4 ...
```

With that corrected, the kernel runs (`kernel set VBE mode, enable=41`), maps and
paints, and the grab is **still** 640x480 two-colour. So the failure is real and
survives the fix — it is not a measurement artefact.

Also note: the VGA sequencer unlock (0x3C4/5 idx 1, clear 0x20) and CRTC
protection clear (0x3D4/5 idx 0x11, clear 0x80) were added before the dispi
sequence and changed nothing.

## Ranked next experiments

1. **Minimal reference payload.** One freestanding binary that does nothing but
   the dispi sequence, then halts. If VNC still shows 640x480, QEMU in this
   configuration cannot switch modes and no amount of OS code will change that;
   the verification path then has to be a real display device or ramfb.
2. **CRTC/sequencer unlock before the dispi write.** Some VBE implementations
   require the VGA sequencer to be unlocked (0x3C4/0x3C5, index 0x01, clear bit
   0x20) and the CRTC protection lifted (0x3D4/0x3D5, index 0x11, clear bit
   0x80) before the mode takes effect on scanout.
3. **Machine-side sizing.** Start QEMU with `-device VGA,vgamem_mb=32` and see
   whether the surface can resize at all.
4. **ramfb path.** If the VGA device is not the console QEMU displays, the
   firmware should publish its framebuffer through `fw_cfg` ramfb instead.

## Kernel-side changes already landed (keep them regardless)

- The kernel re-programs 1024x768x32 after `ExitBootServices`, because EFI tears
  its graphics console down on handoff and the card drops back to text mode.
  Doing it in the kernel means the mode is owned by the component that draws.
- `0xC0000000` is mapped through the VMM before any write. Without this the
  kernel page-faults silently the moment it touches the screen.
- `g_limine_ok` must not gate the handoff: it is truthy without Limine, which is
  why the framebuffer was ignored for several boots.
- `klog_printf` has no `%llx`. Use `%x` with an `unsigned long` cast, or the
  varargs shift and every later field prints nonsense.

## What the GUI may not claim until (1) or (2) succeed

`dosgui_wm_init` is called only from hosted tests. Wiring it into the boot path
before the scanout question is settled would produce a window manager drawing
into memory nobody displays — the exact failure mode this document exists to
prevent.
