# SteamOS Study — Architecture & Feature Inventory
**Source**: SteamOS 3.x (Steam Deck), gamescope, Proton, Pressure Vessel, Steam Client
**Purpose**: Triple DA comparison for WuBuOS gaming/container gap analysis

---

## ═══════════════════════════════════════════════════════════════
## STEAMOS ARCHITECTURE OVERVIEW
## ════════════════════════════════════════════════════════════════

### Core Components (Steam Deck / SteamOS 3.x)
```
SteamOS = Arch Linux base + Steam customizations
├── Steam Client (CEF-based UI)
│   ├── Store, Library, Community, Friends
│   ├── Big Picture Mode (Gamepad UI)
│   └── Desktop Mode (KDE Plasma)
├── gamescope (Wayland Compositor)
│   ├── VRR (FreeSync/G-Sync)
│   ├── HDR (HDR10, Dolby Vision)
│   ├── FSR (FidelityFX Super Resolution)
│   ├── Integer scaling, AMD FidelityFX CAS
│   ├── Nested Wayland (for Proton games)
│   └── Input grab/focus management
├── Proton (Wine + DXVK + VKD3D + D3DMetal)
│   ├── DXVK (D3D9/10/11 → Vulkan)
│   ├── VKD3D-Proton (D3D12 → Vulkan)
│   ├── D3DMetal (D3D12 → Metal for macOS)
│   ├── Wine patches (ntsync, fsync, d3d11, rawinput)
│   ├── Steam Runtime (containerized libraries)
│   └── ProtonDB integration
├── Pressure Vessel (Container Runtime)
│   ├── bwrap-based (bubblewrap)
│   ├── seccomp-bpf filtering
│   ├── User namespaces (unprivileged)
│   ├── Filesystem namespaces (ro bind mounts)
│   ├── Network namespaces
│   ├── GPU device passthrough (/dev/dri)
│   └── Steam Runtime environment
├── Steam Input
│   ├── Controller configs (community + official)
│   ├── Action sets (layers, activators, gyro)
│   ├── Haptics (HD rumble, trigger effects)
│   ├── Touch menus, radial menus
│   └── Chorded bindings, mode shifts
├── Steam Networking
│   ├── SteamNetworkingSockets (relay, P2P)
│   ├── NAT traversal (ICE, STUN, TURN)
│   ├── SDR (Steam Datagram Relay)
│   ├── P2P signaling via Steam
│   └── Custom protocol (not WebRTC)
├── Shader Pre-cache
│   ├── fossilize (Vulkan pipeline cache)
│   ├── dxvk-cache (DXVK state cache)
│   ├── Background compilation
│   └── Distributed via Steam Cloud
├── Steam Cloud
│   ├── Remote storage (save games, config)
│   ├── Conflict resolution
│   ├── Per-app quotas
│   └── Sync on launch/exit
└── System
    ├── A/B rootfs updates (steamos-update)
    ├── Immutable /usr (ostree-like)
    ├── Persistent /home, /var, /etc
    ├── GRUB-Btrfs snapshots
    └── Steam Deck hardware support (Valve HID, fan, battery)
```

---

## ═══════════════════════════════════════════════════════════════
## STEAM CLIENT FEATURES (What WuBuOS Lacks Entirely)
## ════════════════════════════════════════════════════════════════

### CEF-Based UI (Chromium Embedded Framework)
| Feature | Steam Client | WuBuOS |
|---------|--------------|--------|
| Store front | Web-based, personalized, recommendations | Missing |
| Library | Grid/list, categories, collections, hidden | Start Menu only |
| Community | Workshop, guides, reviews, discussions, screenshots | Missing |
| Friends/Chat | Rich presence, voice, group chat, invites | Missing |
| Big Picture | 10-foot UI, gamepad navigation, touch | Win98 shell (mouse/kb) |
| Desktop Mode | KDE Plasma session | Hosted Wayland |
| In-Game Overlay | Shift+Tab, web browser, chat, notes, guides | Missing |
| Remote Play | Stream to other devices, invite friends | Missing |
| Family Sharing | Library sharing, parental controls | Missing |
| Achievements | Global, per-game, stats, leaderboards | Missing |
| Trading/Market | Economy, inventory, trade offers | Missing |

