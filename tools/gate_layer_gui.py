#!/usr/bin/env python3
"""gate_layer_gui.py -- nothing below the host layer may need a host GUI.

The desktop is supposed to own its own display: the kernel draws through VBE and
the compositor owns the scanout. A single `XOpenDisplay` or `wl_display_connect`
underneath the host layer would silently turn WuBuOS into a launcher for someone
else's desktop, and on bare metal it would not link at all.

This is a grep gate on purpose. Link-time would catch bare metal only, and the
hosted leg would keep the dependency alive invisibly for years.

Run from the repo root. Exit 1 when a forbidden symbol appears below the host
layer, and NAME the file so the fix is obvious.
"""
import os
import re
import sys

FORBIDDEN = re.compile(
    r"\b(XOpenDisplay|xcb_connect|wl_display_connect|gtk_init|SDL_Init|"
    r"wl_compositor_create_surface|xcb_create_window)\b"
)

# Symbols are allowed only in the HOST leg -- the code that exists precisely to
# put a window on Linux. Everything else must be display-agnostic.
HOST_DIRS = ("src/hosted",)

# One justified exception: the standalone compositor can run NESTED inside the
# host's Wayland for development. It is the host leg by another name, and it must
# never be linked into the metal path.
ALLOWED = {
    "src/gui/compositor/wubu_compositor_standalone.c":
        "nested development mode: this file IS the host leg",
}

SCAN = ("src/kernel", "src/runtime", "src/gui", "src/apps", "src/shell")

EXTS = (".c", ".h")


def main():
    violations = []
    for root in SCAN:
        if not os.path.isdir(root):
            continue
        for dirpath, dirnames, filenames in os.walk(root):
            # submodules and build trees are other repositories: a submodule's
            # own SDL dev harness is that repo's business, not this gate's. A
            # submodule has a .git FILE (a gitlink), so it is detectable.
            dirnames[:] = [d for d in dirnames
                           if d not in ("build", "build-audit", "externals")
                           and not os.path.exists(os.path.join(dirpath, d, ".git"))]
            if any(dirpath.startswith(h) for h in HOST_DIRS):
                continue
            for fn in filenames:
                if not fn.endswith(EXTS) or fn.endswith("_test.c"):
                    continue
                path = os.path.join(dirpath, fn)
                if path in ALLOWED:
                    continue
                try:
                    with open(path, "r", encoding="utf-8", errors="replace") as f:
                        for lineno, line in enumerate(f, 1):
                            if FORBIDDEN.search(line):
                                violations.append((path, lineno, line.strip()[:100]))
                except OSError:
                    continue

    if not violations:
        print("gate_layer_gui: clean -- no host GUI below the host layer")
        return 0
    print("gate_layer_gui: FAIL -- %d host-GUI reference(s) below the host layer" % len(violations))
    for path, lineno, text in violations[:20]:
        print("  %s:%d: %s" % (path, lineno, text))
    print("  A guest may bring its programs; it may not bring its display.")
    return 1


if __name__ == "__main__":
    sys.exit(main())
