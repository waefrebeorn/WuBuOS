#!/usr/bin/env python3
"""parity_audit.py -- measure Win11 parity against the source, not against memory.

Every row names a user-facing action and the CODE that must exist for it. The verdict is
about wiring, not about behaviour:

  WIRED   a matching handler/symbol exists -- file:line is printed so it can be read
  GAP     nothing in the tree matches any of the row's markers
  ?       markers matched something that looks like a declaration only

Behaviour is proven by the test suite; this tool exists so a gap cannot hide behind a
doc claim, and so the same list can be re-run after every parity batch.

Usage: python3 tools/parity_audit.py [--json PATH] [--only shortcuts|mouse|explorer|bars]
"""

import argparse
import json
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCAN_DIRS = ["src/gui", "src/apps"]


def files(product_only=True):
    """PRODUCT code only, by default.

    First version scanned everything and produced confident nonsense: "Win+P" was found in
    a curve25519 file (the letter p in a regex), "taskbar hover thumbnail" in a codec
    (the word thumbnail), "drag file onto a window" in a package-manager test. A verdict
    of WIRED has to mean the product implements it, so *_test.c and *_stub.c are excluded
    from evidence; they are where behaviour is asserted, not where it is wired.
    """
    out = []
    for d in SCAN_DIRS:
        for base, dirs, names in os.walk(os.path.join(ROOT, d)):
            dirs[:] = [x for x in dirs if x not in (".git", "build") and not x.startswith("build")]
            for n in names:
                if not n.endswith((".c", ".h")):
                    continue
                if product_only and (n.endswith("_test.c") or "_test_" in n or
                                     n.endswith("_stub.c") or "_stub" in n):
                    continue
                out.append(os.path.join(base, n))
    return out


def grep_all(patterns, corpus, scope=None):
    """Return (file, line, text) for every line matching any pattern.

    scope is an optional regex the PATH must match: a taskbar claim must be found in taskbar
    code, not in any file that happens to contain the word.
    """
    hits = []
    for path, text in corpus:
        rel = os.path.relpath(path, ROOT)
        if scope and not re.search(scope, rel):
            continue
        for i, line in enumerate(text.splitlines(), 1):
            for pat in patterns:
                if re.search(pat, line):
                    hits.append((os.path.relpath(path, ROOT), i, line.strip()[:110]))
                    break
    return hits