### Steam Cloud
| Feature | Steam | WuBuOS |
|---------|-------|--------|
| Remote storage sync | Automatic, per-app, conflict resolution | Missing |
| Save game sync | Per-slot, timestamp-based | Missing |
| Config sync | ini, cfg, userdata | Missing |
| Screenshot sync | Auto-upload, privacy settings | Missing |
| Cross-platform | Windows/Linux/macOS/Steam Deck | Missing |

---

## ═══════════════════════════════════════════════════════════════
## GAMESCOPE FEATURES (Wayland Compositor for Gaming)
## ═══════════════════════════════════════════════════════════════

| Feature | gamescope | WuBuOS hosted.c |
|---------|-----------|-----------------|
| VRR (Variable Refresh Rate) | FreeSync/G-Sync via KMS | Missing |
| HDR | HDR10, Dolby Vision, SDR→HDR tonemap | Missing |
| FSR (FidelityFX Super Resolution) | FSR1/2/3, quality modes | Missing |
| Integer scaling | Nearest-neighbor, sharp pixels | Missing |
| AMD CAS | Contrast Adaptive Sharpening | Missing |
| Nested Wayland | Proton games get own Wayland seat | Missing |
| Input grab | Exclusive keyboard/mouse/gamepad | Partial |
| Focus management | Game focus, overlay, notifications | Missing |
| Latency reduction | Tearing, immediate flip, async submit | Missing |
| Multi-monitor | Per-monitor VRR/HDR/scaling | Missing |
| Steam Deck UI integration | Quick access, performance overlay | Missing |

### gamescope CLI Options (Key for WuBuOS)
```
gamescope -W 1280 -H 800 -w 1280 -h 800 -r 60 --fsr -f --adaptive-sync --hdr-enabled --hdr-itm-enable --force-grab-cursor --hide-cursor --steam-deck-ui -- %command%
```

---

## ═══════════════════════════════════════════════════════════════
## PROTON / WINE FEATURES (Windows Compatibility)
## ════════════════════════════════════════════════════════════════

### Proton Architecture
```
Proton = Wine (patched) + DXVK + VKD3D-Proton + Steam Runtime
├── Wine (valve fork)
│   ├── ntsync (NT synchronization primitives)
│   ├── fsync (futex-based sync)
│   ├── d3d11 (WineD3D11 → Vulkan via DXVK)
│   ├── rawinput (improved gamepad/mouse)
│   ├── vulkan (Wine Vulkan loader)
│   ├── kernel32/ntdll (NT API compatibility)
│   └── wow64 (32-bit on 64-bit)
├── DXVK (D3D9/10/11 → Vulkan)
│   ├── State cache (pipeline, shader)
│   ├── Async shader compilation
│   ├── Memory management (VMA)
│   ├── Deferred contexts
│   └── Multi-threaded command submission
├── VKD3D-Proton (D3D12 → Vulkan)
│   ├── DirectX 12 feature parity
│   ├── Ray tracing (DXR → VK_KHR_ray_tracing)
│   ├── Mesh shaders
│   ├── Sampler feedback
│   └── Variable rate shading
├── D3DMetal (macOS)
│   ├── D3D12 → Metal translation
│   └── MoltenVK integration
└── Steam Runtime (container)
    ├── glibc, libstdc++, mesa, pulseaudio, etc.
    ├── Pinned versions (no host dependency)
    ├── Pressure Vessel isolation
    └── GPU driver passthrough
```

