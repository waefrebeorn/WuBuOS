#!/usr/bin/env python3
"""Audit the WuBuOS shell surface against Windows 11 and Linux Mint Cinnamon.

For each well-known shell/desktop feature, look for real evidence in the tree
(not a doc mention -- a symbol or a file that implements it) and report
present / partial / absent.  The point is a prioritized gap list, not a
checkmark exercise.
"""
import re
import subprocess
import sys
from pathlib import Path

TREE = Path("/home/wubu/wubuos")

# (feature, [regexes]); a hit on ANY regex counts as evidence.
WIN11 = [
    ("Start menu (centered, search, pinned)", [r"startmenu_render_win11", r"g_win11_pins"]),
    ("Taskbar search button",                 [r"taskbar.*search", r"search.*taskbar"]),
    ("Task view (Win+Tab overview)",          [r"wm_overview", r"overview_toggle"]),
    ("Widgets board (Win+W)",                 [r"win11_widgets"]),
    ("Snap layouts flyout (Win+Z)",           [r"win11_snap"]),
    ("Snap window to screen edge on drag",    [r"snap_window_to_gaad"]),
    ("Hover flyout on maximize",              [r"snap.*hover", r"hover.*snap"]),
    ("Quick settings (Win+A)",                [r"quick_settings"]),
    ("Notification center",                   [r"notif_center", r"notification_center", r"wm_notify"]),
    ("Calendar/clock flyout",                 [r"clock_menu"]),
    ("Emoji panel (Win+.)",                   [r"emoji"]),
    ("Clipboard history (Win+V)",             [r"clipboard_history", r"clipboard_hist"]),
    ("Voice typing (Win+H)",                  [r"voice_typ", r"dictation"]),
    ("Power-user menu (Win+X)",               [r"win_x_menu", r"power_user_menu"]),
    ("Run dialog (Win+R)",                    [r"run_dialog", r"wm_run"]),
    ("Game Bar (Win+G)",                      [r"game_bar", r"gamebar"]),
    ("Rounded window corners",                [r"vbe_rect_rounded.*win|rounded_buttons"]),
    ("Mica / acrylic material",               [r"mica", r"acrylic"]),
    ("Window animations (open/close/min)",    [r"anim.*window|window.*anim|min_anim"]),
    ("Taskbar progress badges",               [r"taskbar.*progress"]),
    ("Taskbar alignment left/center option",  [r"taskbar_align|align.*taskbar"]),
    ("Taskbar size option",                   [r"taskbar_size"]),
    ("Multiple monitor / per-output taskbar", [r"wl_output.*v[3-9]|taskbar.*output"]),
    ("Refresh-rate / HDR settings",           [r"hdr", r"refresh_rate"]),
    ("Store",                                 [r"pkgmgr|package_manager|store"]),
    ("Windows Update",                        [r"update_manager|windows_update|pkg_update"]),
    ("Task manager",                          [r"task_manager|taskmgr|service_mgr"]),
    ("Device manager / driver manager",       [r"device_manager|driver_manager"]),
    ("Disk management",                       [r"disk_mgmt|partition"]),
    ("Defragmenter",                          [r"defrag"]),
    ("Magnifier",                             [r"magnifier"]),
    ("On-screen keyboard",                    [r"on_screen_keyboard|osk"]),
    ("Narrator / a11y",                       [r"wubu_a11y|narrator"]),
    ("Night light",                           [r"night_light"]),
    ("Focus assist",                          [r"focus_assist"]),
    ("Storage sense",                         [r"storage_sense"]),
    ("Startup apps UI",                       [r"startup_app"]),
    ("User accounts UI",                      [r"user_account"]),
    ("Bluetooth UI",                          [r"bluetooth"]),
    ("VPN UI",                                [r"\bvpn\b"]),
    ("Mobile hotspot",                        [r"hotspot"]),
    ("Project / second screen",               [r"project.*screen|second_screen"]),
    ("Send-to submenu",                       [r"send_to"]),
    ("Libraries",                             [r"libraries"]),
    ("Remote desktop",                        [r"remote_desktop"]),
    ("Web browser",                           [r"browser"]),
    ("Mail client",                           [r"mail_client|thunderbird|geary"]),
    ("Calendar app",                          [r"calendar_app"]),
    ("Photos app",                            [r"photos_app|image_viewer"]),
    ("PDF reader",                            [r"pdf_reader|document_viewer"]),
    ("Archive manager",                       [r"archive_manager"]),
    ("Font viewer",                           [r"font_viewer"]),
    ("Character map",                         [r"character_map"]),
    ("Solitaire / Minesweeper",               [r"solitaire|minesweeper"]),
    ("Copilot",                               [r"copilot"]),
    ("Office suite",                          [r"wubuoffice|libreoffice"]),
]

