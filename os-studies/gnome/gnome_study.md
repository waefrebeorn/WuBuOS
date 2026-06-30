# GNOME Study — Architecture & Feature Inventory
**Source**: GNOME 46+ (current stable), GNOME Shell, GTK4, libadwaita
**Purpose**: Triple DA comparison for WuBuOS Win98 shell gap analysis

---

## ═══════════════════════════════════════════════════════════════
## GNOME ARCHITECTURE OVERVIEW
## ════════════════════════════════════════════════════════════════

### Core Components
```
GNOME Shell (Mutter compositor)  ← Wayland compositor + shell UI
├── GTK── GTK4 (toolkit)               ← CSS theming, widgets, accessibility
├── libadwaita                   ← Adaptive widgets, HIG patterns
├── GLib/GObject                 ← Core object system, main loop, I/O
├── D-Bus                        ← IPC, service activation, introspection
├── Tracker                      ← File indexing, metadata, search
├── GVfs                         ← Virtual filesystems (sftp, smb, dav, etc.)
├── GSettings/dconf              ← Configuration storage
└── systemd --user               ← Session management, services
```

### Process Model
- **gnome-shell**: Wayland compositor + shell (single process, JavaScript + C)
- **gdm**: Display manager (Wayland/X11 sessions)
- **gnome-session**: Session manager (systemd --user integration)
- **gnome-settings-daemon**: Settings plugins (media-keys, power, color, etc.)
- **gnome-keyring**: Secrets storage (PKCS#11, SSH agent, GPG agent)
- **tracker-miner-fs**: File indexer
- **gvfsd**: Virtual filesystem daemons (per-scheme)

---

## ═══════════════════════════════════════════════════════════════
## SHELL FEATURES (What WuBuOS Win98 Shell Must Match/Exceed)
## ════════════════════════════════════════════════════════════════

### Window Management (Mutter)
| Feature | GNOME Implementation | WuBuOS Gap |
|---------|---------------------|------------|
| Dynamic workspaces | Auto-create/destroy, fixed grid optional | Static 4 workspaces |
| Window snapping | Edge tiling, quarter tiling, keyboard shortcuts | Basic move/resize only |
| Overview mode | Activities: windows + apps + search + workspaces | Start Menu only |
| Window animations | Genie/minimize, workspace switch, maximize | None |
| Touch gestures | 3-finger swipe (workspace), 4-finger (overview) | Not applicable |
| HiDPI fractional scaling | Per-monitor, Wayland native | Not implemented |
| VRR/HDR support | gamescope-style, KMS atomic | Missing |

### Top Bar / Panel
| Feature | GNOME | WuBuOS |
|---------|-------|--------|
| System status area | Network, Bluetooth, Power, Sound, Notifications | Basic tray only |
| Calendar/Notifications | Integrated, dismissible, actions | Missing |
| User menu | Power off, restart, suspend, settings, lock | Shutdown only |
| App indicators | StatusNotifierItem (KDE) + legacy tray | Partial |
| Clock/Calendar | World clocks, weather, events | Basic clock |

### Application Launching
| Feature | GNOME | WuBuOS |
|---------|-------|--------|
| App grid | Alphabetical, folders, search, frequent | Start Menu categories |
| Search | Tracker + app metadata + file content | .desktop parsing only |
| App folders | Drag-drop create, rename | Not implemented |
| Pin to dash | Persistent favorites | Taskbar pinning |
| .desktop handling | Full spec: MIME, actions, DBus activation | Basic Exec/Icon only |

### File Manager (Nautilus)
| Feature | GNOME | WuBuOS Explorer |
|---------|-------|-----------------|
| Views | List, Grid, Compact | Details/Icons/List ✅ |
| Search | Recursive, content (Tracker), filters | Name only |
| Tabs | Ctrl+T, drag-drop reorder | Not implemented |
| Bookmarks | Places sidebar, drag-drop | Favorites sidebar |
| Cloud providers | Nextcloud, Google Drive, SMB, SFTP (GVfs) | Local + Styx only |
| Batch rename | Pattern/regex/sequential | Missing |
| Undo/Redo | Full operation history | Missing |
| File operations | Progress dialog, conflict resolution, queue | Basic copy/move |
| Preview | Thumbnails, text, audio, video, PDF | Missing |

---

## ═══════════════════════════════════════════════════════════════
## PLATFORM SERVICES (What WuBuOS Lacks Entirely)
## ════════════════════════════════════════════════════════════════

### D-Bus (System + Session Bus)
- **Activation**: Service files → auto-start on method call
- **Introspection**: org.freedesktop.DBus.Introspectable
- **Properties**: org.freedesktop.DBus.Properties (Get/Set/GetAll)
- **Signals**: Broadcast, unicast, match rules
- **Policy**: /etc/dbus-1/system.d/, per-user .conf
- **WuBuOS**: Missing entirely → blocks: flatpak, systemd, NetworkManager, PipeWire, Polkit

### systemd --user
- **Units**: service, socket, timer, target, path, automount
- **Dependencies**: After, Before, Requires, Wants, Conflicts
- **Socket activation**: Listen on fd, pass to service
- **Timers**: Calendar events, monotonic, persistent
- **Environment**: Import from systemd, dbus, PAM
- **WuBuOS**: Missing → no service management, no socket activation, no timers

### Polkit (PolicyKit)
- **Actions**: org.freedesktop.* + app-specific .policy files
- **Rules**: JavaScript rules.d/ + static .pkla
- **Agents**: gnome-shell, polkit-gnome, polkit-kde
- **Authentication**: PAM, fingerprint, smartcard, FIDO2
- **WuBuOS**: Missing → no privilege escalation UI, no system config auth

### NetworkManager
- **Devices**: Ethernet, WiFi, WWAN, Bluetooth, VPN, WireGuard, VLAN, bond, bridge
- **Connections**: Profiles (keyfile, ifcfg, systemd-networkd), secrets in keyring
- **VPN**: OpenVPN, WireGuard, OpenConnect, strongSwan, libreswan (plugins)
- **DNS**: systemd-resolved integration, per-connection DNS, DNS-over-TLS/HTTPS
- **WuBuOS**: wubu_network.c has netlink bridge/macvlan/vxlan → 15% parity

### PipeWire (Audio/Video Graph)
- **Graph**: Nodes (sources/sinks/filters), ports, links, params
- **Session manager**: WirePlumber (Lua scripts for policy)
- **Bluetooth**: BlueZ 5 + PipeWire Bluetooth module (A2DP, HSP/HFP, LE Audio)
- **Pro Audio**: JACK API, low latency, graph rewire
- **Video**: V4L2, libcamera, screen capture (PipeWire WirePlumber portal)
- **WuBuOS**: Missing entirely → no audio subsystem

### GVfs (Virtual Filesystems)
- **Backends**: sftp, smb, dav, davs, ftp, ftps, mtp, gphoto2, afp, nfs, ssh, admin, trash, burn, recent, computer, network, google, onedrive, nextcloud
- **Mounting**: FUSE daemon per backend, automount on access
- **Metadata**: Custom attributes (emblems, tags, preview)
- **WuBuOS**: Styx 9P only → no cloud, no SMB, no SFTP, no trash

### Tracker (Search/Indexing)
- **Miners**: Files (recursive, content extraction), Emails, Contacts, Calendar
- **Extractors**: PDF, Office, Audio tags, EXIF, Video metadata
- **SPARQL**: RDF query over indexed metadata
- **WuBuOS**: Missing → no file content search, no metadata

### Flatpak (Sandboxed Apps)
- **Runtime**: Freedesktop SDK, GNOME Platform, KDE Platform, Elementary
- **Permissions**: Filesystem, network, devices, D-Bus, IPC, Wayland, X11
- **Portals**: File chooser, print, screenshot, screen cast, camera, location
- **Repositories**: Flathub, custom OCI registries
- **WuBuOS**: wubu_oci.c has registry ops → 20% parity (no sandbox, no portals)

---

## ═══════════════════════════════════════════════════════════════
## GTK4 / libadwaita WIDGET PARITY
## ═══════════════════════════════════════════════════════════════

| Widget | GNOME (GTK4/libadwaita) | WuBuOS Win98 |
|--------|------------------------|--------------|
| Button | GtkButton, AdwButtonContent | Custom |
| Entry | GtkEntry, AdwEntryRow | Custom |
| ComboBox | GtkDropDown, AdwComboRow | Custom |
| List/Grid | GtkListView, GtkGridView, GtkColumnView | Custom list view |
| Tree | GtkTreeView (deprecated) → GtkColumnView | Tree view ✅ |
| Notebook/Tabs | GtkNotebook, AdwTabView | Missing |
| Dialog | GtkDialog, AdwAlertDialog, AdwMessageDialog | Custom |
| Menu | GtkPopoverMenu, AdwActionRow | Context menu ✅ |
| Toolbar | GtkToolbar (deprecated) → GtkCenterBox | Titlebar + toolbar |
| Statusbar | GtkStatusbar | Taskbar status |
| Progress | GtkProgressBar, AdwProgressRow | Missing |
| Spinner | GtkSpinner | Missing |
| Switch | GtkSwitch, AdwSwitchRow | Checkbox only |
| Scale/Slider | GtkScale, AdwScaleRow | Missing |
| Color/Font | GtkColorDialog, GtkFontDialog | Missing |
| FileChooser | GtkFileDialog (portal) | Custom dialog |
| Print | GtkPrintOperation (portal) | Missing |
| Clipboard | GdkClipboard (multi-MIME, async) | wubu_clipboard.c ✅ |
| Drag/Drop | GdkDrag, GdkDrop (multi-MIME) | Explorer drag-drop ✅ |
| Shortcuts | GtkShortcutsWindow, GtkShortcutController | Missing |
| Tooltip | GtkTooltip, AdwTooltip | Missing |
| Popover | GtkPopover, AdwPopoverMenu | Custom |
| Expander | GtkExpander, AdwExpanderRow | Missing |
| Separator | GtkSeparator | Custom |
| Scroll | GtkScrolledWindow, kinetic scrolling | Custom |
| Overlay | GtkOverlay | Missing |
| Stack | GtkStack, AdwViewStack | Missing |
| Carousel | AdwCarousel | Missing |
| TabView | AdwTabView (draggable, pinned, audio) | Missing |
| Navigation | AdwNavigationPage, AdwNavigationView | Missing |
| ToolbarView | AdwToolbarView | Missing |
| Window | AdwApplicationWindow, GtkApplicationWindow | Custom X11/Wayland |
| HeaderBar | AdwHeaderBar, GtkHeaderBar | Win98 titlebar |
| Leaflet | AdwLeaflet (adaptive) | Not needed |
| Squeezer | AdwSqueezer | Not needed |
| Flap | AdwFlap (sidebar) | Explorer tree ✅ |
| SplitView | AdwSplitView | Not needed |
| TabBar | AdwTabBar | Taskbar |
| TabButton | AdwTabButton | Task button |
| WindowTitle | AdwWindowTitle | Titlebar text |
| ActionRow | AdwActionRow, AdwComboRow, AdwEntryRow | Custom |
| Preferences | AdwPreferencesWindow, AdwPreferencesGroup | Control panel missing |
| NavigationView | AdwNavigationView (back swipe) | Missing |
| TabOverview | AdwTabOverview | Missing |
| Toast | AdwToast, AdwToastOverlay | Missing |
| Banner | AdwBanner | Missing |
| StatusPage | AdwStatusPage | Missing |
| Clamp | AdwClamp (max width) | Not needed |
| Breakpoint | AdwBreakpoint (adaptive) | Not needed |

---

## ═══════════════════════════════════════════════════════════════
## ACCESSIBILITY (a11y) — GNOME STANDARD
## ════════════════════════════════════════════════════════════════

| Feature | GNOME | WuBuOS |
|---------|-------|--------|
| Screen reader | Orca (AT-SPI2) | Missing |
| Magnifier | GNOME Magnifier (Zoom) | Missing |
| On-screen keyboard | GNOME On-Screen Keyboard (Caribou) | Missing |
| High contrast | AdwStyleManager: high-contrast | Missing |
| Large text | Scaling factor + text-scaling-factor | Missing |
| Keyboard navigation | Full (Tab, arrows, mnemonics, shortcuts) | Partial |
| Focus indicators | Visible, configurable | Basic |
| Live regions | AT-SPI2 text-changed, object-state-changed | Missing |
| Braille | BrlAPI + Orca | Missing |

---

## ═══════════════════════════════════════════════════════════════
## DEVELOPER TOOLS ECOSYSTEM
## ════════════════════════════════════════════════════════════════

| Tool | Purpose | WuBuOS Equivalent |
|------|---------|-------------------|
| GNOME Builder | IDE (Flatpak, Meson, GTK, Vala, Python) | Missing |
| Devhelp | API documentation browser | Missing |
| Glade | UI designer (GTK4) | Missing |
| Sysprof | Profiler (CPU, GPU, energy) | Missing |
| GJS Console | JavaScript REPL for GNOME Shell | Missing |
| D-Feet | D-Bus debugger | Missing |
| GSettings Editor | dconf editor | Missing |
| GTK Inspector | Ctrl+Shift+D / Ctrl+Shift+I | Missing |
| Accessibility Inspector | Accerciser | Missing |

---

## ═══════════════════════════════════════════════════════════════
## KEY GAPS FOR WUBUOS (Triple DA Verdict)
## ════════════════════════════════════════════════════════════════

### MUST HAVE (Parity with modern desktop)
1. **D-Bus** — Required for: Flatpak portals, systemd, NetworkManager, PipeWire, Polkit, GVfs
2. **systemd --user** — Service management, socket activation, timers
3. **PipeWire + WirePlumber** — Audio graph, Bluetooth, screen capture, pro audio
4. **Polkit** — Privilege escalation for system settings
4. **NetworkManager** — WiFi, VPN, WWAN, DNS, connection profiles
5. **GVfs** — SMB, SFTP, cloud providers, trash, burn
6. **Flatpak + Portals** — Sandboxed apps, file chooser, print, screenshot
7. **Tracker** — File content search, metadata
8. **Accessibility (AT-SPI2)** — Screen reader, magnifier, OSK, high contrast

### SHOULD HAVE (Win98 shell completeness)
9. **Tabbed file explorer** — Ctrl+T, drag reorder
10. **Batch rename** — Pattern/regex/sequential
11. **File operation queue** — Progress, conflict resolution, pause/resume
12. **Thumbnail previews** — Images, video, audio, PDF
13. **Undo/Redo in explorer** — Full operation history
14. **App folders in Start Menu** — Drag-drop create/rename
15. **Search in Start Menu** — Tracker + app metadata + files
16. **Notifications center** — Dismissible, actions, history
17. **Calendar/World clock** — Events, timezones
18. **Multi-monitor HiDPI fractional scaling** — Per-monitor
19. **Touchpad gestures** — Workspace switch, overview
20. **Window snapping/tiling** — Edge, quarter, keyboard shortcuts

### NICE TO HAVE (Modern polish)
21. **Animations** — Genie minimize, workspace switch, maximize
22. **Dynamic workspaces** — Auto create/destroy
23. **Overview mode** — Windows + apps + search + workspaces
24. **VRR/HDR** — gamescope integration
25. **Screen recording** — PipeWire portal
26. **Remote desktop** — GNOME Remote Desktop (RDP/VNC)
27. **Fingerprint/FIDO2 auth** — PAM integration
28. **Power profiles** — Performance/balanced/power-saver
29. **Color management** — ICC profiles, calibration
30. **Thunderbolt/USB4** — Authorization, security levels

---

## ═══════════════════════════════════════════════════════════════
## IMPLEMENTATION PRIORITY FOR WUBUOS
## ════════════════════════════════════════════════════════════════

| Phase | Component | WuBuOS File | Est. Effort |
|-------|-----------|-------------|-------------|
| 1 | D-Bus daemon + libdbus | `runtime/wubu_dbus.c` | 3 sessions |
| 2 | systemd --user integration | `runtime/wubu_systemd.c` | 2 sessions |
| 3 | PipeWire + WirePlumber | `audio/wubu_audio.c` | 4 sessions |
| 4 | Polkit agent + backend | `apps/wubu_polkit.c` | 2 sessions |
| 5 | NetworkManager client | `wubu_network.c` (extend) | 3 sessions |
| 6 | GVfs backends (FUSE) | `runtime/wubu_gvfs.c` | 4 sessions |
| 7 | Flatpak + Portals | `runtime/wubu_flatpak.c` | 5 sessions |
| 8 | Tracker + SPARQL | `apps/wubu_tracker.c` | 3 sessions |
| 9 | AT-SPI2 accessibility | `gui/wubu_a11y.c` | 3 sessions |
| 10 | Explorer tabs + queue + thumbnails | `gui/dosgui_explorer.c` | 2 sessions |
| 11 | Start Menu folders + search | `gui/dosgui_startmenu.c` | 2 sessions |
| 12 | Notifications center | `gui/wubu_notifications.c` | 2 sessions |
| 13 | Multi-monitor HiDPI | `hosted/wubu_metal.c` | 3 sessions |
| 14 | Window snapping/tiling | `gui/dosgui_wm.c` | 2 sessions |
| 15 | Animations | `gui/dosgui_wm.c` | 2 sessions |

**Total estimated**: ~44 sessions for full GNOME parity