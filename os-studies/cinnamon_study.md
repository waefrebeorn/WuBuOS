# Cinnamon — Linux Mint Desktop Environment: Source Study

> Reference target for the WuBuOS desktop shell. Present-day Cinnamon 6.6.9 runs live on the WuBuOS dev host (Linux Mint 22.3, confirmed locally via `cinnamon --version` = 6.6.9 and `/etc/os-release` = `Linux Mint 22.3 (Zena)`) [local observation, unverified]. This study measures the current Win98/XP-style `dosgui` WM against Cinnamon's architecture and maps what to steal.

## What it is / architecture

- Cinnamon is a free/open-source desktop environment for Linux/Unix-like OSes, developed by the Linux Mint team, **forked from GNOME 3** but following **traditional desktop-metaphor conventions** [27]. It is written in **C (GTK), JavaScript, and Python**; the stable release cited is **6.6.9** (first release **20 December 2011**), licensed GPL-2.0 [27].
- History: after GNOME 3's April 2011 abandonment of the conventional desktop, Mint first tried "Mint GNOME Shell Extensions" (MGSE), then forked GNOME Shell as **"Project Cinnamon" in January 2012**; separation from GNOME was completed with **Cinnamon 2.0.0 on 9 October 2013**, after which Cinnamon-specific **applets, extensions, actions, and desklets are no longer compatible with GNOME Shell** [27].
- The cinnamon repo self-describes: *"A Linux desktop featuring a traditional layout, built from modern technology and introducing brand new innovative features"*; the **desktop layout is similar to GNOME 2 while the underlying technology is forked from the GNOME Shell** — GPL-2.0, 9,624 commits, 5.6k stars, 935 forks [28].
- **Muffin** is "the window management library for the Cinnamon desktop (**libmuffin**) and its sample WM binary (**muffin**)"; it is **a fork of GNOME's Mutter**, currently based on a **Mutter 3.36.0** rebase, written ~99.1% in C, with 103 contributors; its contributing page points upstream commits at `gitlab.gnome.org/GNOME/mutter` [29].
- Hybrid **C-core + JavaScript-UI** architecture: the C layer centers on a **`CinnamonGlobal` singleton**, plus **Muffin** (window management & compositing), **Clutter** (scene graph/animation library), **St widgets** (Shell Toolkit, a widget library on Clutter providing themeable components), and **`CinnamonPlugin`** (a Muffin plugin implementing the `MetaPlugin` interface that connects Cinnamon's UI to the WM) [31][30].
- The JS UI layer is entered at **`main.js`**, which drives **LayoutManager** (UI layout/screen organization), the **Panel System**, **WindowManager** (JS interface to Muffin), **Message Tray** (notifications), **Overview/Expo** (workspace management), and the **Extension Systems** (applets, desklets, extensions); UI components sit in a hierarchy under `LayoutManager`, rendered into `global.stage` (a Clutter Stage) [31][30].
- Startup sequence: `main.c` initializes the GObject system + DBus, creates the `CinnamonGlobal` singleton, creates/registers a Muffin plugin instance, initializes the JS environment; `CinnamonPlugin` then starts the JS runtime and evaluates **`main.js`**, whose `start()` initializes all managers and UI components [31][30][27]. Audited source files include `src/cinnamon-global.c`, `src/cinnamon-plugin.c`, `src/cinnamon-wm.c`, `src/main.c`, and the whole `src/st/` St widget tree [30][31].
- **Muffin is not just a Mutter WM clone — vendored Clutter and Cogl are forked with it.** The muffin repo carries entire top-level `clutter/` and `cogl/` source trees alongside `src/`, i.e. the full compositor graphics stack (scene graph + GL/graphics abstraction) stays in-house and is rebased onto upstream Mutter 3.36.0 (e.g. `Rebase onto mutter 3.36.0-0ubuntu0.20.04.1`) [29]. This is the model of a *self-contained* compositor team, and it is exactly WuBuOS's "self-contained C11" posture.
- Window management depth at the JS level (`windowManager.js`): the `WindowManager` class supplies window effects and Alt-Tab with multiple **app switcher styles (Classic, Coverflow, Timeline)**, workspace-switch effects, window tiling/snapping, minimize/map/close/change-size transitions, and keybinding handling [30]. Effects are per-operation and configurable via gsettings keys such as `desktop-effects-map`, `desktop-effects-close`, `desktop-effects-minimize`, `desktop-effects-change-size`, `desktop-effects-workspace` (options like Traditional/Fly/Fade/None) [30].
- `LayoutManager` owns chrome management, monitor tracking, hot corners, virtual keyboard, WM struts, stage input region, and actor z-order — work-area calculation and edge-flip/corner-action triggers [30]. The UI stage is a Clutter Stage (`global.stage`) onto which background, window group, desklet container, UI group, overlay group, and panels are stacked [31].
- Wayland is an active leg: muffin implements `xdg-toplevel-icon-v1` and `xapp-shell-v1` protocols and a new background daemon exporting a dmabuf for per-monitor wallpapers; main.js detects the backend at startup and adjusts/limits behavior and xlets per session type (X11 vs Wayland) [29][30]. Cinnamon is under continuous development — the repo changelog reaches 6.8.0 and muffin 6.8.1 (Nov 2026), while this dev host runs the shipped 6.6.9 [28][29][local].
- C↔JS bridging: `CinnamonGlobal` exposes C objects/functions (properties like `stage`, `display`, `window_manager`) to JS through **GObject Introspection**, and signals carry C→JS events; the build produces `libst.so`, `libcinnamon.so`, and the `cinnamon` executable via Meson [31][30][28].

### Panel, applet / desklet / extensions, and theming

- **Panel System**: a `PanelManager` singleton creates/destroys/positions panels, each `Panel` holding **left / center / right layout boxes** and being placeable at **top, bottom, left, or right** screen edges; panel definitions are stored in the GSettings key `enabled-panels` as `panelID:monitorIndex:position` (e.g. `1:0:top`), and applets are placed per `panel:location:order:uuid:applet_id` (e.g. `panel1:right:0:menu@cinnamon.org:0`) [32].
- **Applets** are panel widgets (menu, launchers, window-list, tray) loaded in-process by `AppletManager` and embedded directly into panel zones — i.e. the taskbar lives *inside* the compositor process, not as a separate app [32]. The window-list applet source is `files/usr/share/cinnamon/applets/window-list@cinnamon.org/applet.js` [32].
- **Extension ("xlet") framework**: a common base `Extension` class covers **applets** (panel widgets, `AppletManager`), **desklets** (desktop widgets, `DeskletManager`), **extensions** (system-wide behavior, `ExtensionSystem`), and **search providers** (`SearchProviderManager`); each loads `metadata.json`, runs load→enable→disable→unload lifecycle, and declares **roles** (e.g. `notifications`, `windowlist`, `tray`) that can be locked to prevent duplication; distribution is via the **Cinnamon Spices** website [34].
- **Theming** is CSS-based: a widget requests a theme node via `st_widget_get_theme_node()`, which resolves **style classes**, **pseudo-classes** (`:hover`, `:focus`, `:active`, `:checked`, `:rtl`/`:ltr`), **hierarchical parent selectors**, and **properties** against CSS; four theme components (GTK, icon, cursor, Cinnamon) are combined, with a fallback mechanism for broken themes [33].

### Cinnamon vs. MATE/GNOME (positioning)

- Cinnamon is often described as similar to Xfce, MATE, GNOME 2, and GNOME Flashback in its conservative/traditional design [27]. Unlike the MATE fork, Cinnamon does **not** preserve a GNOME-2 codebase's WM assumptions; it is its own stack of forked GNOME-3 components (Muffin=Mutter fork, St=Clutter fork) [27][29][31].

## Stealable techniques

1. **WM/compositor split as a pair of surfaces.** Muffin (Mutter fork) is the compositing window manager; the cinnamon UI is only a plugin (CinnamonPlugin/MetaPlugin) that consumes it [29][31]. *WuBuOS /n mapping:* `/n/wm/` and `/n/compositor/` as distinct control surfaces, so `dosgui`'s move/resize/decorate logic separates from the pixel-sourcing compositor. *Delta-by-design:* C11-native compositor (no Clutter/COGL/OpenGL; own software/vulkan compositor); no GTK dependency.
2. **Singleton "global" as the single C↔UI service bus.** `CinnamonGlobal` (singleton) hands the UI layer window mgmt, display, stage, settings (GSettings), DBus [30][31]. *WuBuOS /n mapping:* one authoritative service object exposing the same contracts as `/n/wm`, `/n/display`, `/n/ui/*` read-write files — the shell UI logic reads/writes those files instead of calling kernel functions directly. *Delta:* instead of a JS runtime (GJS), WuBuOS UI logic is self-contained C11 modules plus scriptable handlers that operate on the /n namespace; no GObject-Introspection.
3. **In-process panel/window-list/tray.** AppletManager loads applets (menu, window-list, tray) as live objects into the same compositor process's panel zones [32]. *WuBuOS /n mapping:* the taskbar/window-list is a surface `/n/ui/panel/<id>/zone` that the compositor renders; no separate taskbar process — matches Cinnamon, avoids RPC per keystroke. *Delta:* panel zones are /n files (left/center/right) that any process may edit; the compositor re-renders on file change.
4. **Config-as-truth in a schema keyed store.** Panel/applet layout lives in GSettings (`enabled-panels`, per-applet `panel:location:order:uuid:applet_id`) [32]. *WuBuOS /n mapping:* `/n/ui/panel/`: panel/enabled-panels as plain files (FS keyed list), each applet xlet a directory. *Delta:* GSettings/dconf replaced by our /n files — a property of the 9P control plane, no separate schema daemon.
5. **CSS-ish theme engine with node resolution.** A widget gets a resolved "theme node" computed from its type/style-class/pseudo-class/parent chain, applied live [33]. *WuBuOS /n mapping:* `/n/ui/theme/`: theme CSS file(s) + `st_widget_get_theme_node()`-equivalent resolver; widgets query `/n/ui/theme/<widget>` for their resolved style. *Delta:* our own mini-CSS interpreter in C11 (no libcroco/WebKit); themes are files under /n, rev-able and hot-swappable.
6. **Unified xlet framework with role locking.** Applets/desklets/extensions/search-providers share one lifecycle + metadata + role-lock system, so only one widget claims `windowlist` or `tray` at a time [34]. *WuBuOS /n mapping:* `/n/ui/xlet/<uuid>/` for each extension with metadata + enabled flag; `role` lock files (e.g. `/n/ui/xlet/roles/windowlist`). *Delta:* xlet code is C11/scripts mounted directly on /n; no Spices download portal initially — first ship a local registry directory.

## /n control-plane mapping

| Concern | Cinnamon source | WuBuOS /n surface |
|---|---|---|
| Compositing window manager | Muffin/Mutter fork, libmuffin [29] | `/n/compositor/` (stage/depth, surface tree) |
| Window management | Muffin + JS WindowManager [31] | `/n/wm/` (stack, workspaces, move/resize) |
| Global singleton service bus | CinnamonGlobal [30][31] | `/n/` root service contracts (wm/display/ui exposed as files) |
| UI layout / workspaces | LayoutManager, Overview/Expo [31] | `/n/ui/layout/` (workspaces, screens, hot corners) |
| Panel system | PanelManager, left/center/right zones [32] | `/n/ui/panel/<id>/` (positions, zones, autohide) |
| Taskbar / window list | window-list@cinnamon.org applet [32] | `/n/ui/panel/<id>/zone/windowlist` |
| Applets / desklets / extensions | xlet framework + role locks [34] | `/n/ui/xlet/<uuid>/` (metadata, enabled, roles) |
| Extension registry | Cinnamon Spices [34] | `/n/ui/spices/` (local registry directory) |
| CSS theming | St theme node resolver [33] | `/n/ui/theme/` (CSS file, per-widget resolved style) |
| Settings/configuration | GSettings/dconf keys [32][31] | `/n/ui/settings/` (files; panel/applet keyed lists) |

## P-level build spec (go heavy first)

- **P1 — Compositor core (C11, self-written):** own software/Vulkan compositor replacing the need for Clutter/COGL/Mutter; expose stage + depth + surface tree at `/n/compositor/`; render the existing `dosgui` windows as composited surfaces.
- **P2 — WM/SS split:** refactor `dosgui` into a WM (workspaces, stacking, focus, move/resize via `/n/wm/`) layered over the P1 compositor, mirroring Muffin/WM separation [31][29].
- **P3 — Global service bus + UI-logic layer:** a CinnamonGlobal-equivalent C11 singleton exposing wm/display/theme/settings services; shell UI logic (Win98/XP chrome) rewritten as C11 modules that read/write `/n/*` instead of calling kernel directly [30][31].
- **P4 — Panel system:** PanelManager-equivalent; panels on any edge with left/center/right zones; layout config stored in `/n/ui/panel/` files (the enabled-panels + `panel:location:order:uuid` keys mapped to FS) [32].
- **P5 — xlet framework + theme engine:** unified applet/desklet/extension lifecycle with role locks under `/n/ui/xlet/` [34], plus a CSS-ish `st_widget_get_theme_node()`-equivalent resolver under `/n/ui/theme/` [33]; land a window-list applet in-process as the first xlet [32].

## What could NOT be sourced (marked, not fabricated)

- The dev-host version/OS numbers (Cinnamon **6.6.9**, Linux Mint **22.3** "Zena") were read live from the local machine (`cinnamon --version`, `/etc/os-release`) [unverified]. The stable-6.6.9 figure does also appear in the Wikipedia infobox, but the 22.3/Zena pairing is local only.
- WuBuOS's own current `dosgui` window manager internals, and any dependency/Wayland-leg boundaries for the WuBuOS compositor, are internal design and were not derived from a cited source [unverified].
- The `/n`-mapping table and P-level ordering are WuBuOS engineering decisions (this team's "steal to map to /n" / "go heavy first" direction), not facts from Cinnamon — they are framed as such [unverified].

## Sources

[27] https://en.wikipedia.org/wiki/Cinnamon_(desktop_environment) — Wikipedia: Cinnamon (desktop environment)
[28] https://github.com/linuxmint/cinnamon — github.com/linuxmint/cinnamon readme
[29] https://github.com/linuxmint/muffin — github.com/linuxmint/muffin — libmuffin WM, Mutter fork
[30] https://deepwiki.com/linuxmint/cinnamon — DeepWiki Overview: linuxmint/cinnamon
[31] https://deepwiki.com/linuxmint/cinnamon/1.1-architecture-overview — DeepWiki: Architecture Overview
[32] https://deepwiki.com/linuxmint/cinnamon/2.3-panel-system — DeepWiki: Panel System
[33] https://deepwiki.com/linuxmint/cinnamon/2.4-theming-system — DeepWiki: Theming System
[34] https://deepwiki.com/linuxmint/cinnamon/4-extension-system — DeepWiki: Extension System
