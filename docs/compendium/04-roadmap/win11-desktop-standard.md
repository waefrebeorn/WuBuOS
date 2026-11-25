# Win11 desktop standard — the whole GUI, one owner

Status: **in progress** (started 2026-11-25). Goal, in the user's words: *"fix our
desktop to just copy win11… use .WET and WUA and standardize the whole WuBuOS
GUI."* This document is the contract; code that disagrees with it is the bug.

## The split (non-negotiable)

| Layer | Owner | What it may know |
|---|---|---|
| Compositor, WM, input routing, hit-testing | C11 (`src/gui/wm_*.c`, `wm_input.c`) | rects, focus, capture, scanout |
| Shell surfaces: taskbar, start menu, quick settings, notification centre, snap flyout, control centre | `.WET` applets (Lua on WUA) | layout and drawing ONLY |
| Application windows | `.WET` applets, or a C app that fills a `DosGuiWindow` | its own content rect |
| Numbers: spacing, radii, thresholds, type ramp | `include/wubu_gui_metrics.h` | nothing else may invent one |

The rule that makes this worth doing: **a control never positions itself.** A
panel asks each child for its desired size and hands out rects, so one script
reflows at any width and the same surface works on a 1024×768 VBE and a 4K
scanout. Hand-drawn chrome cannot do that without a rewrite.

**Why C keeps the WM:** a shell cannot own hit-testing or z-order. If the
taskbar were the thing deciding what is on top, a hung applet would freeze input
for the whole machine. The WM stays small, testable and free of Lua.

## Win11 facts this standard encodes

These are the behaviours users actually notice, each already measured or
implemented rather than remembered:

- **48 px taskbar, icons centred.** Win10 left-aligns; the centring is the change.
- **32 px control minimum, 40 px touch target** (`WUBU_METRIC_CONTROL_H`/`TOUCH_H`).
- **8 px window radius, 4 px control radius** (`RADIUS_WINDOW`/`RADIUS_CONTROL`).
- **Snap gap 8 px** (`WUBU_METRIC_SNAP_GAP`): two snapped halves read as two
  windows, not one seam.
- **Drag threshold 4 px** (`WUBU_METRIC_DRAG_THRESHOLD`, SM_CXDRAG): a press that
  moves less than this is still a click. The WM had NO threshold before, so every
  tremor during a press started a drag and windows "jumped".
- **Six snap layouts of two to four panes**, reached from the maximize button's
  hover affordance or **Win+Z**.
- **Win+Left / Win+Right** snap to halves; **Win+Up** maximizes; **Win+Down**
  restores a maximized window and minimizes a floating one.

Implemented and gated (2026-11-25): `wm_snap_half()` in
`src/gui/wm_window_state.c`, bound in `wm_input.c` on the Set-1 arrow scancodes
(Left 0x4B, Right 0x4D, Up 0x48, Down 0x50 — arrows carry no text, so they pass
through `input_scancode_to_key` unchanged). Gate: `src/gui/wm_snap_test.c` pins
the arithmetic `w = (screen_w - 3*gap)/2`, `right_x = screen_w - gap - w`, that a
snapped window is NOT flagged maximized (so Win+Down restores the floating rect,
not a half), that snapping a minimized window brings it back, and that no part of
a snapped window covers the taskbar.

## The migration order

Each step lands with a gate that loads the thing it claims to ship.

1. **Taskbar** — `src/apps/applets/win11_taskbar.wet` (DONE, gated by
   `test_wet_applet`: the shipped file must open through the applet host and
   build a tree with children).
2. **Start menu** — search box, pinned grid, "All apps", power row. `grid` panel
   for pins; the search field is a `textbox` whose `ontextchanged` filters.
3. **Quick settings** — volume/brightness sliders, wifi/bluetooth toggles, as a
   `stack` of `groupbox`es.
4. **Notification centre** — `listbox` of cards; clear-all is a button in the
   header row.
5. **Snap flyout** — the six layout miniatures; clicking a zone calls the same
   `wm_snap_half`-style geometry the keyboard uses. ONE geometry owner: the
   flyout must not compute halves itself.
6. **Control centre / settings panes** — after the shell, because they are apps.

## Rules for every surface

- **No magic numbers at a draw site.** Ask `wubu_gui_metrics.h`. A new number is
  a new token, in that header, with the Win11 value in a comment.
- **No hand-drawn chrome in C.** If it is a panel, it is a `.wet` applet.
- **Every interactive element has an `id`** so a gate can find it without
  coordinates: `wet_app_find(app, "Key2")` is how the calculator gate clicks.
- **A gate asserts a widget or a pixel, never "it did not crash".** Count
  children, read a label's text, count non-black pixels in the content rect.
- **A weak symbol, not a link error.** A shell-only capability called from the
  input path (`win11_snap_toggle`) is declared weak and NULL-checked, because the
  window and applet tests link the WM without the shell. `startmenu_db`'s applet
  accessors set this precedent.
- **A silent return is untestable.** The guests panel's click handler returned
  silently on five different conditions and cost an hour of bisecting a stale
  binary; it now reports which condition ended the click behind `WUBU_GP_DEBUG`.

## What is NOT yet Win11, stated plainly

- Snap **groups** (a snapped pair remembered as one unit, restored together from
  the taskbar) do not exist.
- Snap **assist** (after snapping, offer the other windows for the free zones)
  does not exist.
- Drag-to-edge snapping does not exist; only the keyboard and the flyout do. This is
  the next real gap, and the bar is Win11's behaviour: while dragging, hovering an
  edge must preview the zone, releasing must apply, and the gap between zones must
  stay draggable (Win11 gets the last one wrong).
- ~~The flyout draws six layouts and applies none of them.~~ **CLOSED**: the flyout
  applies the zone it shows (`wubu_snap_apply_layout_zone`, zones tile the work area
  exactly — the truncation dead-strip at 1024/3 is pinned by a test). What is left is
  the *keyboard*, which we now do BETTER than Windows 11: Win11 requires a mouse to
  choose a REGION, while here the arrows move the selection by real geometry
  (`wubu_snap_neighbor`), `1`-`6` switch layout, Enter applies, Esc closes. The
  selection logic is pure (`wubu_snap_key_select`) so it is gated rather than trapped
  inside the shell.
- Mica/acrylic translucency is a compositor effect (`/n/effect` stack), not a
  shell property; it is applied to the compositor's buffer, so a `.wet` surface
  gets it for free once the shell path is standardised.
