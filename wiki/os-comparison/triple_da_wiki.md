# WuBuOS vs GNOME vs SteamOS vs TempleOS vs Arch — Triple Devil's Advocate Wiki
**Purpose**: Comprehensive parity audit identifying every gap between WuBuOS and four reference operating systems
**Method**: Triple DA — (1) What does the reference OS do? (2) What does WuBuOS do? (3) What is the REAL_GAP?
**Updated**: 2026-06-29

---

## ════════════════════════════════════════════════════════════════════════════════
## EXECUTIVE SUMMARY: PARITY MATRIX
## ═══════════════════════════════════════════════════════════════════════════════

| Subsystem | GNOME | SteamOS | TempleOS | Arch | WuBuOS Current | WuBuOS Target |
|-----------|-------|---------|----------|------|----------------|---------------|
| **Kernel** | Linux | Linux | ZealOS (Ring-0) | Linux | ZealOS kernel ✅ | ZealOS + Linux |
| **Init/System** | systemd | systemd + A/B | None (Ring-0) | systemd | Missing | systemd-like |
| **Package Manager** | Flatpak + apt | Steam Runtime + pacman | None | pacman + AUR | Stub (wubu_pkgmgr) | pacman + OCI |
| **Window Manager** | Mutter (Wayland) | gamescope (Wayland) | VGA/VESA direct | KDE/GNOME/WM | Win98 WM (dosgui_wm) | gamescope + Win98 |
| **Compositor** | Mutter | gamescope | None | KWin/Mutter | hosted.c (Wayland client) | gamescope |
| **Shell/UI** | GNOME Shell | Steam Client (CEF) | HolyC REPL | KDE/GNOME | Win98 Shell (dosgui_*) | Steam Client + Win98 |
| **File Manager** | Nautilus | Steam Library | RedSea (DB) | Dolphin/Nautilus | dosgui_explorer ✅ | Nautilus parity |
| **Audio** | PipeWire | PipeWire | PC Speaker | PipeWire | Missing | PipeWire |
| **Network** | NetworkManager | Steam Networking | None | NetworkManager | Netlink (partial) | NetworkManager |
| **Graphics** | Mesa + Vulkan | Mesa + Vulkan + FSR | VGA/VESA | Mesa + Vulkan | DRM/KMS + Vulkan | gamescope + FSR |
| **Containers** | Flatpak + podman | Pressure Vessel | None | systemd-nspawn + podman | bwrap (wubu_ct_isolate) | Pressure Vessel |
| **IPC** | D-Bus | Steam IPC | None | D-Bus | Styx 9P | D-Bus + 9P |
| **Security** | AppArmor + SELinux | seccomp + namespaces | None (Ring-0) | AppArmor/SELinux | seccomp stub | AppArmor + landlock |
| **Boot** | GRUB/systemd-boot | A/B + GRUB | Limine/BIOS | GRUB/systemd-boot/limine | limine ✅ | A/B + Secure Boot |
| **HolyC/JIT** | No | No | Yes (AOT+JIT) | No | minic + JIT (partial) | Full HolyC JIT |

---

## ═════════════════════════════════════════════════════════════════════════════
## DETAILED GAP ANALYSIS BY SUBSYSTEM
## ══════════════════════════════════════════════════════════════════════════════

### 1. KERNEL & BOOT & INIT SYSTEM
**Reference**: systemd (GNOME, SteamOS, Arch) vs ZealOS (TempleOS)

| Feature | GNOME/SteamOS/Arch | TempleOS | WuBuOS | Gap | WuBuOS File |
|---------|-------------------|----------|--------|-----|-------------|
| Parallel service startup | systemd | N/A | Missing | **Entire subsystem** | `runtime/wubu_systemd.c` |
| Socket activation | systemd | N/A | Missing | **Entire subsystem** | `runtime/wubu_systemd.c` |
| Timer units (calendar/monotonic) | systemd | N/A | Missing | **Entire subsystem** | `runtime/wubu_systemd.c` |
| Cgroups v2 resource control | systemd | N/A | Missing | **Entire subsystem** | `runtime/wubu_systemd.c` |
| Journal (structured logging) | systemd | N/A | Missing | **Entire subsystem** | `runtime/wubu_systemd.c` |
| systemd --user | systemd | N/A | Missing | **Entire subsystem** | `runtime/wubu_systemd.c` |
| Machinectl / nspawn | systemd | N/A | Partial (bwrap) | 80% missing | `wubu_ct_isolate.c` |
| A/B rootfs updates | SteamOS | N/A | Missing | **Entire subsystem** | `deploy/wubu_abupdate.c` |
| Immutable /usr (ostree-like) | SteamOS | N/A | Missing | **Entire subsystem** | `deploy/wubu_abupdate.c` |
| GRUB-Btrfs snapshots | SteamOS | N/A | Missing | **Entire subsystem** | `deploy/wubu_abupdate.c` |
| ZealOS kernel boot | N/A | Limine/BIOS | Limine ✅ | Parity | `limine.conf` |
| HolyC kernel config | N/A | HolyC scripts | Missing | **Entire subsystem** | `boot/holyc_config.hc` |

**Triple DA Verdict**: WuBuOS needs **systemd-like init** for Arch/SteamOS parity AND **HolyC kernel config** for TempleOS parity. These are orthogonal — implement both.

---

### 2. PACKAGE MANAGEMENT
**Reference**: pacman + AUR (Arch) vs Steam Runtime (SteamOS) vs Flatpak (GNOME) vs None (TempleOS)

| Feature | Arch | SteamOS | GNOME | TempleOS | WuBuOS | Gap | WuBuOS File |
|---------|------|---------|-------|----------|--------|-----|-------------|
| Binary packages | pacman | Steam Runtime | Flatpak | None | Stub | 95% missing | `runtime/wubu_pacman.c` |
| Dependency resolution | alpm (SAT) | Fixed runtime | Flatpak deps | N/A | Missing | **Entire subsystem** | `runtime/wubu_pacman.c` |
| Repository management | pacman -Syu | Steam updates | Flatpak repo | N/A | Missing | **Entire subsystem** | `runtime/wubu_pacman.c` |
| Package signing (PGP) | pacman-key | Steam keys | Flatpak sigs | N/A | Missing | **Entire subsystem** | `runtime/wubu_pacman.c` |
| Hooks (pre/post trans) | alpm hooks | N/A | Flatpak hooks | N/A | Missing | **Entire subsystem** | `runtime/wubu_pacman.c` |
| Delta updates | xdelta3 | Steam delta | Flatpak delta | N/A | Missing | **Entire subsystem** | `runtime/wubu_pacman.c` |
| AUR (source packages) | yay/paru | N/A | N/A | N/A | Missing | **Entire subsystem** | `apps/wubu_aur.c` |
| PKGBUILD + makepkg | ABS + devtools | N/A | N/A | N/A | Missing | **Entire subsystem** | `runtime/wubu_makepkg.c` |
| Clean chroot build | devtools | Steam Runtime | Flatpak build | N/A | Missing | **Entire subsystem** | `runtime/wubu_makepkg.c` |
| Steam Runtime detection | N/A | Auto | N/A | N/A | Missing | **Entire subsystem** | `runtime/wubu_steamrt.c` |
| Proton prefix management | N/A | Proton | N/A | N/A | Stub | 95% missing | `runtime/wubu_proton.c` |