# (group, action, win11 behaviour, markers that must exist for it to work)
MATRIX = [
    ("mouse", "click selects icon",
     "single click selects, click on empty desktop clears",
     [r"wm_handle_mouse", r"icon_selected_index|\.selected\s*="]),
    ("mouse", "double click opens icon",
     "double click opens; single click only selects",
     [r"ICON_DBLCLICK_MS|dblclick"]),
    ("mouse", "right click context menus",
     "desktop, icon, window and taskbar each have their own menu",
     [r"wm_open_icon_menu|icon_menu|window_show_system_menu|taskbar_show_jump_list"]),
    ("mouse", "drag window by title bar",
     "press-drag on the caption moves the window",
     [r"wm_mouse_drag|dragging|DRAG_MOVE"]),
    ("mouse", "drag window edge to resize",
     "grab any edge/corner and resize",
     [r"hit_test|HT_LEFT|resize_edge|RESIZE"]),
    ("mouse", "drag window to screen edge snaps",
     "drag to left/right edge = half, to top = maximize (Aero Snap)",
     [r"drag.*snap|snap.*drag|snap_preview|drag_edge"]),
    ("mouse", "rubber-band desktop selection",
     "drag on empty desktop to select several icons",
     [r"select_rect|marquee|rubber"]),
    ("mouse", "ctrl-click multi-select icons",
     "ctrl-click adds/removes one icon from the selection",
     [r"MOD_CTRL.*selected|selected.*MOD_CTRL|toggle.*select"]),
    ("mouse", "taskbar click launches / focuses",
     "click a taskbar button: focus it, or restore it if minimized",
     [r"taskbar_tasks|taskbar_hit|taskbar_click"]),
    ("mouse", "taskbar hover thumbnail preview",
     "hovering a taskbar button shows a live thumbnail",
     [r"\bthumb"], r"shell/|taskbar"),      # scoped: a codec's "thumbnail" is not a preview
    ("mouse", "taskbar middle-click new instance",
     "middle click on a taskbar button opens a new instance",
     [r"new_instance"]),                  # btn==2 is the jump list; middle click is absent
    ("mouse", "taskbar shift-click new instance",
     "shift+click on a taskbar button opens a new instance",
     [r"taskbar_shift_new_instance"]),   # own marker: it must not borrow middle-click's
    ("mouse", "desktop wheel cycles windows",
     "wheel over the taskbar cycles windows (Windows does this)",
     [r"btn\s*==\s*4|btn\s*==\s*5|wheel_cycle"]),
    ("mouse", "drag file onto an app window",
     "drop a file on a window and the app opens it",
     [r"drop_file|file_drop|on_drop|DROP"]),

    ("shortcuts", "Win opens Start",
     "Win key toggles Start",
     [r"0xE05B|VK_LWIN"]),
    ("shortcuts", "Win+E opens Explorer",
     "Win+E launches the file manager",
     [r"launch_file_manager"]),
    ("shortcuts", "Win+I opens Settings",
     "Win+I opens the control panel",
     [r"control_launch"]),
    ("shortcuts", "Win+R opens Run",
     "Win+R opens the run dialog",
     [r"run_dialog_open"]),
    ("shortcuts", "Win+X power-user menu",
     "Win+X opens the power-user menu",
     [r"power_menu_open"]),
    ("shortcuts", "Win+D show desktop",
     "Win+D minimizes everything; again restores",
     [r"wm_show_desktop"]),
    ("shortcuts", "Win+L locks",
     "Win+L locks the session",
     [r"wubu_lock_screen_lock"]),
    ("shortcuts", "Win+A quick settings",
     "Win+A opens quick settings",
     [r"quick_settings_toggle"]),
    ("shortcuts", "Win+N notification center",
     "Win+N opens notifications",
     [r"notif_center_toggle"]),
    ("shortcuts", "Win+Tab task view",
     "Win+Tab opens the overview grid",
     [r"wm_overview_toggle"]),
    ("shortcuts", "Alt+Tab switches windows",
     "Alt+Tab cycles; hold Alt to pick",
     [r"alt_held|MOD_ALT.*0x09|0x09.*MOD_ALT"]),
    ("shortcuts", "Alt+F4 closes window",
     "Alt+F4 closes the focused window",
     [r"MOD_ALT.*0x3E|close_win\(g_dwm\.focused_id\)"]),
    ("shortcuts", "Alt+Space system menu",
     "Alt+Space opens the window menu",
     [r"window_show_system_menu"]),
    ("shortcuts", "Win+arrows snap",
     "Win+Left/Right half, Win+Up maximize, Win+Down restore/minimize",
     [r"wm_snap_half|snap_window_to_zone"]),
    ("shortcuts", "Win+Z snap layouts",
     "Win+Z opens the snap layout flyout",
     [r"win11_snap_toggle"]),
    ("shortcuts", "Win+Shift+arrows move desktop",
     "Win+Shift+Left/Right move the window to another desktop",
     [r"wm_move_focused_window"]),
    ("shortcuts", "Win+Ctrl+D new desktop",
     "Win+Ctrl+D creates a virtual desktop",
     [r"wm_add_desktop"]),
    ("shortcuts", "Win+number launches taskbar slot",
     "Win+1..9 launch/focus the pinned app in that slot",
     [r"win_number|taskbar_slot_launch|0x31.*MOD_WIN"]),
    ("shortcuts", "Ctrl+Shift+Esc task manager",
     "Ctrl+Shift+Esc opens the task manager",
     [r"0x1B.*MOD_CTRL.*MOD_SHIFT|task_manager_open|wubulens"]),
    ("shortcuts", "Win+P display projection",
     "Win+P opens the display/projection selector",
     [r"projection_flyout|projection_selector"], r"shell/"),
    ("shortcuts", "F2 rename selected icon",
     "F2 renames the selected desktop icon",
     [r"wm_rename_selected_icon"]),
    ("shortcuts", "Delete removes selected icon",
     "Delete sends the selected icon to the trash",
     [r"wm_delete_selected_icon"]),
    ("shortcuts", "Ctrl+Shift+N new folder",
     "Ctrl+Shift+N creates a folder in explorer",
     [r"new_folder|CTRL_SHIFT_N|MOD_SHIFT.*'n'"]),
    ("shortcuts", "Enter opens / Backspace back",
     "Enter opens the selection, Backspace goes up a folder",
     [r"wm_open_icon|explorer_back|KB_BACK"]),

    ("explorer", "address bar with breadcrumbs",
     "path shown as clickable segments",
     [r"breadcrumb|path_bar|address_bar"]),
    ("explorer", "navigation pane",
     "quick access + drives tree",
     [r"explorer_tree|nav_pane|quick_access"]),
    ("explorer", "view modes",
     "details / list / icons / tiles",
     [r"explorer_view|VIEW_DETAILS|view_mode"]),
    ("explorer", "sort by column header",
     "clicking a column header sorts",
     [r"sort_by|sort_column|column_header"]),
    ("explorer", "search inside a folder",
     "search box filters the current folder",
     [r"explorer_search|filter.*name|search_query"]),
    ("explorer", "tabs",
     "Win11 explorer has tabs",
     [r"explorer_tab|tab_bar.*explorer"]),
    ("explorer", "copy / cut / paste files",
     "Ctrl+C/X/V move files on disk",
     [r"explorer_ops|clipboard_copy|file_copy|fs_copy"]),
    ("explorer", "delete to recycle bin + restore",
     "delete goes to the trash, restore puts it back",
     [r"wubu_trash_restore|trash_restore|wubu_trash_move"]),
    ("explorer", "properties dialog",
     "Alt+Enter shows size, dates, permissions",
     [r"explorer_info|properties"]),
    ("explorer", "status bar item counts",
     "footer shows N items, M selected",
     [r"item_count|status_bar.*count|n_selected"]),
    ("explorer", "drive capacity bars",
     "This PC shows used/free per drive",
     [r"explorer_drives|statvfs"]),

    ("bars", "floating taskbar, centred groups",
     "Win11 bar: centred app group, tray right",
     [r"TASKBAR_H|taskbar_render"]),
    ("bars", "system tray + overflow",
     "tray icons with an overflow flyout",
     [r"wm_systray|tray_overflow"]),
    ("bars", "quick settings flyout",
     "wifi/bt/volume/brightness toggles",
     [r"quick_settings_render|quick_settings_toggle"]),
    ("bars", "calendar flyout",
     "clicking the clock opens a calendar",
     [r"Mini calendar|mini month calendar"]),
    ("bars", "notification toasts with actions",
     "toasts slide in and can be acted on",
     [r"wubu_notify|toast"]),
    ("bars", "widgets board",
     "widgets panel",
     [r"win11_widgets"]),
    ("bars", "Start pinned grid + all apps",
     "pinned tiles plus an all-apps list",
     [r"startmenu_tree|startmenu_search"]),
    ("bars", "jump lists",
     "right-click a taskbar button shows recent/actions",
     [r"taskbar_show_jump_list"]),
    ("bars", "taskbar progress / badges",
     "a window can report progress on its button",
     [r"wm_set_taskbar_progress|taskbar.*progress"], r"shell/|wm_window|wm\.h"),
    ("mouse", "drag window to a screen edge snaps",
     "drag to left/right edge = half, top = maximize (Aero Snap with preview)",
     [r"wm_snap_on_drag_release"], r"wm_(input|window_state)|wm\.h"),
    ("mouse", "taskbar overflow for many windows",
     "more buttons than fit collapse into an overflow menu",
     [r"overflow"], r"shell/taskbar|shell/wm_taskbar"),
    ("mouse", "pin / unpin from the jump list",
     "right-click a taskbar button and pin or unpin the app",
     [r"pin|unpin"], r"shell/(wm_taskbar|taskbar_tasks|wm_ctxmenu)"),

    ("shortcuts", "Alt+Tab shows live previews",
     "holding Alt shows thumbnails, not just titles",
     [r"alt_tab.*thumb|thumb.*alt_tab|switcher"], r"switcher|alt_tab|wm_input|shell/"),
    ("shortcuts", "Alt+Tab stays open while Alt is held",
     "release Alt to commit; the switcher is a modal picker",
     # alt_held alone is just (mods & MOD_ALT) -- that is a modifier test, not a switcher.
     [r"alt_tab_open|switcher_open|switcher_render"], r"wm_input|switcher"),

    ("explorer", "ribbon / toolbar commands",
     "new, delete, view and sort reachable from a toolbar",
     [r"ex_render_toolbar|toolbar_visible"], r"explorer"),
    ("explorer", "view mode persists across navigation",
     "details/list selection survives folder changes",
     [r"view_mode"], r"explorer"),

    ("bars", "notification toast actions",
     "a toast can carry buttons (dismiss, open)",
     [r"action_count|NotifyActionCallback|supports_actions"], r"wubu_notify"),
    ("bars", "quick settings toggles act",
     "wifi/bt/airplane/brightness each change something real",
     [r"quick_settings_handle_click|quick_settings_toggle"], r"shell/quick_settings"),
    ("bars", "Start all-apps list",
     "an A-Z list of every installed app",
     [r"startmenu_tree|all_apps"], r"shell/startmenu"),
    ("bars", "window open/close animation",
     "windows animate in and out rather than popping",
     [r"win_anim|window_anim|open_anim|close_anim"], r"wm_render|shell/window_chrome"),
    ("bars", "snap assist after snapping",
     "after a half snap, the other half offers the remaining windows",
     [r"snap_assist|snap_group"]),
]


