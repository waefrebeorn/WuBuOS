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

**WSL hook pattern**: WSL2 runs a Linux kernel in a Hyper-V VM. WuBuOS runs the Arch kernel as its driver layer — same pattern, different purpose. WSL exposes Linux to Windows. WuBuOS exposes Linux drivers to ZealOS apps.

---

## DA1: The Optimist — "This Architecture Works"

### Why Arch kernel as driver layer is correct

1. **DRM/KMS is the ONLY path for direct display on Linux.** X11 is a middleman. Wayland compositors (Mutter, KWin, Sway) ALL use DRM/KMS underneath. Going direct removes the compositor entirely. SteamOS does exactly this — games talk to DRM directly.

2. **NVIDIA drivers ONLY work through the Linux kernel.** There is no port of nvidia.ko to any other OS. If WuBuOS wants GPU acceleration for SteamOS containers, it MUST have the Linux kernel running nvidia.ko. Arch's `nvidia-dkms` package compiles against whatever kernel is running.

3. **The WSL pattern proves it works.** WSL2 runs a full Linux kernel (Microsoft's fork) inside a Hyper-V partition. The Windows host can access Linux filesystems via `\\wsl$\`, Linux PIDs are visible, and Linux networking is bridged. WuBuOS just reverses the polarity: ZealOS is the "host" and Arch is the "driver partition."

4. **Container runtime already works.** `wubu_host_exec.c` does real `fork()+chroot()+execv()`. The Arch kernel is ALREADY running (we're on WSL2 right now). We just need to:
   - Open `/dev/dri/card0` directly (replacing X11)
   - Use `drmModeSetCRTC()` for mode setting
   - Use `evdev` for keyboard/mouse (replacing XInput)
   - Use GBM for buffer allocation
   - Done. Zero X11 dependency.

5. **The 9P namespace makes the split clean.** ZealOS side mounts `/dev/draw` from the Arch side via Styx. The DOS/Temple side doesn't know or care that DRM is providing the framebuffer — it just reads/writes pixels via the 9P mount, exactly like Inferno OS.

6. **This IS the Inferno model.** Inferno's `emu` on Linux uses the host's X11/DRM for display. Inferno's `emu` on Plan 9 uses the host's `draw(3)` device. WuBuOS on Arch uses DRM/KMS. Same abstraction, different host.

### The WSL hook opportunity

WSL2 exports these to the Windows host:
- `/dev` — device files (including `/dev/dxg` — GPU paravirtualization!)
- `/proc` — process information
- `/sys` — kernel sysfs
- Network via Hyper-V virtual switch

WuBuOS can hook the SAME mechanisms:
- `/dev/dri/card0` → DRM/KMS display (instead of X11)
- `/dev/input/eventX` → evdev keyboard/mouse (instead of XInput)
- `/dev/nvidia*` → NVIDIA GPU passthrough
- `/dev/snd/*` → ALSA audio

**Key insight**: WSL2's `/dev/dxg` is a **paravirtualized GPU** — it exposes the Windows GPU to the Linux guest via a custom VMBus channel. WuBuOS can use the SAME Hyper-V VMBus mechanism to expose the GPU to the ZealOS side. This is the "WSL method hook" — we're not replicating WSL, we're riding the same infrastructure.

---

## DA2: The Realist — "Here Are The Actual Gaps In The Code Right Now"

### Gap 1: wubu_display.h is 63 lines of HEADER ONLY

```c
// src/hosted/wubu_display.h
int  wubu_display_init(WubuDisplay *d, int width, int height);
void wubu_display_swap(WubuDisplay *d);
int  wubu_display_poll_input(WubuDisplay *d);
void wubu_display_shutdown(WubuDisplay *d);
```

**None of these functions exist in a .c file.** There is no `wubu_display.c`. Cell 380 is marked 🟡 but it hasn't been started. The hosted binary compiles against `libX11` and will crash if X11 isn't available.

**Verdict**: The DRM/KMS path is aspirational. Current code CANNOT run without X11.

### Gap 2: No /dev/dri access in WSL2

```bash
$ ls /dev/dri/
ls: cannot access '/dev/dri/': No such file or the directory does not exist
```

WSL2 does NOT expose `/dev/dri` by default. You need:
- `wsl.conf` `[boot]` + GPU paravirtualization (newer WSL2 builds)
- Or `D3D12` rendering via DirectX paravirtualization (not DRM)

**This means**: Even if we implement DRM/KMS, it won't work on the current development environment (WSL2). We'd need to test on bare Linux or in a VM with GPU passthrough.

**Alternative**: WSL2's `/dev/dxg` + `mesa`'s `d3d12` driver creates a Gallium3D driver that renders via DirectX. This is NOT standard DRM/KMS. We'd need either:
- A real Linux environment (Arch bare metal, Arch VM)
- Or implement a DX12/Vulkan paravirtualized path via `/dev/dxg`

### Gap 3: evdev input needs /dev/input which WSL2 also lacks

```bash
$ ls /dev/input/
ls: cannot access '/dev/input/': No such file or the directory does not exist
```

X11 input works because X server has the input driver. DRM/KMS + evdev require `/dev/input/eventX` which WSL2 doesn't provide. The fallback path must include X11 or libinput over udev.

### Gap 4: hosted.c is deeply coupled to X11 types

```c
// hosted.c line 25-27
typedef Display    XDpy;
typedef Window     XWin;  
typedef GC         XGc;
```

The entire event loop (`hosted_run`) is structured around `XPending/XNextEvent/XPutImage`. Switching to DRM/KMS + evdev means rewriting the entire event loop. This is NOT a small refactor — it's a new frontend for the same backend (VBE framebuffer).

### Gap 5: Container GPU passthrough is configured but never applied

```c
// wubu_host_exec.c line 199
wubu_ct_add_bind(ct, "/dev/dri", "/dev/dri", false);
```

The binds are STORED in `ct->binds[]` but the `wubu_ct_start` function does `fork()+chroot()+execv()` WITHOUT mounting any bind mounts. The `ct->binds` array is dead configuration — never consumed.

**To actually do GPU passthrough**: After `chroot()` but before `execv()`, we need:
```c
for (int i = 0; i < ct->n_binds; i++) {
    mount(ct->binds[i].host, ct->binds[i].guest, NULL, MS_BIND|MS_REC, NULL);
}
```

### Gap 6: ZealOS side has NO concept of the Arch kernel

The `zealos_parity.h` maps ZealOS names to WuBuOS C equivalents but there's no mechanism for ZealOS code to "call into" the Arch kernel. On bare metal, ZealOS IS the kernel. On hosted, the Arch kernel IS the kernel. The ZealOS side needs a **9P callout mechanism**:

```
ZealOS app calls LFBFlush() → WuBuOS translates to Twrite on /dev/draw → Arch DRM/KMS renders
```

This callout doesn't exist. `vbe_swap()` copies pixels internally. There's no 9P dispatch to a renderer.

---

## DA3: The Pessimist — "This Will Fail Because..."

### Failure mode 1: DRM master contention

When WuBuOS opens `/dev/dri/card0` and calls `drmSetMaster()`, it becomes the DRM master. No other compositor can be master simultaneously. If the user runs WuBuOS inside a desktop session (GNOME, KDE, Sway), the desktop compositor is ALREADY the DRM master. `drmSetMaster()` will FAIL with EPERM.

**Fix paths**:
- Run WuBuOS as the ONLY display server (no compositor — like SteamOS's gamescope)
- Or run as a DRM **lease** client (DRM leasing — unprivileged, compositor grants a CRTC lease)
- Or fallback to X11/Wayland client when not DRM master

### Failure mode 2: VT switching and input stealing

DRM/KMS runs on a virtual terminal (VT). When WuBuOS becomes DRM master on VT2, input goes directly via evdev. But Linux's VT subsystem (kbd-leds, console switching) fights with evdev raw reads. The kernel may deliver key events to BOTH the VT console and the evdev fd. This causes double-input, stuck keys, etc.

**Real precedent**: Every KMS compositor (sway, wlroots, kmscon) has bugs around VT switching. The fix requires `libseat` or `sd_seat` to manage seat/VT/DRM master lifecycle correctly. This is a 1000+ LOC subsystem.

### Failure mode 3: NVIDIA DRM support is incomplete

NVIDIA's kernel driver (`nvidia.ko`) does NOT implement the full KMS API. Key gaps:
- `DRM_IOCTL_MODE_CREATE_DUMB` — fails (no dumb buffer support)
- GBM — needs `gbm-nvidia` wrapper, not standard `gbm`
- Explicit fencing — missing on older driver versions

SteamOS works around this with `gamescope` (a nested Wayland compositor that renders to an Xwayland surface). Going "direct DRM" with NVIDIA means either:
- Requiring `nvidia-drm.modeset=1` (kernel param, may break X11)
- Or accepting that NVIDIA needs X11/Wayland as an intermediary (same as everyone else)

### Failure mode 4: The WSL hook is parasitic, not symbiotic

WSL2 works because Microsoft controls BOTH sides:
- The Windows kernel exposes VMBus channels
- The Linux kernel has Microsoft's VMBus drivers (`hv_utils`, `hv_vmbus`)
- `/dev/dxg` is a custom Microsoft device with matching Mesa driver

WuBuOS trying to "hook WSL methods" means:
- Depending on Microsoft's proprietary VMBus protocol (undocumented, changes between Windows builds)
- Competing with WSL's own Linux instance (can two Linux VMs share one GPU?)
- Breaking every time Microsoft updates WSL2 (which happens monthly)

This isn't a "hook" — it's a fragile reverse-engineering dependency. If we want GPU on Windows, the CORRECT path is: WuBuOS runs AS the WSL2 distro. The user does `wsl -d WuBuOS` and our Arch-based distribution starts with GPU already available via `/dev/dxg`.

### Failure mode 5: The DOS/ZealOS split has no seam

"Arch for drivers, ZealOS for DOS" sounds clean but ZealOS's HolyC JIT compiles and executes code that DIRECTLY writes to VGA registers (`0xA0000`, `outb(0x3C4, ...)`. These are x86 I/O port instructions. The Arch kernel CANNOT relay these — DRM/KMS has no concept of VGA I/O ports.

On bare metal: ZealOS owns ring 0, writes VGA directly. Works.  
On hosted (Arch kernel): VGA I/O port writes from ZealOS code SILENTLY FAIL. There's no VGA hardware to write to. The hosted mode already works around this via `VBE_HOSTED` (calloc-backed framebuffer), but the **ZealOS apps that use DOS/VGA APIs directly** (not the WuBuOS GUI shell) cannot run without full VGA register emulation.

This is the fundamental split:
- **WuBuOS GUI apps** (Win98 desktop, WM, taskbar) → render to VBE framebuffer → Arch DRM/KMS displays it. **WORKS.**
- **ZealOS DOS apps** (HolyC JIT'd code writing VGA registers) → no VGA hardware on hosted → **BROKEN.**

---

## Synthesis: What To Actually Build

### Phase 1: The Display Backend Abstraction (DO THIS NOW)

Create `wubu_display.c` with TWO backends:
```c
typedef enum {
    WUBU_DISPLAY_DRM,    // DRM/KMS direct (production: SteamOS, bare Arch)
    WUBU_DISPLAY_X11,    // X11 fallback (dev: WSL2, nested Desktop)
} WubuDisplayBackend;
```

`wubu_display_init()` tries DRM first, falls back to X11. Same interface:
- `wubu_display_swap()` — either DRM page flip or XPutImage
- `wubu_display_poll_input()` — either evdev or XNextEvent

This unblocks Cell 380 and keeps WSL2 dev environment working.

### Phase 2: Run WuBuOS AS a WSL2 Distribution

Instead of "hooking WSL methods", BE a WSL2 distro:
- `wsl --install WuBuOS` registers our Arch rootfs
- GPU available via `/dev/dxg` + Mesa d3d12
- Networking via Hyper-V virtual switch
- `/mnt/c/` automatically available

This is zero-effort GPU + networking. The `wubu` binary runs as a Wayland client (since WSL2 now supports Wayland natively via WSLg).

### Phase 3: Container Bind Mount Implementation

Apply the dead `ct->binds` array in `wubu_ct_start()`:
```c
// After chroot(), before execv():
for (int i = 0; i < ct->n_binds; i++) {
    mkdir_p(ct->binds[i].guest);  // ensure mount point exists
    mount(ct->binds[i].host, ct->binds[i].guest, NULL,
          MS_BIND|MS_REC, NULL);
}
```

This makes GPU passthrough ACTUALLY WORK instead of being stored-but-never-applied.

### Phase 4: 9P Styx Bridge Between ZealOS and Arch

```
Arch kernel (DRM/KMS) ←── Styx ──→ ZealOS apps (ring-0 HolyC)
     /dev/draw                      LFBFlush()
     /dev/mouse                     GetMouse()
     /dev/cons                      Print()
```

ZealOS `LFBFlush()` → `Twrite` on `/dev/draw` → Arch DRM renders.  
Arch `evdev` → `Tread` from `/dev/mouse` → ZealOS `GetMouse()` returns data.

### Phase 5: Real Arch Rootfs for Containers

Build a minimal Arch rootfs (~200MB) with:
- `pacman -S nvidia-dkms mesa libdrm wayland`
- `pacman -S steam-devices alsa-utils`
- Strip to bare minimum

This is the container base that `.wubu` containers `chroot()` into.

---

## Scorecard

| Proposition | DA1 (Optimist) | DA2 (Realist) | DA3 (Pessimist) | Verdict |
|-------------|----------------|---------------|------------------|---------|
| Arch kernel as driver layer | ✅ Correct arch | ⬜ wubu_display.c doesn't exist | ⬜ DRM master contention | **Build Phase 1** |
| DRM/KMS replaces X11 | ✅ SteamOS does it | ⬜ No /dev/dri in WSL2 | ⬜ NVIDIA DRM incomplete | **Dual backend: DRM+X11** |
| ZealOS DOS side via 9P | ✅ Inferno model | ⬜ No 9P dispatch to renderer | ⬜ VGA I/O ports can't be relayed | **Phase 4 — 9P bridge** |
| WSL method hooking | ✅ GPU via /dev/dxg | ⬜ WSL is proprietary | 🔴 Fragile, breaks on updates | **BE a WSL2 distro instead** |
| Container GPU passthrough | ✅ Config exists | 🔴 ct->binds never mounted | — | **Phase 3 — mount binds** |

## Next Cells (from this DA)

| Cell | What | Priority |
|------|------|----------|
| 380 | `wubu_display.c` — DRM/KMS + X11 dual backend | 🔴 HIGHEST |
| 383 | Container bind mount implementation (apply ct->binds) | 🔴 HIGH |
| 384 | WuBuOS as WSL2 distribution (wsl --install WuBuOS) | 🟡 MEDIUM |
| 385 | 9P Styx bridge: ZealOS LFBFlush → Arch DRM render | 🟡 MEDIUM |
| 386 | Arch rootfs builder (minimal + GPU drivers) | ⬜ LOW (infra) |
| 387 | libseat/seatd for DRM master management | ⬜ LOW (hard) |