**Triple DA Verdict**: WuBuOS needs **full pacman/alpm** for Arch parity, **Steam Runtime integration** for SteamOS parity, and **Flatpak support** for GNOME parity. These can share infrastructure (OCI registry in wubu_oci.c).

---

### 3. WINDOW MANAGEMENT & COMPOSITOR
**Reference**: Mutter (GNOME) vs gamescope (SteamOS) vs KWin (Arch KDE) vs VGA/VESA (TempleOS) vs Win98 WM (WuBuOS)

| Feature | GNOME (Mutter) | SteamOS (gamescope) | TempleOS | WuBuOS (Win98) | Gap | WuBuOS File |
|---------|----------------|---------------------|----------|----------------|-----|-------------|
| Wayland compositor | ✅ Mutter | ✅ gamescope | ❌ | Client only | **Compositor missing** | `hosted/wubu_gamescope.c` |
| VRR (FreeSync/G-Sync) | ✅ | ✅ | ❌ | ❌ | **Missing** | `hosted/wubu_metal.c` |
| HDR (HDR10/Dolby Vision) | ✅ | ✅ | ❌ | ❌ | **Missing** | `hosted/wubu_metal.c` |
| FSR (FidelityFX Super Resolution) | ❌ | ✅ | ❌ | ❌ | **Missing** | `hosted/wubu_gamescope.c` |
| Integer scaling | ❌ | ✅ | ❌ | ❌ | **Missing** | `hosted/wubu_gamescope.c` |
| Nested Wayland (for games) | ❌ | ✅ | ❌ | ❌ | **Missing** | `hosted/wubu_gamescope.c` |
| Input grab/focus management | ✅ | ✅ | ✅ (raw) | Partial | 60% missing | `hosted/hosted.c` |
| Dynamic workspaces | ✅ | ❌ | ❌ | Static 4 | **Missing** | `gui/dosgui_wm.c` |
| Window snapping/tiling | ✅ | ❌ | ❌ | Basic move/resize | **Missing** | `gui/dosgui_wm.c` |
| Overview (Activities) | ✅ | ❌ | ❌ | Start Menu only | **Missing** | `gui/dosgui_wm.c` |
| Window animations | ✅ | ❌ | ❌ | None | **Missing** | `gui/dosgui_wm.c` |
| Touch gestures | ✅ | ❌ | ❌ | N/A | **Missing** | `gui/dosgui_wm.c` |
| HiDPI fractional scaling | ✅ | ✅ | ❌ | Not implemented | **Missing** | `hosted/wubu_metal.c` |
| Multi-monitor VRR/HDR | ✅ | ✅ | ❌ | Missing | **Missing** | `hosted/wubu_metal.c` |
| VGA/VESA direct | ❌ | ❌ | ✅ | DRM/KMS | Different paradigm | `kernel/wubu_vga.c` |
| HolyC graphics primitives | ❌ | ❌ | ✅ | Missing | **Entire subsystem** | `kernel/wubu_vga.c` |

**Triple DA Verdict**: WuBuOS needs **gamescope** for SteamOS parity (gaming) AND **Mutter-like features** for GNOME parity (desktop). The Win98 WM is a distinct paradigm — keep it, add gamescope as nested compositor for games.

---

### 4. SHELL & APPLICATION LAUNCHING
**Reference**: GNOME Shell (JS) vs Steam Client (CEF) vs HolyC REPL (TempleOS) vs KDE/Plasma (Arch) vs Win98 (WuBuOS)

| Feature | GNOME Shell | Steam Client | TempleOS | WuBuOS (Win98) | Gap | WuBuOS File |
|---------|-------------|--------------|----------|----------------|-----|-------------|
| App grid/search | ✅ Tracker | ✅ Store/Library | ✅ HolyC REPL | Start Menu ✅ | Parity (different) | `gui/dosgui_startmenu.c` |
| .desktop parsing | ✅ Full spec | ✅ Steam .desktop | ❌ | Basic ✅ | 60% missing | `gui/dosgui_startmenu.c` |
| MIME handling | ✅ Shared MIME | ✅ Steam MIME | ❌ | wubu_mime ✅ | Parity | `gui/wubu_mime.c` |
| App folders | ✅ Drag-drop | ❌ | ❌ | Missing | **Missing** | `gui/dosgui_startmenu.c` |
| Pin to dash/taskbar | ✅ | ❌ | ❌ | Taskbar ✅ | Parity | `gui/dosgui_wm.c` |
| Notifications center | ✅ | ✅ Overlay | ❌ | Missing | **Missing** | `gui/wubu_notifications.c` |
| Calendar/World clock | ✅ | ❌ | ❌ | Basic clock | **Missing** | `gui/dosgui_startmenu.c` |
| System status area | ✅ | ✅ Overlay | ❌ | Basic tray | 70% missing | `gui/dosgui_wm.c` |
| User menu (power, settings) | ✅ | ✅ | ❌ | Shutdown only | **Missing** | `gui/dosgui_startmenu.c` |
| In-Game Overlay | ❌ | ✅ Shift+Tab | ❌ | Missing | **Missing** | `gui/wubu_overlay.c` |
| Big Picture / 10-foot UI | ❌ | ✅ Gamepad | ❌ | Mouse/kb only | **Missing** | `gui/wubu_overlay.c` |
| Remote Play | ❌ | ✅ | ❌ | Missing | **Missing** | `gui/wubu_overlay.c` |
| HolyC REPL | ❌ | ❌ | ✅ Immediate | Stub (eval=0) | **Entire subsystem** | `runtime/wubu_holyd.c` |
| Persistent compiler state | ❌ | ❌ | ✅ | Partial | 80% missing | `runtime/wubu_holyd.c` |
| Compiler as library | ❌ | ❌ | ✅ | Partial (minic) | 60% missing | `compiler/holyc_lib.c` |

