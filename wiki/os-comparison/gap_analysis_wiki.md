# WuBuOS Gap Analysis Wiki — OS Feature → WuBuOS Gap Mapping
**Source**: Triple DA Wiki (`triple_da_wiki.md`)
**Purpose**: Actionable gap tracking — every row maps to a WuBuOS file and REAL_GAP count
**Updated**: 2026-06-29

---

## ══════════════════════════════════════════════════════════════════════════════════
## HOW TO USE THIS WIKI
## ══════════════════════════════════════════════════════════════════════════════════

| Column | Meaning |
|--------|---------|
| **Ref OS Feature** | Feature in GNOME/SteamOS/TempleOS/Arch |
| **WuBuOS File** | File to implement/modify (or "NEW FILE") |
| **Current Status** | ✅ Done / 🟡 Partial / ❌ Missing |
| **REAL_GAPs** | Estimated void casts / stubs / system() calls to eliminate |
| **Priority** | 0=Blocking / 1=Critical / 2=High / 3=Medium / 4=Low |
| **Sessions** | Estimated sessions to close |

**Workflow**: Pick highest priority gap → Implement in WuBuOS file → Update status → Run tests → Update REAL_GAP count

---

## ═════════════════════════════════════════════════════════════════════════════════
## TIER 0: FOUNDATIONAL GAPS (Block Everything Else)
## ═════════════════════════════════════════════════════════════════════════════════

| # | Ref OS Feature | WuBuOS File | Status | REAL_GAPs | Priority | Sessions |
|---|----------------|-------------|--------|-----------|----------|----------|
| 0.1 | D-Bus system/session bus, activation, introspection | `runtime/wubu_dbus.c` (NEW) | ❌ Missing | ~200 | 0 | 3 |
| 0.2 | systemd-like init (services, sockets, timers, cgroups, journal) | `runtime/wubu_systemd.c` (NEW) | ❌ Missing | ~300 | 0 | 10 |
| 0.3 | PipeWire + WirePlumber (audio graph, Bluetooth, screen capture) | `audio/wubu_audio.c` | ❌ Missing | ~150 | 0 | 8 |
| 0.4 | NetworkManager client (WiFi, VPN, WWAN, DNS, profiles) | `wubu_network.c` (extend) | 🟡 Partial (netlink only) | ~120 | 0 | 6 |
| 0.5 | pacman + alpm library (deps, hooks, signatures, rollback) | `runtime/wubu_pacman.c` (NEW) | ❌ Missing | ~250 | 0 | 8 |
| 0.6 | Flatpak + Portals (sandbox, file chooser, screenshot) | `runtime/wubu_flatpak.c` (NEW) | ❌ Missing | ~180 | 1 | 6 |
| 0.7 | Polkit agent + backend (privilege escalation) | `apps/wubu_polkit.c` (NEW) | ❌ Missing | ~80 | 1 | 4 |
| 0.8 | systemd-homed / sysusers (portable homes, declarative users) | `runtime/wubu_homed.c` (NEW) | ❌ Missing | ~100 | 2 | 4 |

---

## ════════════════════════════════════════════════════════════════════════════════
## TIER 1: STEAMOS GAMING PARITY
## ════════════════════════════════════════════════════════════════════════════════

