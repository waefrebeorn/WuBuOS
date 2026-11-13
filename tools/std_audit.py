#!/usr/bin/env python3
"""std_audit.py -- measure the tree against the WuBuOS standards.

WHY A SCRIPT AND NOT A DOCUMENT
  A standard nobody can check is just an opinion.  This turns each rule into a
  mechanical measurement so that "bring this file up to standard" becomes a
  command you run, not a review you argue about.  It is deliberately small and
  cheap: it reads files and counts things, and it fails the build only on the
  rules that are unambiguous.

USAGE
  tools/std_audit.py            report, exit 0 if no hard violation
  tools/std_audit.py --quiet    only print violations
  tools/std_audit.py --json     machine-readable

EXIT CODES
  0  clean (or warnings only)
  1  at least one HARD violation
  2  usage / internal error
"""

import argparse
import json
import os
import re
import sys

# ---------------------------------------------------------------- rules

# Files above this many lines are candidates for splitting.  The covenant says
# "~400-500"; 600 gives legacy code room while still catching true monoliths.
MAX_LINES = 600

# Includes above this many suggest a god header / a module doing too much.
MAX_INCLUDES = 40

# Only first-party code is held to the standard.  Vendored trees have their
# own conventions and rewriting them would be churn without benefit.
EXCLUDED_PREFIXES = (
    "third_party/",
    "thirdparty/",
    "/~WuBu~/thirdparty/",
    "wubuoffice/",          # separate subproject with its own build
    "reference/",           # read-only reference material
)

# Geometry must come from wubu_tokens.h.  These are the literal forms that mean
# somebody hand-placed a pixel instead of using the scale.
MAGIC_NUMBER_RE = re.compile(r"(?<![\w.])([0-9]+)(?=[\s]*[;,)])")

# Tokens are allowed: they ARE the standard.
TOKEN_FILE = os.path.join("src", "gui", "wubu_tokens.h")


def is_first_party(rel):
    return not any(p in rel for p in EXCLUDED_PREFIXES)


def collect_sources(root):
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        # Build trees are generated, not written -- CMake's own probe sources
        # would otherwise dominate the report.
        dirnames[:] = [d for d in dirnames
                       if d not in (".git",) and not d.startswith("build")]
        for fn in filenames:
            if fn.endswith((".c", ".h")):
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, root)
                if is_first_party(rel):
                    out.append(rel)
    return sorted(out)


def read(path):
    try:
        with open(path, "r", errors="replace") as f:
            return f.read()
    except OSError:
        return ""


def audit_lines_and_includes(sources, root):
    """HARD rules: file size and include fan-in."""
    long_files, fat_headers = [], []
    for rel in sources:
        if not rel.endswith(".c"):
            continue
        text = read(os.path.join(root, rel))
        n = text.count("\n") + 1
        if n > MAX_LINES:
            long_files.append((rel, n))

        inc = sum(1 for ln in text.splitlines() if ln.startswith("#include"))
        if inc > MAX_INCLUDES:
            fat_headers.append((rel, inc))
    return long_files, fat_headers


# Only the surfaces the token scale governs.  A tree-wide "any number" rule
# produced thousands of hits on enum values, colour channels and generated
# Wayland protocol constants -- noise that trains people to ignore the tool.
# The honest rule is narrow: these files draw the desktop, so their spacing
# must come from wubu_tokens.h.
TOKEN_GOVERNED = (
    os.path.join("src", "gui", "shell", "window_chrome.c"),
    os.path.join("src", "gui", "shell", "taskbar.c"),
    os.path.join("src", "gui", "shell", "wm_taskbar.c"),
    os.path.join("src", "gui", "shell", "startmenu.c"),
    os.path.join("src", "gui", "shell", "wm_systray.c"),
    os.path.join("src", "gui", "shell", "wm_ctxmenu.c"),
    os.path.join("src", "gui", "shell", "win11_widgets.c"),
    os.path.join("src", "gui", "shell", "desktop.c"),
)


def audit_magic_geometry(sources, root):
    """HARD rule: layout literals in the surfaces the token scale governs."""
    hits = []
    for rel in sources:
        if rel.replace(os.sep, "/") not in TOKEN_GOVERNED:
            continue
        text = read(os.path.join(root, rel))
        for lineno, line in enumerate(text.splitlines(), 1):
            stripped = line.strip()
            if stripped.startswith("*") or stripped.startswith("/*"):
                continue
            if "WUBU_SPACE_" in line or "WUBU_RADIUS_" in line or "WUBU_TITLE_" in line:
                continue
            for m in MAGIC_NUMBER_RE.finditer(line):
                val = int(m.group(1))
                if val in (0, 1, 2, -1, 255, 0xFF):
                    continue
                # Offsets and sizes are geometry; colour maths and string
                # formatting are not.  Requiring a coordinate-ish neighbour
                # keeps this about LAYOUT rather than every constant.
                if not re.search(r"(\b[xXyYwWhH]\b|rect|pad|gap|pitch|w \-|h \-|\+ [0-9]|- [0-9])",
                                 line):
                    continue
                hits.append((rel, lineno, val, stripped[:70]))
                break
    return hits