### ProtonDB Integration
- Community compatibility reports: Platinum/Gold/Silver/Bronze ratings
- Per-game launch options, tweaks
- Automated: protontricks, protonup-qt
- Steam Client: "ProtonDB" button in game properties

### WuBuOS wubu_proton.c Status
| Component | wubu_proton.c | Gap |
|-----------|---------------|-----|
| PE32/64 loader | ✅ Implemented | — |
| Win32→VSL translation | ✅ Partial | 60% |
| Wine launch (fork+exec) | ✅ Implemented | — |
| DXVK integration | ❌ Stub | 95% missing |
| VKD3D-Proton | ❌ Missing | 100% missing |
| Steam Runtime detection | ❌ Missing | 100% missing |
| Prefix management | ❌ Missing | 100% missing |
| ProtonDB integration | ❌ Missing | 100% missing |
| ntsync/fsync | ❌ Missing | 100% missing |
| RawInput | ❌ Missing | 100% missing |

---

## ═══════════════════════════════════════════════════════════════
## PRESSURE VESSEL (Container Runtime)
## ════════════════════════════════════════════════════════════════

### Architecture
```
Pressure Vessel = bubblewrap + seccomp + namespaces + Steam Runtime
├── bwrap (bubblewrap)
│   ├── --unshare-user (user namespace)
│   ├── --unshare-pid (PID namespace)
│   ├── --unshare-net (network namespace)
│   ├── --unshare-ipc (IPC namespace)
│   ├── --unshare-uts (UTS namespace)
│   ├── --unshare-cgroup (cgroup namespace)
│   ├── --ro-bind /usr /usr (read-only base)
│   ├── --bind /home/user/.steam/root /home/user/.steam/root
│   ├── --bind /run/user/1000 /run/user/1000 (D-Bus, PipeWire)
│   ├── --dev /dev/dri (GPU passthrough)
│   ├── --dev /dev/input (input devices)
│   ├── --proc /proc (filtered procfs)
│   ├── --tmpfs /tmp (private tmp)
│   └── --setenv STEAM_RUNTIME=1
├── seccomp-bpf filter
│   ├── Allow: read, write, openat, close, mmap, munmap, futex, rt_sigreturn, exit_group, ...
│   ├── Deny: ptrace, process_vm_readv, process_vm_writev, kexec_load, ...
│   └── Per-arch (x86_64, aarch64)
├── Steam Runtime (Scout/Soldier)
│   ├── Fixed glibc, libstdc++, libgcc
│   ├── Mesa (OpenGL/Vulkan)
│   ├── PulseAudio/PipeWire libs
│   ├── SDL2, FFmpeg, FreeType, etc.
│   └── No host library leakage
└── Environment
    ├── STEAM_RUNTIME=1
    ├── STEAM_RUNTIME_PREFER_HOST_LIBRARIES=0
    ├── LD_LIBRARY_PATH=/usr/lib/steamrt/...
    └── VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/...
```

### WuBuOS wubu_ct_isolate.c Status
| Feature | wubu_ct_isolate.c | Gap |
|---------|-------------------|-----|
| bwrap fork+exec | ✅ Implemented | — |
| User namespace | ✅ Implemented | — |
| PID namespace | ✅ Implemented | — |
| Network namespace | ❌ Missing | 100% missing |
| IPC namespace | ❌ Missing | 100% missing |
| UTS namespace | ❌ Missing | 100% missing |
| cgroup namespace | ❌ Missing | 100% missing |
| seccomp-bpf filter | ❌ Stub | 90% missing |
| GPU passthrough (/dev/dri) | ✅ Partial | 50% missing |
| Steam Runtime env | ❌ Missing | 100% missing |
| Read-only /usr bind | ❌ Missing | 100% missing |
| D-Bus/PipeWire socket pass | ❌ Missing | 100% missing |

---

## ═══════════════════════════════════════════════════════════════
## STEAM INPUT (Controller System)
## ════════════════════════════════════════════════════════════════

