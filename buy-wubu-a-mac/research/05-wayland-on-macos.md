# 05 — Wayland on macOS: the protocol machinery is NOT a dead end

The worry: libwayland-server uses epoll/timerfd/eventfd, which are
Linux-only, so maybe the whole compositor can't build on macOS.
Verified 2026-11-19: that worry is already solved upstream.

## The facts

- libwayland runs on Darwin. Sergey Bugaev ported client/server/cursor
  + wayland-scanner years ago; the standard non-Linux build path
  (FreeBSD/NetBSD/OpenBSD, and macOS via MacPorts) is libwayland +
  epoll-shim.
- epoll-shim (github.com/jiixyj/epoll-shim) implements
  epoll/timerfd/signalfd/eventfd on top of kqueue. It has had a
  maintained macOS port since Jan 2023 and is in MacPorts.
- Reading wayland 1.23.1 source (per the jailbreak wayland-plan
  analysis): src/wayland-os.c is ALREADY Darwin-aware (xucred /
  LOCAL_PEERCRED credentials branch; accept4/memfd_create/
  MSG_CMSG_CLOEXEC feature-detected with portable fallbacks). No C
  source patch needed; the only change is a one-line meson edit so
  epoll-shim is pulled on darwin (upstream limits it to freebsd/openbsd).
- One wrinkle: wayland needs a version-matched NATIVE wayland-scanner
  to codegen its own headers — build the scanner natively first, then
  cross-build the libs against it (the MacPorts recipe does this).
- MacPorts carries: wayland (XQuartz fork, builds on darwin),
  wayland-protocols, epoll-shim, libxkbcommon, pixman, cairo, pango.
- Prior art: iland (github.com/CoreBedtime/iland) is a Wayland
  compositor support library for macOS (weston runs on it). It needs
  SIP disabled + DYLD_INSERT_LIBRARIES shims — hacky; we do NOT need
  that path, because OUR compositor is the server and only needs
  libwayland + epoll-shim, not weston's whole platform stack.

## What this means for WuBuCompositor

The split already isolates the host-bound leaves (wcc_gpu.c first;
wcc_present/wcc_x11/wcc_monitors next). On macOS:

- KEEP: all protocol logic, wcc_spawn (Apple branch already written),
  the xkbcommon keymap path, POSIX shm transport (already fixed).
- BUILD: libwayland-server + epoll-shim via MacPorts (or vendored
  meson with the one-line darwin edit).
- REPLACE: the X11 host leaves with Quartz leaves —
  NSWindow/NSView or the CoreGraphics C API for output,
  IOSurface/CVPixelBuffer for buffer import, CGEventTap for input,
  CVDisplayLink for frame pacing. Skeleton in stubs/wcc_quartz.*.stub.
- XRandR enumeration (wcc_monitors.c) maps to CGGetActiveDisplayList /
  CGDisplayCopyAllDisplayModes.

So the macOS port is: one library recipe + one backend's worth of
leaves. The guest (pure Wayland client) is unchanged. That is the
WSL-replacement philosophy holding up under inspection.

## Verify order (all free)

1. GitHub macOS runner: compile wcc_spawn + run the spawn gate (today).
2. Runner: build libwayland + epoll-shim from MacPorts; compile the
   protocol half of the compositor against it (no X11 leaves).
3. Quartz leaves implemented; full build; --screenshot gate on macOS.
4. iMac fallback for bare-metal/legacy checks.