| # | Ref OS Feature | WuBuOS File | Status | REAL_GAPs | Priority | Sessions |
|---|----------------|-------------|--------|-----------|----------|----------|
| 1.1 | gamescope compositor (VRR, HDR, FSR, nested Wayland) | `hosted/wubu_gamescope.c` (NEW) | ❌ Missing | ~200 | 1 | 5 |
| 1.2 | DXVK (D3D9/10/11 → Vulkan translation) | `runtime/wubu_dxvk.c` (NEW) | ❌ Missing | ~400 | 1 | 6 |
| 1.3 | VKD3D-Proton (D3D12 → Vulkan translation) | `runtime/wubu_vkd3d.c` (NEW) | ❌ Missing | ~500 | 1 | 8 |
| 1.4 | Pressure Vessel (full namespaces, seccomp, Steam Runtime) | `wubu_ct_isolate.c` (extend) | 🟡 Partial (user/pid ns only) | ~150 | 1 | 4 |
| 1.5 | Steam Input (controller configs, gyro, haptics, action sets) | `input/wubu_steaminput.c` (NEW) | ❌ Missing | ~180 | 2 | 5 |
| 1.6 | Steam Networking Sockets (relay, P2P, NAT traversal, SDR) | `net/wubu_steamnet.c` (NEW) | ❌ Missing | ~250 | 2 | 5 |
| 1.7 | Shader pre-cache (fossilize + dxvk-cache) | `runtime/wubu_fossilize.c` (NEW) | ❌ Missing | ~120 | 2 | 4 |
| 1.8 | Steam Cloud (remote storage sync, conflict resolution) | `runtime/wubu_steamcloud.c` (NEW) | ❌ Missing | ~100 | 3 | 3 |
| 1.9 | Proton prefix management (per-game wineprefix, backup/restore) | `runtime/wubu_proton.c` (extend) | 🟡 Partial (launch only) | ~80 | 2 | 3 |
| 1.10 | ntsync/fsync kernel module (Wine sync primitives) | `kernel/wubu_ntsync.c` (NEW) | ❌ Missing | ~60 | 2 | 3 |
| 1.11 | In-Game Overlay (CEF, Shift+Tab, web, chat, performance) | `gui/wubu_overlay.c` (NEW) | ❌ Missing | ~300 | 3 | 6 |
| 1.12 | ProtonDB integration (compat ratings, launch options) | `apps/wubu_protondb.c` (NEW) | ❌ Missing | ~40 | 4 | 2 |
| 1.13 | A/B rootfs updates (atomic, rollback, delta, GRUB-Btrfs) | `deploy/wubu_abupdate.c` (NEW) | ❌ Missing | ~200 | 2 | 4 |
| 1.14 | Steam Deck hardware drivers (HID, fan, battery, trackpad, gyro) | `kernel/wubu_deckhw.c` (NEW) | ❌ Missing | ~120 | 4 | 4 |

---

## ════════════════════════════════════════════════════════════════════════════════
## TIER 2: GNOME DESKTOP PARITY
## ════════════════════════════════════════════════════════════════════════════════

