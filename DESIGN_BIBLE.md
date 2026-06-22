# WuBuOS Design Bible
## "TempleOS on a RamDisk" via ZealOS → WuBuDos → WuBuNT

---

## 1. Vision Statement

**WuBuOS** is a GUI shell + container runtime wrapping the **ZealOS kernel** (TempleOS C-port).
- **ZealOS IS the kernel**: Ring-0, single-user, HolyC JIT, boots on metal
- **WuBuOS IS the shell**: Win98 desktop, Styx/9P namespace, .wubu containers
- **Inferno emu pattern**: One binary, any host OS
- **Arch base** rips through Linux drivers for SteamOS/Proton compat

**Endgame progression:**
1. **WuBuOS** — Hosted binary with Win98 shell (current)
2. **WuBuDos** — Bare-metal TempleOS on RamDisk (ZealOS kernel + WuBu shell)
3. **WuBuNT** — Arch backend desktop (WuBuOS shell on Arch userspace, Proton native)

---

## 2. Architecture Pillars

### 2.1 Kernel Layer (ZealOS)
- HolyC → C translation (zealos_parity.h maps ZealOS function names)
- mmap-based JIT compiler
- Framebuffer graphics (VBE/DRM-KMS)
- Single-address-space, no processes, tasks = coroutines
- TXFS (TempleOS File System) + FAT32 + AHCI
- **wubu_archd**: Arch Linux daemon — epoll event loop, Unix socket + JSON protocol (16 tests)
- **wubu_holyd**: HolyC DOS daemon — epoll event loop, Unix socket + JSON protocol (27 tests)

### 2.2 Shell Layer (WuBuOS)
- **wubu_wm**: Full window manager with GAAD snap, virtual desktops
- **wubu_theme**: Runtime-switchable themes (Win98, XP Luna, XP Media, WuBu Green)
- **dosgui_wm**: Fable Windowing Agent — z-order, drag, focus, taskbar, icons (Cell 400)
- **dosgui_desktop**: Win98 desktop with launchable app icons (Cell 401)
- **dosgui_startmenu**: Cascading start menu with program groups (Cell 402)

### 2.2a Fable Windowing Agent (DosGui)
The "sauce" ported from Mythos Fable's bare-metal kernel (filipvabrousek/osdev).
Fable's kernel provided: VBE framebuffer, 8x8 bitmap font, window manager with
drag/z-order/focus, taskbar with "+ NEW" button, software mouse cursor, and
demo apps (Welcome, Notepad, Palette, Bounce). We port this as the rendering
and window management backbone of the WuBuOS hosted desktop.

```
Fable bare-metal kernel → DosGui Windowing Agent (hosted)
  gfx.c primitives    → vbe.c (font, gradient, circle, shade, cursor)
  wm.c window manager  → dosgui_wm.c (z-order, drag, focus, taskbar)
  ps2.c input          → hosted Wayland input → dosgui_wm_handle_mouse
  demo apps            → WuBuOS desktop apps (calc, terminal, explorer, etc.)
```

### 2.3 Namespace Layer (Styx/9P)
- Plan 9 9P2000 protocol over Unix sockets
- .wubu container format: binary + deps + config + 9P mounts
- Container runtime = host process (not syscall emulation)

### 2.4 Container Format (.wubu)
```
.wubu/
├── manifest.json    # metadata, entrypoint, mounts
├── bin/             # executables
├── lib/             # shared libraries
├── etc/             # config
├── share/           # assets
└── rootfs/          # optional rootfs overlay
```

### 2.5 RamDisk Layer (Cell 392)
- **RAM Mode**: tmpfs mount, fast, ephemeral, containers
- **DISK Mode**: Arch base on SSD, persistent, bare metal
- `install_to_disk()` → WuBuDos
- `snapshot()` → save state

---

## 3. Win98 Desktop Bible

### 3.1 Visual Identity
| Element | Win98 Classic | XP Luna Blue | XP Media Orange | WuBu Green |
|---------|---------------|--------------|------------------|------------|
| Desktop BG | Teal (0x008080) | Bliss Blue (0x00528A) | Near-black (0x1A1A1A) | Dark Green (0x0A2A1A) |
| Window Face | Silver (0xC0C0C0) | Light Gray (0xE8E8E8) | Dark Gray (0x2A2A2A) | Green-tint (0x1A3A2A) |
| Active Title | Navy (0x000080) | Luna Blue (0x00539E) | Orange (0xE86C00) | WuBu Green (0x008050) |
| Taskbar | Silver | Blue (0x316AC5) | Black + Orange | Green + Border |
| Start Button | Raised 3D | Green Orb | Orange | Green |