**Triple DA Verdict**: WuBuOS Win98 shell is functional for desktop. Needs **Steam Client parity** (CEF UI, overlay, Big Picture) for SteamOS. Needs **HolyC REPL parity** for TempleOS. These are separate UIs — can coexist.

---

### 5. FILE MANAGEMENT
**Reference**: Nautilus (GNOME) vs Steam Library (SteamOS) vs RedSea (TempleOS) vs Dolphin (Arch KDE) vs dosgui_explorer (WuBuOS)

| Feature | Nautilus | Steam Library | RedSea | WuBuOS Explorer | Gap | WuBuOS File |
|---------|----------|---------------|--------|-----------------|-----|-------------|
| Views (list/grid/compact) | ✅ | ✅ Grid | ❌ (DB) | Details/Icons/List ✅ | Parity | `gui/dosgui_explorer.c` |
| Tabs (Ctrl+T) | ✅ | ❌ | ❌ | Missing | **Missing** | `gui/dosgui_explorer.c` |
| Recursive search | ✅ Tracker | ✅ | ❌ | Name only | **Missing** | `apps/wubu_tracker.c` |
| Content search | ✅ Tracker | ❌ | ❌ | Missing | **Missing** | `apps/wubu_tracker.c` |
| Bookmarks/Places | ✅ Sidebar | ❌ | ❌ | Favorites ✅ | Parity | `gui/dosgui_explorer.c` |
| Cloud providers (GVfs) | ✅ SMB/SFTP/Cloud | ❌ | ❌ | Styx 9P only | **Missing** | `runtime/wubu_gvfs.c` |
| Batch rename | ✅ | ❌ | ❌ | Missing | **Missing** | `gui/dosgui_explorer.c` |
| Undo/Redo | ✅ | ❌ | ❌ | Missing | **Missing** | `gui/dosgui_explorer.c` |
| File operations queue | ✅ Progress | ❌ | ❌ | Basic | **Missing** | `gui/dosgui_explorer.c` |
| Thumbnails | ✅ Images/Video/PDF | ✅ Game art | ❌ | Missing | **Missing** | `gui/dosgui_explorer.c` |
| Preview pane | ✅ | ✅ | ❌ | Missing | **Missing** | `gui/dosgui_explorer.c` |
| Drag-drop (internal/external) | ✅ | ❌ | ❌ | Explorer ✅ | Parity | `gui/dosgui_explorer.c` |
| MIME launch | ✅ xdg-open | ✅ Steam | ❌ | wubu_mime ✅ | Parity | `gui/wubu_mime.c` |
| RedSea (DB, no paths) | ❌ | ❌ | ✅ | Styx 9P | Different paradigm | `runtime/styxfs.c` |
| Contiguous allocation | ❌ | ❌ | ✅ | Not guaranteed | **Missing** | `kernel/redsea.c` |

**Triple DA Verdict**: WuBuOS Explorer is strong on basics. Needs **tabs, search, thumbnails, queue, cloud providers** for GNOME parity. Needs **RedSea** for TempleOS parity (optional).

---

### 6. AUDIO SUBSYSTEM
**Reference**: PipeWire + WirePlumber (GNOME, SteamOS, Arch) vs PC Speaker (TempleOS) vs Missing (WuBuOS)

| Feature | PipeWire | TempleOS | WuBuOS | Gap | WuBuOS File |
|---------|----------|----------|--------|-----|-------------|
| Audio graph (nodes/ports/links) | ✅ | ❌ | Missing | **Entire subsystem** | `audio/wubu_audio.c` |
| Session manager (WirePlumber) | ✅ Lua | ❌ | Missing | **Entire subsystem** | `audio/wubu_audio.c` |
| Bluetooth (A2DP, HSP/HFP, LE) | ✅ BlueZ 5 | ❌ | Missing | **Entire subsystem** | `audio/wubu_audio.c` |
| Pro Audio (JACK API) | ✅ | ❌ | Missing | **Entire subsystem** | `audio/wubu_audio.c` |
| Screen capture (portal) | ✅ | ❌ | Missing | **Entire subsystem** | `audio/wubu_audio.c` |
| Video (V4L2, libcamera) | ✅ | ❌ | Missing | **Entire subsystem** | `audio/wubu_audio.c` |
| PulseAudio compat | ✅ | ❌ | Missing | **Entire subsystem** | `audio/wubu_audio.c` |
| ALSA compat | ✅ | ❌ | Missing | **Entire subsystem** | `audio/wubu_audio.c` |
| PC Speaker (Beep/Sound) | ❌ | ✅ | Missing | **Missing** | `audio/wubu_audio.c` |
| Raw PCM (Sound Blaster) | ❌ | ✅ | Missing | **Missing** | `audio/wubu_audio.c` |
| MIDI / Music() | ❌ | ✅ | Missing | **Missing** | `audio/wubu_audio.c` |

**Triple DA Verdict**: WuBuOS **must implement PipeWire** for GNOME/SteamOS/Arch parity. Can add **PC Speaker + Raw PCM** as legacy TempleOS compatibility layer.

---

### 7. NETWORKING
**Reference**: NetworkManager (GNOME, Arch) vs Steam Networking (SteamOS) vs None (TempleOS) vs Netlink (WuBuOS)