| Feature | Steam Input | WuBuOS |
|---------|-------------|--------|
| Controller configs | Community + official, per-game | Missing |
| Action sets | Layers, activators, mode shifts | Missing |
| Gyro aiming | Sensitivity, curve, flick stick | Missing |
| Touch menus | Radial, touchpad, custom layouts | Missing |
| Haptics | HD rumble, trigger effects, PC speaker | Missing |
| Chorded bindings | Multi-button combos | Missing |
| Mouse/keyboard emulation | Virtual devices via uinput | Missing |
| Gamepad UI navigation | Big Picture, overlay | Missing |
| Per-game profiles | Auto-switch, cloud sync | Missing |
| Developer API | ISteamInput, action handles | Missing |

---

## ═══════════════════════════════════════════════════════════════
## STEAM NETWORKING (Relay/P2P)
## ════════════════════════════════════════════════════════════════

| Feature | Steam Networking | WuBuOS |
|---------|------------------|--------|
| SteamNetworkingSockets | Custom protocol (not WebRTC) | Missing |
| Relay (SDR) | Valve-operated global relays | Missing |
| P2P | ICE, NAT traversal, direct connect | Missing |
| Signaling | Via Steam backend | Missing |
| Encryption | AES-GCM, certificate pinning | Missing |
| Reliability | Sequenced, reliable, unreliable | Missing |
| LAN discovery | Broadcast, mDNS | Missing |
| Lobby/Matchmaking | Steam Matchmaking | Missing |
| Voice | Steam Voice (Opus, relay/P2P) | Missing |

---

## ═══════════════════════════════════════════════════════════════
## SHADER PRE-CACHE (Pipeline Compilation)
## ════════════════════════════════════════════════════════════════

| Feature | SteamOS | WuBuOS |
|---------|---------|--------|
| fossilize | Vulkan pipeline cache collection/replay | Missing |
| dxvk-cache | DXVK state cache (pipeline, shader) | Missing |
| Background compilation | Idle-time, low-priority | Missing |
| Distributed cache | Steam Cloud, per-app, per-driver | Missing |
| Driver-specific | NVIDIA/AMD/Intel separate caches | Missing |
| Replay on launch | Warm GPU pipelines before game starts | Missing |

---

## ═══════════════════════════════════════════════════════════════
## STEAM DECK HARDWARE SUPPORT
## ════════════════════════════════════════════════════════════════

| Component | Steam Deck | WuBuOS |
|-----------|------------|--------|
| Valve HID (controller) | kernel driver (hid-steam-deck) | Missing |
| Fan control | hwmon, userspace daemon | Missing |
| Battery/charge | BQ25710, SBS, health | Missing |
| Touchscreen | Goodix, gestures | Missing |
| Trackpads | Haptic, click, gestures | Missing |
| Gyro/Accel | ST LSM6DSO, Steam Input integration | Missing |
| Audio | Realtek ALC285, DSP, headphone amp | Missing |
| Display | 800p 60Hz VRR, HDR400, panel-specific | Missing |
| WiFi/Bluetooth | MediaTek MT7921, firmware | Missing |
| Suspend/Resume | Modern Standby (S0ix) | Missing |

---

## ═══════════════════════════════════════════════════════════════
## A/B ROOTFS UPDATES (steamos-update)
## ════════════════════════════════════════════════════════════════

| Feature | SteamOS | WuBuOS |
|---------|---------|--------|
| A/B partitions | rootfs-A, rootfs-B | Missing |
| Atomic updates | ostree-like, single commit | Missing |
| Rollback | Automatic on boot failure | Missing |
| Delta updates | Binary diffs, bandwidth efficient | Missing |
| Verification | dm-verity, signed commits | Missing |
| Immutable /usr | Read-only, verified | Missing |
| Persistent dirs | /home, /var, /etc (bind mounts) | Missing |
| GRUB-Btrfs snapshots | Boot menu, rollback | Missing |

