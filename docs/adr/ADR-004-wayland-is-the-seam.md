# ADR-004: Wayland is the display seam, never the kernel's WM

- **Status:** accepted (proposed 2026-11-24)
- **Deciders:** WaefreBeorn
- **Supersedes:** nothing
- **Related:** ADR-001 (centralized window chrome), ADR-002 (opaque seams);
  `docs/case-study-window-system.md`; the `wubucompositor` repo.

## Question

Should WuBuOS/WuBuKernel use Wayland for its window manager — and does that mean
Wayland lives in the kernel?

## Context (measured in this tree, 2026-11-24)

- **The desktop is already a Wayland client.** `src/hosted/wubu` speaks
  xdg-shell + `wl_shm` + xkbcommon and contains no Xlib, Quartz or Win32 in its
  client core.
- **The compositor is the host seam, not the desktop.** `wubucompositor` owns
  the host's display and input and *serves* Wayland to the guest. Today its host
  side is X11 (Xlib + XShm/GLX present, XRandR, XGrabKeyboard); a Quartz/Metal
  backend swaps the host side without touching the protocol logic, the guest, or
  anything above it. This is the WSLg shape, written as our own C11.
- **The metal kernel has the display primitives, not a WM:**
  `src/kernel/vbe.h` (GOP/VBE scanout, `vbe_scanline`, `vbe_set_render_target`),
  `src/kernel/input.h` (`input_key_poll`, `input_mouse_poll`), `crt0.S` +
  `metal_main.c` (boots, draws the desktop and the mascot), plus ring-3 support
  (`wubu_user.c`, `wubu_tss.h`, `interrupt_syscall.c`) and a 9P server/client.
  `grep -rn 'wm_init\|desktop_init' src/kernel/` is **empty**: every window,
  taskbar and panel lives in the hosted binary.
- **Missing for an on-metal Wayland compositor:** AF_UNIX sockets with handle
  passing, shm objects with fd-like lifetime, and a page-flip/scanout API for a
  user process. The kernel has none of the three (`grep 'socket(' kernel/*.h` is
  empty; there is no shm type; VBE is drawn in-kernel).

## Decision

1. **Wayland is the display seam, not the kernel's window manager.** The kernel
   owns scanout + input and stays a policy-free display provider, the way Linux
   provides DRM/KMS + evdev and nothing more. Every real system does this;
   putting a compositor in the kernel would force protocol parsing, buffer
   lifetimes and client scheduling into ring 0 for no user-visible gain.
2. **The metal desktop keeps its native WM as the fallback/splash.** The
   in-kernel desktop + mascot is what a machine with no user-space compositor
   shows — it is already bootable and is the honest baseline, not a competitor.
3. **The Wayland path to bare metal is a new backend on the existing seam.**
   `wubucompositor` gains a "WuBuOS backend" that presents into the kernel's
   framebuffer and reads the kernel's input queues, exactly as it presents into
   X11 today. The protocol code, the guest and every client above it stay
   unchanged — that is the entire reason the seam exists.
4. **The kernel grows four primitives, in this order**, because a compositor
   cannot be written without them:
   - (a) user-mappable framebuffer + page flip (mmap of the scanout, flip on
     present) — today `vbe` writes in-kernel only;
   - (b) an AF_UNIX-style local socket with handle passing (wl_display + sendmsg
     fd semantics);
   - (c) shm objects with fd-like lifetime (wl_shm buffers);
   - (d) an input handoff: the compositor becomes the owner of the queues that
     `input.h` already fills.

## Why not X11

X11 is feature-frozen upstream and its design serializes every client through
one server with a round trip per window operation. Wayland's per-client buffers
+ frame callbacks are the reason Gamescope (SteamOS) can do HDR, scaling and
frame limiting without a compositor rewrite. The argument for Wayland here is
the buffer/frame model, **not** "GNOME says so" — GNOME's decisions and the
protocol's merits are separate questions.

## Consequences

- **Good:** one desktop codebase, host-agnostic; a Metal/Quartz/WuBuOS backend is
  a swap, not a port; the kernel stays small and auditable; XWayland/Proton keep
  working where they already do (hosted).
- **Bad:** bare-metal Wayland is blocked on (a)–(d) above; until then the metal
  leg has a native WM and no third-party clients — which is a real, stated gap,
  not a hidden one.
- **Cost estimate:** the seam work is a backend in `wubucompositor` (present +
  input + output enumeration); the kernel work is (a)–(d), each independently
  testable (`test_input_queue`, a socket round-trip, an shm map/unmap, a flip
  that changes what `vbe` scans out).

## Delivered 2026-11-24: the first primitive (item (a)), verified externally

`wubu_vmm_map_user_page()` (US on the PTE **and every table above it**),
`vbe_present_from(src, src_pitch, ...)` (explicit source stride, so a packed
client buffer cannot shear into a padded scanout), `vbe_map_user()` and the
`fbmap` console command. The gate boots the real firmware + kernel, runs the
command, and treats **QEMU's physical memory as the oracle**:

```
fbmap: WROTE phys=c0000000 pitch=4096 dmg=8:8:ff00ff usr=16:16:cafeb0 map=ok bytes=300000
damage present  8,8 expect 0x00ff00ff: ok
ring-3 mapping  16,16 expect 0x00cafeb0: ok
test_fbmap: PASS (client-buffer present + ring-3 scanout write both landed)
```

