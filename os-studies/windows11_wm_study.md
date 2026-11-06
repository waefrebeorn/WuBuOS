# Windows 11 Window Manager — Source Study (DWM / MIL compositor, Snap, Task View, taskbar)

Target design for the WuBuOS shell: replace the immediate-mode single-framebuffer dosgui WM with a
DWM-style retained compositor, and adopt Windows 11's Snap layouts/groups and virtual-desktop model.
Study grounded in Microsoft Learn, Microsoft Windows Learning Center, Microsoft Support, and Wikipedia.

## What it is / architecture  (authoritative facts)

- **Desktop composition** was introduced in Windows Vista: with it enabled, individual windows no
  longer draw directly to the screen or primary display device; instead their drawing is redirected
  to **off-screen surfaces in video memory**, which are then rendered into a desktop image and
  presented on the display. This is performed by the **Desktop Window Manager (DWM)**, which runs
  as a Windows service (enabled/disabled as "Desktop Window Manager Session Manager") [1].
- Before Vista, a program would write directly to the memory buffer shown by the video card; when
  windows overlap, both paint the same area of memory and a slow repaint of the covered window
  produces artifacts. Under DWM each window draws to an **offscreen surface** and DWM composites
  those surfaces to the screen [2].
- DWM's stated advantages over the old architecture: **fewer repaint messages** (an obstructed
  window never repaints itself), **reduced artifacts**, **visual effects** (translucency and blur,
  because DWM owns the composition), automatic scaling for high DPI, and **alternative views**
  (Flip 3D, live thumbnails, animated transitions) [2].
- DWM is a **compositing window manager**: each program holds a buffer it writes to; DWM composites
  each buffer into a final image. It uses **DirectX** to composite/render on the GPU, freeing the CPU
  of moving off-screen buffers to the display [3].
- The desktop itself is a **full-screen Direct3D surface**; windows are represented as a two-triangle
  mesh (a transformed 2D rectangle) with the UI chrome mapped as a texture, and window transitions
  are implemented as mesh transformations via shader programs [3].