| Feature | NetworkManager | Steam Networking | TempleOS | WuBuOS | Gap | WuBuOS File |
|---------|----------------|------------------|----------|--------|-----|-------------|
| Ethernet | ✅ | ❌ | ❌ | Netlink bridge ✅ | Parity (low-level) | `wubu_network.c` |
| WiFi (scan, connect, profiles) | ✅ | ❌ | ❌ | Missing | **Entire subsystem** | `wubu_network.c` |
| WWAN (modem) | ✅ | ❌ | ❌ | Missing | **Entire subsystem** | `wubu_network.c` |
| VPN (OpenVPN, WireGuard, etc.) | ✅ plugins | ❌ | ❌ | Missing | **Entire subsystem** | `wubu_network.c` |
| WireGuard | ✅ | ❌ | ❌ | Missing | **Entire subsystem** | `wubu_network.c` |
| VLAN/Bond/Bridge/Team | ✅ | ❌ | ❌ | Netlink ✅ | Parity (low-level) | `wubu_network.c` |
| DNS (systemd-resolved) | ✅ | ❌ | ❌ | Missing | **Entire subsystem** | `wubu_network.c` |
| DHCP client | ✅ | ❌ | ❌ | Missing | **Entire subsystem** | `wubu_network.c` |
| Connection profiles | ✅ keyfile | ❌ | ❌ | Missing | **Entire subsystem** | `wubu_network.c` |
| Secrets (libsecret) | ✅ | ❌ | ❌ | Missing | **Entire subsystem** | `wubu_network.c` |
| Dispatcher scripts | ✅ | ❌ | ❌ | Missing | **Entire subsystem** | `wubu_network.c` |
| SteamNetworkingSockets | ❌ | ✅ Custom | ❌ | Missing | **Entire subsystem** | `net/wubu_steamnet.c` |
| Relay (SDR) | ❌ | ✅ Valve relays | ❌ | Missing | **Entire subsystem** | `net/wubu_steamnet.c` |
| P2P (ICE, NAT traversal) | ❌ | ✅ | ❌ | Missing | **Entire subsystem** | `net/wubu_steamnet.c` |
| Signaling via Steam | ❌ | ✅ | ❌ | Missing | **Entire subsystem** | `net/wubu_steamnet.c` |

**Triple DA Verdict**: WuBuOS needs **NetworkManager client** for desktop parity AND **SteamNetworkingSockets** for SteamOS parity. These are different stacks — implement both.

---

### 8. GRAPHICS & GPU
**Reference**: Mesa + Vulkan (all Linux) vs gamescope (SteamOS) vs VGA/VESA (TempleOS) vs DRM/KMS + Vulkan (WuBuOS)

| Feature | Linux (Mesa) | gamescope | TempleOS | WuBuOS | Gap | WuBuOS File |
|---------|--------------|-----------|----------|--------|-----|-------------|
| OpenGL 4.6+ | ✅ Mesa | ✅ | ❌ | Missing | **Missing** | `kernel/wubu_drm_direct.c` |
| Vulkan 1.3+ | ✅ Mesa | ✅ | ❌ | Loader ✅ | Parity (loader) | `kernel/wubu_vulkan.c` |
| Compute shaders | ✅ | ✅ | ❌ | bear_vulkan ✅ | Parity (4 pipelines) | `bear/bear_vulkan.c` |
| VRR (FreeSync/G-Sync) | ✅ | ✅ | ❌ | Missing | **Missing** | `hosted/wubu_metal.c` |
| HDR (HDR10, Dolby Vision) | ✅ | ✅ | ❌ | Missing | **Missing** | `hosted/wubu_metal.c` |
| FSR (1/2/3) | ✅ FSR | ✅ Built-in | ❌ | Missing | **Missing** | `hosted/wubu_gamescope.c` |
| Integer scaling | ✅ | ✅ | ❌ | Missing | **Missing** | `hosted/wubu_gamescope.c` |
| AMD CAS | ✅ | ✅ | ❌ | Missing | **Missing** | `hosted/wubu_gamescope.c` |
| Nested Wayland | ❌ | ✅ | ❌ | Missing | **Missing** | `hosted/wubu_gamescope.c` |
| Latency reduction (tearing) | ❌ | ✅ | ❌ | Missing | **Missing** | `hosted/wubu_gamescope.c` |
| VGA/VESA direct | ❌ | ❌ | ✅ | DRM/KMS | Different paradigm | `kernel/wubu_vga.c` |
| HolyC graphics primitives | ❌ | ❌ | ✅ | Missing | **Entire subsystem** | `kernel/wubu_vga.c` |

**Triple DA Verdict**: WuBuOS has **Vulkan compute** (bear_vulkan). Needs **gamescope** for SteamOS gaming parity. Needs **Mesa/OpenGL** for desktop app parity. Needs **VGA/VESA** for TempleOS HolyC graphics parity.

---

### 9. CONTAINERS & SANDBOXING
**Reference**: Pressure Vessel (SteamOS) vs Flatpak (GNOME) vs systemd-nspawn/podman (Arch) vs None (TempleOS) vs bwrap (WuBuOS)

| Feature | Pressure Vessel | Flatpak | systemd-nspawn | WuBuOS (bwrap) | Gap | WuBuOS File |
|---------|-----------------|---------|----------------|----------------|-----|-------------|
| bwrap fork+exec | ✅ | ✅ | ❌ | ✅ | Parity | `wubu_ct_isolate.c` |
| User namespace | ✅ | ✅ | ✅ | ✅ | Parity | `wubu_ct_isolate.c` |
| PID namespace | ✅ | ✅ | ✅ | ✅ | Parity | `wubu_ct_isolate.c` |
| Network namespace | ✅ | ✅ | ✅ | ❌ | **Missing** | `wubu_ct_isolate.c` |
| IPC namespace | ✅ | ✅ | ✅ | ❌ | **Missing** | `wubu_ct_isolate.c` |
| UTS namespace | ✅ | ✅ | ✅ | ❌ | **Missing** | `wubu_ct_isolate.c` |
| cgroup namespace | ✅ | ✅ | ✅ | ❌ | **Missing** | `wubu_ct_isolate.c` |
| seccomp-bpf filter | ✅ | ✅ | ✅ | Stub | 90% missing | `wubu_ct_isolate.c` |
| GPU passthrough (/dev/dri) | ✅ | ✅ | ✅ | Partial | 50% missing | `wubu_ct_isolate.c` |
| Read-only /usr bind | ✅ | ✅ | ❌ | Missing | **Missing** | `wubu_ct_isolate.c` |
| Steam Runtime env | ✅ | ❌ | ❌ | Missing | **Missing** | `runtime/wubu_steamrt.c` |
| D-Bus/PipeWire socket pass | ✅ | ✅ | ✅ | Missing | **Missing** | `wubu_ct_isolate.c` |
| OCI image support | ❌ | ✅ | ✅ | wubu_oci.c ✅ | Parity (registry) | `runtime/wubu_oci.c` |
| Flatpak portals | ❌ | ✅ | ❌ | Missing | **Entire subsystem** | `runtime/wubu_flatpak.c` |
| AppArmor/SELinux profiles | ❌ | ✅ | ✅ | Missing | **Entire subsystem** | `kernel/wubu_apparmor.c` |
| landlock (unpriv sandbox) | ❌ | ✅ | ❌ | Missing | **Entire subsystem** | `kernel/wubu_landlock.c` |

**Triple DA Verdict**: WuBuOS bwrap is a good start. Needs **all namespaces + seccomp + Steam Runtime** for Pressure Vessel parity. Needs **Flatpak portals** for GNOME parity. Needs **systemd-nspawn/OCI** for Arch parity.

