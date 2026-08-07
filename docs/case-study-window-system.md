# Case Study: The WuBuOS Window — How It Works, and How Windows Get Updated

> Grounded in the code as of 2026-08-07 (post desktop-graphics overhaul).
> The journey from "fix the buttons" to "the window is a system" — this
> document is the windowing architecture, the draw/input/state pipelines,
> and the *update* model: the difference between a desktop that redraws
> everything every frame and a desktop where windows update on demand.

---

## 1. The window object

A window in WuBuOS is a `DosGuiWindow` (`src/gui/dosgui_wm.h:41-61`):

```c
typedef struct DosGuiWindow {
    int            id;                /* unique handle */
    DosGuiWinFlags flags;             /* NORMAL/FOCUSED/MINIMIZED/MAXIMIZED */
    int            x, y, w, h;        /* geometry (screen space) */
    int            min_x, min_y, min_w, min_h; /* saved pre-maximize */
    int            desktop;           /* virtual desktop index */
    char           title[64];
    bool           alive;             /* slot liveness */
    bool           is_modal;
    DosGuiWindow  *parent;            /* modal parent link */
    void         (*on_draw)(DosGuiWindow *, uint32_t *fb, int w, int h);
    void         (*on_key)(DosGuiWindow *, uint32_t key, uint32_t mods);
    void         (*on_mouse)(DosGuiWindow *, int x, int y, int btn, int kind);
    void         (*on_resize)(DosGuiWindow *, int w, int h);
    void          *user_data;
    uint32_t      icon_color;
} DosGuiWindow;
```

Every concept real windowing systems have, in 21 fields:

| Concept | WuBuOS | X11 | Win32 | Classic Mac OS |
|---|---|---|---|---|
| Identity | `id` | `Window` | `HWND` | `WindowPtr` |
| Geometry | `x,y,w,h` + clamps | geometry | `GetWindowRect` | portRect |
| State bits | `flags` | attributes | styles | `visRgn` / hidden |
| Save-undo for maximize | `min_*` | — (WM does it) | `placement` | `stdState` |
| Content paint | `on_draw` callback | `Expose` events | `WM_PAINT` | `updateRgn` |
| Input | `on_key`/`on_mouse` | event queue | `WM_KEY*`/`WM_MOUSE*` | events |
| User baggage | `user_data` | `XSetUserData` | `GWLP_USERDATA` | refCon |
| Relationship | `parent` (modal) | parent/transient | owner | — |

The window is **not heap-allocated**: it lives in a fixed 32-slot array
(`g_dwm.windows[DOSGUI_MAX_WINDOWS]`, `dosgui_wm_internal.h:44`), slots are
recycled via `alive`, and Z-order is a separate index array
(`g_dwm.zorder[]`, bottom..top). This is the TempleOS/ZealOS heritage — a
bounded arena, not a linked list. It makes the WM allocation-free and
deterministic; the cost is a hard 32-window ceiling and the "slot"
rather than "object" mental model.

## 2. Lifecycle

- **Spawn** (`spawn_window`, `dosgui_wm_window.c:32`): first free slot →
  `memset` → assign `id` (monotonic), geometry, title, `alive=true`, push
  onto `zorder`, make it focused.
- **Raise** (`raise_win:23`): move the index to the top of `zorder` (O(n)
  shift, n ≤ 32).
- **Close** (`close_win:53`): `alive=false`, remove from `zorder`, clear
  drag/focus, and focus falls back to the new top (or -1). **No window
  destruction callback** — apps detect death by polling `alive` or via the
  taskbar; there is no `on_close`/WM_DELETE_WINDOW equivalent yet.
- **Focus** is a single `focused_id`; hitting any window raises it (input
  routing does this, `dosgui_wm_input.c`).

Key structural note: `hit_test` (`dosgui_wm_window.c:67`) walks `zorder`
top-down and returns the first window containing the point — this single
function is the backbone of BOTH input routing and (implicitly) painting
order. There is no occlusion computation; everything above draws over
everything below.

## 3. The render pipeline — immediate mode, full redraw