### 3.2 Desktop Icons (Fixed Grid)
- **My Computer** (20, 20) — opens File Manager
- **Temple REPL** (20, 80) — HolyC REPL window
- **WuBuOS Paint** (20, 140) — Paint app
- **WuBuOS Notepad** (20, 200) — Text editor
- **Terminal** (20, 260) — Shell
- **Settings** (20, 320) — Control Panel

### 3.3 Taskbar (28px bottom)
- Start button (60×22, 3D raised)
- Window buttons (dynamic, 80×22, sunken when focused)
- System tray (right): clock, volume, network, battery
- Desktop switcher (1-9 indicators)

### 3.4 Start Menu (Cascading)
```
▶ Programs
  ▶ Accessories
    ▷ Notepad
    ▷ Paint
    ▷ Calculator
    ▷ Terminal
  ▶ WuBuOS
    ▷ Temple REPL
    ▷ Package Manager
    ▷ Container Manager
  ▶ System
    ▷ File Manager
    ▷ Settings
▶ Documents
▶ Settings
  ▷ Control Panel
  ▷ Taskbar Settings
  ▷ Display Settings
  ▷ Theme Selector
▶ Find
▶ Help
▶ Run...
▶ Separator
▶ Shutdown
  ▷ Shut Down
  ▷ Restart
  ▷ Restart in MS-DOS Mode (WuBuDos)
  ▷ Log Off
```

---

## 4. Core Desktop Apps (Required)

### 4.1 Calculator (calc.c)
- Standard / Scientific / Programmer modes
- Win98 visual: 3D buttons, memory indicators
- History tape (scrollable)
- Keyboard shortcuts: NumPad, Esc=Clear, Enter==

### 4.2 Terminal (terminal.c)
- PTY backend via `forkpty()`
- Win98 console window (not xterm)
- HolyC REPL embedded (Ctrl+Alt+T toggles)
- Tab support (Ctrl+Shift+T)
- Scrollback buffer (10k lines)

### 4.3 File Manager (explorer.c)
- Two-pane (Norton Commander style) + single-pane (Win98)
- Toolbar: Back, Forward, Up, View (Icons/List/Details)
- Address bar (editable path)
- 9P namespace integration (Styx mounts visible)
- Drag-drop between panes
- Context menu: Open, Cut, Copy, Paste, Delete, Rename, Properties

### 4.4 Settings / Control Panel (control.c)
- **Display**: Resolution, refresh rate, wallpaper
- **Theme**: Win98 / XP Luna / XP Media / WuBu Green
- **Desktop**: Icon size, auto-arrange, grid snap
- **Taskbar**: Auto-hide, clock format, tray icons
- **Input**: Mouse speed, double-click, keyboard repeat
- **Startup**: Auto-login, boot mode (RAM/DISK)
- **Containers**: Default mounts, resource limits
- **Network**: WiFi, Ethernet, proxy
- **About**: WuBuOS version, ZealOS kernel hash, GAAD φ

---

## 5. WuBuDos — Bare Metal on RamDisk

### 5.1 Boot Flow
```
Limine bootloader
    ↓
ZealOS kernel (ring-0, identity-mapped)
    ↓
mem_init() → vbe_init() → tasking_init()
    ↓
wubu_rd_create(WUBU_RD_RAM, "initrd.cgz")
    ↓
wubu_rd_boot() → mounts tmpfs at /run/wubu/ramdisk
    ↓
Extract initrd.cgz → populate rootfs
    ↓
styx_server_start() on Unix socket
    ↓
wubu_wm_init(screen_w, screen_h)
    ↓
desktop_init() + taskbar_init() + startmenu_init()
    ↓
Launch: Temple REPL (PID 1), Explorer, Taskbar
    ↓
Main loop: vbe_swap() + input_poll() + wm_render()
```

### 5.2 RamDisk Layout
```
/run/wubu/ramdisk/
├── bin/           # wubu_wm, wubu_terminal, wubu_explorer, holyc_repl
├── lib/           # libc, libm (wubu_math), libwayland (stub)
├── etc/
│   ├── wubu.conf       # theme=win98, desktop=4, ramdisk=2048m
│   ├── fstab.9p        # 9P mount points
│   └── splash.ppm      # boot splash
├── share/
│   ├── themes/         # theme palettes
│   ├── wallpapers/     # .ppm backgrounds
│   ├── icons/          # 32×32 .ppm icons
│   └── fonts/          # 8×16, 8×8 raster fonts
├── home/
│   └── user/
│       ├── Desktop/    # .lnk shortcuts
│       ├── Documents/
│       └── .wubu/      # container configs
├── tmp/
├── proc/              # synthetic (9P)
├── dev/               # synthetic (9P)
└── sys/               # synthetic (9P)
```