def main(argv):
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", default="")
    ap.add_argument("--baseline", default="", help="JSON from an earlier run to hold the line")
    ap.add_argument("--write-baseline", default="", help="write the current WIRED set here")
    ap.add_argument("--only", default="")
    args = ap.parse_args(argv)

    def load(paths):
        out = []
        for f in paths:
            try:
                out.append((f, open(f, encoding="utf-8", errors="ignore").read()))
            except OSError:
                pass
        return out

    product = set(files(product_only=True))
    corpus = load(sorted(product))
    test_corpus = load([f for f in files(product_only=False) if f not in product])

    rows = []
    counts = {"WIRED": 0, "TESTONLY": 0, "GAP": 0}
    for row in MATRIX:
        group, action, behaviour, markers = row[0], row[1], row[2], row[3]
        scope = row[4] if len(row) > 4 else None
        if args.only and args.only != group:
            continue
        hits = grep_all(markers, corpus, scope)
        near = [] if hits else grep_all(markers, test_corpus, scope)
        # TESTONLY: the product does not have it, but a test/stub mentions it. That is a
        # different fact from GAP and the two must not be merged.
        verdict = "WIRED" if hits else ("TESTONLY" if near else "GAP")
        counts[verdict] = counts.get(verdict, 0) + 1
        rows.append({
            "group": group, "action": action, "win11": behaviour,
            "verdict": verdict, "hits": len(hits),
            "evidence": [f"{h[0]}:{h[1]}" for h in hits[:3]],
            "sample": hits[0][2] if hits else (f"only in tests/stubs: {near[0][0]}:{near[0][1]}" if near else ""),
        })

    width = max(len(r["action"]) for r in rows)
    cur = ""
    for r in rows:
        if r["group"] != cur:
            cur = r["group"]
            print(f"\n== {cur} ==")
        mark = {"WIRED": "ok  ", "TESTONLY": "tst ", "GAP": "GAP "}[r["verdict"]]
        ev = r["evidence"][0] if r["evidence"] else r["sample"] or "-"
        print(f"  {mark}{r['action']:<{width}}  {ev}")

    print(f"\n{counts['WIRED']} wired, {counts['TESTONLY']} test-only, {counts['GAP']} gaps "
          f"({counts['WIRED'] * 100 // max(sum(counts.values()), 1)}% wired in product code)")
    print("WIRED = code exists (file:line). Behaviour is the test suite's job; a GAP is the "
          "only claim this tool makes about absence.")

    if args.json:
        with open(args.json, "w") as fh:
            json.dump({"counts": counts, "rows": rows}, fh, indent=2)
        print(f"json -> {args.json}")

    # RATCHET. The gap list may be non-empty; what must never happen is a row that was WIRED
    # becoming a GAP without someone deciding to. Storing the NAME SET (not a count) is what
    # lets the failure say which action regressed.
    wired = {r["action"] for r in rows if r["verdict"] == "WIRED"}
    if args.write_baseline:
        with open(args.write_baseline, "w") as fh:
            json.dump({"wired": sorted(wired)}, fh, indent=2)
        print(f"baseline -> {args.write_baseline} ({len(wired)} wired actions)")
    if args.baseline:
        try:
            base = json.load(open(args.baseline)).get("wired", [])
        except (OSError, ValueError) as e:
            print(f"BASELINE UNREADABLE: {args.baseline}: {e}")
            return 2
        lost = [a for a in base if a not in wired]
        gained = [a for a in wired if a not in base]
        for a in lost:
            print(f"  REGRESSION: '{a}' was wired in the baseline and is not now")
        for a in gained:
            print(f"  new: '{a}' is now wired (update the baseline)")
        if lost:
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