`dosgui_wm_render(fb, w, h)` (`dosgui_wm_render.c:50`) is the **single**
entry point and redraws the entire screen every frame:

```
1. advance anim clocks (buddy tick, human typer)
2. draw desktop background (wallpaper mode)
3. draw icons (+ selection plates, glyphs)
4. for each live window in zorder (bottom→top):
     a. dosgui_chrome_draw_window(...)   → chrome (frame/title/buttons)
     b. vbe_set_clip(content rect)       → on_draw content, clip-reset
5. taskbar (+ focused button, Start flag, clock well)
6. notification center (if open)
7. clock popup (if open)
8. a11y cluster (on the focused window)  — no panel, floats on the window
9. WuBu Buddy (mascot + bubble)
10. eased cursor
```

Per-window drawing (`draw_window:32`) is two layers: **chrome** (the
standardized `dosgui_chrome_draw_window`, `dosgui_window_chrome.c` — frame
bevels, navy/theme title bar, drawn black glyph buttons, title text) and
**content** (`on_draw`, clipped to the content rect so a buggy app can never
spill outside its frame — the scissor/clip discipline, `vbe_set_clip`).

Everything goes into the VBE **backbuffer** (`g_vbe.back`); `vbe_swap()`
copies to the front buffer. The whole frame is rebuilt from nothing, every
frame, at whatever rate the host loop drives (headless harness, hosted
Wayland/SHM, tests).

### What this model is good at
- **Trivial correctness**: no invalidation bookkeeping, no stale pixels,
  no expose bugs. Occlusion, damage, ordering — all free.
- **Deterministic tests**: `vbe_init` → render → `vbe_get_pixel` assertions.
- **Simple compositing**: translucent elements (panel shadow, lasso, buddy
  bubble) are just blended draws in paint order.

### What it costs
- **Every frame re-paints everything**: hidden windows' `on_draw` runs even
  when fully covered; the wallpaper re-stretches; chrome re-bevels; glyphs
  re-draw. On a 1024×768 software framebuffer that is ~0.8M pixel writes per
  frame minimum — fine for a demo, wasteful for real hardware.
- **No per-window identity in the draw loop**: a window cannot say "only my
  text changed"; the WM cannot say "only the region under this rect changed".
- **Animation forces full redraw** even when nothing else moved (the buddy
  bob, the cursor easing, the clock seconds — all full-frame).

The code even documents this history: *"the previous design had THREE render
entry points … plus a dead dirty-region tracking system that nothing
consumed (the hosted loop redraws every frame). All of that is collapsed
here … the dirty tracking is gone."* (`dosgui_wm_render.c:8-13`). The
immediate-mode collapse was the right call *then*; the case study below is
how to do damage tracking that is actually consumed, this time.

## 4. The input pipeline — a priority cascade, then the window

`dosgui_wm_handle_mouse(x, y, btn, kind)` (`dosgui_wm_input.c`) routes one
event through a strict order:

```
1. if start menu open            → startmenu owns clicks (+ swallow release)
2. if clock menu open            → click outside popup closes it
3. if a11y enabled + focused win → a11y cluster (pill/orbs/grip) first
4. taskbar region                → Start btn / window btns / pager /
                                   systray / notif center / clock well
5. WuBu Buddy                    → grab-state mascot hit
6. window hit_test (top-down)    → chrome buttons (close/max/min)
                                   → edge resize zones (8 handles)
                                   → title bar drag (move w/ GAAD snap)
                                   → client on_mouse
7. desktop                       → icon hit → icon drag
                                   → empty → lasso rubber-band select
```

Key design points:
- **One event, one owner** — the cascade returns after the first hit; the
  taskbar region is checked *before* windows so Start/clocks can't be
  swallowed by a maximized window.
- **Chrome vs content split**: `dosgui_chrome_hit_test_button` handles
  close/max/min; `hit_test_edge` (8-way border mask) handles resize before
  the title-bar drag; otherwise the event goes to `on_mouse` — apps never
  see chrome clicks.
- **Drag/resize are WM-owned**: `drag_id` + offsets and `resize_id` +
  `resize_edge`/`resize_ow/oh` in the global state; moves clamp to the
  screen and above the taskbar, maximize restores from `min_*`.
