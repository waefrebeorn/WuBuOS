# WuBuOS Design Bible

**Version:** 1.0  
**Date:** 2025  
**Status:** Living Document  

---

## Table of Contents

1. [Vision & Philosophy](#1-vision--philosophy)
2. [Architecture Overview](#2-architecture-overview)
3. [Kernel Layer (ZealOS-based)](#3-kernel-layer-zealos-based)
4. [Hosted Runtime (Inferno emu pattern)](#4-hosted-runtime-inferno-emu-pattern)
5. [GUI Shell (WinXP Classic Themed)](#5-gui-shell-winxp-classic-themed)
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
WuBuOS is a **GUI shell + container runtime** wrapping the **ZealOS kernel**. It provides a single 720KB static binary that runs on Linux (Wayland), WSL2, bare metal, OCI containers, and macOS AVF — delivering TempleOS/ZealOS HolyC app compatibility, Windows game support via Proton, and a familiar WinXP Classic desktop experience.

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
│  │  │  (WinXP)    │ │  RUNTIME    │ │   (AOT/JIT)         │ │   │
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
│  │              ┌─────────────────────────┐                  │   │
│  │              │    STYX/9P NAMESPACE    │                  │   │
│  │              │  /wubu /dev /prog /net  │                  │   │
│  │              └───────────┬─────────────┘                  │   │
│  │                          │                                 │   │
│  └──────────────────────────┼─────────────────────────────────┘   │
└─────────────────────────────┼─────────────────────────────────────┘
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
```
Total: ~720KB
├── Kernel (ZealOS):     ~45KB
├── GUI Shell:           ~120KB
├── Container Runtime:   ~35KB
├── Proton/Wine:         ~40KB
├── HolyC VM:            ~90KB
├── Theme Engine:        ~15KB
├── Settings/Session:    ~25KB
├── Styx/9P:             ~20KB
├── Deployment:          ~15KB
├── Package Manager:     ~45KB
├── Wayland/DRM:         ~30KB
└── Stdlib/Utils:        ~240KB
```

---

## 3. Kernel Layer (ZealOS-based)

### Source Location
```
src/kernel/
├── memory.c/h        # Buddy allocator, heap, page management
├── tasking.c/h       # Round-robin + priority scheduler
├── vbe.c/h           # Framebuffer, graphics primitives
├── input.c/h         # Keyboard/mouse queues
├── interrupt.c/h     # IDT, ISR, PIC/APIC
├── fat32.c/h         # FAT32 filesystem
├── ahci.c/h          # SATA controller
├── txfs.c/h          # Transactional FS
├── wubu_gaad.c/h     # Golden Angle Area Decomposition
├── ps2.c/h           # PS/2 controller
└── isr_stubs.S       # Assembly ISR entry points
```

### Key Design Decisions

#### VBE Framebuffer (Theme-Agnostic)
- **Resolution**: Configurable at init (default 1024×768)
- **Double-buffered**: Front + back buffer for flicker-free rendering
- **Primitives Only**: Rect, line, circle, text, blit, rounded rect
- **No Theme Knowledge**: Callers pass colors; VBE knows nothing of themes
- **Hosted Mode**: SHM-backed buffer shared with Wayland compositor

#### Tasking
- **Scheduler**: Round-robin with 4 priority levels (0-3)
- **Context Switch**: Assembly routine (`tasking_switch.S`) saves full x86_64 state
- **Idle Task**: Always runs at priority 0
- **Sleep/Wake**: Tick-based with millisecond resolution

#### Memory Manager
- **Buddy Allocator**: Power-of-two blocks, 4KB pages
- **Kernel Heap**: Separate from userspace
- **Guard Pages**: Detect overflow/underflow
- **Signatures**: Allocation headers with magic for double-free detection

#### Graphics Abstraction (GAAD)
- **Golden Angle Decomposition**: Recursively splits screen into φ-proportioned regions
- **Window Snapping**: Drag-end snaps to nearest GAAD region
- **Resolution Independence**: Translates TempleOS 640×480 coords to any resolution
- **Feng Shui Cardinal Mirrors**: N/S asymmetry for natural window placement

### Kernel Symbols
```c
// Exported for hosted mode
vbe_init(), vbe_shutdown(), vbe_flip()
tasking_init(), task_create(), task_schedule()
mem_init(), mem_alloc(), mem_free()
input_init(), input_key_push(), input_mouse_push()
styx_serve(), styx_mount(), styx_walk()
```

---

## 4. Hosted Runtime (Inferno emu Pattern)

### Source Location
```
src/hosted/
├── hosted.c          # Main entry, Wayland client, event loop
├── hosted.h          # Wayland state, globals
├── wubu_metal.c      # Bare metal entry (MYSEED_METAL)
├── wubu_drm_direct.c # DRM/KMS direct (no libdrm)
├── xdg-shell-*.c     # Wayland xdg-shell protocol
└── wubu_display_test.c
```

### Wayland Client Architecture
```
┌─────────────────────────────────────────────────────────┐
│                    wubu (hosted)                        │
├─────────────────────────────────────────────────────────┤
│  Wayland Event Loop (wl_display_dispatch)               │
├─────────────────────────────────────────────────────────┤
│  Registry Bindings:                                     │
│  • wl_compositor    → surface creation                  │
│  • xdg_wm_base      → toplevel/popup                    │
│  • wl_shm           → shared memory buffers             │
│  • wl_seat          → input (keyboard, pointer)         │
│  • zxdg_decoration_v1 → server-side decorations         │
├─────────────────────────────────────────────────────────┤
│  Double-Buffered SHM:                                    │
│  • Front buffer: displayed by compositor                │
│  • Back buffer: VBE draws into it                       │
│  • wl_surface_attach + damage + commit on flip          │
├─────────────────────────────────────────────────────────┤
│  Input Routing:                                          │
│  • Key events    → input_key_push() → WM                │
│  • Pointer events→ input_mouse_push() → WM              │
│  • Touch events  → (future)                             │
└─────────────────────────────────────────────────────────┘
```

### Resize Handling
```c
// On configure event:
1. Allocate new SHM buffer (wl_shm_create_pool)
2. Create new VBE buffers (vbe_recreate)
3. Update global width/height
4. Notify WM/desktop of resize
5. Commit new buffer
```

### Headless Mode
- **Flag**: `-h` or no Wayland display
- **Framebuffer**: malloc'd memory
- **Use Case**: CI, demo capture, automated testing
- **Demo Output**: 100-frame GIF/MP4 via `demo_capture.c`

---

## 5. GUI Shell (WinXP Classic Themed)

### Source Location
```
src/gui/
├── dosgui_wm.c/h         # Window manager (XP/Win98 chrome)
├── dosgui_desktop.c/h    # Desktop background, icons
├── dosgui_startmenu.c/h  # Start menu (XP sidebar + Win98 fallback)
├── wubu_theme.c/h        # 4-theme engine + color tables
├── wubu_settings.c/h     # Settings daemon (7 categories)
├── wubu_session.c/h      # Session manager + autostart
├── wubu_notify.c/h       # Notification daemon
├── wubu_clipboard.c/h    # Wayland clipboard/primary
├── wubu_screenshot.c/h   # PrintScr/Alt+PrintScr/Region
├── wubu_mime.c/h         # MIME database + .desktop parser
├── wubu_trash.c/h        # FreeDesktop Trash spec
├── wubu_proton.c/h       # Proton/Wine integration
├── wubu_gamelib.c/h      # Steam game library
├── wubu_container.c/h    # Container runtime
├── wubu_holyc.c/h        # HolyC VM
├── wubu_deploy.c/h       # Multi-target deployment
├── wubu_pkgmgr.c/h       # Package manager
└── wm.c/taskbar.c/       # Legacy WM (being phased out)
```

### Window Manager (dosgui_wm)

#### XP Classic Chrome
```
┌──────────────────────────────────────────┐
│  ███████████████████████████████████████  │  ← Gradient titlebar (XP)
│  My Window                    [_] [□] [X] │  ← Rounded buttons
├──────────────────────────────────────────┤
│                                          │  ← Client area
│                                          │
│                                          │
└──────────────────────────────────────────┘  ← Rounded corners + drop shadow
```

#### Win98 Fallback
```
┌──────────────────────────────────────────┐
│ My Window                        [_][□][X]│  ← Flat titlebar, 3D buttons
├──────────────────────────────────────────┤
│                                          │
│                                          │
└──────────────────────────────────────────┘  ← Sharp corners, 3D borders
```

#### Features
| Feature | XP Mode | Win98 Mode |
|---------|---------|------------|
| Titlebar | Gradient | Flat |
| Buttons | Rounded, hover glow | 3D raised/sunken |
| Borders | Rounded + shadow | Sharp + 3D bevel |
| Close button | Red hover | Gray |
| Min/Max | Separate buttons | Combined |
| Drop shadow | Yes (8px) | No |

### Theme Engine

#### 4 Built-in Themes (Ctrl+T cycles)
| Theme | Taskbar | Window Title | Desktop | Accent |
|-------|---------|--------------|---------|--------|
| **Win98 Classic** | Teal (#008080) | Flat teal | Teal | None |
| **XP Luna Blue** | Blue gradient | Blue gradient | Blue | Blue |
| **XP Media Orange** | Orange gradient | Orange gradient | Orange | Orange |
| **WuBu Green** | Green gradient | Green gradient | Dark green | Green |

#### Theme API
```c
// Global accessor
const WubuThemeColors* tc = wubu_theme_colors();

// Cycling
wubu_theme_cycle();  // 98 → Luna → Orange → WuBu → 98

// Theme struct (in wubu_theme.h)
typedef struct {
    uint32_t desktop_bg;
    uint32_t taskbar_bg;
    uint32_t taskbar_fg;
    uint32_t window_title_bg;
    uint32_t window_title_fg;
    uint32_t window_border;
    uint32_t button_bg;
    uint32_t button_fg;
    uint32_t button_hover;
    uint32_t button_pressed;
    uint32_t close_hover;
    uint32_t start_btn_bg;
    uint32_t start_btn_hover;
    uint32_t icon_bg;
    uint32_t icon_border;
    uint32_t selection_bg;
    uint32_t selection_fg;
    uint32_t menu_bg;
    uint32_t menu_fg;
    uint32_t menu_hover_bg;
    uint32_t tooltip_bg;
    uint32_t tooltip_fg;
    bool gradient_titles;    // XP: true, Win98: false
    bool rounded_corners;    // XP: true, Win98: false
    bool drop_shadows;       // XP: true, Win98: false
} WubuThemeColors;
```

### Start Menu (dosgui_startmenu)

#### XP Mode (Sidebar Layout)
```
┌──────────────┬────────────────────────┐
│  [User Pic]  │  Programs ▸            │
│  WuBuOS      │  Documents             │
│              │  Settings ▸            │
│  [Search Box]│  ─────────────────     │
│              │  Run...                │
│  Pinned:     │  ─────────────────     │
│  [Term] [FM] │  Log off               │
│  [Browser]   │  Shut down             │
└──────────────┴────────────────────────┘
```

#### Win98 Mode (Classic Popup)
```
┌────────────────────────┐
│  Programs ▸            │
│  Documents             │
│  Settings ▸            │
│  ──────────────────    │
│  Run...                │
│  ──────────────────    │
│  Log off               │
│  Shut down             │
└────────────────────────┘
```

### Desktop Icons
- **Alignment**: Grid snap (GAAD-assisted)
- **Selection**: Rubber-band multi-select
- **Actions**: Double-click launch, right-click context menu
- **Icon Theming**: Uses `tc()->icon_bg`, `tc()->icon_border`

---

## 6. Namespace & Styx/9P

### Source Location
```
src/runtime/
├── styx.c/h        # 9P2000 protocol implementation
├── styxfs.c/h      # FS server (files as 9P objects)
├── wubu_arch.c/h   # Arch Linux bootstrap
├── wubu_ramdisk.c/h# Ramdisk with SSD persistence
├── wubu_exec.c/h   # Container execution
├── wubu_host_exec.c/h # Host process execution
├── wubu_ct_bwrap.c/h  # Bubblewrap integration
├── wubu_container.c/h # Container API
├── wubu_proton.c/h   # Proton runtime
├── wubu_vsl.c/h      # Virtual System Layer
└── wubu_gc.c/h       # GC for applets
```

### Namespace Structure
```
/                          # Root
├── wubu/                  # WuBuOS internal
│   ├── config/            # Settings
│   ├── themes/            # Theme files
│   └── logs/              # System logs
├── dev/                   # Devices
│   ├── cons               # Console
│   ├── mouse              # Mouse
│   ├── kb                 # Keyboard
│   ├── fb                 # Framebuffer
│   └── null/zero/random   # Standard
├── prog/                  # Programs (Styx files)
│   ├── wm                 # Window manager
│   ├── desktop            # Desktop
│   ├── startmenu          # Start menu
│   └── apps/              # User apps
├── net/                   # Network
│   ├── tcp                # TCP stack
│   ├── dns                # DNS resolver
│   └── http               # HTTP client
└── tmp/                   # Scratch space
```

### 9P Messages
| Message | Purpose |
|---------|---------|
| Tattach | Connect to root fid |
| Twalk   | Traverse path |
| Topen   | Open fid for I/O |
| Tread   | Read data |
| Twrite  | Write data |
| Tclunk  | Close fid |
| Tremove | Delete file |
| Tstat   | Get metadata |
| Twstat  | Set metadata |

### StyxFS (File Server)
- **In-Memory**: All files served from RAM
- **Dynamic**: Programs register as Styx files
- **Containers**: `.wubu` files appear as single Styx files
- **Permissions**: Unix-style (rwxr-xr-x)

---

## 7. Container Runtime (Bubblewrap)

### Source Location
```
src/runtime/wubu_ct_bwrap.c/h
src/gui/wubu_container.c/h
```

### Architecture
```
┌─────────────────────────────────────────────────────────┐
│  wubu_container_start()                                 │
├─────────────────────────────────────────────────────────┤
│  1. Build bwrap argv[]                                  │
│  2. Add mounts (--ro-bind, --bind, --dev-bind)          │
│  3. Add env vars (--setenv, --unsetenv)                 │
│  4. Add namespaces (--unshare-all, --new-session)       │
│  5. Add seccomp filter (if strict)                      │
│  6. fork() → child: exec bwrap                          │
│  7. Parent: track PID, return container handle          │
└─────────────────────────────────────────────────────────┘
```

### Predefined Profiles
| Profile | Network | GPU | /home | Use Case |
|---------|---------|-----|-------|----------|
| `default` | ✅ | ❌ | ❌ | General apps |
| `game` | ✅ | ✅ DXVK | ❌ | Windows games |
| `dos` | ❌ | ❌ | ❌ | DOSBox apps |
| `proton` | ✅ | ✅ DXVK/VKD3D | ✅ | Steam games |
| `strict` | ❌ | ❌ | ❌ | Untrusted code |
| `development` | ✅ | ✅ | ✅ | Build tools |

### GPU Passthrough
```c
// Automatic for game/proton profiles
--dev-bind /dev/dri /dev/dri
// Environment:
MESA_LOADER_DRIVER_OVERRIDE=iris
VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/intel_icd.x86_64.json
```

### SteamOS Compat Environment
```c
STEAM_COMPAT_DATA_PATH=/home/user/.steam/steam/steamapps/compatdata/<appid>/
PROTON_PATH=/usr/lib/proton-ge/
DXVK_ASYNC=1
ESYNC=1 FSYNC=1
```

### Container Handle
```c
typedef struct {
    pid_t pid;
    int stdin_fd, stdout_fd, stderr_fd;
    char styx_path[256];      // 9P socket path
    bool gpu_passthrough;
    uint64_t mem_limit_bytes;
    uint64_t cpu_quota_us;
} WuBuContainer;
```

---

## 8. Proton/Wine Integration

### Source Location
```
src/gui/wubu_proton.c/h
src/gui/wubu_gamelib.c/h
```

### Architecture
```
┌─────────────────────────────────────────────────────────┐
│  Steam Library Scanner                                  │
│  ├── ~/.steam/steam/steamapps/libraryfolders.vdf       │
│  ├── ~/.local/share/Steam/                              │
│  ├── Flatpak: ~/.var/app/com.valvesoftware.Steam/      │
│  └── Parse appmanifest_*.acf for appid, name, dir      │
├─────────────────────────────────────────────────────────┤
│  Game Database (wubu_gamelib)                           │
│  ├── 14 Categories: All, Favorites, Recent, Steam,     │
│  │   Epic, GOG, Ubi, EA, Battle.net, Heroic, Lutris,  │
│  │   Native, Windows, Not Installed                    │
│  ├── Metadata: playtime, genres, tags, images,         │
│  │   controller, cloud sync                            │
│  └── Filtering: source, genre, tag, favorite, text     │
├─────────────────────────────────────────────────────────┤
│  Proton Manager (wubu_proton)                           │
│  ├── Wine Prefix Management                             │
│  │   ├── Create/remove/set default                     │
│  │   ├── Per-game prefixes                             │
│  │   └── Config persistence (~/.local/share/wubu/proton/)│
│  ├── DXVK/VKD3D Installation                            │
│  │   ├── Via winetricks                                 │
│  │   └── Detection (d3d11.dll, d3d12.dll)              │
│  ├── Steam Compat Data                                  │
│  │   ├── STEAM_COMPAT_DATA_PATH                        │
│  │   └── PROTON_PATH                                    │
│  └── Wine Execution                                     │
│      ├── ESYNC/FSYNC enabled                           │
│      ├── DLL overrides                                 │
│      └── Bubblewrap GPU + prefix                       │
└─────────────────────────────────────────────────────────┘
```

### Wine Prefix Structure
```
~/.local/share/wubu/proton/
├── config.json              # Global config
├── prefixes/
│   ├── default/             # Default prefix
│   │   ├── drive_c/
│   │   ├── user.reg
│   │   └── system.reg
│   └── game-<appid>/        # Per-game prefix
│       ├── drive_c/
│       ├── user.reg
│       └── system.reg
└── dxvk/                    # DXVK version cache
```

### Game Launch Flow
```
User clicks "Play"
      │
      ▼
wubu_gamelib_get_game(appid)
      │
      ▼
wubu_proton_launch(game, prefix)
      │
      ├─► Ensure prefix exists
      ├─► Ensure DXVK/VKD3D installed
      ├─► Build bubblewrap args (game profile)
      ├─► Set env: STEAM_COMPAT_DATA_PATH, PROTON_PATH
      ├─► Set env: DXVK_ASYNC=1, ESYNC=1, FSYNC=1
      ├─► wine64 path/to/game.exe
      │
      ▼
Game runs in sandbox
```

---

## 9. HolyC Compatibility Layer

### Source Location
```
src/gui/wubu_holyc.c/h
src/compiler/
├── holyc_lexer.c/h
├── holyc_parse.c/h
├── holyc_codegen.c/h
└── holyc.h
```

### Architecture
```
┌─────────────────────────────────────────────────────────┐
│  HolyC Source (.hc)                                     │
├─────────────────────────────────────────────────────────┤
│  Lexer (30+ token types)                                │
│  ├── Keywords: I64, U0, IF, WHILE, FOR, CLASS, STRUCT  │
│  ├── Operators: + - * / % == != < > <= >= && ||        │
│  ├── Literals: int, hex, bin, float, string, char      │
│  └── Comments: // ... /* ... */                        │
├─────────────────────────────────────────────────────────┤
│  Parser (AST)                                           │
│  ├── Expressions: binary, unary, ternary, call         │
│  ├── Statements: if, while, for, return, break, continue│
│  ├── Declarations: func, var, struct, class, enum      │
│  └── Types: primitive, ptr, array, struct, class       │
├─────────────────────────────────────────────────────────┤
│  C Transpiler                                           │
│  ├── Emits C99 + HolyC runtime intrinsics              │
│  ├── holyc_print(), holyc_malloc(), holyc_free()       │
│  ├── holyc_spawn(), holyc_sleep()                      │
│  └── VBE graphics bindings                             │
├─────────────────────────────────────────────────────────┤
│  AOT Compiler                                           │
│  ├── Transpile → .c → gcc -shared → .so                │
│  ├── dlopen() → resolve symbols → run                  │
│  └── Dependency graph for multi-file                   │
├─────────────────────────────────────────────────────────┤
│  JIT Interpreter                                        │
│  ├── Direct AST execution for REPL                     │
│  ├── No compilation step                               │
│  └── Slower but instant feedback                       │
├─────────────────────────────────────────────────────────┤
│  Syscall Bridge (25+ mappings)                          │
│  ├── FileRead/Write  → read/write                      │
│  ├── MAlloc/Free     → malloc/free                     │
│  ├── Spawn           → fork+execve                     │
│  ├── Sleep           → nanosleep                       │
│  ├── GrBlit/GrRect   → VBE primitives                  │
│  ├── NetConnect      → socket+connect                  │
│  └── FsMakeDir       → mkdir                           │
└─────────────────────────────────────────────────────────┘
```

### Example HolyC Program
```holyc
// hello.hc
U0 Main() {
  "Hello from HolyC!\n";
  I64 x = 42;
  "%d\n", x;
  
  // Graphics
  GrRect(100, 100, 200, 150, RED);
  GrBlit(300, 200, my_sprite);
  
  // File I/O
  I64 h = FileOpen("test.txt", "w");
  FileWrite(h, "data", 4);
  FileClose(h);
  
  // Network
  I64 sock = NetConnect("example.com", 80);
  NetWrite(sock, "GET / HTTP/1.0\r\n\r\n", 20);
}
```

### Module System
```c
// Load .so plugin
void* handle = wubu_holyc_load_module("plugin.so");
if (handle) {
    MyFunc fn = wubu_holyc_get_symbol(handle, "MyFunc");
    if (fn) fn();
}
```

---

## 10. Deployment Targets

### Source Location
```
src/gui/wubu_deploy.c/h
deploy/
├── baremetal/
├── wsl2/
├── oci/
└── macos/
```

### Target Matrix
| Target | Output | Boot Method | Use Case |
|--------|--------|-------------|----------|
| **Bare Metal** | `.iso` | Limine + initramfs | Physical hardware, VMs |
| **WSL2** | `.tar.gz` | `wsl --import` | Windows integration |
| **OCI** | Docker image | `docker run` | Containers, CI/CD |
| **macOS** | `.app` bundle | Virtualization.framework | Apple Silicon/Intel Macs |

### Bare Metal (Limine)
```
wubuos.iso
├── boot/
│   ├── vmlinuz           # Linux kernel
│   ├── initramfs.img     # Cpio+gzip rootfs
│   └── limine.conf       # Boot config
└── EFI/BOOT/BOOTX64.EFI  # UEFI bootloader
```

**Kernel Cmdline**: `quiet loglevel=3 mitigations=off`

**Initramfs Contents**:
```
/init                    # Custom init script
/bin/wubu               # WuBuOS binary
/etc/{passwd,group,hostname,resolv.conf}
/var, /tmp, /proc, /sys, /dev, /run
/usr/bin, /usr/lib, /lib, /lib64
```

### WSL2
```bash
# Build
make wsl2

# Install
wsl --import WuBuOS C:\WuBuOS wubuos-wsl2.tar.gz --version 2

# wsl.conf (auto-generated)
[boot]
systemd=true
[automount]
enabled=true; root=/mnt/; options="metadata,uid=1000,gid=1000"
[network]
generateResolvConf=true; generateHosts=true
[interop]
enabled=true; appendWindowsPath=true
[user]
default=wubu
```

### OCI/Docker
```dockerfile
# Scratch base (smallest)
FROM scratch
COPY rootfs/ /
USER 1000:1000
WORKDIR /home/wubu
ENTRYPOINT ["/usr/bin/wubu"]
CMD ["-w", "1024", "768"]

# Build
docker build -t wubuos:latest .
docker run --rm -it \
  -v /tmp/.X11-unix:/tmp/.X11-unix \
  -v /run/user/1000:/run/user/1000 \
  -e WAYLAND_DISPLAY=wayland-0 \
  wubuos:latest
```

### macOS AVF
```swift
// wubu_launcher.swift (generated)
@main struct WuBuOSLauncher {
    static func main() async {
        let config = VZVirtualMachineConfiguration()
        config.cpuCount = 4
        config.memorySize = 4 * 1024 * 1024 * 1024
        
        let bootLoader = VZLinuxBootLoader(kernelURL: kernelURL)
        bootLoader.commandLine = "quiet loglevel=3"
        bootLoader.initialRamdiskURL = initrdURL
        config.bootLoader = bootLoader
        
        // VirtIO GPU for Wayland
        if #available(macOS 14.0, *) {
            let gpu = VZVirtioGPUDeviceConfiguration()
            gpu.scanouts = [VZVirtioGPUScanoutConfiguration()]
            config.graphicsDevices = [gpu]
        }
        
        // Rosetta for x86_64 Linux
        if #available(macOS 13.0, *) {
            try config.setRosettaDirectoryShare(VZLinuxRosettaDirectoryShareConfiguration())
        }
        
        let vm = VZVirtualMachine(configuration: config)
        try await vm.start()
    }
}
```

**Build Script**: `build_macos.sh` (run on macOS with Xcode 15+)

---

## 11. Package Manager (.wubu)

### Source Location
```
src/gui/wubu_pkgmgr.c/h
~/.local/share/wubu/pkgmgr/pkgmgr.db  # SQLite database
~/.config/wubu/repos/                # Repo configs
~/.cache/wubu/pkgmgr/                # Download cache
```

### .wubu Container Format
```
WUBU Container (binary)
├── Header (64 bytes)
│   ├── Magic: "WUBU" (0x55425557)
│   ├── Version: 1
│   ├── Arch: x86_64/aarch64/riscv64/any
│   ├── Payload Type: native/linux/win32/holyc/wasm/script/data
│   ├── Manifest Size (compressed)
│   ├── Payload Size (compressed)
│   ├── Ed25519 Signature (64 bytes)
│   └── Build metadata
├── Manifest (zstd-compressed JSON)
│   ├── id, name, version, description
│   ├── depends/recommends/conflicts/provides
│   ├── entrypoints (desktop integration)
│   ├── files[] {src, dst, mode}
│   └── sandbox_profile
└── Payload (zstd-compressed tar)
    └── Files to install
```

### Repository Index
```json
{
  "repo": "stable",
  "packages": [
    {
      "id": "firefox",
      "name": "Firefox",
      "version": "128.0",
      "download_url": "https://repo.wubuos.org/stable/firefox-128.0.wubu",
      "sha256": "...",
      "download_size": 52428800,
      "installed_size": 157286400,
      "arch": "x86_64",
      "depends": ["glibc", "gtk3", "dbus"],
      "provides": ["browser"]
    }
  ]
}
```

### Commands (API)
```c
// Init
wubu_pkgmgr_init(&config);

// Repos
wubu_pkgmgr_repo_add("stable", "https://repo.wubuos.org/stable", pubkey, 10);
wubu_pkgmgr_repo_update("stable");  // or NULL for all

// Search/Install
wubu_pkgmgr_search("browser", results, 20);
wubu_pkgmgr_install("firefox", false);  // from repo
wubu_pkgmgr_install("/path/to/app.wubu", false);  // local

// Manage
wubu_pkgmgr_remove("firefox", true);  // auto-remove deps
wubu_pkgmgr_upgrade_all(false);

// Create package
wubu_pkgmgr_create_package("./myapp", "myapp.wubu", &manifest, sign_key);

// Desktop integration (auto on install)
wubu_pkgmgr_register_desktop(&installed_pkg);
```

### Sandbox Profiles (auto-applied)
| Profile | Network | GPU | /home | Seccomp |
|---------|---------|-----|-------|---------|
| `default` | ✅ | ❌ | ❌ | Basic |
| `game` | ✅ | ✅ | ❌ | Game |
| `proton` | ✅ | ✅ | ✅ | Wine |
| `strict` | ❌ | ❌ | ❌ | Strict |

---

## 12. Security Model

### Principle of Least Privilege
- **Single User**: No multi-user support (like ZealOS)
- **No SUID**: Binary runs as user, no privilege escalation
- **Containers**: All external apps run in bubblewrap
- **No Kernel Modules**: Everything in-process

### Sandboxing Layers
```
1. Bubblewrap Namespaces
   ├── --unshare-all (user, pid, net, ipc, uts, cgroup)
   ├── --new-session
   ├── --die-with-parent
   └── --ro-bind /usr /usr (read-only)

2. Seccomp-bpf (strict profile)
   ├── allow: read, write, openat, close, mmap, munmap
   ├── allow: futex, nanosleep, getpid, gettid
   └── deny: execve, ptrace, mount, reboot

3. GPU Isolation
   ├── /dev/dri render nodes only
   └── No control node access

4. Filesystem
   ├── No host /home (except proton profile)
   ├── Private /tmp per container
   └── Read-only /usr, /lib
```

### Signature Verification
- **Ed25519**: All .wubu packages signed by repo key
- **Config Option**: `verify_signatures` (default true)
- **Untrusted**: `allow_untrusted` for local dev

---

## 13. Build System

### Makefile Structure
```makefile
# Top-level targets
all: kernel jit compiler runtime tools gui bridge apps worldsim metal audio shell bear hosted_objs

# Hosted binary (main product)
hosted: $(HOSTED_OBJS) $(HOSTED)/xdg-shell-private.o
	$(CC) $(CFLAGS) -DVBE_HOSTED ... -o $(HOSTED)/wubu

# Test suite
test: test_jit test_memory test_tasking ... test_pkgmgr

# Clean
clean: rm -f */*.o */*_test $(HOSTED)/wubu
```

### Compilation Flags
```makefile
CC = gcc
CFLAGS = -Wall -Wextra -Wno-unused-function -std=c11 -O2 -g \
         -D_POSIX_C_SOURCE=200809L -Wno-array-bounds -DWUBU_NO_LIBM
LDFLAGS = -lwayland-client -lxkbcommon -lm -lsqlite3 -lzstd
```

### Wayland Protocol Compilation
```makefile
$(HOSTED)/xdg-shell-private.o: $(HOSTED)/xdg-shell-private.code
	$(CC) $(CFLAGS) -I$(HOSTED) -x c -c $< -o $@
```

### Assembly Files
```makefile
$(KERNEL)/%.o: $(KERNEL)/%.S
	$(CC) $(CFLAGS) -I$(KERNEL) -c $< -o $@
# Uses GAS (gcc -c), NOT nasm
```

### Dependencies
| Library | Purpose | Static/Shared |
|---------|---------|---------------|
| wayland-client | Wayland protocol | Shared |
| xkbcommon | Keyboard handling | Shared |
| sqlite3 | Package DB | Shared |
| zstd | Compression | Shared |
| m | Math (avoided where possible) | Shared |

---

## 14. Testing Strategy

### Test Organization
```
test: test_jit test_memory test_tasking test_input test_worldsim
      test_fat32 test_holyc test_wubu test_apps test_vsl
      test_bridge test_bridge_flip test_proton test_ahci
      test_iso test_weights test_gc test_txfs test_dbuf
      test_wm test_startmenu test_styx test_styxfs test_hosted
      test_host_exec test_arch test_ramdisk test_gaad
      test_wubu_wm test_apps2 test_proton2 test_audio
      test_screenshot test_dosgui_wm test_deploy test_pkgmgr
```

### Test Categories

| Test | Target | Description |
|------|--------|-------------|
| `test_jit` | JIT | Compilation, execution, memory |
| `test_memory` | Kernel | Alloc/free, calloc, realloc, stress |
| `test_tasking` | Kernel | Create, priority, sleep/wake |
| `test_wm` | GUI | Window create, minimize, maximize, virtual desktops |
| `test_startmenu` | GUI | Open, navigate, submenus |
| `test_hosted` | Full | Boot kernel+GUI, Wayland, Styx, cells 200-207 |
| `test_proton` | Runtime | Prefix, DXVK, game launch |
| `test_pkgmgr` | Package | Create, install, remove, repos |
| `test_deploy` | Deploy | Rootfs, limine, wsl.conf, Dockerfile |
| `test_holyc` | Compiler | Lex, parse, codegen, eval (76 tests) |

### Headless Demo Capture
```bash
# Generates docs/wubuos_demo.gif + .mp4
./src/hosted/wubu -h  # Headless mode
./demo_capture        # 100 frames, 7 phases, 4 themes
```

**Demo Phases**:
1. Win98 desktop (640×480 TempleOS resolution)
2. XP Luna desktop (1920×1080)
3. Theme cycle demo (98→Luna→Orange→WuBu)
4. Window operations (create, move, min, max, restore)
5. Start menu (XP sidebar, Win98 popup)
6. App launch (Notepad, Paint, Calculator)
7. Settings dialog

### CI Integration
- All tests run in headless mode (`-h` flag)
- No Wayland compositor required
- Exit code 0 = all pass

---

## 15. Future Roadmap

### Phase 4 (Current)
- ✅ Item 1: Multi-Target Deployment
- ✅ Item 2: Package Manager + .wubu App Store
- 🔄 Item 3: OS Design Bible Document

### Phase 5: HolyC Ecosystem
- [ ] HolyC Package Index (hpkgs)
- [ ] HolyC Language Server (LSP)
- [ ] HolyC Debugger Integration
- [ ] ZealOS Driver Porting Guide

### Phase 6: Desktop Polish
- [ ] File Manager (dosgui_explorer)
- [ ] Terminal Emulator (dosgui_terminal)
- [ ] System Monitor (dosgui_control)
- [ ] Network Manager GUI

### Phase 7: Gaming
- [ ] Steam Deck Optimizations
- [ ] Anti-cheat Compatibility Layer
- [ ] Cloud Gaming Integration (GeForce Now, etc.)

### Phase 8: Enterprise
- [ ] Multi-user Support (optional)
- [ ] Remote Desktop (RDP/VNC server)
- [ ] Centralized Management API

### Phase 9: Hardware
- [ ] ARM64 Native (Raspberry Pi, Apple Silicon)
- [ ] RISC-V Port
- [ ] GPU Driver Stack (Nouveau, AMD, Intel)

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
| `src/compiler/holyc_codegen.c` | C transpiler |

---

**Document Version:** 1.0  
**Last Updated:** 2025  
**Maintainers:** WuBuOS Team  

*This is a living document. Update with each major architectural change.*