- DWM uses the **Media Integration Layer (MIL)**, the unmanaged compositor it shares with
  **Windows Presentation Foundation (WPF)**, to represent windows as **composition nodes in a
  composition tree**; the tree represents the desktop and all windows hosted in it, rendered by MIL
  from the back of the scene to the front. Because all windows contribute to the final image, the
  color of a resultant pixel can be decided by multiple windows — enabling per-pixel transparency and
  blur (built-in Pixel Shader 2.0 programs average a window's pixel with the window behind it) [3].
- MIL is a **retained-mode** graphics system that **caches composition trees**, so repainting and
  refreshing the screen when windows move is handled by DWM/MIL, freeing applications of the
  responsibility. Background data already lives in the composition tree and off-screen buffers and is
  reused directly. In pre-Vista Windows, background apps had to be asked to re-render via the
  `WM_PAINT` message [3]. DWM uses **double-buffered** graphics to prevent flickering/tearing and
  composite optimizations such as back-face **culling** and skipping unchanged areas; it is
  multi-monitor aware [3].
- The off-screen buffers are kept live, so thumbnails/embedded views are **dynamic**, not static
  snapshots; DWM exposes APIs to get them at arbitrary scale, and this is how live taskbar/Alt-Tab
  previews and Flip work [3].
- **Windows 11 Snap layouts** arrange open apps into **neat, predefined patterns** (halves, thirds,
  quarters grids): press **Win+Z** to open the layout menu, or drag a window to the top of the
  screen on touch [4]. Layouts range from two windows side-by-side (each a half) or four quarters,
  to one-window-third + rest, configurable to the display [5]. Developer-facing, layouts are
  **tailored to current screen size and orientation** (three side-by-side on large landscape,
  top/bottom stack on portrait); if a window has a maximize button, hovering it auto-shows layouts [9].
- **Snap Assist**: after snapping the first window it shows all other open windows as thumbnails to
  pick the next; **Snap groups** are formed automatically when windows are snapped together, and
  hover the taskbar icon of any member restores the whole group with one click — no need to
  re-create the layout [4][5][10]. Snap groups also appear in **Task View and Alt+Tab** [10].
- **Snap restores across monitor reconnect**: plugging back into an external display, apps
  automatically return to their original snapped positions [4].
- Snap is configurable under **Settings > System > Multitasking** with a **Snap windows** toggle and
  per-behavior checkboxes (Snap Assist, the maximize-button flyout, the top-drag Snap bar, and
  showing snap groups on taskbar hover / Task View / Alt+Tab) [5][10].
- **Task View / virtual desktops**: Windows 11 lets you create separate desktops per project; the
  taskbar **Task View** button (two overlapping squares) or **Win+Tab** opens a layout of open apps
  with a "+ New Desktop"; **Win+Ctrl+D** creates a desktop, **Win+Ctrl+Left/Right** switches,
  **Win+Ctrl+F4** closes one (closed desktops' apps move to the next desktop) [6][7]. Each desktop
  can be **renamed and given a distinct wallpaper**, and Snap layouts can be applied per-desktop [6].
- Taskbar: in Windows 11 the taskbar is **centered by default** and can be re-aligned to the left via
  taskbar settings; it sits at the bottom of the screen [8].

## Stealable techniques  (fact -> /n mapping for WuBuOS -> our delta-by-design)

1. **Off-screen per-window surfaces + compositor owns the frame.**
   Fact: each window draws to its own offscreen surface in video memory; DWM composites to screen [1][2].
   /n: give every dosgui window a backing surface (`/n/compositor/w/<id>/surface`); the compositor
   blits surfaces in Z-order to the framebuffer once per frame instead of each window calling the
   framebuffer.
   Delta: retain the single framebuffer as the *composite target* (self-contained C11, no GPU Blt
   dependency yet), but refactor ownership so only the compositor writes it.

2. **Retained composition tree, not immediate redraw.**
   Fact: MIL caches a composition tree of nodes; repaint/refresh on move is handled by DWM/MIL, and
   obstructed windows send no repaint messages [2][3].
   /n: `/n/compositor/tree` — a retained node list (window id, geometry, surface, Z, visible); moving
   a window only mutates geometry nodes, never re-renders the windows.
   Delta: the cpu is the GPU today — our "compositor" is the blit loop keyed off the tree's dirty
   set (per-surface dirty rects + culling), matching MIL's "don't redraw unchanged areas".

3. **Dynamic live thumbnails from the same surfaces.**
   Fact: buffers stay live, so taskbar/Alt-Tab previews are dynamic, not snapshots [2][3].
   /n: `/n/compositor/thumb/<w>/<scale>` returns a scaled down-blit of the window's surface, live.
   Delta: thumbnails are just a scale-blt in software; requires surface memory be retained while
   minimized (DWM keeps the last buffer for minimized GDI windows [3]).

4. **Snap layouts as predefined grid templates.**
   Fact: Win+Z exposes predefined patterns (halve, third, quarter) tailored to screen size/orientation [4][5][9].
   /n: `/n/compositor/snap/layouts` lists templates; `/n/compositor/snap/layouts/apply` sets window
   rects from a chosen template; `/n/compositor/snap/layouts/<name>` to query/edit one.
   Delta: keep the layout table data-driven and resolution-aware (landscape/portrait variants), per [9].

5. **Snap assist fills the rest of the layout.**
   Fact: after the first snap, open windows appear as thumbnails to pick the remainder [4][5][10].
   /n: `/n/compositor/snap/assist/<layout>` returns candidate window ids; selecting one auto-positions it.
   Delta: reuses the live-thumbnail surface; assist is a picker over the existing window list, not new state.

6. **Snap groups persist + restore the whole snapped set.**
   Fact: snapping several windows forms a group; hovering any member restores the entire layout [4][5][10].
   /n: `/n/compositor/snap/groups/<id>` (member window ids + layout); `/n/compositor/snap/groups/<id>/restore`.
   Delta: groups are named, first-class /n objects, so they can persist across sessions and monitor
   reconnect [4] — stronger than Windows' implicit group.

7. **Virtual desktops as renamable workspaces with per-desktop looks.**
   Fact: desktops created via Task View/Win+Tab or Win+Ctrl+D, switched with Win+Ctrl+arrows, closed
   with Win+Ctrl+F4 (apps migrate), each renameable with its own wallpaper [6][7].
   /n: extend the existing WuBuOS desktops (`src/gui/wubu_wm_desktop.c`:
   `g_wm.desktops.count/current`, `wubu_wm_desktop_switch`) with
   `/n/wm/desktops/<i>/{create,close,rename,wallpaper,switch}`; closing migrates windows to the next desktop [7].
   Delta: WuBuOS already owns a virtual-desktop model; fold per-window Snap groups *into* desktop
   instances so a desktop's snapped layout restores on switch (per [6] "use Snap layouts on each one").

8. **Lifecycle: retained-mode frees the app from WM_PAINT.**
   Fact: pre-Vista background apps were driven by WM_PAINT; DWM/MIL own repaint via cached buffers [3].
   /n: `/n/wm/window/<id}/{geometry,visibility,z}` set operations that never re-invoke the app's
   draw; apps draw to their surface on their own schedule.
   Delta: this is the core inversion — window *geometry* becomes state the compositor owns, and the
   "draw" is a surface update, matching DWM [3].

9. **Centered taskbar + pinned launches as default furniture.**
   Fact: the Windows 11 taskbar is centered by default and re-alignable; it hosts Start/Search/Task
   View/Widgets/Chat plus pinned app links [8].
   /n: `/n/wm/taskbar/{align,pins}` config mirrors the centered-by-default model.
   Delta: skin-friendly taskbar config, not a hard-coded layout.

## /n control-plane mapping

| Concern | Source | /n surface |
|---------|--------|-----------|
| Per-window offscreen surface | [1][2] | `/n/compositor/w/<id>/surface` (frame, dirty rects) |
| Retained composition tree | [2][3] | `/n/compositor/tree` (node = win id, geom, surface, z, visible) |
| Compositor owns repaint / culling | [3] | `/n/compositor/frame` (dirty-set blit, skip unchanged) |
| Live dynamic thumbnails | [2][3] | `/n/compositor/thumb/<w>/<scale>` |
| Snap layout templates | [4][5][9] | `/n/compositor/snap/layouts` (+/apply, resolution-aware) |
| Snap assist picker | [4][5][10] | `/n/compositor/snap/assist/<layout>` |
| Snap groups (persist/restore) | [4][5][10] | `/n/compositor/snap/groups/<id>` (+/restore) |
| Monitor-reconnect restore | [4] | `/n/compositor/snap/groups/<id>/relayout` on uevent |
| Virtual desktops | [6][7] | `src/gui/wubu_wm_desktop.c` -> `/n/wm/desktops/<i>/{create,close,rename,wallpaper,switch}` |
| Desktop close migrates windows | [7] | `/n/wm/desktops/<i>/close` (migrate to next) |
| Taskbar alignment / pins | [8] | `/n/wm/taskbar/{align,pins}` |

## P-level build spec (go heavy first)

- **P1 — Surface abstraction:** give every dosgui window its own offscreen surface + dirty rects; only
  the compositor blits to the framebuffer; introduce `/n/compositor/w/<id>/surface`. [1][2]
- **P2 — Retained composition tree:** build `/n/compositor/tree` (geom/z/visibility as state, no
  app-repaint on move), dirty-set + culling blit loop. [2][3]
- **P3 — Snap layouts + Snap assist:** data-driven, resolution-aware templates (`/n/compositor/snap/layouts`),
  Win+Z menu, top-drag Snap bar, assist picker. [4][5][9]
- **P4 — Snap groups + restore:** `/n/compositor/snap/groups/<id>/restore`, taskbar hover restore,
  monitor-reconnect relayout. [4][10]
- **P5 — Virtual-desktop + taskbar integration:** migrate `wubu_wm_desktop.c` under `/n/wm/desktops`
  with rename/wallpaper per desktop and per-desktop snap layouts; centered taskbar config. [6][7][8]

## Sources

[1] https://learn.microsoft.com/en-us/windows/win32/dwm/dwm-overview — DWM overview - Microsoft Learn Win32
[2] https://learn.microsoft.com/en-us/windows/win32/learnwin32/the-desktop-window-manager — The Desktop Window Manager - Learn Win32
[3] https://en.wikipedia.org/wiki/Desktop_Window_Manager — Desktop Window Manager - Wikipedia
[4] https://www.microsoft.com/en-us/windows/learning-center/organize-desktop-snap-layouts — Organize desktop with Snap layouts - Microsoft
[5] https://www.microsoft.com/en-us/windows/learning-center/organize-screen-with-snap-layouts — How to use Snap layouts - Microsoft
[6] https://www.microsoft.com/en-us/windows/learning-center/stay-organized-using-multiple-desktops-windows-11 — Multiple desktops / Task View - Microsoft
[7] https://learn.microsoft.com/en-us/answers/questions/2153315/how-do-i-enable-windows-11s-virtual-desktop-featur — Virtual Desktop feature - Microsoft Q&A
[8] https://support.microsoft.com/help/17203 — Customize the Taskbar in Windows - MS Support
[9] https://learn.microsoft.com/en-us/windows/apps/desktop/modernize/ui/apply-snap-layout-menu — Support snap layouts for desktop apps - Learn
[10] https://support.microsoft.com/en-us/windows/experience/snap-your-windows — Snap Your Windows - MS Support