### 5.3 Install to Disk (WuBuDos → WuBuNT)
```c
int wubu_rd_install_to_disk(WubuRamdisk *rd, const char *target) {
    // 1. pacstrap base + linux + linux-firmware + mesa + wayland
    // 2. Copy /run/wubu/ramdisk → target
    // 3. Generate fstab (SSD root + EFI)
    // 4. Install limine bootloader
    // 5. Write /etc/wubu.conf (mode=disk)
    // 6. Return 0 on success
}
```

---

## 6. WuBuNT — Arch Backend Desktop

### 6.1 Architecture
```
┌─────────────────────────────────────────────┐
│           WuBuOS Shell (Hosted Binary)      │
│  wubu_wm • taskbar • desktop • startmenu    │
├─────────────────────────────────────────────┤
│         Styx/9P Namespace Bridge            │
│   /mnt/host → host FS   /mnt/wubu → containers  │
├─────────────────────────────────────────────┤
│         Arch Userspace (native)             │
│   pacman • systemd • mesa • pipewire • proton  │
└─────────────────────────────────────────────┘
```

### 6.2 Proton Integration (Cell 388/389 + wubu_proton2.c)
- DRM/KMS direct (no libdrm) → wubu_drm_direct.c
- Custom GBM → wubu_gbm.c
- Wine/Proton runs as host process, 9P exports `C:\` as `/mnt/wubu/proton/<app>`
- DXVK/VKD3D → Vulkan → RADV/ANV (native)

### 6.3 Package Manager (wubu_pkg.c)
- `.wubu` containers from registry
- `wubu install <name>` → downloads, verifies, unpacks, registers 9P mount
- `wubu run <name>` → launches in container namespace
- `wubu list` → installed containers

---

## 7. GAAD Resolution System (bytropix THEORY)

### 7.1 Φ-Structured Decomposition
- Screen → GAAD decomposition → φ-banded regions
- Feng Shui snap: cardinal mirrors (N/S/E/W quadrants)
- Window drag → snap preview → release → GAAD align

### 7.2 Virtual Desktop Mapping
- Desktop 0-8 → GAAD super-regions
- Sticky windows → all desktops (taskbar, system tray)
- Ctrl+Alt+Left/Right → switch desktop

---

## 8. HolyC / JIT Integration

### 8.1 Temple REPL App
- Embedded HolyC JIT (jit.c)
- REPL window with syntax highlighting
- `#include` from `/run/wubu/ramdisk/include/`
- Graphics: `vbe_fill_rect()`, `vbe_line()` via syscall bridge
- `F5` → cycle theme, `F11` → fullscreen

### 8.2 App Scripting
- All WuBuOS apps expose HolyC callbacks
- `wubu_wm_create()` callable from HolyC
- Event loop integration: `wm_handle_key()`, `wm_handle_mouse()`

---

## 9. Third-Party Dep Replacement Roadmap

| External | Replacement | Status |
|----------|-------------|--------|
| libX11 | DRM/KMS direct (wubu_drm_direct.c) | ✅ Cell 388 |
| libdrm | ioctl(DRM_IOCTL_MODE_*) | 🔄 wubu_drm.c |
| libgbm | wubu_gbm.c (BO alloc + modifiers) | 🔄 stub |
| libwayland | wl_stub.c (minimal client) | 🔄 hosted.c |
| libm | wubu_math.h (sin, cos, sqrt, log) | 🔄 partial |
| libpng | wubu_png.c (decode only) | ❌ |
| freetype | wubu_font.c (8×16 raster) | ✅ built-in |
| pulseaudio/pipewire | wubu_audio.c (alsa direct) | 🔄 stub |

---

## 10. Build Targets

| Target | Command | Output |
|--------|---------|--------|
| All | `make` | All libs + tests |
| Hosted binary | `make hosted` | `src/hosted/wubu` (Wayland) |
| Metal (bare metal) | `make metal` | `src/hosted/wubu_metal` |
| Win98 apps | `make apps` | paint, notepad, doom, editor, canvas, codec |
| Tests | `make test` | 747+ tests |
| RamDisk test | `make test_ramdisk` | Cell 392 validation |
| WM test | `make test_wubu_wm` | Cell 395 validation |
| Theme test | `make test_gaad` | Cell 394 validation |

---

## 11. Current State (Phase 12 - Cells 200-402)