- **Human-lag layer** (added 2026-08-07): `mouse_x/y` = instant input
  target for logic; `cursor_x/y` = eased render position (×0.30/frame
  exponential approach); `dosgui_wm_typer_start/tick` delivers keystrokes
  with human 28-51ms jitter + 120-220ms reaction delay.

Key dispatch: `dosgui_wm_handle_key` — Alt+Tab cycling, Win hotkeys, theme
cycling, virtual-desktop switching, then the focused window's `on_key`.

## 5. State transitions

`dosgui_wm_window_state.c` is pure geometry logic:

- `resize` clamps to [100, screen_w] × [50, screen_h-taskbar] and fires
  `on_resize` (apps relayout).
- `move` clamps into the usable screen (never under the taskbar).
- `maximize` saves `min_*`, expands to full usable screen, sets flag, fires
  `on_resize`.
- `minimize` sets a flag; `restore` undoes either, refiring `on_resize`.

No repaint scheduling — a state change just mutates fields; the next full
render picks it up. (Contrast: Win32 sends `WM_SIZE`/`WM_MOVE` and the app
invalidates; here the WM *is* the loop.)

## 6. The update model — "windows can be updated"

The phrase to hold on to: today the desktop **redraws**; the upgrade path is
a desktop that **updates**. Real systems do this with three cooperating
mechanisms, and each has a direct WuBuOS analog:

### 6.1 Invalidation (the window says *what* changed)
- **X11**: `Expose` events on damage; apps repaint the exposed region.
- **Win32**: `WM_PAINT` + `GetUpdateRect`; `InvalidateRect` marks damage.
- **Classic Mac**: the `updateRgn` / `BeginUpdate`/`EndUpdate` pair — the
  Window Manager tells the app exactly which region to repaint.
- **WuBuOS (proposal)**: `dosgui_wm_invalidate(DosGuiWindow *w)` sets a
  per-window dirty flag; `dosgui_wm_invalidate_rect(w, x, y, w, h)` records
  damage into a small rect list. Apps call these instead of nothing; the WM
  aggregates.

The buddy's bubble, the clock seconds, the lasso, the cursor — these are
**WM-owned dirty regions** the WM can mark itself (e.g.
`dosgui_wm_invalidate_rect` for the clock well only when the second
changes).

### 6.2 Damage aggregation + partial present (the WM says *where*)
- **Wayland**: `wl_surface.damage` — the compositor only repaints the
  damaged rectangles of the surface's buffer.
- **X11**: `XClearArea`/`XDamage` + the server's backing store.
- **WuBuOS (proposal)**: keep the full-frame software compositor, but
  compute a **damage union** (a bounded list of up to N rects, or a
  coarse grid of tiles, e.g. 32×32) during a frame; `vbe_swap` becomes
  `vbe_present(damage)` that the hosted backends consume — the Wayland/SHM
  backend already exists, it just needs to pass `wl_surface.damage` for the
  dirty tiles instead of the whole buffer.

This is the piece that was removed in the render collapse — the lesson is
to rebuild it as *consumed* state: the WM marks damage, and the host loop
(either the headless harness or hosted.c) reads `dosgui_wm_take_damage()`
and skips the present entirely when nothing is dirty and nothing is
animating.

### 6.3 Backing store / occlusion (windows that aren't painted)
- **Win98**: each window's content is owned by the app; the system
  repaints exposed areas (composited desktop arrived with XP).
- **Classic Mac**: the WMG always kept the full window image (backing
  store) so window drags were cheap; the app only painted on updateRgn.
- **X11**: optional server backing store; modern compositors keep
  redirected surfaces.
- **WuBuOS (proposal)**: render each window's chrome+content into its own
  small offscreen buffer (its `DosGuiWindow` gains a `pixels` +
  `dirty` + `w/h`). Then the compositor's paint loop becomes:
  - `if (win->dirty) redraw chrome+content into win->pixels` (clipped),
  - then **blit** win->pixels at (x,y) — hidden windows are skipped
    entirely, and drag/resize is a pure blit of a cached image (the
    Classic Mac drag model), with a live repaint only on release.
  This is the single biggest win: covered windows cost 0 paint, and
  window dragging becomes a memcpy per frame instead of a full redraw.

