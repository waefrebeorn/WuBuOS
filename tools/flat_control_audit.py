#!/usr/bin/env python3
"""Audit control OUTLINES at the draw site, not the palette.

Why this exists: tools/contrast_audit.py can prove that btn_face against win_face is 1.1:1 (a control
fill that does not separate from its window) but it cannot know whether the control is OUTLINED. When
an outline exists the pair is informational, because the boundary the eye uses is the outline, not the
fill. So the same requirement needs a second, code-level check: every control surface painted with a
face role must have a contrasting outline drawn with it.

What counts as an outline: within OUTLINE_WINDOW lines after the fill, a vbe_rect_rounded /
vbe_rect / vbe_3d_raised call on the same variables, using a border/outline role.

The audit fails in BOTH directions, like the contrast baseline: a new flat control fails, and a listed
flat control that has since gained an outline fails as STALE (debt that cannot shrink is an excuse
with a timestamp).
"""
import re
import sys
import pathlib

# Face roles that mean "this rectangle is a control surface".
FACE_ROLES = ("btn_face", "start_btn_face", "list_bg")
OUTLINE_ROLES = ("border_darkest", "border_dark", "border_light", "win_border", "taskbar_border")
OUTLINE_WINDOW = 10          # lines after the fill in which an outline must appear

FILL_RE = re.compile(r"vbe_fill_rect_rounded\s*\(")
RECT_RE = re.compile(r"vbe_(rect_rounded|rect|3d_(raised|sunken)(_rounded)?(_colors)?)\s*\(")


def scan_file(path: pathlib.Path):
    """Yield (line_no, snippet, has_outline) for every control-face fill in one file."""
    lines = path.read_text(errors="replace").splitlines()
    for i, ln in enumerate(lines):
        if not FILL_RE.search(ln) or not any(r in ln for r in FACE_ROLES):
            continue
        # An outline call may sit on the same line or within the window after it.
        window = "\n".join(lines[i:i + OUTLINE_WINDOW])
        outlined = bool(RECT_RE.search(window)) and any(r in window for r in OUTLINE_ROLES)
        yield i + 1, ln.strip(), outlined


def main(argv):
    root = pathlib.Path(argv[1] if len(argv) > 1 else "src/gui")
    baseline_path = pathlib.Path(argv[2]) if len(argv) > 2 else None

    flat, outlined = [], 0
    for path in sorted(root.rglob("*.c")):
        if path.name.endswith("_test.c"):
            continue
        for line_no, snippet, has_outline in scan_file(path):
            if has_outline:
                outlined += 1
            else:
                flat.append("%s:%d" % (path, line_no))

    known = set()
    if baseline_path and baseline_path.exists():
        for ln in baseline_path.read_text().splitlines():
            ln = ln.split("#", 1)[0].strip()
            if ln:
                known.add(ln)

    new = [f for f in flat if f not in known]
    stale = sorted(k for k in known if k not in flat)

    for f in new:
        print("  FLAT CONTROL (no outline within %d lines): %s" % (OUTLINE_WINDOW, f))
    for s in stale:
        print("  STALE BASELINE (now outlined or gone -- remove it): %s" % s)

    print("flat_control_audit: %d control fill(s), %d outlined, %d flat, %d baselined"
          % (len(flat) + outlined, outlined, len(flat), len(known)))
    if new or stale:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