| # | Ref OS Feature | WuBuOS File | Status | REAL_GAPs | Priority | Sessions |
|---|----------------|-------------|--------|-----------|----------|----------|
| 2.1 | GNOME Shell overview (Activities: windows + apps + search + workspaces) | `gui/dosgui_wm.c` (extend) | ❌ Missing | ~80 | 2 | 2 |
| 2.2 | Dynamic workspaces (auto create/destroy) | `gui/dosgui_wm.c` (extend) | ❌ Missing | ~30 | 3 | 2 |
| 2.3.3 | Window snapping/tiling (edge, quarter, keyboard) | `gui/dosgui_wm.c` (extend) | ❌ Missing | ~50 | 2 | 2 |
| 2.4 | Window animations (genie, workspace switch, maximize) | `gui/dosgui_wm.c` (extend) | ❌ Missing | ~60 | 3 | 2 |
| 2.5 | Touchpad gestures (3-finger workspace, 4-finger overview) | `gui/dosgui_wm.c` (extend) | ❌ Missing | ~40 | 4 | 2 |
| 2.6 | HiDPI fractional scaling (per-monitor) | `hosted/wubu_metal.c` (extend) | ❌ Missing | ~70 | 2 | 3 |
| 2.7 | Multi-monitor VRR/HDR/scaling | `hosted/wubu_metal.c` (extend) | ❌ Missing | ~80 | 3 | 3 |
| 2.8 | Notifications center (dismissible, actions, history) | `gui/wubu_notifications.c` (NEW) | ❌ Missing | ~60 | 2 | 2 |
| 2.9 | Calendar/World clock (events, timezones) | `gui/dosgui_startmenu.c` (extend) | ❌ Missing | ~40 | 3 | 2 |
| 2.10 | System status area (network, Bluetooth, power, sound, notifications) | `gui/dosgui_wm.c` (extend) | 🟡 Basic tray only | ~70 | 2 | 2 |
| 2.11 | App folders in app grid (drag-drop create/rename) | `gui/dosgui_startmenu.c` (extend) | ❌ Missing | ~30 | 3 | 2 |
| 2.12 | Search in Start Menu (Tracker + app metadata + files) | `gui/dosgui_startmenu.c` (extend) | 🟡 .desktop only | ~50 | 2 | 2 |
| 2.13 | File Explorer tabs (Ctrl+T, drag reorder) | `gui/dosgui_explorer.c` (extend) | ❌ Missing | ~40 | 2 | 2 |
| 2.14 | File Explorer recursive/content search | `gui/dosgui_explorer.c` (extend) | 🟡 Name only | ~50 | 2 | 2 |
| 2.15 | File operation queue (progress, conflict resolution, pause/resume) | `gui/dosgui_explorer.c` (extend) | 🟡 Basic only | ~60 | 2 | 2 |
| 2.16 | Thumbnails (images, video, audio, PDF) | `gui/dosgui_explorer.c` (extend) | ❌ Missing | ~80 | 3 | 3 |
| 2.17 | Preview pane (text, images, metadata) | `gui/dosgui_explorer.c` (extend) | ❌ Missing | ~50 | 3 | 2 |
| 2.18 | Batch rename (pattern/regex/sequential) | `gui/dosgui_explorer.c` (extend) | ❌ Missing | ~30 | 4 | 1 |
| 2.19 | Undo/Redo in Explorer | `gui/dosgui_explorer.c` (extend) | ❌ Missing | ~40 | 3 | 2 |
| 2.20 | GVfs backends (SMB, SFTP, WebDAV, cloud, trash, burn) | `runtime/wubu_gvfs.c` (NEW) | ❌ Missing | ~200 | 3 | 4 |
| 2.21 | Tracker/SPARQL (file content search, metadata) | `apps/wubu_tracker.c` (NEW) | ❌ Missing | ~150 | 3 | 3 |
| 2.22 | Accessibility (AT-SPI2: Orca, magnifier, OSK, high contrast) | `gui/wubu_a11y.c` (NEW) | ❌ Missing | ~200 | 4 | 3 |

---

## ════════════════════════════════════════════════════════════════════════════════
## TIER 3: TEMPLEOS / ZEALOS HOLYC PARITY
## ════════════════════════════════════════════════════════════════════════════════

| # | Ref OS Feature | WuBuOS File | Status | REAL_GAPs | Priority | Sessions |
|---|----------------|-------------|--------|-----------|----------|----------|
| 3.1 | HolyC JIT (AOT + JIT, whole-program optimization) | `compiler/holyc_codegen.c` | 🟡 minic + basic JIT | ~300 | 1 | 8 |
| 3.2 | Compiler as library (Lex, Parse, Opt, CodeGen callable) | `compiler/holyc_lib.c` (NEW) | 🟡 Partial (minic) | ~200 | 1 | 5 |
| 3.3 | Real-time REPL (immediate execution, no compile step) | `runtime/wubu_holyd.c` (extend) | 🟡 Stub (eval=0) | ~150 | 1 | 4 |
| 3.4 | Persistent compiler state (symbols, types, macros survive) | `runtime/wubu_holyd.c` (extend) | 🟡 Session save only | ~100 | 2 | 3 |
| 3.5 | Doc/DolDoc (hyperlinked docs, graphics, songs, forms, live code) | `apps/wubu_doldoc.c` (NEW) | ❌ Missing | ~250 | 2 | 6 |
| 3.6 | HolyC language features (class, exception, coroutine, reflection) | `compiler/holyc_features.c` (NEW) | ❌ Missing | ~300 | 2 | 8 |
| 3.7 | RedSea filesystem (contiguous, B-tree index, defrag, no paths) | `kernel/redsea.c` (NEW) | ❌ Missing | ~200 | 3 | 6 |
| 3.8 | VGA/VESA direct graphics (for HolyC graphics primitives) | `kernel/wubu_vga.c` (NEW) | ❌ Missing | ~80 | 3 | 3 |
| 3.9 | PC Speaker + Raw PCM audio (HolyC Sound/Music) | `audio/wubu_audio.c` (extend) | ❌ Missing | ~40 | 3 | 2 |
| 3.10 | Task = HolyC function (cooperative + preemptive, shared addr space) | `runtime/wubu_holyctask.c` (NEW) | ❌ Missing | ~120 | 3 | 4 |
| 3.11 | Identity-mapped memory mode (for bare-metal HolyC compat) | `kernel/wubu_identity_map.c` (NEW) | ❌ Missing | ~60 | 4 | 3 |
| 3.12 | Ring-0 execution option (for HolyC kernel modules) | `kernel/wubu_ring0.c` (NEW) | ❌ Missing | ~80 | 4 | 3 |
| 3.13 | HolyC boot/config scripts (kernel config in HolyC) | `boot/holyc_config.hc` (NEW) | ❌ Missing | ~30 | 4 | 2 |
| 3.14 | HolyC loadable kernel modules (.HC kernel code) | `kernel/holyc_module.c` (NEW) | ❌ Missing | ~100 | 4 | 4 |