### ✅ Working
- Kernel: memory, tasking, VBE, input, interrupt, FAT32, AHCI, TXFS
- JIT: mmap RWX, emit x86-64, mprotect RX, call
- HolyC: lexer, parser, codegen, REPL
- **DosGui WM** (Cell 400): z-order, drag, focus, taskbar, icons — Fable sauce
- **DosGui Desktop** (Cell 401): Win98 desktop with launchable app icons
- **DosGui StartMenu** (Cell 402): Cascading start menu with program groups
- **Fable font** (64-glyph 8x8 bitmap), **cursor**, **gradient**, **circle**, **shade**
- VBE: double-buffer, 32bpp, primitives, title bar, close box
- GUI (legacy): wm.c, desktop.c, taskbar.c, startmenu.c, theme.c
- GUI (wubu_wm): full WM with GAAD snap, virtual desktops, themes
- Apps: paint, notepad, doom (stub), editor, canvas, codec, calc, terminal, explorer, control
- Runtime: Styx/9P, container, VSL, Proton, Arch, RamDisk, GC
- Hosted: Wayland binary, input routing, SHM render, DosGui desktop
- Tests: 763+ passing (747 legacy + 16 dosgui_wm)

### 🔄 In Progress
- Desktop apps: wiring calc/terminal/explorer into DosGui windows
- Fable demo apps (Notepad, Palette, Bounce) as in-process window content
- DRM/KMS direct + custom GBM (Cells 388/389)
- Taskbar system tray (volume, network, battery icons)

### ❌ Missing (Design Bible Implementation)
- **WuBuDos bare-metal boot** (MBR + stage2 + kernel + initrd)
- **install_to_disk** implementation (pacstrap)
- **WuBuNT Proton integration** (wubu_proton2 → DXVK/VKD3D)
- **Wallpaper support** (.ppm loading, tiling/center/stretch)
- **Desktop icon drag-drop** (rearrange icons on grid)

---

## 12. Implementation Priority

### Phase A: Desktop Apps (Week 1-2)
1. `src/apps/calc.c` — Calculator
2. `src/apps/terminal.c` — PTY terminal
3. `src/apps/explorer.c` — File Manager
4. `src/apps/control.c` — Control Panel

### Phase B: Shell Polish (Week 2-3)
5. System tray in taskbar.c
6. Desktop icons with .lnk + drag
7. Wallpaper loader + modes
8. wubu_wm → hosted.c integration

### Phase C: Bare Metal (Week 3-4)
9. Limine config + kernel entry
10. initrd.cgz builder
11. wubu_rd_install_to_disk (pacstrap)
12. WuBuDos boot test (QEMU)

### Phase D: WuBuNT (Week 4+)
13. wubu_drm.c (complete DRM/KMS)
14. wubu_gbm.c (BO alloc)
15. wubu_proton2 → Vulkan swapchain
16. .wubu registry + package manager

---

## 13. Testing Checklist

- [ ] `make hosted` → `./src/hosted/wubu` launches Win98 desktop
- [ ] Start menu opens, programs launch
- [ ] Paint: brush, fill, shapes, palette, undo
- [ ] Notepad: type, backspace, save/load
- [ ] Calculator: 2+2=4, scientific mode, programmer mode
- [ ] Terminal: `ls`, `holyc`, `vim`, tabs work
- [ ] Explorer: two-pane, 9P mounts visible, drag-drop
- [ ] Control Panel: theme switch, resolution, startup mode
- [ ] Virtual desktops: Ctrl+Alt+Left/Right switches
- [ ] GAAD snap: drag window → release → snaps to region
- [ ] Theme cycle: F5 cycles Win98→XP Luna→XP Media→WuBu Green
- [ ] `make test` → 747+ pass
- [ ] `make test_ramdisk` → Cell 392 passes
- [ ] QEMU: `qemu-system-x86_64 -kernel wubudos.bin -initrd initrd.cgz`

---

## 14. Sacred Constants

```c
/* GAAD φ */
#define GAAD_PHI       1.618033988749895
#define GAAD_PHI_INV   0.618033988749895

/* Screen */
#define WUBU_DEFAULT_W  1024
#define WUBU_DEFAULT_H  768

/* RamDisk */
#define WUBU_RD_RAM_PATH      "/run/wubu/ramdisk"
#define WUBU_RD_DISK_PATH     "/var/wubu/roots/arch-base"
#define WUBU_RD_DEFAULT_SIZE  "2048m"

/* Themes */
#define THEME_WIN98_CLASSIC     0
#define THEME_XP_LUNA_BLUE      1
#define THEME_XP_MEDIA_ORANGE   2
#define THEME_WUBU_CUSTOM       3
#define THEME_COUNT             4

/* Window Manager */
#define WUBU_WM_MAX_WINDOWS   64
#define WUBU_WM_MAX_DESKTOPS  9
#define WUBU_WM_SNAP_DIST     30
```

---

*Design Bible v1.2 — WuBuOS / WuBuDos / WuBuNT*
*Generated from bytropix THEORY papers + ZealOS C-port + Win98 nostalgia*
*"TempleOS on a RamDisk, shipping as a clickable binary"*
*"Lossless C11 wrapper bundling Arch Linux + ZealOS + Fable sauce"*