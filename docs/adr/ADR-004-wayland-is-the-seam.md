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

## Verification of this ADR's claims

Re-runnable today:
```
grep -rn 'wm_init\|desktop_init' src/kernel/            # empty
grep -n 'input_key_poll\|input_mouse_poll' src/kernel/input.h
grep -rn 'socket(' src/kernel/*.h                        # empty
grep -rn 'wl_shm\|xdg_shell\|xkbcommon' src/hosted/*.c | head
```