---

### 10. IPC & SERVICE ACTIVATION
**Reference**: D-Bus (GNOME, Arch, SteamOS) vs Steam IPC (SteamOS) vs None (TempleOS) vs Styx 9P (WuBuOS)

| Feature | D-Bus | Steam IPC | TempleOS | WuBuOS (9P) | Gap | WuBuOS File |
|---------|-------|-----------|----------|-------------|-----|-------------|
| System bus | ✅ | ❌ | ❌ | Missing | **Entire subsystem** | `runtime/wubu_dbus.c` |
| Session bus | ✅ | ❌ | ❌ | Missing | **Entire subsystem** | `runtime/wubu_dbus.c` |
| Service activation | ✅ .service files | ❌ | ❌ | Missing | **Entire subsystem** | `runtime/wubu_dbus.c` |
| Introspection | ✅ | ❌ | ❌ | Missing | **Entire subsystem** | `runtime/wubu_dbus.c` |
| Properties (Get/Set) | ✅ | ❌ | ❌ | Missing | **Entire subsystem** | `runtime/wubu_dbus.c` |
| Signals (broadcast) | ✅ | ❌ | ❌ | Missing | **Entire subsystem** | `runtime/wubu_dbus.c` |
| Policy (system.d) | ✅ | ❌ | ❌ | Missing | **Entire subsystem** | `runtime/wubu_dbus.c` |
| Flatpak portals | ✅ | ❌ | ❌ | Missing | **Entire subsystem** | `runtime/wubu_flatpak.c` |
| Steam IPC | ❌ | ✅ Custom | ❌ | Missing | **Entire subsystem** | `net/wubu_steamnet.c` |
| Styx 9P (file namespace) | ❌ | ❌ | ❌ | ✅ | Unique to WuBuOS | `runtime/styxfs.c` |
| RedSea (DB namespace) | ❌ | ❌ | ✅ | Styx 9P | Different paradigm | `kernel/redsea.c` |

**Triple DA Verdict**: WuBuOS **must implement D-Bus** — it's required for Flatpak, systemd, NetworkManager, PipeWire, Polkit, GVfs. Styx 9P is WuBuOS's unique namespace — keep it alongside D-Bus.

---

### 11. SECURITY & SANDBOXING
**Reference**: AppArmor/SELinux (GNOME, Arch) vs seccomp+namespaces (SteamOS) vs None (TempleOS) vs seccomp stub (WuBuOS)

| Feature | AppArmor/SELinux | SteamOS | TempleOS | WuBuOS | Gap | WuBuOS File |
|---------|------------------|---------|----------|--------|-----|-------------|
| MAC (Mandatory Access Control) | ✅ AppArmor/SELinux | seccomp only | None | Missing | **Entire subsystem** | `kernel/wubu_apparmor.c` |
| seccomp-bpf | ✅ (containers) | ✅ Pressure Vessel | None | Stub | 90% missing | `wubu_ct_isolate.c` |
| landlock (unpriv sandbox) | ✅ Linux 5.13+ | ❌ | None | Missing | **Entire subsystem** | `kernel/wubu_landlock.c` |
| capabilities (file/ambient) | ✅ | ✅ | None (Ring-0) | Missing | **Entire subsystem** | `kernel/wubu_caps.c` |
| Secure Boot | ✅ shim+GRUB | ✅ shim+GRUB | None | limine ✅ | Parity (limine) | `limine.conf` |
| dm-verity (rootfs verify) | ✅ | ✅ | None | Missing | **Entire subsystem** | `deploy/wubu_verity.c` |
| fsverity (file integrity) | ✅ | ❌ | None | Missing | **Entire subsystem** | `deploy/wubu_fsverity.c` |
| TPM2 (PCR binding) | ✅ systemd-cryptenroll | ❌ | None | Missing | **Entire subsystem** | `deploy/wubu_tpm2.c` |
| Ring-0 (no memory protection) | ❌ | ❌ | ✅ | Hosted (Linux) | Different paradigm | N/A |

**Triple DA Verdict**: WuBuOS needs **AppArmor/landlock/seccomp** for Linux parity. TempleOS Ring-0 model is a different threat model — not applicable to hosted WuBuOS.

---

### 12. HOLYC / JIT COMPILER
**Reference**: TempleOS HolyC (AOT+JIT, REPL, compiler lib) vs minic+JIT (WuBuOS) vs None (others)

| Feature | TempleOS HolyC | WuBuOS (minic+JIT) | Gap | WuBuOS File |
|---------|----------------|-------------------|-----|-------------|
| AOT compilation | ✅ ELF | ❌ | **Entire subsystem** | `compiler/holyc_codegen.c` |
| JIT from string (ExeCode) | ✅ | Partial (minic) | 70% missing | `compiler/holyc_codegen.c` |
| Compiler as library | ✅ Lex/Parse/Opt/CodeGen | Partial (minic) | 60% missing | `compiler/holyc_lib.c` |
| Real-time REPL | ✅ Immediate | Stub (eval=0) | 95% missing | `runtime/wubu_holyd.c` |
| Persistent compiler state | ✅ Symbols/types/macros | Partial (session) | 80% missing | `runtime/wubu_holyd.c` |
| Whole-program optimization | ✅ | ❌ | **Entire subsystem** | `compiler/holyc_codegen.c` |
| HolyC language (class, exception, coro) | ✅ | ❌ | **Entire subsystem** | `compiler/holyc_features.c` |
| Reflection (ClassRep, MemberRep) | ✅ | ❌ | **Entire subsystem** | `compiler/holyc_features.c` |
| Meta-programming (compile-time exec) | ✅ | ❌ | **Entire subsystem** | `compiler/holyc_features.c` |
| Doc/DolDoc (hyperlinked docs) | ✅ | ❌ | **Entire subsystem** | `apps/wubu_doldoc.c` |
| Task = HolyC function | ✅ | VSL process | Different paradigm | `runtime/wubu_holyctask.c` |
| Identity-mapped memory | ✅ | Hosted (Linux) | Different paradigm | `kernel/wubu_identity_map.c` |
| Ring-0 execution | ✅ | Hosted (Linux) | Different paradigm | `kernel/wubu_ring0.c` |

**Triple DA Verdict**: WuBuOS has **minic + basic JIT**. Needs **full HolyC JIT (AOT+JIT)** for TempleOS parity. This is a **major subsystem** — 8+ sessions.

---