MINT = [
    ("Bottom panel",                          [r"taskbar"]),
    ("Panel applets",                         [r"applet"]),
    ("Main menu",                             [r"startmenu|start_menu"]),
    ("Window list",                           [r"window_list|task_slot"]),
    ("System tray",                           [r"systray"]),
    ("Show-desktop button",                   [r"show_desktop"]),
    ("Workspace expo",                        [r"overview|expo"]),
    ("Window scale (alt+tab)",                [r"alt.*tab|alttab"]),
    ("Desktop icons",                         [r"wm_icons|desktop_icon"]),
    ("Desktop context menu",                  [r"ctxmenu|context_menu"]),
    ("File manager",                          [r"explorer"]),
    ("File manager extensions",               [r"nemo_extension|explorer_extension"]),
    ("X-Apps (xed/xviewer/xreader/xplayer)",  [r"\bxed\b|xviewer|xreader|xplayer"]),
    ("Update manager",                        [r"update.*manager|pkgmgr"]),
    ("Software manager",                      [r"software_manager|pkgmgr"]),
    ("Driver manager",                        [r"driver_manager"]),
    ("System settings",                       [r"controlpanel|control_panel"]),
    ("Desktop effects / compositor effects",  [r"wubu_effect|desktop_effect"]),
    ("Hot corners",                           [r"hot_corner|hotcorner"]),
    ("Presentation mode",                     [r"presentation_mode"]),
    ("Night light",                           [r"night_light"]),
    ("Locale settings",                       [r"locale_settings"]),
    ("Users & groups",                        [r"user_account|users_groups"]),
    ("Backup tool / Timeshift",               [r"timeshift|backup_tool"]),
    ("Archive manager (file-roller)",         [r"file_roller|archive_manager"]),
    ("Image viewer",                          [r"image_viewer|xviewer"]),
    ("Media player",                          [r"media_player|xplayer|music"]),
    ("Web browser",                           [r"browser"]),
    ("Terminal",                              [r"wubu_cmd|terminal"]),
    ("Text editor",                           [r"wubu_editor|editor"]),
    ("Calculator",                            [r"calc"]),
    ("Screenshot tool",                       [r"screenshot"]),
    ("Sticky notes",                          [r"notes"]),
    ("Todo app",                              [r"todo"]),
    ("Weather",                               [r"weather"]),
]


def scan(patterns):
    """Return the list of files matching any pattern (source only)."""
    hits = []
    rx = [re.compile(p, re.I) for p in patterns]
    for p in TREE.rglob("*"):
        if p.suffix not in {".c", ".h"}:
            continue
        if "third_party" in p.parts or "submodule" in p.parts:
            continue
        try:
            txt = p.read_text(errors="ignore")
        except OSError:
            continue
        for r in rx:
            if r.search(txt):
                hits.append(str(p.relative_to(TREE)))
                break
    return sorted(set(hits))


def report(title, items):
    present, partial, absent = [], [], []
    for name, pats in items:
        hits = scan(pats)
        if hits:
            present.append((name, hits))
        else:
            absent.append(name)
    print(f"\n## {title}")
    print(f"  present: {len(present)}   absent: {len(absent)}")
    if absent:
        print("  -- absent --")
        for n in absent:
            print(f"     {n}")
    return present, absent


if __name__ == "__main__":
    w = report("Windows 11 shell surface", WIN11)
    m = report("Linux Mint Cinnamon surface", MINT)
    print("\n## summary")
    print(f"  win11: {len(w[0])}/{len(WIN11)} present, {len(w[1])} absent")
    print(f"  mint : {len(m[0])}/{len(MINT)} present, {len(m[1])} absent")
    sys.exit(0)
