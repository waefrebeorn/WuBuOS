#!/usr/bin/env python3
"""Fail if a suite run leaves untracked files in the repo.

A test that writes into the repository root is a defect whether or not anyone notices: it can be
committed by accident, it breaks `git status` for the next person, and it hides which test did it.
This gate names the file at the end of the suite instead of leaving it to be found weeks later.

Allowlist: paths in .gitignore are invisible to `git status --porcelain` already; anything else is
either a real artifact that belongs in .gitignore (fix the test) or a stray (fix the test).
"""
import pathlib
import subprocess
import sys

ALLOW = pathlib.Path(__file__).resolve().parent / "workspace_allowlist.txt"

def main():
    allowed = {}
    if ALLOW.exists():
        for line in ALLOW.read_text().splitlines():
            line = line.split("#", 1)[0].strip()
            if line:
                allowed[line] = 1
    out = subprocess.run(["git", "status", "--porcelain"], capture_output=True, text=True,
                         cwd=str(__import__("pathlib").Path(__file__).resolve().parent.parent)).stdout
    strays = []
    for ln in out.splitlines():
        if ln.startswith("?? "):
            path = ln[3:].strip()
            if path in ("src/lua/www",):        # a submodule with its own uncommitted work
                continue
            strays.append(path)
    new = [s for s in strays if s not in allowed]
    stale = [a for a in allowed if a not in strays]
    for s in stale:
        print("  STALE ALLOWLIST ENTRY (the stray is gone -- remove it): %s" % s)
    if new or stale:
        for s in new:
            print("  STRAY FILE left in the repo: %s" % s)
        print("workspace_clean: %d new stray(s), %d stale allowlist entry(ies)" % (len(new), len(stale)))
        return 1
    print("workspace_clean: no untracked files (the suite left the tree as it found it)")
    return 0

if __name__ == "__main__":
    sys.exit(main())