---

## ════════════════════════════════════════════════════════════════════════════════
## TIER 4: ARCH LINUX DISTRO PARITY
## ════════════════════════════════════════════════════════════════════════════════

| # | Ref OS Feature | WuBuOS File | Status | REAL_GAPs | Priority | Sessions |
|---|----------------|-------------|--------|-----------|----------|----------|
| 4.1 | PKGBUILD parser + makepkg (build from source) | `runtime/wubu_makepkg.c` (NEW) | ❌ Missing | ~180 | 1 | 6 |
| 4.2 | AUR RPC client (search, download, verify, build) | `apps/wubu_aur.c` (NEW) | ❌ Missing | ~100 | 2 | 3 |
| 4.3 | devtools clean chroot build (extra-x86_64-build) | `runtime/wubu_makepkg.c` (extend) | ❌ Missing | ~80 | 2 | 2 |
| 4.4 | mkinitcpio hooks (encrypt, lvm2, raid, btrfs, zfs, keyboard, fsck) | `deploy/wubu_mkinitcpio.c` (NEW) | 🟡 Basic only | ~120 | 2 | 4 |
| 4.5 | dracut (event-driven initramfs) | `deploy/wubu_dracut.c` (NEW) | ❌ Missing | ~150 | 3 | 4 |
| 4.6 | GRUB / systemd-boot (with Secure Boot shim) | `deploy/wubu_grub.c` (NEW) | 🟡 limine only | ~100 | 2 | 3 |
| 4.7 | systemd-resolved (DNS, DoT, DoH, DNSSEC) | `wubu_network.c` (extend) | ❌ Missing | ~80 | 2 | 3 |
| 4.8 | systemd-networkd (declarative network config) | `wubu_network.c` (extend) | 🟡 Netlink only | ~60 | 3 | 2 |
| 4.9 | systemd-nspawn / podman (OCI containers, quadlet) | `wubu_ct_isolate.c` (extend) | 🟡 bwrap only | ~150 | 2 | 5 |
| 4.10 | AppArmor / landlock / seccomp (MAC, unpriv sandbox) | `kernel/wubu_apparmor.c` (NEW) | ❌ Missing | ~120 | 2 | 4 |
| 4.11 | Secure Boot (shim + GRUB/limine signed) | `deploy/wubu_secureboot.c` (NEW) | 🟡 limine unsigned | ~60 | 2 | 3 |
| 4.12 | dm-verity / fsverity (rootfs/file integrity) | `deploy/wubu_verity.c` (NEW) | ❌ Missing | ~80 | 3 | 2 |
| 4.13 | TPM2 (PCR binding, systemd-cryptenroll) | `deploy/wubu_tpm2.c` (NEW) | ❌ Missing | ~80 | 3 | 2 |
| 4.14 | CUPS (printing, IPP, drivers) | `apps/wubu_cups.c` (NEW) | ❌ Missing | ~100 | 4 | 3 |
| 4.15 | plymouth (boot splash) | `deploy/wubu_plymouth.c` (NEW) | ❌ Missing | ~40 | 4 | 1 |
| 4.16 | fwupd (firmware updates) | `deploy/wubu_fwupd.c` (NEW) | ❌ Missing | ~80 | 4 | 2 |
| 4.17 | power-profiles-daemon (performance/balanced/power-saver) | `apps/wubu_power.c` (NEW) | ❌ Missing | ~40 | 4 | 1 |
| 4.18 | bolt (Thunderbolt authorization) | `apps/wubu_bolt.c` (NEW) | ❌ Missing | ~50 | 4 | 2 |
| 4.19 | fprintd (fingerprint auth) | `apps/wubu_fprint.c` (NEW) | ❌ Missing | ~60 | 4 | 2 |
| 4.20 | ostree (atomic updates, like SteamOS) | `deploy/wubu_ostree.c` (NEW) | ❌ Missing | ~150 | 3 | 4 |
| 4.21 | zram-generator (compressed swap) | `deploy/wubu_zram.c` (NEW) | ❌ Missing | ~30 | 4 | 1 |

