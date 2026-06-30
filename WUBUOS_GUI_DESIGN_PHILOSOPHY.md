# WuBuOS GUI Design Philosophy

## Synthesis: Apple + Windows 98/XP + Zune + UNIX + SteamOS

---

## 1. APPLE DESIGN PHILOSOPHY (Human Interface Guidelines)

### Core Principles
- **Consistency** - Same actions produce same results everywhere
- **Direct Manipulation** - Users interact directly with objects (drag, drop, resize)
- **Feedback** - Immediate visual/auditory response to every action
- **Metaphors** - Real-world concepts (desktop, folders, trash)
- **User Control** - User initiates actions, not the system
- **Forgiveness** - Easy to undo, hard to lose data

### Foundations (from HIG)
- **Layout** - Grid-based, alignment, visual hierarchy
- **Typography** - San Francisco / system fonts, dynamic type
- **Color** - Semantic colors (systemBlue, systemRed), light/dark mode
- **Materials** - Translucency, vibrancy, depth layers
- **Motion** - Meaningful transitions, spring animations
- **Accessibility** - VoiceOver, Dynamic Type, contrast

---

## 2. WINDOWS 98 DESIGN (MSDN "Official Guidelines for UI Developers")

### The Desktop
- Primary work area, fills screen
- Convenient location for file system objects
- Private workspace for networked computer
- Icons represent objects (files, folders, apps)

### The Taskbar
- **Operational anchor** - home base for interface
- **Start Button** - global command access
- **Quick Launch** - user-customized frequent apps
- **Window Buttons** - one per primary window
  - ToolTips for truncated titles
  - Drag-drop onto buttons opens window
- **Status Notification Area** - clock, volume, background apps

### Windows (from MSDN ch10a)
- **SDI** (Single Document Interface) - one window per document
- **MDI** (Multiple Document Interface) - container with children
- **Workbooks** - tabbed documents
- **Projects** - hierarchical organization
- Window management: minimize, maximize, restore, close

