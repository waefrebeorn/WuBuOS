# Win11 gap index

*Built from the tiny11 24H2 (26100.1742) harvest in `docs/reference/win11/` and measured
against this tree on 2026-11-25. Every count below came from a `grep -rInE` over
`src/**/*.c` and `*.h`; the "we have" column is a file that exists and is named in a
build, not a file that might exist.*

The purpose is not "be Windows". It is: Windows 11 spent a decade deciding which shell
surfaces a desktop needs. That decision is a checklist we can measure ourselves against
instead of rediscovering each item after a user reports it.

## The index

| Win11 surface | Where it lives for us | Evidence / count | Verdict |
|---|---|---|---|
| Start menu | `startmenu*.c` (5 units: db, search, tree, power) | present, in `GUI_OBJS` | have |
| Taskbar | `wm_taskbar.c`, `taskbar_tasks.c` | floating bar, 48 px, 8 px gap | have |
| System tray | `wm_systray.c`, `wm_tray_world.c` | present | have |
| Notifications / toasts | `wubu_notify.c`, `wubu_osd.c` | 131 `toast|action_center` hits | have |
| Quick settings (Win11's Action Center) | `quick_settings.c` | present | have |
| Snap layouts | `win11_snap.c`, `wm_layout.c` | keyboard zone pick + half-open hit test | have (better) |
| Task view / Alt-Tab | `wm_overview.c` | present | have |
| Context menus | `wm_ctxmenu.c` + `wm_ctxmenu_engine.c` | New/Folder/Shortcut/Sort By wired | have |
| Window chrome | `window_chrome.c` + `wubu_icons_data.h` | icons now data, AA corners | have |
| Search | `startmenu_search.c`, `run_dialog.c` | present | have |
| Lock / sign-in | `wubu_lock_screen.c`, `wubu_shell_greeter.c` | present | have |
| Widgets | `win11_widgets.c` | present | have |
| Wallpapers | `wubu_wallpaper.c` | **1** bundled image; harvest holds **20** at 3840×2400 | thin |
| Cursors | 55 `cursor_theme|XCURSOR|xcb_cursor` hits | themed, not Windows-sized (harvest: 206 cursors, 24 px) | have |
| Fonts | `src/gui/fonts/wubu_font_text.h` (baked Segoe UI Variable 13 px) | 94 glyphs, 5,681 coverage bytes; AA through `vbe_draw_text()` | have (one size) |
| Taskbar jump lists | `taskbar_show_jump_list()` in `wm_ctxmenu.c` | Focus/Open, Close window, Pin/Unpin | partial (no recency) |
| Visual styles (.msstyles) | 0 hits | deliberate: our theme is `wubu_theme.c` | n/a |

## The two real gaps

### 1. Fonts: baked, one size (was the largest gap)

There is still no `.ttf` shipped at runtime, and that is deliberate: the face is baked
into a coverage table (`tools/font_bake.py` -> `src/gui/fonts/wubu_font_text.h`), so the
product carries 5,681 bytes of coverage instead of a font engine. Segoe UI Variable at
13 px, 94 glyphs, antialiased through `vbe_draw_text()`, with bytes outside the baked
range falling back to the old bitmap glyph per character.

What is still missing is the SIZE LADDER: Win11 renders 12/14/16/20 px for caption, body,
subtitle and title. One baked size means one text size on screen. Each role needs its own
bake (`--out src/gui/fonts/wubu_font_ui.h`, etc.) -- scaling a coverage mask reintroduces
the blockiness the bake exists to remove.

### 2. Jump lists: the menu exists, the recency does not

Right-clicking a taskbar button now opens a real menu (`taskbar_show_jump_list`):
Focus/Open, Close window, Pin/Unpin. What is still missing is the part that makes it a
*jump* list: Win11 lists the recent documents for that app. `taskbar_tasks.c` stores
`exec` and title only, and nothing records "these files were opened by this app".

The architecture is already there: `wubu_launch_*` sees every launch, and the task
registry is keyed by app. A bounded recency ring per exec (app key -> last N paths) plus a
"Recent" section in the menu is the whole feature -- and it needs a rule about privacy
before it ships, because a desktop that lists what you opened is a desktop that remembers.

## What the harvest gives that we have not consumed yet

- **20 wallpapers, 3840×2400** — we ship one. `WallpaperStyle=10` (fill) is recorded.
- **1,270 PNGs inside `aero.msstyles`** — the Win11 control bitmaps. We are drawing chrome
  ourselves on purpose; this is a reference for geometry, not assets to import.
- **206 cursors, 24 px** — our cursor set has no measured size parity.
- **`ColorizationColor=0xC40078D4`** — accent `#0078D4`, which our `THEME_WIN11` already
  uses.
- **`LogPixels=96`** — the DPI assumption to test against once fonts scale.

## Rule this page adds

A gap is only a gap if it was measured: *"we have X"* means a file that exists and is
named in a link graph, and *"we lack X"* means a search over `src/` that returned zero,
with the pattern shown. The first draft of this page reported five zeros that were all
false — `grep -E` with BRE `\|` alternation matches nothing, so every pattern looked
absent. The numbers were re-measured with real ERE alternation before this page was
written.
