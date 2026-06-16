# WuBuOS Distro Roadmap — "Ubuntu through Our Own Paradigm"

## Vision
A **lightweight Linux distribution** with:
- **Custom kernel** (ZealOS-based, runs in-process)
- **Custom desktop** (XP Classic themed, GNOME-quality UX)
- **Proton/Wine** for Windows gaming
- **TempleOS/ZealOS** app compatibility
- **Multi-target**: bare metal, WSL, containers
- **C11** for maximum portability

## Current State (v0.5.0)
| Component | Status | Notes |
|-----------|--------|-------|
| Kernel (ZealOS in-process) | ✅ | mem, tasking, vbe, input, fat32, ahci, txfs, gaad |
| JIT (HolyC compiler) | ✅ | Lexer, parser, x86_64 codegen |
| GUI Shell (DosGui WM) | ✅ | 4 themes, XP/Win98 chrome, virtual desktops |
| Desktop Icons + Apps | ✅ | 11 apps, drag-drop, FreeDoom via bubblewrap |
| Start Menu | ✅ | Cascading, XP sidebar, hover tracking |
| Wayland Client | ✅ | xdg-shell, SHM double-buffer, resize |
| Styx/9P Namespace | ✅ | In-memory FS, socket server |
| Container Runtime | ✅ | bubblewrap, proton stub, VSL |
| HolyC REPL | ✅ | In-process TempleOS apps |
| Tests | 95% | 1/31 test suites fails (bridge theme link) |

---

## Phase 1: Fix GUI Glitches & Polish (Week 1-2)

### Critical Bugs
| Bug | File | Fix |
|-----|------|-----|
| `vbe_3d_raised/sunken_rounded` call `wubu_theme_colors()` — breaks standalone build | `src/kernel/vbe.c:411,425` | Move theme-dependent rounded 3D to GUI layer; VBE stays theme-agnostic |
| Icon rendering hardcodes `0x008080` color | `dosgui_wm.c:752` | Use theme-aware icon background |
| Start menu shutdown item missing handler | `dosgui_startmenu.c` | Wire to `hosted_shutdown()` |
| `F5` theme cycle doesn't update already-open windows | `dosgui_wm.c:412` | Force full re-render on theme change |
| Window maximize doesn't account for taskbar on all themes | `dosgui_wm.c:426` | Use dynamic taskbar height |
| Mouse drag uses wrong coordinate space after resize | `hosted.c:233` | Reset drag state on xdg_toplevel_configure |

### UX Polish
- [ ] Smooth window animations (slide/fade)
- [ ] Tooltip system (hover delay → show hint)
- [ ] Right-click context menus (desktop, taskbar, titlebar)
- [ ] Keyboard navigation (Alt+Tab, Win+D, Win+number)
- [ ] Window snapping (left/right half, corners)
- [ ] Desktop icon align-to-grid on create/move

---

## Phase 2: GNOME-Standard Desktop Ecosystem (Week 3-6)

### Core Desktop Services
| Service | Purpose | Implementation |
|---------|---------|----------------|
| **Settings Daemon** | Theme, fonts, keyboard, mouse, display | `src/shell/wubu_settings.c` + Styx config |
| **Session Manager** | Auto-start, shutdown, logout, restart | `src/shell/wubu_session.c` |
| **Notification Daemon** | Desktop notifications (libnotify compatible) | `src/gui/wubu_notify.c` |
| **Clipboard Manager** | Wayland primary + clipboard selection | `src/hosted/wubu_clipboard.c` |
| **Screenshot Tool** | PrintScr, Alt+PrintScr, region select | Extend `src/tools/screenshot.c` |
| **File Associations** | MIME → app mapping, .desktop files | `src/runtime/wubu_mime.c` |
| **Trash/Recycle Bin** | `.local/share/Trash` implementation | `src/apps/wubu_trash.c` |

### System Integration
| Feature | GNOME Equivalent | WuBuOS Approach |
|---------|------------------|-----------------|
| App grid / overview | GNOME Shell overview | Start menu + virtual desktops |
| Search / run dialog | Alt+F2, Super | Start menu "Run...", Win+R |
| System monitor | GNOME System Monitor | Task Manager app (Ctrl+Shift+Esc) |
| Disk analyzer | Baobab | File Manager → Properties |
| Terminal | GNOME Terminal | WuBu Terminal app (existing) |
| Text editor | GNOME Text Editor | Notepad + Editor apps (existing) |
| Archive manager | File Roller | File Manager → Extract |
| Software center | GNOME Software | Package Manager app (stub) |