### 6.4 Render-on-demand (the loop decides *when*)
Today the hosted loop redraws unconditionally. With damage tracking the
loop becomes:
```
for (;;) {
    pump input;
    if (events_pending || wm_has_damage() || buddy_animating || cursor_moving)
        dosgui_wm_render(...);   // full frame, but cheap: only dirty
                                  // windows repaint content, rest blit
    present(damage_rects);
}
```
Idle cost drops to ~0 (a 0.8M-pixel memcpy becomes nothing between
interactions) — which matters for the "runs on all computers" goal from the
framebuffer work, and for battery-driven targets.

## 7. Roadmap (phases, each independently shippable + testable)

| Phase | Change | Files | Test |
|---|---|---|---|
| **P0** (now) | immediate-mode full redraw, as-is | — | 33/33 wm tests + desktop_shot audit |
| **P1** | `dosgui_wm_invalidate(w)` / `invalidate_rect`; WM marks its own dirty regions (clock, bubble, lasso, cursor); `dosgui_wm_has_damage()` + `take_damage()` consumed by the harness/hosted loop | dosgui_wm_internal.h, dosgui_wm.c, render.c, hosted_run.c | new test: invalidate → has_damage true → render clears it |
| **P2** | **Chrome cache**: render chrome to a per-window 16-bit tile once per state change; blit + only draw content live | dosgui_window_chrome.c, wubu_bonzi.c (unrelated) | pixel test: chrome identical via cache |
| **P3** | **Per-window backing store**: `win->pixels` offscreen buffer; blit-based compositing; hidden windows skip paint; drag = cached blit | dosgui_wm_render.c, dosgui_wm.h | occlusion test: covered window's on_draw NOT called |
| **P4** | **Damage rects + partial present**: `wl_surface.damage` on the Wayland/SHM backend, `vbe_present(dirty)` | vbe.c/h, hosted_wayland_shm.c | frame-equality test: damaged region only |
| **P5** | **Render-on-demand** loop; idle ~0 CPU | hosted.c, desktop_shot.c | idle test: no render without input/anim |

Each phase keeps the full-frame software renderer as the fallback (P0
behavior) behind a `WM_DIRTY_REDRAW` flag, so a regression anywhere just
flips back to "redraw everything" — the two models are complementary, not
exclusive. The immediate-mode path is also the *verification oracle*: the
dirty path must produce pixel-identical frames, which is exactly what the
existing `vbe_init/vbe_get_pixel` render tests can assert.

## 8. Lessons from the systems we studied (the "case" in this case study)

- **dwm / xfdesktop (X11)**: exposure-driven repaint + tiny WM core. The
  lesson: the WM's job is *ordering and routing* (which we have), and the
  repaint problem is delegated to damage events (which we should add).
- **Classic Mac OS / System 7**: the window manager OWNS the window image
  (backing store) and only tells the app to draw the *update region*. This
  is the direct ancestor of P3 and the reason drags feel instant there.
- **Win98**: `WM_PAINT` + invalidate-rect + taskbar button = window state
  mirror. We already mirrored the *look* (chrome, Start flag, clock well +
  its own menu, focused taskbar button); the *mechanism* (invalidate) is
  the remaining gap.
- **Wayland**: damage lists are a first-class protocol concept; our SHM
  backend already has the double buffer — it just needs the damage payload.

---

### TL;DR

The WuBuOS window is a compact, arena-backed object with clean geometry,
a priority-cascade input router, and an immediate-mode compositor that is
**correct by construction**. The upgrade from "redraw" to "update" is a
four-part mechanism — invalidation (P1), chrome cache (P2), per-window
backing store (P3), damage+partial present (P4) — each with the full-frame
renderer kept as both fallback and oracle. Nothing in the current design
blocks it: the arena windows, the zorder array, the clip discipline and the
VBE backbuffer are exactly the seams the damage model hangs off.