### 13. BOOT & INITRAMFS
**Reference**: GRUB/systemd-boot/limine + mkinitcpio/dracut (Arch) vs A/B + GRUB (SteamOS) vs Limine (TempleOS/ZealOS) vs limine (WuBuOS)

| Feature | Arch | SteamOS | TempleOS | WuBuOS | Gap | WuBuOS File |
|---------|------|---------|----------|--------|-----|-------------|
| limine bootloader | ❌ | ❌ | ✅ | ✅ | Parity | `limine.conf` |
| GRUB / systemd-boot | ✅ | ✅ | ❌ | ❌ | **Missing** | `deploy/wubu_grub.c` |
| Secure Boot (shim) | ✅ | ✅ | ❌ | limine signed | Parity (limine) | `deploy/wubu_secureboot.c` |
| mkinitcpio hooks | ✅ | ❌ | ❌ | create-initramfs.sh | 70% missing | `deploy/wubu_mkinitcpio.c` |
| dracut | ✅ opt | ❌ | ❌ | ❌ | **Missing** | `deploy/wubu_dracut.c` |
| A/B rootfs partitions | ❌ | ✅ | ❌ | Missing | **Entire subsystem** | `deploy/wubu_abupdate.c` |
| Atomic updates (ostree) | ❌ | ✅ | ❌ | Missing | **Entire subsystem** | `deploy/wubu_abupdate.c` |
| Rollback on boot fail | ❌ | ✅ | ❌ | Missing | **Entire subsystem** | `deploy/wubu_abupdate.c` |
| Delta updates | ✅ | ✅ | ❌ | Missing | **Entire subsystem** | `deploy/wubu_abupdate.c` |
| GRUB-Btrfs snapshots | ❌ | ✅ | ❌ | Missing | **Entire subsystem** | `deploy/wubu_abupdate.c` |
| Encryption (LUKS) hook | ✅ | ✅ | ❌ | Missing | **Missing** | `deploy/wubu_mkinitcpio.c` |
| LVM/RAID/Btrfs/ZFS hooks | ✅ | ❌ | ❌ | Missing | **Missing** | `deploy/wubu_mkinitcpio.c` |

**Triple DA Verdict**: WuBuOS has **limine** (good for ZealOS parity). Needs **mkinitcpio hooks** for Arch parity. Needs **A/B updates** for SteamOS parity.

---

## ═════════════════════════════════════════════════════════════════════════════
## CONSOLIDATED GAP PRIORITY MATRIX
## ══════════════════════════════════════════════════════════════════════════════

### TIER 0: FOUNDATIONAL (Blocks everything else)
| Priority | Gap | Required For | WuBuOS File | Sessions |
|----------|-----|--------------|-------------|----------|
| 0.1 | D-Bus daemon + libdbus | Flatpak, systemd, NM, PipeWire, Polkit | `runtime/wubu_dbus.c` | 3 |
| 0.2 | systemd-like init (services, sockets, timers, cgroups, journal) | Arch, SteamOS, GNOME parity | `runtime/wubu_systemd.c` | 10 |
| 0.3 | PipeWire + WirePlumber | Audio, Bluetooth, screen capture | `audio/wubu_audio.c` | 8 |
| 0.4 | NetworkManager client | WiFi, VPN, WWAN, profiles, DNS | `wubu_network.c` | 6 |
| 0.5 | pacman + alpm library | Arch base, package management | `runtime/wubu_pacman.c` | 8 |

### TIER 1: STEAMOS GAMING PARITY
| Priority | Gap | Required For | WuBuOS File | Sessions |
|----------|-----|--------------|-------------|----------|
| 1.1 | gamescope compositor (VRR, HDR, FSR, nested Wayland) | SteamOS gaming | `hosted/wubu_gamescope.c` | 5 |
| 1.2 | DXVK (D3D9/10/11 → Vulkan) | Proton/Windows games | `runtime/wubu_proton_dxvk.c` + `runtime/wubu_dxvk_conf.c` (was `runtime/wubu_dxvk.c`) | 6 |
| 1.3 | VKD3D-Proton (D3D12 → Vulkan) | DX12 games | `runtime/wubu_vkd3d.c` | 8 |
| 1.4 | Pressure Vessel (full namespaces, seccomp, Steam Runtime) | Game containers | `wubu_ct_isolate.c` | 4 |
| 1.5 | Steam Input (controller configs, gyro, haptics) | Steam Deck controls | `input/wubu_steaminput.c` | 5 |
| 1.6 | Steam Networking Sockets (relay, P2P, SDR) | Multiplayer | `net/wubu_steamnet.c` | 5 |
| 1.7 | Shader pre-cache (fossilize + dxvk-cache) | Launch performance | `runtime/wubu_fossilize.c` | 4 |
| 1.8 | Proton prefix management + ProtonDB | Game compatibility | `runtime/wubu_proton.c` | 3 |

### TIER 2: GNOME DESKTOP PARITY
| Priority | Gap | Required For | WuBuOS File | Sessions |
|----------|-----|--------------|-------------|----------|
| 2.1 | Flatpak + Portals (file chooser, screenshot, etc.) | Sandboxed apps | `runtime/wubu_flatpak.c` | 6 |
| 2.2 | GVfs (SMB, SFTP, cloud, trash) | Cloud filesystems | `runtime/wubu_gvfs.c` | 4 |
| 2.3 | Tracker + SPARQL (file content search) | Search | `apps/wubu_tracker.c` | 3 |
| 2.4 | AT-SPI2 accessibility (Orca, magnifier, OSK) | A11y compliance | `gui/wubu_a11y.c` | 3 |
| 2.5 | Polkit agent + backend | Privilege escalation | `apps/wubu_polkit.c` | 4 |
| 2.6 | AppArmor/landlock | Sandboxing | `kernel/wubu_apparmor.c` | 4 |
| 2.7 | Explorer: tabs, thumbnails, search, queue | Nautilus parity | `gui/dosgui_explorer.c` | 2 |
| 2.8 | Start Menu: folders, search, notifications | GNOME Shell parity | `gui/dosgui_startmenu.c` | 2 |