---

## ═══════════════════════════════════════════════════════════════
## KEY GAPS FOR WUBUOS (Triple DA Verdict)
## ════════════════════════════════════════════════════════════════

### MUST HAVE (SteamOS Gaming Parity)
1. **gamescope** — VRR, HDR, FSR, nested Wayland, latency reduction
2. **Proton (DXVK + VKD3D-Proton)** — D3D9/10/11/12 → Vulkan translation
3. **Pressure Vessel (full)** — seccomp, all namespaces, Steam Runtime env, GPU passthrough
4. **Steam Input** — Controller configs, action sets, gyro, haptics
5. **Steam Networking** — Relay, P2P, NAT traversal, SDR
6. **Shader Pre-cache (fossilize + dxvk-cache)** — Background compilation, distributed cache
7. **Steam Cloud** — Remote storage sync, conflict resolution

### SHOULD HAVE (Container/Compatibility)
8. **Steam Runtime detection** — Scout/Soldier, pinned libraries
9. **Prefix management** — per-game wineprefix, backup/restore
10. **ProtonDB integration** — Compat ratings, launch options
11. **ntsync/fsync** — Wine sync primitives for performance
12. **RawInput** — Improved gamepad/mouse/keyboard
13. **In-Game Overlay** — Shift+Tab, web, chat, notes, performance

### NICE TO HAVE (Steam Deck Hardware)
14. **Valve HID driver** — Steam Deck controller kernel driver
15. **Fan/thermal control** — hwmon, userspace daemon
16. **Battery management** — BQ25710, health, charging
17. **Trackpad haptics** — Click, gestures, haptic feedback
18. **Gyro/accel integration** — Steam Input fusion
19. **A/B rootfs updates** — Atomic, rollback, delta, verified
20. **Modern Standby (S0ix)** — Instant resume, low power

---

## ════════════════════════════════════════════════════════════════
## IMPLEMENTATION PRIORITY FOR WUBUOS
## ════════════════════════════════════════════════════════════════

| Phase | Component | WuBuOS Component | WuBuOS File | Est. Effort |
|-------|-------------|-------------|-------------|
| 1 | gamescope Wayland compositor | `hosted/wubu_gamescope.c` | 5 sessions |
| 2 | DXVK integration (D3D9/10/11→Vulkan) | `runtime/wubu_dxvk.c` | 6 sessions |
| 3 | VKD3D-Proton (D3D12→Vulkan) | `runtime/wubu_vkd3d.c` | 8 sessions |
| 4 | Pressure Vessel full (seccomp, namespaces) | `wubu_ct_isolate.c` (extend) | 4 sessions |
| 5 | Steam Runtime detection/env | `runtime/wubu_steamrt.c` | 3 sessions |
| 6 | Steam Input subsystem | `input/wubu_steaminput.c` | 5 sessions |
| 7 | Steam Networking Sockets | `net/wubu_steamnet.c` | 5 sessions |
| 8 | Shader pre-cache (fossilize) | `runtime/wubu_fossilize.c` | 4 sessions |
| 9 | Steam Cloud sync | `runtime/wubu_steamcloud.c` | 3 sessions |
| 10 | Proton prefix management | `runtime/wubu_proton.c` (extend) | 3 sessions |
| 11 | ntsync/fsync kernel module | `kernel/wubu_ntsync.c` | 3 sessions |
| 12 | In-Game Overlay (CEF) | `gui/wubu_overlay.c` | 6 sessions |
| 13 | ProtonDB integration | `apps/wubu_protondb.c` | 2 sessions |
| 14 | A/B rootfs updates | `deploy/wubu_abupdate.c` | 4 sessions |
| 15 | Steam Deck hardware drivers | `kernel/wubu_deckhw.c` | 4 sessions |

**Total estimated**: ~65 sessions for full SteamOS parity