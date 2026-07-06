# WuBuOS Design Bible

**Version:** 1.0
**Status:** Living Document
**Gap Count:** 1562 REAL_GAPs (Triple DA verified)

---

## Table of Contents

1. [Vision & Philosophy](#1-vision--philosophy)
2. [Architecture Overview](#2-architecture-overview)
3. [Kernel Layer (ZealOS-based)](#3-kernel-layer-zealos-based)
4. [Hosted Runtime (Inferno emu pattern)](#4-hosted-runtime-inferno-emu-pattern)
5. [GUI Shell (Win98/XP Classic Themed)](#5-gui-shell-win98xp-classic-themed)
6. [Namespace & Styx/9P](#6-namespace--styx9p)
7. [Container Runtime (Bubblewrap)](#7-container-runtime-bubblewrap)
8. [Proton/Wine Integration](#8-protonwine-integration)
9. [HolyC Compatibility Layer](#9-holyc-compatibility-layer)
10. [Deployment Targets](#10-deployment-targets)
11. [Package Manager (.wubu)](#11-package-manager-wubu)
12. [Security Model](#12-security-model)
13. [Build System](#13-build-system)
14. [Testing Strategy](#14-testing-strategy)
15. [Future Roadmap](#15-future-roadmap)

---

## 1. Vision & Philosophy

### Core Mission
WuBuOS is a **GUI shell + container runtime** wrapping the **ZealOS kernel**. It provides a single 720KB static binary that runs on Linux (Wayland), WSL2, bare metal, OCI containers, and macOS AVF — delivering TempleOS/ZealOS HolyC app compatibility, Windows game support via Proton, and a familiar Win98/XP Classic desktop experience.

### Design Principles

| Principle | Description |
|-----------|-------------|
| **Single Binary** | One 720KB statically-linked executable (`src/hosted/wubu`) |
| **Inferno emu Pattern** | Host OS abstraction layer; kernel runs in-process |
| **ZealOS IS the Kernel** | Ring-0, single-user, HolyC JIT, boots on metal |
| **WuBuOS IS the Shell** | Win98/XP desktop, Styx namespace, .wubu containers |
| **Wayland Native** | No X11 code; DRM/KMS + Wayland client |
| **C11 Portability** | Maximum portability, no C++ dependencies |
| **Theme Engine** | 4 switchable themes (Ctrl+T): Win98, XP Luna Blue, XP Media Orange, WuBu Green |
| **Honesty Over Inflation** | ~15K real LOC, not 41K; no fake features |
| **Release Early, Release Honest** | Every claimed feature works at runtime |

### Non-Goals
- Not a Linux distribution (we're a shell on top)
- Not a Windows compatibility layer (we use Proton/Wine)
- Not a microkernel (ZealOS is monolithic)
- Not systemd-based (we run single-user, no init system)

---

## 2. Architecture Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                        USER SPACE                               │
├─────────────────────────────────────────────────────────────────┤
│  ┌─────────────────────────────────────────────────────────┐   │
│  │                  wubu (single binary)                   │   │
│  │  ┌─────────────┐ ┌─────────────┐ ┌─────────────────────┐ │   │
│  │  │   GUI SHELL │ │  CONTAINER  │ │   HOLYC VM          │ │   │
│  │  │  (Win98/XP) │ │  RUNTIME    │ │   (AOT/JIT)         │ │   │
│  │  ├─────────────┤ ├─────────────┤ ├─────────────────────┤ │   │
│  │  │ • Desktop   │ │ • Bubblewrap│ │ • Lexer/Parser      │ │   │
│  │  │ • WM        │ │ • Profiles  │ │ • C Transpiler      │ │   │
│  │  │ • StartMenu │ │ • GPU Pass  │ │ • AOT Compiler      │ │   │
│  │  │ • Taskbar   │ │ • DXVK/VKD3D│ │ • JIT Interpreter   │ │   │
│  │  │ • Theme Eng │ │ • Steam Lib │ │ • Syscall Bridge    │ │   │
│  │  └──────┬──────┘ └──────┬──────┘ └──────────┬──────────┘ │   │
│  │         │               │                    │            │   │
│  │         └───────────────┼────────────────────┘            │   │
│  │                         ▼                                 │   │
│  │              ┌─────────────────────────────────────┐      │   │
│  │              │    STYX/9P NAMESPACE    │              │      │   │
│  │              │  /wubu /dev /prog /net    │              │      │   │
│  │              └───────────┬─────────────┘              │      │   │
│  │                          │                            │      │   │
│  └──────────────────────────┼────────────────────────────┘      │
└─────────────────────────────┼───────────────────────────────────┘
                              │
                    ┌─────────┴─────────┐
                    │   ZEALOS KERNEL   │
                    │  (in-process)     │
                    ├───────────────────┤
                    │ • VBE Framebuffer │
                    │ • Tasking/Sched   │
                    │ • Memory Manager  │
                    │ • Interrupt/ISR   │
                    │ • HolyC JIT       │
                    │ • Styx Server     │
                    └───────────────────┘
```

### Binary Composition

---

## 3. Kernel Layer (ZealOS-based)

### 3.1 Memory Manager (`src/kernel/memory.c`)
- Buddy allocator with red-zone canaries
- Page frame allocator (4K pages)
- Kernel heap with `mem_debug_dump()` introspection
- VSL syscall memory interface (315 void casts remaining)

### 3.2 Tasking/Scheduler (`src/kernel/tasking.c`)
- Round-robin with priority inheritance
- FPU/SSE context save/restore (REAL_GAP: FPU save incomplete)
- Process/thread model maps to ZealOS tasks
- 6 REAL_GAPs remaining

### 3.3 VBE Framebuffer (`src/kernel/vbe.c`)
- 64-glyph 8×16 font baked into binary
- Gradient, circle, shade, clip primitives
- Double-buffered SHM for Wayland surface
- 0 REAL_GAPs (functional)

### 3.4 Interrupt/ISR (`src/kernel/interrupt.c`)
- 41 void casts CLOSED: LAPIC, IOAPIC, MSI/MSI-X, TSC deadline
- SYSCALL/SYSRET fast path
- PIC cascade legacy support
- 0 REAL_GAPs remaining

### 3.5 Filesystems
- **FAT32** (`src/kernel/fat32.c`): 20/20 tests pass, LFN support (12 REAL_GAPs: lfn_chk unused, no pre-allocation)
- **TXFS** (`src/kernel/txfs.c`): WAL transactional FS, 25/25 tests pass (10 REAL_GAPs: atomic commit, replay verify, auto-checkpoint)
- **AHCI** (`src/kernel/ahci.c`): SATA with simulator, 16/16 tests pass (8 REAL_GAPs: FIS receive, real PHY, interrupt completion)

### 3.6 Styx/9P Server (`src/kernel/styx.c`)
- 9P2000.L protocol in-kernel
- Mount points: `/wubu`, `/dev`, `/prog`, `/net`
- Container isolation via namespace chroot

---

## 4. Hosted Runtime (Inferno emu pattern)

### 4.1 Wayland Client (`src/hosted/hosted.c`)
- Registry, compositor, shell, SHM, seat, keyboard, pointer
- 72 void casts: ~30 remaining (seat caps, data device, touch, tablet, output, xdg-shell)
- Headless mode (`-h` flag) for CI — no compositor needed

### 4.2 DRM/KMS Direct (`src/hosted/wubu_drm_direct.c`)
- Atomic modesetting (no libdrm)
- Plane composition (primary, cursor, overlay)
- Connector hotplug + EDID parsing
- Vulkan surface creation (X11/Wayland/DRM)

### 4.3 Metal Abstraction (`src/hosted/wubu_metal.c`)
- Audio backends: ALSA (dlopen), PulseAudio (dlopen), PipeWire (dlopen), X11 (dlopen)
- GAAD mode selection (Golden Angle Area Decomposition)
- 31 void casts + 6 weak aliases + stubs CLOSED

### 4.4 VSL (Virtual System Layer) (`src/runtime/wubu_vsl.c`)
- 347 void casts in syscall dispatch (315 remaining)
- 17 syscalls implemented: rt_sigaction, rt_sigprocmask, select, pipe2, clone3, io_uring*, readlinkat, fchmodat, fchownat, utimensat, futimesat, renameat, mkdirat, symlinkat, linkat, mknodat, getwd, fchdir + statx fix
- Key gaps: namespaces (clone flags), fanotify, landlock, bpf, perf_event

### 4.5 StyxFS (`src/runtime/styxfs.c`)
- 14 void casts CLOSED — full POSIX API: stat, open, read, write, close, readdir, opendir, closedir, create, remove, rename, mkdir, rmdir
- .wubu container detection, mount/unmount, directory walk/read/create/remove/clunk
- 11/11 tests passing

### 4.6 Container Runtime
- `wubu_ct.c` — chroot-based isolation
- `wubu_ct_bwrap.c` — bubblewrap profiles (unprivileged)
- `wubu_ct_isolate.c` — cgroups v2 (mem/cpu/pids) + seccomp-bpf

---

## 5. GUI Shell (Win98/XP Classic Themed)

### 5.1 Window Manager (`src/gui/dosgui_wm.c`)
- XP/Win98 chrome (titlebar, min/max/close, resize handles)
- Virtual desktops (4), focus stack, snap-to-grid
- 22 void casts CLOSED
- 16/16 tests passing

### 5.2 Desktop (`src/gui/dosgui_desktop.c`)
- Icon grid layout, wallpaper (solid color), right-click context
- 12 REAL_GAPs: wallpaper image load, auto-arrange, real FS watch

### 5.3 Start Menu (`src/gui/dosgui_startmenu.c`)
- Win98 popup + XP sidebar (toggle F11)
- .desktop file parser, category map
- 24 void casts CLOSED + 2 system() ELIMINATED
- 4/4 tests passing

### 5.4 Explorer (`src/gui/dosgui_explorer.c`)
- 9P/Styx file ops, real ZIP mount (libzip dlopen)
- 31 void casts CLOSED
- 74/74 tests passing

### 5.5 Terminal (`src/gui/dosgui_term.c`)
- PTY fork+exec, VT100/ANSI parser, scrollback, tabs, copy/paste
- HolyC REPL integration via dosgui_wm_spawn_holyc_term
- 23 void casts CLOSED
- 17/17 tests passing

### 5.6 Theme Engine (`src/gui/wubu_theme.c`)
- 4 themes: Win98 Classic, XP Luna Blue, XP Media Orange, WuBu Green
- Ctrl+T cycling, live preview in Control Panel
- 5 REAL_GAPs: theme file loading (INI/JSON), CSS parser, runtime customization

### 5.7 Clipboard (`src/gui/wubu_clipboard.c`)
- Wayland clipboard + primary selection
- Multi-MIME: text/plain, text/html, image/png, text/uri-list
- 43 void casts CLOSED
- 17/17 tests passing

### 5.8 Notifications (`src/gui/wubu_notify.c`)
- Toast notifications, history
- 4 REAL_GAPs: timeout dismissal, history persistence

---

## 6. Namespace & Styx/9P

### 6.1 Global Namespace
```
/wubu     → WuBuOS config, containers, themes
/dev      → ZealOS device nodes (fb, kbd, mouse, audio)
/prog     → Installed apps (.desktop entries)
/net      → Network interfaces, sockets
```

### 6.2 Per-Container Namespace
Each `.wubu` container gets private view:
- `/wubu` → container metadata
- `/dev` → virtual devices
- `/prog` → container apps
- `/net` → isolated network stack

### 6.3 9P Protocol (`src/runtime/styx.c`)
- T-version, T-attach, T-walk, T-open, T-read, T-write, T-clunk, T-stat, T-wstat, T-create, T-remove
- QID versioning for cache invalidation
- Container-aware path normalization

---

## 7. Container Runtime (Bubblewrap)

### 7.1 Profiles (`src/runtime/wubu_ct_bwrap.c`)
- **unprivileged** — no CAP_SYS_ADMIN, user namespaces
- **gpu** — /dev/dri, Vulkan ICD pass-through
- **network** — isolated netns, bridge/macvlan
- **steam** — Steam runtime env, Proton prefix

### 7.2 Cgroups v2 (`src/runtime/wubu_ct_isolate.c`)
- Memory: limit, swap, oom_control
- CPU: quota, period, shares
- PIDs: max descendants
- seccomp-bpf: syscall allowlist per profile

### 7.3 OCI Registry (`src/runtime/wubu_oci.c`)
- HTTP+TLS (mbedTLS), manifest/blob/index/config
- Auth providers, multi-platform index (NOT STARTED: 14 void casts + 9 system())
- Cosign verification (NOT STARTED)

---

## 8. Proton/Wine Integration

### 8.1 Proton Launcher (`src/runtime/wubu_proton.c`)
- PE32/64 loader, Win32→VSL syscall translation
- Wine launch via fork+exec in container
- DXVK/VKD3D integration stub (95% missing)

### 8.2 Proton PE (`src/runtime/wubu_proton2.c`)
- Real Wine+DXVK+VKD3D in Arch container
- Steam library detection, prefix management
- 0 REAL_GAPs (functional — defensive returns only)

---

## 9. HolyC Compatibility Layer

### 9.1 Compiler (`src/compiler/`)
- **Lexer** (`holyc_lexer.c`): Tokenizer, 0 REAL_GAPs
- **Parser** (`holyc_parse.c`): AST builder, 0 REAL_GAPs
- **Codegen** (`holyc_codegen.c`): x86_64 JIT, 29 placeholders CLOSED
- **PTX Backend** (`holyc_ptx.c`): CUDA PTX emit, 4 REAL_GAPs (shared memory tiling, CUDA driver, kernel launch)
- **JIT** (`src/jit/`): mmap executable, encoder, disasm, MIR/ASMJIT/minic backends

### 9.2 DOS Daemon (`src/runtime/wubu_holyd.c`)
- Session/window management via 9P
- Eval/compile wired to compiler
- REAL_GAPs: real-time REPL, persistent compiler state, symbol table, macro expansion

### 9.3 ZealOS Parity (`src/kernel/zealos_parity.h`)
- 96/96 name mappings: `MAlloc`→`wubu_malloc`, `Free`→`wubu_free`, `Print`→`wubu_print`, etc.
- 32 aliases added

---

## 10. Deployment Targets

| Target | Binary | Kernel | GUI | Containers |
|--------|--------|--------|-----|------------|
| **Linux Wayland** | ✅ | ✅ | ✅ | ✅ |
| **WSL2** | ✅ | ✅ | ✅ | ✅ |
| **Bare Metal (Limine)** | ✅ | ✅ | VBE | Chroot |
| **OCI Container** | ✅ | ✅ | Headless | Nested |
| **macOS AVF** | 🔄 | ✅ | Stub | Stub |

---

## 11. Package Manager (.wubu)

### 11.1 Format
- SquashFS payload + JSON manifest
- Manifest: name, version, deps, entrypoints, caps, icon
- Signed with Ed25519 (cosign compatible)

### 11.2 Runtime (`src/gui/wubu_pkgmgr.c`)
- Repo sync, dependency resolution, hooks
- Install: mount SquashFS at `/wubu/apps/<name>`
- 14 void casts + 9 system() NOT STARTED

---

## 12. Security Model

- Single-user (UID 1000 mapped to 0 in container)
- No setuid binaries in image
- seccomp-bpf per container profile
- cgroups v2 resource limits
- Styx namespace isolation
- Wayland security: no global compositor access from containers

---

## 13. Build System

### 13.1 Make Targets
- `make all` — builds hosted binary + all test targets
- `make gui` — builds GUI targets
- `make test` — runs 747+ assertions across 30+ targets
- `make hosted` — single static binary

### 13.2 Compiler Flags
```
-std=c11 -O2 -pipe -fPIC -fvisibility=hidden
-Wall -Wextra -Wpedantic -Werror=vla -Werror=implicit-function-declaration
-D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L
```

### 13.3 Dependencies
- **Build**: gcc/clang, make, pkg-config
- **Runtime (dlopen)**: wayland-client, wayland-egl, xkbcommon, vulkan, mbedtls, libzip, fluidsynth, alsa, pulse, pipewire, X11
- **Optional**: bubblewrap, fuse-overlayfs, btrfs-progs, zfsutils, lvm2

---

## 14. Testing Strategy

### 14.1 Test Categories
- **Unit**: 747+ assertions, 30+ targets
- **Integration**: Container lifecycle, 9P ops, VSL syscalls
- **Headless**: `./src/hosted/wubu -h` runs all GUI tests without compositor

### 14.2 Key Test Targets
| Target | Tests | Status |
|--------|-------|--------|
| test_edr | 12 | ✅ |
| test_dosgui_explorer | 74 | ✅ |
| test_dosgui_wm | 16 | ✅ |
| test_dosgui_startmenu | 4 | ✅ |
| test_dosgui_term | 17 | ✅ |
| test_clipboard | 17 | ✅ |
| test_holyc | 84 | ✅ |
| test_holyc_ptx | 9 | ✅ |
| test_vsl | 55 | ✅ |
| test_styxfs | 11 | ✅ |
| test_audio | 15 | ✅ |
| test_memory | 29 | ✅ |
| test_syscall | 26 | ✅ |

---

## 15. Future Roadmap

### Tier 1: Audio Engine
- Furnace (12 chips: NES, SNES, GB, Genesis, etc.)
- TinySoundFont (SF2: RIFF pdta/sdta, samples, envelopes, modulators)
- Ardour DAW parity (sample-accurate automation, LV2/VST3/CLAP, JACK, AAF/OMF, video sync)
- AI plugin container streaming

### Tier 2: SteamOS Integration
- Steam Client (CEF UI, store, library, friends)
- Steam Input (controller configs, action sets, haptics)
- Steam Networking (relay, P2P, NAT traversal)
- Proton (Wine + DXVK + VKD3D + D3DMetal)
- gamescope (Wayland compositor, VRR, HDR, FSR)
- Pressure Vessel (container runtime, seccomp, namespaces)
- Steam Deck UI (game mode, desktop mode, quick access)
- Shader pre-cache (fossilize, dxvk-cache)
- ProtonDB integration (compat reports)
- Steam Cloud (remote storage sync)

### Tier 3: Ubuntu/Arch Integration
- systemd (init, services, sockets, timers, units)
- apt/pacman (package manager, repos, deps, hooks)
- NetworkManager (wifi, ethernet, vpn, dns, dhcp)
- Polkit (authorization, privilege escalation)
- D-Bus (system/session bus, activation, introspection)
- GNOME/KDE (desktop shell, settings, extensions) — different paradigm
- PulseAudio/PipeWire (audio graph, bluetooth, devices)
- CUPS (printing, IPP, drivers)
- AppArmor/SELinux (MAC, profiles)
- systemd-homed / systemd-sysusers (user management)
- mkinitcpio / dracut (initramfs generation)
- GRUB/systemd-boot (bootloader, secure boot)

### Tier 4: TempleOS Soul
- HolyC JIT (AOT + JIT, whole-program optimization)
- Doc/DolDoc (hyperlinked docs, graphics, songs)
- Compiler as library (JIT compile from string)
- Identity-mapped memory (no paging in user mode)
- Ring-0, no memory protection (single address space)
- File system = database (RedSea, no paths)
- God word / Oracle / Divine intellect
- Graphics: VGA/VESA direct, no GPU drivers
- Audio: PC speaker + raw PCM
- Network: None (air-gapped design)

---

## Appendix: Key Files Quick Reference

### Kernel
| File | Purpose |
|------|---------|
| `src/kernel/vbe.c` | Framebuffer, graphics primitives |
| `src/kernel/tasking.c` | Scheduler, context switch |
| `src/kernel/memory.c` | Buddy allocator |
| `src/kernel/input.c` | Keyboard/mouse queues |
| `src/kernel/wubu_gaad.c` | Golden Angle Area Decomposition |

### Runtime
| File | Purpose |
|------|---------|
| `src/runtime/styx.c` | 9P protocol |
| `src/runtime/styxfs.c` | File server |
| `src/runtime/wubu_ct_bwrap.c` | Bubblewrap integration |
| `src/runtime/wubu_host_exec.c` | Host process execution |
| `src/runtime/wubu_arch.c` | Arch bootstrap |

### GUI
| File | Purpose |
|------|---------|
| `src/gui/dosgui_wm.c` | Window manager (XP/Win98 chrome) |
| `src/gui/dosgui_desktop.c` | Desktop background, icons |
| `src/gui/dosgui_startmenu.c` | Start menu |
| `src/gui/wubu_theme.c` | 4-theme engine |
| `src/gui/wubu_pkgmgr.c` | Package manager |
| `src/gui/wubu_deploy.c` | Multi-target deployment |

### Hosted
| File | Purpose |
|------|---------|
| `src/hosted/hosted.c` | Wayland client, event loop |
| `src/hosted/wubu_drm_direct.c` | DRM/KMS (no libdrm) |

### Compiler
| File | Purpose |
|------|---------|
| `src/compiler/holyc_lexer.c` | Tokenizer |
| `src/compiler/holyc_parse.c` | AST builder |