### TIER 3: ARCH DISTRO PARITY
| Priority | Gap | Required For | WuBuOS File | Sessions |
|----------|-----|--------------|-------------|----------|
| 3.1 | PKGBUILD parser + makepkg | Building packages | `runtime/wubu_makepkg.c` | 6 |
| 3.2 | AUR RPC client + helper | Community packages | `apps/wubu_aur.c` | 3 |
| 3.3 | mkinitcpio hooks (encrypt, lvm, raid, btrfs, zfs) | Initramfs | `deploy/wubu_mkinitcpio.c` | 4 |
| 3.4 | systemd-homed / sysusers | Portable homes | `runtime/wubu_homed.c` | 4 |
| 3.5 | systemd-nspawn / OCI runtime | Containers | `wubu_ct_isolate.c` | 5 |
| 3.6 | Secure Boot (shim + limine/GRUB) | Hardware support | `deploy/wubu_secureboot.c` | 3 |
| 3.7 | CUPS printing | Printing | `apps/wubu_cups.c` | 3 |
| 3.8 | GRUB-Btrfs snapshots | Rollback | `deploy/wubu_abupdate.c` | 4 |

### TIER 4: TEMPLEOS/ZEALOS PARITY
| Priority | Gap | Required For | WuBuOS File | Sessions |
|----------|-----|--------------|-------------|----------|
| 4.1 | HolyC JIT (AOT + JIT, whole-program opt) | HolyC parity | `compiler/holyc_codegen.c` | 8 |
| 4.2 | Compiler as library (Lex/Parse/Opt/CodeGen) | HolyC parity | `compiler/holyc_lib.c` | 5 |
| 4.3 | Real-time REPL + persistent state | HolyC REPL | `runtime/wubu_holyd.c` | 4 |
| 4.4 | HolyC language features (class, exception, coro) | HolyC parity | `compiler/holyc_features.c` | 8 |
| 4.5 | Doc/DolDoc system | TempleOS docs | `apps/wubu_doldoc.c` | 6 |
| 4.6 | RedSea filesystem (contiguous, B-tree) | TempleOS FS | `kernel/redsea.c` | 6 |
| 4.7 | VGA/VESA direct graphics | HolyC graphics | `kernel/wubu_vga.c` | 3 |
| 4.8 | PC Speaker + Raw PCM audio | HolyC Sound/Music | `audio/wubu_audio.c` | 2 |
| 4.9 | Task = HolyC function model | TempleOS process | `runtime/wubu_holyctask.c` | 4 |

### TIER 5: POLISH & HARDWARE
| Priority | Gap | Required For | WuBuOS File | Sessions |
|----------|-----|--------------|-------------|----------|
| 5.1 | A/B rootfs updates + atomic rollback | SteamOS-like updates | `deploy/wubu_abupdate.c` | 4 |
| 5.2 | Steam Deck hardware drivers (HID, fan, battery) | Deck hardware | `kernel/wubu_deckhw.c` | 4 |
| 5.3 | In-Game Overlay (CEF-based) | Steam Client parity | `gui/wubu_overlay.c` | 6 |
| 5.4 | Mesa/OpenGL 4.6+ | Desktop apps | `kernel/wubu_drm_direct.c` | 4 |
| 5.5 | Multi-monitor HiDPI fractional scaling | Modern displays | `hosted/wubu_metal.c` | 3 |
| 5.6 | Window snapping/tiling/animations | Modern WM | `gui/dosgui_wm.c` | 2 |
| 5.7 | fwupd (firmware updates) | Hardware support | `deploy/wubu_fwupd.c` | 2 |
| 5.8 | Power profiles daemon | Laptop battery | `deploy/wubu_power.c` | 2 |

---

## ══════════════════════════════════════════════════════════════════════════════
## TRIPLE DEVIL'S ADVOCATE: CONFLICTING REQUIREMENTS
## ══════════════════════════════════════════════════════════════════════════════

### Conflict 1: Init System
- **Arch/GNOME/SteamOS**: systemd (complex, featureful, Linux-standard)
- **TempleOS**: None (Ring-0, no init)
- **WuBuOS Decision**: Implement **systemd-like** for hosted mode (required for Flatpak, NM, PipeWire). For bare-metal ZealOS path, keep **minimal init** (limine → kernel → shell). These are separate boot paths.

### Conflict 2: Compositor
- **GNOME**: Mutter (general desktop, accessibility, extensions)
- **SteamOS**: gamescope (gaming, VRR, HDR, FSR, low latency)
- **TempleOS**: None (VGA/VESA direct)
- **WuBuOS Decision**: **gamescope as primary compositor** (covers gaming + basic desktop). Implement **Mutter-like features** (a11y, extensions) as gamescope plugins or optional second compositor. TempleOS VGA/VESA is bare-metal only.

### Conflict 3: Package Management
- **Arch**: pacman + AUR (binary + source, rolling, user-centric)
- **SteamOS**: Steam Runtime + Pressure Vessel (fixed, containerized, game-focused)
- **GNOME**: Flatpak (sandboxed, cross-distro, portal-mediated)
- **TempleOS**: None (HolyC compiles everything)
- **WuBuOS Decision**: **Three-layer approach**:
  1. **pacman/alpm** for system packages (Arch parity)
  2. **Steam Runtime + Pressure Vessel** for game containers (SteamOS parity)
  3. **Flatpak + Portals** for sandboxed desktop apps (GNOME parity)
  4. **HolyC JIT** for TempleOS apps (compile-on-demand)

### Conflict 4: Window Manager
- **GNOME**: Mutter (dynamic workspaces, overview, animations, a11y)
- **SteamOS**: gamescope (fullscreen games, VRR/HDR, nested Wayland)
- **TempleOS**: No WM (HolyC draws directly)
- **WuBuOS**: Win98 WM (dosgui_wm - taskbar, start menu, static workspaces)
- **WuBuOS Decision**: **Keep Win98 WM as default desktop shell**. Run **gamescope nested** for Steam games. Add **Mutter-like features** (dynamic workspaces, overview, animations) to Win98 WM optionally. TempleOS apps run in **HolyC REPL window** (dosgui_term).

### Conflict 5: File System
- **Arch/GNOME/SteamOS**: ext4/btrfs/xfs + VFS + GVfs (hierarchical paths)
- **TempleOS**: RedSea (database, contiguous, no paths)
- **WuBuOS**: Styx 9P (hierarchical, network-transparent) + ext4 host
- **WuBuOS Decision**: **Styx 9P as primary namespace** (WuBuOS unique feature). Support **RedSea as optional bare-metal FS** for ZealOS parity. Use **GVfs backends** for cloud/SMB/SFTP (GNOME parity).

