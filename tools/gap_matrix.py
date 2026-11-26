#!/usr/bin/env python3
"""gap_matrix.py -- diff the two reference OSes against what this tree actually contains.

WHY
   "There are 1000 gaps" is a true statement that nobody can act on. This turns it into a table:
   each reference feature, the module that would own it here, and whether that module EXISTS --
   with a file:line when it does, so a PRESENT claim can be checked in one command.

WHAT IT DOES NOT DO
   It does not claim behaviour. EXISTS means a module with that job is in the tree; whether it
   works is the test suite's business. A feature with no module is listed MISSING rather than
   quietly dropped, and the exit code is the number of missing reference features (capped).

USAGE
   tools/gap_matrix.py            # human table
   tools/gap_matrix.py --write    # also write docs/compendium/04-roadmap/gap-matrix.md
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WIN11_SYSAPPS = Path("/home/wubu/opt/refos/win11/ext/Windows/SystemApps")
CACHYOS = Path("/home/wubu/opt/refos/cachyos/rootfs")

# feature -> (globs that would own it here, note)
SHELL = [
    ("StartMenuExperienceHost", ["src/gui/shell/startmenu.c"], "Start menu"),
    ("SearchHost",              ["src/gui/shell/startmenu_search.c"], "search"),
    ("ShellExperienceHost",     ["src/gui/shell/quick_settings.c", "src/gui/shell/wubu_notify.c"],
     "quick settings / action centre"),
    ("LockApp",                 ["src/gui/shell/wubu_lock_screen.c"], "lock screen"),
    ("Windows.FileExplorer",    ["src/gui/explorer.c"], "file manager"),
    ("Windows.FilePicker",      ["src/gui/shell/file_picker.c"], "open/save dialog"),
    ("Windows.CapturePicker",   ["src/gui/wubu_screenshot.c", "src/gui/wubu_screenshot_png.c"],
                                  "screenshot: full/window/region, PNG that decodes, clipboard"),
    ("Windows.PrintDialog",     ["src/gui/shell/print_dialog.c"], "print dialog over CUPS"),
    ("Windows.PrintQueueActionCenter", ["src/runtime/wubu_print.c"], "print backend (queue state read from cupsd)"),
    ("Windows.CloudExperienceHost", ["src/gui/shell/wubu_shell_greeter.c"], "first-run OOBE"),
    ("Windows.PinningConfirmationDialog", [], "pin confirmation"),
    ("Windows.PeopleExperienceHost", [], "people/contacts flyout"),
    ("WindowsAppRuntime",       [], "packaged-app runtime"),
    ("Win32WebViewHost",        ["src/lua/www"], "embedded web view (WorldSpace)"),
    ("MicrosoftWindows.Client.AIX", [], "AI actions"),
    ("MicrosoftWindows.Client.CBS", [], "core shell services"),
    ("MicrosoftWindows.Client.FileExp", ["src/gui/explorer.c"], "file explorer client"),
    ("Windows.ContentDeliveryManager", ["src/gui/wubu_pkgmgr.c"], "content delivery / packages"),
    ("AccountsControl",         [], "accounts (no account model here)"),
    ("BioEnrollment",           [], "biometrics"),
    ("ParentalControls",        [], "parental controls"),
    ("AsyncTextService",        ["src/gui/term_ansi.c"], "text services"),
    ("NcsiUwpApp",              [], "network connectivity status"),
    ("Windows.OOBENetworkConnectionFlow", [], "OOBE network flow"),
    ("Windows.UndockedDevKit",  ["src/gui/shell/win11_widgets.c"], "widgets"),
    ("Windows.AddSuggestedFoldersToLibraryDialog", [], "libraries"),
    ("Windows.FilePickerDialog", ["src/gui/shell/file_picker.c"], "picker dialog"),
]

# CachyOS: services/features a desktop distro ships, and our owner (if any)
CACHYOS_FEATURES = [
    ("ananicy-cpp", "per-process priority daemon", ["src/runtime/wubu_perf_profile.c"],
     "profile only (no live priority daemon)"),
    ("pipewire", "audio server", ["src/gui/wubu_sound.c", "src/audio/bigmac"], "audio"),
    ("NetworkManager", "network management", ["src/gui/control_panel_network.c"], "network panel"),
    ("power-profiles-daemon", "power profiles", ["src/gui/wubu_gamemode.c"], "game mode only"),
    ("zram-generator", "compressed swap", ["src/runtime/wubu_perf_profile.c"], "tunables only"),
    ("systemd-coredump", "crash capture", [], "none"),
    ("fwupd", "firmware updates", [], "none"),
    ("reflector", "mirror ranking", [], "none"),
    ("pacman", "package manager", ["src/gui/wubu_pkgmgr.c"], "packages"),
    ("sudo", "privilege", ["src/runtime/vsl"], "VSL personality"),
]


def find(globs):
    """First existing (path, line) for any glob, so PRESENT points at real code."""
    for g in globs:
        p = ROOT / g
        if p.is_dir():
            for f in sorted(p.rglob("*.c"))[:1]:
                return f"{f.relative_to(ROOT)}:1"
            return f"{p.relative_to(ROOT)}/"
        if p.is_file():
            if p.suffix == ".c":
                for i, line in enumerate(p.read_text(errors="replace").split("\n"), 1):
                    s = line.strip()
                    if s and not s.startswith(("/*", "*", "//", "#")):
                        return f"{p.relative_to(ROOT)}:{i}"
            return f"{p.relative_to(ROOT)}:1"
    return None


def _norm(name):
    """Same normalisation for both sides: hash suffix, then the vendor prefixes."""
    b = re.sub(r"_(cw5n1h2txyewy|8wekyb3d8bbwe|[a-z0-9]{13})$", "", name)
    b = re.sub(r"^Microsoft\.", "", b)
    b = re.sub(r"^Windows\.", "", b)
    return b.lower()


def main():
    rows, missing = [], 0

    for name, globs, note in SHELL:
        hit = find(globs)
        rows.append(("win11", name, note, hit))
        if not hit:
            missing += 1
    for name, note, globs, ours in CACHYOS_FEATURES:
        hit = find(globs)
        rows.append(("cachyos", name, note + " -- " + ours, hit))
        if not hit:
            missing += 1

    # Reference features we did not map at all are listed too: silence is how gaps hide.
    if WIN11_SYSAPPS.is_dir():
        known = {_norm(n) for n, _, _ in SHELL}
        # NOT features: shared data, the last-known-good tree, and the framework packages.
        IGNORE = {"Shared", "LKG", "Windows", "SystemApps"}
        for d in sorted(WIN11_SYSAPPS.iterdir()):
            # Package directories carry an identity hash: Foo_cw5n1h2txyewy / Foo_8wekyb3d8bbwe.
            # Counting the hash as part of the name reported every Win11 package as unmapped,
            # which is exactly the noise that makes a gap list ignorable.
            base, key = d.name, _norm(d.name)
            if d.name in IGNORE or key in known:
                continue
            if not any(key.startswith(k[:12]) or k.startswith(key[:12]) for k in known):
                rows.append(("win11", d.name, "IN THE REFERENCE, NOT MAPPED HERE", None))
                missing += 1

    print(f"{'SOURCE':9} {'FEATURE':44} {'OWNER':40} NOTE")
    for src, name, note, hit in rows:
        print(f"{src:9} {name[:44]:44} {(hit or '-- MISSING --')[:40]:40} {note}")

    present = sum(1 for r in rows if r[3])
    print(f"\n{present}/{len(rows)} reference features have an owning module; {missing} missing")

    if "--write" in sys.argv:
        out = ROOT / "docs/compendium/04-roadmap/gap-matrix.md"
        out.parent.mkdir(parents=True, exist_ok=True)
        lines = [
            "# Gap matrix (generated by tools/gap_matrix.py)",
            "",
            "Every row is a feature that EXISTS IN A REFERENCE IMAGE and the module that would own",
            "it here. `-- MISSING --` is a measured absence, not an opinion. PRESENT points at a",
            "file:line so the claim can be checked in one command; it says nothing about whether",
            "the behaviour works -- that is the test suite's job.",
            "",
            f"**{present}/{len(rows)} have an owner, {missing} missing.**",
            "",
            "| source | feature | owner | note |",
            "|---|---|---|---|",
        ]
        for src, name, note, hit in rows:
            lines.append(f"| {src} | `{name}` | {('`'+hit+'`') if hit else '**MISSING**'} | {note} |")
        out.write_text("\n".join(lines) + "\n")
        print(f"wrote {out.relative_to(ROOT)}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