---

## ════════════════════════════════════════════════════════════════════════════════
## TIER 5: WUBUOS CORE SUBSYSTEMS (From BATTLESHIP.md)
## ════════════════════════════════════════════════════════════════════════════════

| # | WuBuOS Component | File | Status | REAL_GAPs | Priority | Sessions |
|---|------------------|------|--------|-----------|----------|----------|
| 5.1 | VSL syscall handlers (332 void casts remaining) | `runtime/wubu_vsl.c` | 🟡 45/377 done | 332 | 1 | 8 |
| 5.2 | Hosted Wayland metal (DRM/ALSA/Pulse/X11/Vulkan surface) | `hosted/wubu_metal.c` | ❌ 31 void casts + 6 weak | ~100 | 1 | 4 |
| 5.3 | Canvas subsystem (layers, filters, Vulkan texture, export) | `apps/wubu_canvas.c` | ❌ 41 void casts + 3 system() | ~100 | 2 | 4 |
| 5.4 | HolyC codegen (JIT backpatching, register allocation) | `compiler/holyc_codegen.c` | ❌ 29 placeholders | ~200 | 1 | 8 |
| 5.5 | StyxFS 9P callbacks (auth, wstat, fsync, symlink, mknod, mkdir) | `runtime/styxfs.c` | 🟡 8/14 done | ~50 | 2 | 2 |
| 5.6 | Control panel applets (display, network, sound, user, datetime) | `apps/control.c` | ❌ 20 void casts | ~80 | 3 | 3 |
| 5.7 | Built-in apps (notepad, calc, paint, cmd, taskmgr, regedit) | `apps/dosgui_apps.c` | ❌ 16 void casts | ~100 | 3 | 4 |
| 5.8 | Package manager (pacman/apt wrapper, repo sync, deps, hooks) | `gui/wubu_pkgmgr_test.c` | ❌ 14 void casts + 9 system() | ~150 | 2 | 4 |
| 5.9 | Bear RL env (MuJoCo, Atari, custom env API) | `bear/bear_env.c` | 🟡 CartPEND: 13 void casts | ~60 | 3 | 3 |
| 5.10 | Terminal emulator (PTY, VT100/ANSI, scrollback, tabs, GPU render) | `apps/terminal.c` | ❌ 13 void casts + 2 not_impl | ~120 | 3 | 4 |
| 5.11 | Bear Vulkan full GPU integration (26 void casts remaining) | `bear/bear_vulkan.c` | 🟡 7/33 done | ~100 | 2 | 4 |
| 5.12 | Hosted Wayland callbacks (seat, data device, touch, tablet, xdg-shell) | `hosted/hosted.c` | 🟡 ~30 void casts | ~80 | 2 | 3 |
| 5.13 | wubu_holyd HolyC DOS daemon (REPL, persistent state, symbols) | `runtime/wubu_holyd.c` | 🟡 Sessions/windows/9P/eval wired | ~150 | 1 | 4 |
| 5.14 | wubu_proton (DXVK, VKD3D, Steam Runtime, prefix mgmt, compat DB) | `runtime/wubu_proton.c` | 🟡 PE loader + Wine launch | ~400 | 1 | 8 |

---

## ════════════════════════════════════════════════════════════════════════════════
## GAP CLOSURE TRACKER
## ════════════════════════════════════════════════════════════════════════════════

