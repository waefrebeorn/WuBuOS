# Win11 visual parity: the numbers, and where we actually differ

Complaint that started this: the desktop is "a bootleg visually". This note replaces opinion with
Microsoft's published numbers and then says which of them we already meet. Two of the four areas
are already correct, which is worth knowing before anyone "fixes" them again.

## Sources

- Title bar anatomy — <https://learn.microsoft.com/en-us/windows/apps/design/basics/titlebar-design>
- Corner radius / Windows 11 geometry —
  <https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/geometry>
- `DWM_SYSTEMBACKDROP_TYPE` (backdrop materials) —
  <https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ne-dwmapi-dwm_systembackdrop_type>
- Typography in Windows (Segoe UI Variable) —
  <https://learn.microsoft.com/en-us/windows/apps/design/signature-experiences/typography>
- Selawik (open replacement for Segoe UI, SIL OFL 1.1) —
  <https://learn.microsoft.com/en-us/typography/font-list/selawik> and
  <https://github.com/microsoft/Selawik>

## What the sources state

| area | value | source |
|------|-------|--------|
| title bar height | 32 px | titlebar-design |
| window icon | 16×16 px, 16 px from the leading edge, vertically centred (8 px margins at 32 px) | titlebar-design |
| title text | 16 px from icon/back button, "caption" type ramp, ellipsised | titlebar-design |
| corner radius, top-level window / flyout / dialog | 8 px | geometry |
| corner radius, in-page control (button, checkbox, list backplate) | 4 px | geometry |
| corner radius, snapped or maximised window | 0 px | geometry |
| backdrop | `DWMSBT_NONE`=1, `DWMSBT_MAINWINDOW` (Mica)=2, `DWMSBT_TRANSIENTWINDOW` (Acrylic)=3, `DWMSBT_TABBED` (Mica Alt)=4 | dwmapi |
| system font | Segoe UI Variable, axes `wght` 100–700 and optical size | typography |
| open substitute | Selawik — "an open source replacement for Segoe UI", SIL OFL 1.1, weights Regular/Bold/Light/Semibold/Semilight | font-list/selawik |

## What we already meet

Measured in the tree, not assumed:

- `include/wubu_gui_metrics.h`: `WUBU_METRIC_TITLEBAR_H 32`, `WUBU_METRIC_TASKBAR_H 48`,
  `WUBU_METRIC_RADIUS_WINDOW 8`, `WUBU_METRIC_RADIUS_CONTROL 4`, `WUBU_METRIC_CONTROL_H 32`,
  `WUBU_METRIC_TOUCH_H 40`, drag threshold 4 (`SM_CXDRAG`), snap gap 8.
- `src/gui/wm_render.c` masks window corners with `WUBU_METRIC_RADIUS_WINDOW` and skips maximised
  windows — the 8 px / 0 px rule.
- `src/gui/wubu_theme.h` already names the caption close hover `#C42B1C`.

So "make the corners 8 px" is not an open task. It is done, and a session that "fixes" it again
will be undoing correct work.

## Where we actually differ: there is no proportional font at all

- Text is rendered from a compiled bitmap font (`src/gui/fonts/wubu_font_text_bold.h`: 13 px
  cell, ascent 15, descent 4, line height 19). There is no outline font path anywhere.
- `src/gui/wubu_settings_defaults.c` defaults `font_family` to `"DejaVu Sans"`, and
  `grep -rn font_family src/gui/ | grep -v settings` is **empty**: nothing reads it. The setting
  is stored and never consumed, so it cannot change a single pixel.
- Consequence: matching Win11 typography is impossible at the metric layer. It needs a TrueType
  rasteriser plus a licensed-for-redistribution face. Selawik is the face; the rasteriser is ours
  to write (the tree's standing rule is slerm, not vendor).

## Why this is the real "bootleg" cause

Metrics are geometry: a rounded rectangle at 8 px is a rounded rectangle at 8 px. Type is not.
A 13 px bitmap cell cannot reproduce Segoe UI Variable's optical sizing or its hinting at small
sizes, so window titles, menus and dialog text read as "some retro OS" no matter how correct the
chrome around them is.

## Open work this implies

1. A TrueType rasteriser (SLERM: our own implementation; oracle = FreeType/PIL rendering the same
   glyph, compared pixel-wise with a stated tolerance).
2. Selawik (OFL 1.1) as the shipped UI face once the rasteriser exists; Cozette can remain for
   the terminal where a bitmap is correct.
3. `font_family` must either drive the renderer or stop being written to the config. A setting
   with no consumer is the same class of defect as the theme that claimed an 8 px radius and drew
   square windows.
