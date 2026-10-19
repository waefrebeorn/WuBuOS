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

## Decisive result: QEMU *can* switch — and where it scans out

`src/firmware/vbe_probe.c` is a 3 KB multiboot payload that does nothing but the
dispi sequence and paints the LFB. Run it directly, with no firmware, no loader,
no kernel:

```
gcc -m32 -ffreestanding -nostdlib -fno-pic -no-pie -Wl,-T,build/probe.ld \
    -o build/vbe_probe.elf vbe_probe.c
qemu-system-x86_64 -machine q35 -kernel build/vbe_probe.elf -display vnc=127.0.0.1:9
tools/qemu_vnc_shot.py 127.0.0.1 5909 /tmp/probe.ppm
```

Result: **1024x768, one colour (0x20C060)**. So QEMU switches modes fine on q35.

Two facts fall out:

1. The payload painted **0xFD000000** and it appeared on screen. That is the
   scanout LFB.
2. The EFI firmware reports its LFB at **0xC0000000** (BAR0, 16 MB) and
   readback confirms the 1024x768x32 mode was accepted there — yet the display
   stays 640x480.

So the firmware is programming a linear framebuffer at an address the scanout
does not read. The remaining work is not in the kernel: it is to make the
firmware publish (or the kernel adopt) the address QEMU actually scans out, or to
relocate the VGA BAR to 0xC0000000 so the two agree.

## Working hypothesis (2026-10-19, after the probe)

SeaBIOS **executes the VGA option ROM** before handing control to a payload; our
UEFI firmware never does. A card whose option ROM has not run will accept dispi
register writes and echo them back, but its sequencer/CRTC/DAC state is still the
POST default, so no graphics scanout is produced.

That single difference explains every observation so far:

| observation | explanation under this hypothesis |
|---|---|
| probe on any machine works | SeaBIOS ran the option ROM |
| firmware readback says 1024x768 | dispi registers are just registers |
| painting 0xC0000000 or 0xFD000000 from the OS shows nothing | card never entered graphics scanout |
| EFI text shell displays fine | text mode needs no option ROM |

## Two ways forward (a design decision, not a bug fix)

1. **Execute the VGA option ROM in WuBuFW.** Locate the ROM BAR on 0:1.0, map it,
   and run its init entry (needs a real-mode/V86 shim in 64-bit EFI, or a
   minimal reimplementation of the mode-set sequence). Large, but it makes the
   firmware a proper graphics owner.
2. **Boot the kernel multiboot-style** with SeaBIOS running the option ROM, and
   keep WuBuFW as an alternate platform. Fastest path to a visible GUI, at the
   cost of the "our own firmware" chain.

Until one is chosen, GUI work stays blocked: the window manager would draw into
a card that never leaves text mode.

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


## The other blocker, found by inspection (2026-10-19)

Even with a working display there is no GUI in the kernel to draw. `mk/objects.mk`
defines `GUI_OBJS` (~120 objects: `dosgui_wm`, wm render/input, start menu,
explorer, terminal, control panel, wallpaper, Bonzi) but **`KERNEL_OBJS` contains
zero GUI objects**:

```
sed -n '15,22p' mk/objects.mk | tr ' ' '\n' | grep -c 'gui/'   -> 0
```

So the kernel binary links no window manager at all; `dosgui_wm_init` is reachable
only from the hosted build and tests. The GUI is a hosted subsystem that was never
linked bare-metal.

That means "self-sufficient GUI" needs two independent pieces of work:

1. **display** (blocked: firmware VGA handoff never reaches scanout; the
   multiboot payload proves the kernel-side drawing is fine), and
2. **a kernel GUI link target** — a `GUI_KERNEL_OBJS` subset with the hosted-only
   dependencies (`wubu_ui_hosted`, `wubu_wayland_stub`, proton/dxvk, pkgmgr)
   either stubbed or excluded, plus a `dosgui_wm_init(1024,768)` call after the
   framebuffer is attached.

Doing (2) without (1) is verifiable only in memory (window count and pixel
statistics logged by the kernel), which is still worth doing: it proves the WM
runs bare-metal before the screen works.


## Kernel GUI dependency closure (measured, 2026-10-19)

Adding `dosgui_wm_render.o` to `KERNEL_OBJS` fails the link with this exact set
(unresolved symbols, in discovery order):

```
draw_desktop_bg                     -> dosgui_wm_layout.c
dosgui_wm_draw_icon_glyph           -> dosgui_wm_icon_glyphs.c
dosgui_wm_draw_icon_selection       -> dosgui_wm_icon_glyphs.c
taskbar_height_dynamic              -> dosgui_wm_clock.c
__strncpy_chk                       -> fortified string builtin (stubbed in
                                       gui_kernel_stubs.c)
dosgui_chrome_draw_window           -> dosgui_window_chrome.c
wubu_a11y_is_enabled                -> a11y module
wubu_bonzi_is_enabled               -> wubu_bonzi (GUI variant)
dosgui_taskbar_render               -> dosgui_wm_taskbar.c
dosgui_notif_center_render          -> wubu_notify.c
dosgui_icon_hit_test                -> dosgui_wm_icons.c
```

Linking was reverted after `dosgui_chrome_draw_window` and friends appeared:
each added object pulls more, and a partial set produces a dead render path.
The correct approach is to build a **GUI dependency closure in one pass**
(generate the object list from the symbol graph rather than by hand), or to
stub the optional features (a11y, bonzi, notifications) so a desktop + windows
renderer links first.

Current state is green: kernel links and boots, `WM init rc=0 screen=1024x768`,
render not called.