### Session 2026-06-28/29 Closed (128 REAL_GAPs)
| Gap | File | Void Casts Closed | system() Eliminated |
|-----|------|-------------------|---------------------|
| GUI: WM | `gui/dosgui_wm.c` | 22 | 0 |
| GUI: Terminal | `gui/dosgui_term.c` | 23 | 0 |
| GUI: Explorer | `gui/dosgui_explorer.c` | 31 | 0 |
| GUI: Start Menu | `gui/dosgui_startmenu.c` | 24 | 2 |
| GUI: Clipboard | `gui/wubu_clipboard.c` | 43 | 0 |
| Bear: Vulkan | `bear/bear_vulkan.c` | 7 | 0 |
| Kernel: Interrupt | `kernel/interrupt.c` | 41 | 0 |
| Bridge: Syscall | `bridge/wubu_syscall.c` | 97 | 2 |
| VSL: Syscalls | `runtime/wubu_vsl.c` | 15 | 0 |
| Hosted: Wayland | `hosted/hosted.c` | ~42 | 0 |
| Bear: cuDNN | `bear/bear_cudnn.c` | 117 | 0 |
| Name Parity | `zealos_parity.h` | 32 aliases | 0 |
| StyxFS | `runtime/styxfs.c` | 14 | 0 |

**Net: 128 REAL_GAPs eliminated (1562 → 1434)**

---

## ════════════════════════════════════════════════════════════════════════════════
## NEXT SESSION PICK LIST (Priority Order)
## ════════════════════════════════════════════════════════════════════════════════

| Order | Gap ID | File | First Task |
|-------|--------|------|------------|
| 1 | 5.2 | `hosted/wubu_metal.c` | Implement DRM/KMS atomic commit |
| 2 | 5.1 | `runtime/wubu_vsl.c` | Implement clone flags (namespaces) |
| 3 | 5.4 | `compiler/holyc_codegen.c` | Implement JIT backpatching |
| 4 | 5.3 | `apps/wubu_canvas.c` | Implement layer compositing |
| 5 | 5.5 | `runtime/styxfs.c` | Implement auth/wstat/fsync |
| 6 | 0.1 | `runtime/wubu_dbus.c` | Implement D-Bus daemon + libdbus |
| 7 | 5.14 | `runtime/wubu_proton.c` | Integrate DXVK loader |
| 8 | 1.1 | `hosted/wubu_gamescope.c` | Implement gamescope compositor |
| 9 | 5.6 | `apps/control.c` | Implement display applet |
| 10 | 5.7 | `apps/dosgui_apps.c` | Implement notepad |

---

## ════════════════════════════════════════════════════════════════════════════════
## REAL_GAP COUNT BY SUBSYSTEM (Current: 1434)
## ════════════════════════════════════════════════════════════════════════════════

| Subsystem | Files | REAL_GAPs | % of Total |
|-----------|-------|-----------|------------|
| Runtime (VSL, OCI, Network, Snapshot, Holyd, Proton, Image) | 7 | ~350 | 24% |
| Kernel (Interrupt, FAT32, TXFS, AHCI, DRM, Vulkan) | 6 | ~50 | 3% |
| Bridge (Syscall) | 1 | 0 | 0% |
| Hosted (Wayland, Metal, Gamescope) | 3 | ~180 | 13% |
| GUI (WM, Term, Explorer, StartMenu, Clipboard, Canvas) | 6 | ~50 | 3% |
| Bear RL (Arena, SIMD, Env, NN, PPO, Opt, Vulkan, cuDNN) | 8 | ~100 | 7% |
| Compiler (HolyC codegen, minic, JIT) | 3 | ~300 | 21% |
| Apps (Editor, Canvas, Control, Terminal, PkgMgr, AUR, Tracker) | 8 | ~300 | 21% |
| Audio | 1 | ~150 | 10% |
| Container (bwrap, isolate, seccomp, namespaces) | 2 | ~100 | 7% |
| **TOTAL** | **73** | **1434** | **100%** |

---

*Update this wiki every session. When a gap is closed, move it from the tier table to the Session Closed log and update REAL_GAP counts.*