i.e. a 1x1 damage rect presented from a packed client buffer reached the live
scanout, and a write through a ring-3-reachable mapping of that scanout reached
it too. Item (a) of the primitive list below is done; (b) sockets with handle
passing, (c) shm objects and (d) input ownership remain.

### History: the first attempt, and why it was reverted


The first step (user-mappable scanout + damage-rect present) was written,
gated and then **reverted unshipped**, because it could not be proven:

- `wubu_vmm_map_user_page()` (US at every table level), `vbe_present_from()` (an
  explicit source stride, so a packed client buffer cannot shear into a padded
  scanout) and `vbe_map_user()` all compiled and mapped the real LFB
  (`phys=0xc1000000`, `bytes=0x300000` = exactly 1024x768x4).
- **But no write to that scanout could be read back at the probe pixel.** The
  measurement, repeated through three formulations (raw pointer, `vbe_set_pixel`
  + `vbe_get_pixel`, and a 1x1 present from a client buffer):

  ```
  fbmap: FAIL phys=c1000000 bytes=300000 damage=FAIL user=FAIL
  fbmap: dbg  w=1024 pitch=4096 got=0 want=ff00ff fbva=ffffffff98000000 rb=123456
  fbmap: dbg3 direct=0                       (write v->fb[idx]=0xAB12CD, read 0)
  fbmap: dbg4 fb=ffffffff:98000000 back=ffffffff:98000000 fbrd=0 bkrd=0
  ```

  A write at pixel (20,20) read back fine (`rb=123456`), while (6,6) never did --
  with `fb == back`, `pitch == 4096`, `mfence` in place, and a direct store into
  `v->fb[idx]`. That contradiction is unresolved. **Nothing here may be claimed
  until it is.**

- Consequence: the metal desktop's present path may be fine and the probe wrong,
  or the pointer the console command sees may not be the surface that scans out.
  Either way the honest state is: *the compositor seam is still unbuilt*, and the
  next session starts by resolving that single contradiction (a 2-pixel write
  matrix at several offsets, printing the pointer both ways, ideally checked
  against a QEMU `memsave` of the LFB physo range -- which does not depend on the
  kernel's read path at all).

**Resolved 2026-11-24 (`a36ffb2f`):** the writes were going to a device BAR. The
bochs path read PCI config `0x80001010` (bus 0 dev 2 fn 0) believing it was the
VGA; on q35 that slot is a NIC (`0:2.0 id=0x10D38086`), so the kernel took the
NIC's BAR0 (0xC1000000) as the framebuffer and drew the desktop into a network
device's MMIO window -- which discards writes, hence "direct store reads back 0".
The kernel now finds the display by class (`0:1.0 BAR0=c0000008` -> 0xC0000000),
which matches the loader's handoff.

## Closed 2026-11-24 (`d3bb0043`): a gate now SEES the metal desktop

`test_scanout` boots the real firmware + kernel, parses the address the kernel
reports (`LFB phys=...`), and dumps that physical range out of QEMU with
`pmemsave` -- no kernel cooperation, so the kernel's own read path cannot fool it.
Measured on the fixed tree: **786 397 of 786 432 pixels non-black (100.0 %),
3 213 distinct colours**, i.e. the wallpaper, icons, mascot and taskbar really are
in the live framebuffer.

Proven RED then GREEN against the bug that motivated it:

| build | reported LFB | non-black | colours | verdict |
|---|---|---|---|---|
| pre-fix (`a36ffb2f^`) | `0xc1000000` (a NIC's BAR) | 12 851 (1.6 %) | 67 | FAIL |
| with the fix | `0xc0000000` | 786 397 (100.0 %) | 3 213 | PASS |

**A QEMU `screendump` is NOT a valid oracle here:** under `-display none` the VGA
console surface reports 640x480 black even when the LFB is fully painted, so the
kernel was never the thing being measured. Anything that claims "the screen is
black" in this configuration must say which surface it looked at.

## Gate cost discipline (learned the expensive way)

A gate that boots hardware must not rebuild the world -- and **measure with the
machine idle.** Two QEMU gates were first blamed for taking the suite from 378 s to
995 s / 1015 s. The real cause was my own load test: ~20 `yes` processes were left
running for 57 minutes (`kill %1..%20` inside `sh -c` silently does nothing, there
is no job control), so those runs were taken at load average 22. After killing
them, `make -k test_critical_kernel` is **37 s** with the gate in it.

The gate's own waste was still real and is fixed independently: it ran a nested
`make -C ROOT kernel` from inside an already `-j24` suite (serial nested make plus
24-way oversubscription) and rebuilt a 64 MB ESP image every run.

Rules that came out of it:
- the test target depends on the artifacts (`$(WEMU_KERNEL_ELF)`) so **make** owns
  the build order; the gate script never invokes make;
- build-esp.sh honours `SKIP_KERNEL_MAKE=1` for exactly this reason;
- the ESP is cached and rebuilt only when it is older than the kernel or firmware;
- a hardware gate gets a seconds budget, not a minutes one: with a cached image
  `test_metal_display` is 4 s standalone.

## Verification of this ADR's claims

Re-runnable today:
```
grep -rn 'wm_init\|desktop_init' src/kernel/            # empty
grep -n 'input_key_poll\|input_mouse_poll' src/kernel/input.h
grep -rn 'socket(' src/kernel/*.h                        # empty
grep -rn 'wl_shm\|xdg_shell\|xkbcommon' src/hosted/*.c | head
```