### Visual Style (Windows Classic)
- 3D raised/sunken borders (light/dark edges)
- Gray backgrounds (#C0C0C0)
- Blue title bars for active windows
- 16-color icon standard

---

## 3. WINDOWS XP LUNA / ZUNE THEME

### Luna (Default XP)
- **Blue** - default (Start button, title bars)
- **Olive Green** - Home Edition
- **Silver** - Media Center / Professional
- Rounded corners on buttons/windows
- Gradient title bars
- Drop shadows on menus/tooltips

### Zune Theme Colors
- **Dark backgrounds** - #1A1A1A, #2A2A2A
- **Accent orange** - #E86C00 (Zune brand)
- **Teal/green highlights** - #008050
- **White/light gray text** - high contrast
- Subtle glow effects on focus

### Visual Style Engine (uxtheme.dll)
- `.msstyles` files with bitmaps for each control state
- 9-slice scaling for borders
- Per-control part/state definitions

---

## 4. UNIX PHILOSOPHY (Applied to GUI)

### Core Tenets
- **Everything is a file** - 9P/Styx namespace for all resources
- **Small sharp tools** - Each GUI component does one thing well
- **Composition over monolith** - Pipe output of one to input of another
- **Text streams** - Universal interface (JSON over 9P)
- **No policy in mechanism** - Mechanism in kernel, policy in user space

### Applied to WuBuOS
- `/dev/window/*` - window control files
- `/dev/input/*` - input event streams
- `/net/clipboard` - clipboard as network resource
- `/wubu/theme/current` - active theme as file
- `styx` server mediates all namespace operations

---

## 5. STEAMOS / GAMESCOPE PHILOSOPHY

### Key Innovations
- **Proton** - Wine + DXVK + VKD3D translation layer
- **gamescope** - Wayland compositor with:
  - FSR (FidelityFX Super Resolution) upscaling
  - HDR tone mapping
  - VRR (Variable Refresh Rate)
  - Nested compositor for games
- **Steam Input** - Controller abstraction layer
- **Pressure Vessel** - Container runtime for games
- **Shader pre-caching** - Fossilize / dxvk-cache

### Architecture Lessons
- Linux kernel + userspace translation = Windows compat
- Compositor handles display, not individual apps
- Container isolation for security/compatibility
- Proactive driver updates via system updates

---

## 6. SDL2 AS WAYLAND TRANSLATION LAYER

### Why SDL2?
- Abstracts window/input/audio across platforms
- Native Wayland backend + X11 fallback
- Gamepad/controller abstraction (Steam Input compatible)
- Thread-safe event pump
- 20+ years of battle testing

### WuBuOS SDL2 Architecture
```
WuBuOS App → SDL2 API → Wayland / X11 / DRM/KMS
                ↓
         Proton / DXVK → Vulkan
```

### Benefits
- Single codebase for hosted + bare-metal
- Wayland compositor integration
- Hot-plug input devices
- High-DPI / fractional scaling support

---

## 7. NATURAL PATH DRIVERS (Plumbing)

### Missing Plumbing to Implement
```
/dev/window/{id}/framebuffer  - direct fb access
/dev/window/{id}/geometry     - x,y,w,h
/dev/window/{id}/title        - window title
/dev/window/{id}/state        - minimized/maximized/fullscreen
/dev/window/{id}/focus        - focus events
/dev/input/mouse              - relative +relative motion, buttons
/dev/input/keyboard           - key events
/dev/input/gamepad            - SDL_Gamepad events
/net/clipboard                - text/image data
/wubu/theme/current           - active theme file
/wubu/namespace/window        - window registry
```

### 9P/Styx Message Types
- `Tread` / `Twread` - read window state
- `Twrite` / `Twwrite` - write window commands
- `Twstat` - change metadata (title, geometry)
- `Tattach` / `Tclunk` - connect/disconnect

---

## 8. TEXT OVERLAP FIXES (Current WuBuOS Issues)

### Root Causes
1. `vbe_draw_text` uses fixed 6px advance, no kerning
2. Icon label drawn at `icon->y + ICON_SIZE + 2` - no bounds check
3. Taskbar clock overlaps window buttons
4. Start menu items truncate at 128 chars
5. No text measurement before drawing

### Solutions
- Add `vbe_text_width()` with proper font metrics
- Implement ellipsis truncation `...`
- Reserve space for clock in taskbar layout
- Use `snprintf` with buffer sizes everywhere
- Add `DOSGUI_ICON_LABEL_H` constant for spacing

---

## 9. WINDOW DESIGN PHILOSOPHY IMPLEMENTATION

### Window Chrome (Windows 98/XP)
```
┌─────────────────────────────────────┐ ← Title bar (24px XP, 22px 98)
│ 📁  My Computer              [_] [□] [X] │ ← Icon, title, min/max/close
├─────────────────────────────────────┤
│                                     │ ← Client area
│         Client content              │
│                                     │
├─────────────────────────────────────┤ ← Status bar (optional)
│ Ready                               │
└─────────────────────────────────────┘
```

### Resize Handles
- 4px border on all edges (invisible, hit-test)
- 16px corner squares for diagonal resize
- Cursor changes: ↔ ↕ ↖↘ ↗↙

### Focus Model
- Click to focus (Windows)
- Alt+Tab cycles Z-order
- Active window: blue title bar, raised buttons
- Inactive: gray title bar, sunken buttons

### Z-Order Management
- Click raises to top
- Minimize sends to bottom
- Always-on-top flag for tooltips/menus

---

## 10. THEME SYSTEM ARCHITECTURE

### Current Themes (wubu_theme.c)
1. **Win98 Classic** - Gray, 3D borders, blue active
2. **XP Luna Blue** - Blue gradients, rounded
3. **XP Luna Olive** - Green variant
4. **XP Luna Silver** - Media Center
5. **XP Media Center Dark** - Dark mode
6. **Zune** - NEW: Dark, orange accent

### Theme Structure
```c
typedef struct {
    // Window chrome
    uint32_t titlebar_active_grad_top;
    uint32_t titlebar_active_grad_bottom;
    uint32_t titlebar_inactive;
    uint32_t titlebar_border;
    uint32_t titlebar_text;
    uint32_t close_btn_face;
    uint32_t close_btn_hover;
    
    // Desktop
    uint32_t desktop_bg_top;
    uint32_t desktop_bg_bottom;
    uint32_t icon_bg;
    uint32_t icon_border;
    uint32_t icon_text;
    uint32_t icon_text_shadow;
    
    // Taskbar
    uint32_t taskbar_bg;
    uint32_t taskbar_border;
    uint32_t start_btn_face;
    uint32_t start_btn_text;
    
    // Buttons/Controls
    uint32_t btn_face;
    uint32_t btn_text;
    uint32_t select_bg;
    uint32_t select_text;
    uint32_t border_light;
    uint32_t border_face;
    uint32_t border_dark;
    uint32_t border_darkest;
    
    // Style flags
    bool Luna_start_button;
    bool rounded_buttons;
    bool rounded_window_corners;
    bool drop_shadows;
} WubuTheme;
```

---

## 11. IMPLEMENTATION ROADMAP

### Phase 1: Foundation Fixes (Week 1-2)
- [ ] Fix text overlap in dosgui_wm.c
- [ ] Add proper text measurement + ellipsis
- [ ] Fix taskbar layout (clock reservation)
- [ ] Add Zune theme to wubu_theme.c

### Phase 2: Window Chrome (Week 3-4)
- [ ] Implement proper title bar rendering
- [ ] Add resize handles (invisible hit-test)
- [ ] Implement focus visual states
- [ ] Add minimize/maximize/close buttons with hover

### Phase 3: SDL2 Wayland Layer (Week 5-6)
- [ ] Create `src/hosted/wubu_sdl2.c`
- [ ] SDL2 window + renderer + event pump
- [ ] Wayland compositor integration
- [ ] Input abstraction (mouse/keyboard/gamepad)

### Phase 4: Natural Path Drivers (Week 7-8)
- [ ] 9P/Styx window namespace
- [ ] `/dev/window/*` control files
- [ ] Input event streams
- [ ] Clipboard network resource

### Phase 5: SteamOS Patterns (Week 9-10)
- [ ] Pull gamescope compositor code
- [ ] FSR upscaling integration
- [ ] Proton/DXVK container support
- [ ] Pressure Vessel isolation

---

## 12. GITHUB REPOS TO STUDY

| Repo | Purpose |
|------|---------|
| `ValveSoftware/gamescope` | Wayland compositor + FSR |
| `ValveSoftware/Proton` | Wine + DXVK + VKD3D |
| `ValveSoftware/pressure-vessel` | Container runtime |
| `HansKristian-Work/vkd3d-proton` | D3D12 → Vulkan |
| `doitsujin/dxvk` | D3D9/10/11 → Vulkan |
| `libsdl-org/SDL` | Cross-platform abstraction |
| `ValveSoftware/steam-runtime` | Linux runtime for games |
| `ValveSoftware/steamos` | SteamOS base |
| `microsoft/WinUI` | Modern Windows UI |
| `apple/swift-corelibs-foundation` | UNIX patterns |

---

## 13. SYNERGY: HOW IT ALL FITS

```
┌─────────────────────────────────────────────────────────────┐
│                    WuBuOS Architecture                       │
├─────────────────────────────────────────────────────────────┤
│  Apps (Win98/XP style)                                       │
│       ↓ SDL2 API                                             │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │ Wayland      │  │ X11/DRM      │  │ Bare Metal       │  │
│  │ Compositor   │  │ Fallback     │  │ VBE/Framebuffer  │  │
│  └──────────────┘  └──────────────┘  └──────────────────┘  │
│       ↓              ↓                    ↓                 │
│  ┌──────────────────────────────────────────────────────┐  │
│  │              9P/Styx Namespace                       │  │
│  │  /dev/window/*  /dev/input/*  /net/clipboard/*       │  │
│  └──────────────────────────────────────────────────────┘  │
│       ↓                                                    │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │ Proton       │  │ gamescope    │  │ Pressure Vessel  │  │
│  │ (Wine/DXVK)  │  │ (compositor) │  │ (containers)     │  │
│  └──────────────┘  └──────────────┘  └──────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

### Design Philosophy Integration

| Layer | Apple | Windows 98/XP | Zune | UNIX | SteamOS |
|-------|-------|---------------|------|------|---------|
| Visual | Consistency, depth | Classic 3D chrome | Dark/orange accent | Minimal | Gaming focus |
| Interaction | Direct manipulation | Click-to-focus, taskbar | Glow feedback | Text streams | Controller-first |
| Architecture | Composition | SDI/MDI windows | Theme engine | Everything=file | Translation layers |
| Plumbing | Namespace | OLE/COM/DDE | Registry | 9P/Styx | Containers |

---

*This document synthesizes 30+ years of GUI design wisdom into a coherent vision for WuBuOS - a Win98/XP compatible desktop that runs on modern Linux with SteamOS-grade Windows compatibility.*