### Accessibility (a11y)
- High contrast themes (WCAG AA)
- Screen reader hooks (speech-dispatcher)
- Keyboard-only navigation
- Font scaling (100%-200%)

---

## Phase 3: Proton/Wine Integration (Week 7-10)

### Architecture
```
┌─────────────────────────────────────────────────────────────┐
│                    WuBuOS Desktop                            │
├─────────────────────────────────────────────────────────────┤
│  Wine/Proton Layer (host process)                           │
│  ├── wine64 (host binary)                                   │
│  ├── DXVK/VKD3D (Vulkan translation)                        │
│  ├── wined3d (OpenGL fallback)                              │
│  └── Steam client (flatpak/native)                          │
├─────────────────────────────────────────────────────────────┤
│  Container Runtime (bubblewrap)                             │
│  ├── .wubu containers (ZealOS apps)                         │
│  ├── Proton prefixes (~/.local/share/wubu/proton/)          │
│  └── Steam library integration                              │
├─────────────────────────────────────────────────────────────┤
│  ZealOS Kernel (in-process)                                 │
│  ├── HolyC JIT                                              │
│  ├── VBE framebuffer                                        │
│  └── Styx namespace                                         │
└─────────────────────────────────────────────────────────────┘
```

### Implementation Tasks
| Task | File | Notes |
|------|------|-------|
| Proton prefix manager | `src/runtime/wubu_proton.c` | Create/manage Wine prefixes per game |
| Steam integration | `src/apps/wubu_steam.c` | Launch Steam, parse library.vdf |
| DXVK/VKD3D detection | `src/runtime/wubu_vulkan.c` | Check Vulkan ICD, select translation |
| Game launch .desktop files | `src/runtime/wubu_mime.c` | Auto-generate from Steam library |
| Controller support | `src/runtime/wubu_input.c` | SDL2/GameControllerDB mapping |
| Anti-cheat workaround | `src/runtime/wubu_proton.c` | Document limitations (kernel-level) |

### Minimal Proton Bundle (~200MB)
```
dist/
├── bin/
│   ├── wine64
│   ├── wineserver
│   └── winedbg
├── lib/
│   ├── dxvk/
│   ├── vkd3d/
│   └── wined3d/
└── share/
    ├── wine/
    └── proton/
```

---

## Phase 4: TempleOS/ZealOS Compatibility (Week 11-14)

### HolyC Ecosystem
| Component | Status | Target |
|-----------|--------|--------|
| HolyC Lexer/Parser | ✅ | Complete |
| x86_64 Codegen | ✅ | Complete |
| Standard Library | 🟡 | Port ZealOS `Kernel/*.ZC` → C headers |
| Graphics (VBE) | ✅ | `vbe.c` + `dosgui_wm` |
| File System (RedSea) | 🟡 | FAT32 + TXFS stub → RedSea driver |
| Network | 🟡 | Styx/9P over TCP |
| Audio | 🟡 | `wubu_audio.c` → HolyC bindings |
| Compiler Self-Host | 🔴 | HolyC compiles HolyC |

### ZealOS App Ports
| App | Source | Port Strategy |
|-----|--------|---------------|
| TempleOS Browser | ZealOS Apps | HolyC → in-process window |
| TempleOS Compiler | ZealOS Kernel | Already have JIT |
| TempleOS Games | ZealOS Demos | HolyC → window + VBE |
| TempleOS Tools | ZealOS Utils | HolyC → CLI + GUI |

### HolyC Standard Library (`src/compiler/holyc_lib/`)
```
holyc_lib/
├── core.hc          # Memory, strings, math
├── graphics.hc      # VBE, sprites, fonts
├── filesystem.hc    # RedSea, FAT32, Styx
├── network.hc       # TCP/UDP, Styx
├── audio.hc         # PCM, MIDI
├── gui.hc           # Windows, menus, controls
└── kernel.hc        # Tasking, interrupts
```

---

## Phase 5: Multi-Target Deployment (Week 15-18)