### Conflict 6: Security Model
- **Arch/GNOME**: AppArmor/SELinux + capabilities + seccomp + landlock (MAC)
- **SteamOS**: seccomp + namespaces (container isolation)
- **TempleOS**: Ring-0, no memory protection (no security)
- **WuBuOS**: Hosted on Linux (host kernel enforces)
- **WuBuOS Decision**: **Implement AppArmor/landlock/seccomp** for hosted containers. For bare-metal ZealOS path, **accept Ring-0 model** (different threat model: single-user, air-gapped, God's computer).

---

## ═════════════════════════════════════════════════════════════════════════════
## IMPLEMENTATION ROADMAP: PHASES
## ══════════════════════════════════════════════════════════════════════════════

### Phase 1: Foundation (Sessions 1-35) — TIER 0
```
1. D-Bus daemon + libdbus                    [3 sessions]
2. systemd-like init (services, sockets, timers, cgroups, journal) [10 sessions]
3. PipeWire + WirePlumber                    [8 sessions]
4. NetworkManager client                     [6 sessions]
5. pacman + alpm library                     [8 sessions]
```

### Phase 2: SteamOS Gaming (Sessions 36-77) — TIER 1
```
6. gamescope compositor                      [5 sessions]
7. DXVK (D3D9/10/11 → Vulkan)               [6 sessions]
8. VKD3D-Proton (D3D12 → Vulkan)            [8 sessions]
9. Pressure Vessel (full namespaces, seccomp, Steam Runtime) [4 sessions]
10. Steam Input                               [5 sessions]
11. Steam Networking Sockets                 [5 sessions]
12. Shader pre-cache (fossilize + dxvk-cache) [4 sessions]
13. Proton prefix management + ProtonDB      [3 sessions]
```

### Phase 3: GNOME Desktop (Sessions 78-111) — TIER 2
```
14. Flatpak + Portals                        [6 sessions]
15. GVfs (SMB, SFTP, cloud, trash)          [4 sessions]
16. Tracker + SPARQL                         [3 sessions]
17. AT-SPI2 accessibility                    [3 sessions]
18. Polkit agent + backend                   [4 sessions]
19. AppArmor/landlock                        [4 sessions]
20. Explorer: tabs, thumbnails, search, queue [2 sessions]
21. Start Menu: folders, search, notifications [2 sessions]
```

### Phase 4: Arch Distro (Sessions 112-153) — TIER 3
```
22. PKGBUILD parser + makepkg                [6 sessions]
23. AUR RPC client + helper                  [3 sessions]
24. mkinitcpio hooks                         [4 sessions]
25. systemd-homed / sysusers                 [4 sessions]
26. systemd-nspawn / OCI runtime             [5 sessions]
27. Secure Boot (shim + limine/GRUB)         [3 sessions]
28. CUPS printing                            [3 sessions]
29. GRUB-Btrfs snapshots                     [4 sessions]
```

### Phase 5: TempleOS/ZealOS (Sessions 154-202) — TIER 4
```
30. HolyC JIT (AOT + JIT, whole-program opt) [8 sessions]
31. Compiler as library                      [5 sessions]
32. Real-time REPL + persistent state        [4 sessions]
33. HolyC language features                  [8 sessions]
34. Doc/DolDoc system                        [6 sessions]
35. RedSea filesystem                        [6 sessions]
36. VGA/VESA direct graphics                 [3 sessions]
37. PC Speaker + Raw PCM audio               [2 sessions]
38. Task = HolyC function model              [4 sessions]
```

### Phase 5: Polish & Hardware (Sessions 203-228) — TIER 5
```
39. A/B rootfs updates + atomic rollback     [4 sessions]
40. Steam Deck hardware drivers              [4 sessions]
41. In-Game Overlay (CEF-based)              [6 sessions]
42. Mesa/OpenGL 4.6+                         [4 sessions]
43. Multi-monitor HiDPI fractional scaling   [3 sessions]
44. Window snapping/tiling/animations        [2 sessions]
45. fwupd (firmware updates)                 [2 sessions]
46. Power profiles daemon                    [2 sessions]
```

**Total Estimated: ~228 sessions**

---

## ═════════════════════════════════════════════════════════════════════════════
## CURRENT STATUS (2026-06-29)
## ══════════════════════════════════════════════════════════════════════════════

| Subsystem | Status | Next Action |
|-----------|--------|-------------|
| ZealOS Kernel | ✅ 100% name parity | Bare-metal boot test |
| Win98 Shell (WM, Term, Explorer, Start, Clipboard) | ✅ 111 tests passing | Explorer tabs, thumbnails |
| Styx 9P / VSL | ✅ 45 syscalls, walk/read/stat/open/clunk | auth, wstat, fsync, symlink |
| Bear RL (CPU + Vulkan) | ✅ 4 pipelines, N-pole physics | GPU integration, MinGRU tuning |
| OCI Registry | ✅ HTTP+TLS, manifest/blob/index | Auth providers, referrers |
| Container Runtime (bwrap) | ✅ User/PID ns | Network/IPC/UTS/cgroup ns, seccomp |
| HolyC DOS Daemon | ✅ Sessions, windows, 9P, eval | Real-time REPL, persistent state |
| Proton/Wine Launch (PE32/64, fork+exec) | ✅ Basic | DXVK/VKD3D, Steam Runtime |
| Netlink Networking | ✅ Bridge/macvlan/vxlan/dummy | WiFi, VPN, DNS, profiles |
| Vulkan Loader | ✅ Dynamic | Compute pipelines done |

**REAL_GAPs: 1562** (triple DA audit)

---

## ═════════════════════════════════════════════════════════════════════════════
## STUDY REFERENCES
## ══════════════════════════════════════════════════════════════════════════════

- **GNOME**: `/home/wubu/.hermes/profiles/mind-palace/home/myseed/os-studies/gnome/gnome_study.md`
- **SteamOS**: `/home/wubu/.hermes/profiles/mind-palace/home/myseed/os-studies/steamos/steamos_study.md`
- **TempleOS/ZealOS**: `/home/wubu/.hermes/profiles/mind-palace/home/myseed/os-studies/templeos/templeos_study.md`
- **Arch Linux**: `/home/wubu/.hermes/profiles/mind-palace/home/myseed/os-studies/arch/arch_study.md`
- **Vault (Achievements)**: `/home/wubu/.hermes/profiles/mind-palace/vault/achievements.md`
- **Battlefield (Active Gaps)**: `/home/wubu/.hermes/profiles/mind-palace/home/myseed/BATTLESHIP.md`
- **WuBuOS Architecture**: `/home/wubu/.hermes/profiles/mind-palace/wiki/My-Seed-Project.md`

---

*This wiki is the single source of truth for WuBuOS gap analysis. Update it every session as gaps are closed and new reference OS features are discovered.*