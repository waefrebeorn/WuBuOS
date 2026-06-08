# WuBuOS — Triple Devil's Advocate: Arch-as-Driver + WSL-Hook Architecture

**Date**: Session 2026-06-08
**Trigger**: "triple devils advocate" + Arch kernel as desktop driver system + WSL method hooking
**Methodology**: Three independent advocacies on the SAME code and architecture. No delegation. Source code read directly.

---

## The Proposition

**Replace X11/Wayland with the Arch Linux kernel as the desktop driver layer.**

Current: `wubu` → X11 (libX11) → X server → DRM/KMS → GPU
Proposed: `wubu` → Arch kernel (DRM/KMS direct) → GPU

**DOS side = ZealOS/TempleOS side** — the ring-0 kernel that boots on bare metal.
**Arch side = driver/desktop layer** — runs Linux drivers natively for DRM/GPU/NVIDIA/audio/net.

**WSL hook pattern**: WSL2 runs a Linux kernel in a Hyper-V VM. WuBuOS runs the Arch kernel as its driver layer — same pattern, different purpose.

---

## DA1: The Optimist — "This Architecture Works"

1. **DRM/KMS is the ONLY path for direct display on Linux.** X11 is a middleman. Wayland compositors ALL use DRM/KMS underneath. SteamOS does exactly this.

2. **NVIDIA drivers ONLY work through the Linux kernel.** No port of nvidia.ko to any other OS exists. Arch's `nvidia-dkms` compiles against whatever kernel is running.

3. **The WSL pattern proves it works.** WSL2 runs a full Linux kernel inside a Hyper-V partition. WuBuOS reverses the polarity: ZealOS is the "host" and Arch is the "driver partition."

4. **Container runtime already works.** `wubu_host_exec.c` does real `fork()+chroot()+execv()+mount()`. The Arch kernel is ALREADY running (we're on WSL2 right now).

5. **The 9P namespace makes the split clean.** ZealOS side mounts `/dev/draw` from the Arch side via Styx. The DOS/Temple side doesn't know or care that DRM provides the framebuffer.

6. **This IS the Inferno model.** Inferno's `emu` on Linux uses the host's X11/DRM. WuBuOS on Arch uses DRM/KMS. Same abstraction, different host.

7. **Apple Virtualization.framework IS the macOS equivalent of WSL2.** Same pattern: host OS provides a Linux kernel in a VM. WuBuOS runs as init. GPU via VirtIO.

---

## DA2: The Realist — "Here Are The Actual Gaps In The Code Right Now"

### Gap 1: wubu_display.h was header only → FIXED
`wubu_display.c` now implements DRM/KMS probe, init, swap, evdev input, shutdown.

### Gap 2: No /dev/dri in WSL2 → X11 fallback works
WSL2 doesn't expose `/dev/dri`. The display backend probes and falls back to X11 gracefully.

### Gap 3: Container bind mounts were dead config → FIXED
`ct->binds[]` is now mounted via `mount(MS_BIND|MS_REC)` after `chroot()`, before `execv()`.

### Gap 4: hosted.c deeply coupled to X11 types → Needs refactor
The event loop is structured around `XPending/XNextEvent/XPutImage`. DRM/KMS + evdev needs a new frontend. `wubu_display.c` provides the backend abstraction.

### Gap 5: ZealOS side has NO concept of the Arch kernel → Cell 385
No 9P callout mechanism exists. `vbe_swap()` copies pixels internally. Need: ZealOS `LFBFlush()` → `Twrite` on `/dev/draw` → Arch DRM renders.

### Gap 6: Name parity was 8% → Now 64%
`zealos_parity.h` maps 64/96 core functions. Remaining 35 are documented with source locations.

### Gap 7: 171 (void) suppression casts → Needs systematic elimination
These are hollow API signatures. Each one is a real gap that needs implementation or removal.

### Gap 8: 42 TODO/FIXME markers in source
Spread across jit.c, fat32.c, memory.c, wubu_exec.c, wubu_vsl.c, holyc_codegen.c.

---

## DA3: The Pessimist — "This Will Fail Because..."

### Failure mode 1: DRM master contention
When WuBuOS opens `/dev/dri/card0` and calls `drmSetMaster()`, it becomes the DRM master. No other compositor can be master simultaneously. **Fix**: Run WuBuOS as the ONLY display server, or use DRM leasing, or fallback to X11.

### Failure mode 2: VT switching and input stealing
DRM/KMS runs on a virtual terminal. Linux's VT subsystem fights with evdev raw reads. **Fix**: `libseat` or `sd_seat` to manage seat/VT/DRM master lifecycle.

### Failure mode 3: NVIDIA DRM support is incomplete
NVIDIA's kernel driver does NOT implement the full KMS API. No dumb buffer support. **Fix**: Require `nvidia-drm.modeset=1` or accept X11 intermediary.

### Failure mode 4: The WSL hook is parasitic, not symbiotic
WSL2 works because Microsoft controls BOTH sides. WuBuOS trying to "hook WSL methods" means depending on undocumented VMBus protocol. **Fix**: BE a WSL2 distro instead (`wsl --install WuBuOS`).

### Failure mode 5: ZealOS VGA I/O ports fail on hosted
HolyC JIT'd code that writes `outb(0x3C4, ...)` silently fails on hosted. **Fix**: Only VBE framebuffer rendering works on hosted. DOS apps need bare metal or full VGA emulation.

### Failure mode 6: macOS VirtIO GPU needs kernel driver
The macOS VM guest needs `virtio-gpu.ko` loaded. Without it, no display. **Fix**: Include virtio-gpu driver in initramfs.

---

## Scorecard

| Proposition | DA1 | DA2 | DA3 | Verdict |
|-------------|-----|-----|-----|---------|
| Arch kernel as driver layer | ✅ Correct arch | ⬜ wubu_display.c now exists | ⬜ DRM master contention | **Build dual backend** |
| DRM/KMS replaces X11 | ✅ SteamOS does it | ⬜ No /dev/dri in WSL2 | ⬜ NVIDIA DRM incomplete | **Dual backend: DRM+X11** |
| ZealOS DOS side via 9P | ✅ Inferno model | ⬜ No 9P dispatch to renderer | 🔴 VGA I/O ports can't be relayed | **Cell 385** |
| WSL method hooking | ✅ GPU via /dev/dxg | ⬜ WSL is proprietary | 🔴 Fragile, breaks on updates | **BE a WSL2 distro** |
| Container GPU passthrough | ✅ Config exists | ✅ Now actually mounted | — | **Cell 383 done** |
| Name parity 1:1 | ✅ 64/96 mapped | ⬜ 35 remaining | — | **Cell 305** |

## Next Cells (from this DA)

| Cell | What | Priority |
|------|------|----------|
| 300 | input.c: real event queue | 🔴 HIGHEST |
| 303 | tasking.c: timer tick + preemption | 🔴 HIGH |
| 311 | codegen: function calls/structs/strings | 🔴 HIGH |
| 381 | libm → pure C math | 🟡 MEDIUM |
| 385 | 9P Styx bridge: ZealOS↔Arch | 🟡 MEDIUM |
| 388 | libdrm → direct ioctl | ⬜ LOW |
| 389 | libgbm → custom GBM | ⬜ LOW |
| 391 | MIR → self-contained JIT | 🟡 MEDIUM |