### Target Matrix
| Target | Kernel | GUI | Container | Boot Method |
|--------|--------|-----|-----------|-------------|
| **Bare Metal (x86_64)** | ZealOS native | DRM/KMS | bubblewrap | Limine/GRUB |
| **WSL2** | ZealOS in-process | Wayland (weston) | bubblewrap | `wubu` binary |
| **Container (Docker/Podman)** | ZealOS in-process | Headless/Xvfb | Nested bubblewrap | `FROM scratch` |
| **VM (QEMU/KVM)** | ZealOS native | DRM/KMS + virtio-gpu | bubblewrap | ISO/Limine |
| **macOS (AVF)** | ZealOS in-process | Metal/Cocoa | N/A | `.app` bundle |

### Build System
```makefile
# Top-level targets
all: wubu-linux wubu-wsl wubu-container wubu-iso wubu-macos

wubu-linux:     CC=gcc      TARGET=linux   make hosted
wubu-wsl:       CC=gcc      TARGET=wsl     make hosted
wubu-container: CC=gcc      TARGET=container make hosted
wubu-iso:       CC=i686-elf TARGET=metal    make metal kernel
wubu-macos:     CC=clang    TARGET=macos   make hosted_metal
```

### Distribution Artifacts
| Artifact | Size | Contents |
|----------|------|----------|
| `wubu-<version>-linux-x86_64.tar.gz` | ~50MB | Binary + themes + apps + proton stub |
| `wubu-<version>-wsl.tar.gz` | ~50MB | Same + WSL integration scripts |
| `wubu-<version>-container.tar.gz` | ~30MB | Minimal (no proton) |
| `wubu-<version>-iso` | ~100MB | Bare metal bootable ISO |
| `wubu-<version>-macos.dmg` | ~60MB | macOS app bundle |

---

## Phase 6: Package Management & App Store (Week 19-22)

### .wubu Container Format
```
myapp.wubu/
├── manifest.yaml      # name, version, deps, entry, icon
├── rootfs/            # squashfs/EROFS read-only
│   ├── bin/
│   ├── lib/
│   └── share/
├── runtime/           # "wubu" | "proton" | "holyc" | "native"
└── metadata/          # screenshots, description, license
```

### Package Manager (`src/apps/wubu_pkgmgr.c`)
- **Repository**: Git-based (like Arch AUR)
- **Dependencies**: Styx namespace composition
- **Sandboxing**: bubblewrap + seccomp
- **Updates**: Delta sync via rsync/zstd

### Default Repositories
| Repo | Content |
|------|---------|
| `core` | Base system, kernel, GUI, proton |
| `extra` | Apps: Browser, Office, Media, Dev tools |
| `holyc` | TempleOS/ZealOS apps |
| `steam` | Proton prefixes + launchers |
| `community` | User-submitted .wubu containers |

---

## Technical Debt & Cleanup (Ongoing)

| Area | Issue | Priority |
|------|-------|----------|
| `vbe.c` theme leakage | 3D rounded calls theme | High |
| Warning cleanup | strncpy truncation, unused params | Medium |
| Test coverage | Add GUI integration tests | High |
| Documentation | API docs, man pages | Medium |
| CI/CD | GitHub Actions for all targets | High |
| License audit | All deps compatible (MIT/BSD/Apache) | High |

---

## Milestone Deliverables

| Milestone | Date | Deliverable |
|-----------|------|-------------|
| M1 | Week 2 | GUI glitch-free, theme system solid |
| M2 | Week 6 | GNOME-parity desktop services |
| M3 | Week 10 | Proton runs Steam games |
| M4 | Week 14 | ZealOS apps run natively |
| M5 | Week 18 | Multi-target builds passing |
| M6 | Week 22 | Package manager + app store |
| **v1.0** | Week 24 | **Public release** |

---

## Resource Requirements

| Resource | Current | Needed |
|----------|---------|--------|
| Dev time | 1 FTE | 2-3 FTE |
| Test hardware | WSL + 1 Linux | Bare metal x86_64, ARM64, macOS |
| CI runners | None | GitHub Actions (Linux/macOS/Windows) |
| Vulkan GPU | Integrated | Discrete (for DXVK testing) |

---

## Risk Mitigation

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| Proton ABI breaks | High | High | Pin Wine/Proton versions, test matrix |
| Wayland protocol drift | Medium | Medium | Version xdg-shell, fallback to XWayland |
| ZealOS HolyC incompatibility | Medium | High | Comprehensive test suite, versioned stdlib |
| Legal (Wine/Proton) | Low | High | Use LGPL components, document |
| Performance (in-process kernel) | Medium | Medium | Benchmark, optimize hot paths |

---

*Generated: $(date)*
*WuBuOS v0.5.0 → v1.0 Roadmap*