def audit_markers(sources, root):
    """WARN: unfinished work markers in first-party code."""
    pat = re.compile(r"\b(TODO|FIXME|XXX)\b")
    placeholder = re.compile(r"\bplaceholder\b", re.I)
    counts = {}
    for rel in sources:
        text = read(os.path.join(root, rel))
        n = len(pat.findall(text))
        p = len(placeholder.findall(text))
        if n or p:
            counts[rel] = {"markers": n, "placeholder": p}
    return counts


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--baseline", metavar="FILE",
                    help="ratchet file: fail only when violations GROW past it "
                         "(use --write-baseline to create one)")
    ap.add_argument("--write-baseline", metavar="FILE",
                    help="record today's violations as the accepted baseline")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    sources = collect_sources(root)

    long_files, fat_headers = audit_lines_and_includes(sources, root)
    magic = audit_magic_geometry(sources, root)
    markers = audit_markers(sources, root)

    report = {
        "files_scanned": len(sources),
        "max_lines": MAX_LINES,
        "max_includes": MAX_INCLUDES,
        "long_files": [{"file": f, "lines": n} for f, n in long_files],
        "fat_headers": [{"file": f, "includes": n} for f, n in fat_headers],
        "magic_geometry": [
            {"file": f, "line": l, "value": v, "text": t} for f, l, v, t in magic
        ],
        "unfinished": markers,
    }

    def key_lf(r):  return f"{r[0]}:{r[1]}"
    def key_fh(r):  return f"{r[0]}:{r[1]}"
    def key_mg(r):  return f"{r[0]}:{r[1]}"

    now_counts = {
        "long": sorted(key_lf(r) for r in long_files),
        "fat":  sorted(key_fh(r) for r in fat_headers),
        "magic": sorted(key_mg(r) for r in magic),
    }

    baseline = None
    if args.baseline and os.path.exists(args.baseline):
        try:
            with open(args.baseline) as f:
                baseline = json.load(f)
        except (OSError, ValueError):
            baseline = None

    if args.write_baseline:
        with open(args.write_baseline, "w") as f:
            json.dump(now_counts, f, indent=2, sort_keys=True)
        print(f"baseline written to {args.write_baseline} "
              f"({len(now_counts['long'])} long, {len(now_counts['fat'])} fat, "
              f"{len(now_counts['magic'])} geometry)")
        return 0

    grew = {}
    if baseline is not None:
        for k in ("long", "fat", "magic"):
            before = set(baseline.get(k) or [])
            new = set(now_counts[k]) - before
            if new:
                grew[k] = sorted(new)
    hard = bool(grew) if baseline is not None else bool(long_files or fat_headers or magic)

    if args.json:
        print(json.dumps(report, indent=2))
        return 1 if hard else 0

    if baseline is not None and not args.quiet:
        print(f"(ratchet active: {args.baseline})")

    if not args.quiet:
        print(f"WuBuOS standard audit -- {report['files_scanned']} first-party files")
        print(f"rules: <= {MAX_LINES} lines/file, <= {MAX_INCLUDES} includes, "
              f"no geometry literals outside {TOKEN_FILE}")
        print()

    def show(title, rows, fmt, cap=20):
        if not rows:
            return
        shown = rows[:cap]
        more = len(rows) - len(shown)
        print(f"{title} ({len(rows)})")
        for r in shown:
            print("  " + fmt(r))
        if more > 0:
            print(f"  ... and {more} more")
        print()

    if grew:
        for k, label in (("long", "NEW long files"),
                         ("fat", "NEW god headers"),
                         ("magic", "NEW geometry literals")):
            if k in grew:
                print(f"HARD: {label} ({len(grew[k])})")
                for entry in grew[k]:
                    print(f"  {entry}")
                print()

    show("HARD: files over the line budget", long_files,
         lambda r: f"{r[1]:5d} lines  {r[0]}")
    show("HARD: god headers", fat_headers,
         lambda r: f"{r[1]:5d} includes  {r[0]}")
    show("HARD: geometry literals outside the token scale", magic,
         lambda r: f"{r[0]}:{r[1]}  value={r[2]}  {r[3]}")

    if markers:
        total_m = sum(v["markers"] for v in markers.values())
        total_p = sum(v["placeholder"] for v in markers.values())
        print(f"WARN: unfinished work -- {total_m} markers, "
              f"{total_p} placeholders across {len(markers)} files")
        for f, v in sorted(markers.items()):
            print(f"  {f}: {v['markers']} markers, {v['placeholder']} placeholders")
        print()

    if not hard and not args.quiet:
        print("clean: no hard violations")
    elif hard and not args.quiet:
        print("FAIL: hard violations present -- see docs/STANDARDS_AND_WORKFLOW.md")

    return 1 if hard else 0


if __name__ == "__main__":
    sys.exit(main())
