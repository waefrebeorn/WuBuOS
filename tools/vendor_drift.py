#!/usr/bin/env python3
"""vendor_drift.py -- a vendored checkout must not be a different revision than its original.

The tree vendors the same code more than once: `wua` lives in src/gui/wet/externals and again
in src/gui/vws/externals/wet/externals, and `wet` itself is checked out inside vws. Nothing
noticed when those copies drifted apart, so vws silently built against a toolkit one commit
older than the one the rest of the OS links.

This finds every nested git checkout under a path segment named `externals` (or a `~WuBu~`
checkout), pairs it with the copy of the same repository that the main tree uses, and fails
when the revisions differ. Pairing is by repository basename, and only when exactly one
non-nested original exists -- an ambiguous pair is reported, never guessed.

Exit: 0 = every vendored copy matches, 1 = drift (or an ambiguous pair).
"""

import os
import subprocess
import sys
from collections import defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIP = {".git", "build", "node_modules", "third_party", "__pycache__"}


def git(path, *args):
    r = subprocess.run(["git", "-C", path, *args], capture_output=True, text=True)
    return r.stdout.strip() if r.returncode == 0 else ""


def find_checkouts():
    """Every directory that is itself a git checkout, with its depth under ROOT."""
    out = []
    for base, dirs, _ in os.walk(os.path.join(ROOT, "src")):
        dirs[:] = [d for d in dirs if d not in SKIP]
        if os.path.exists(os.path.join(base, ".git")):
            out.append(base)
            # KEEP DESCENDING: the drift that matters is a checkout INSIDE a checkout
            # (vws vendors wet, which vendors wua). Stopping here made this gate report
            # "no drift" on a tree that had exactly that.
    return out


def main():
    checkouts = find_checkouts()
    # The outermost checkout of a repository is the original; nested ones are the copies.
    by_repo = defaultdict(list)
    for c in checkouts:
        url = git(c, "config", "--get", "remote.origin.url")
        name = os.path.basename(url.rstrip("/")).removesuffix(".git") if url else ""
        by_repo[name or "(no-remote)"].append(c)

    drift, ambiguous, ok = [], [], []
    for repo, paths in sorted(by_repo.items()):
        paths.sort(key=lambda p: p.count(os.sep))
        if len(paths) < 2:
            continue
        head = {p: git(p, "rev-parse", "--short", "HEAD") for p in paths}
        originals = [p for p in paths if "externals" not in p.split(os.sep) and "~WuBu~" not in p]
        if len(originals) != 1:
            # No path is an obvious original (all live under externals/ or a ~WuBu~ checkout).
            # Majority wins: two copies of a revision outvote one, and the odd one out is the
            # drift. Only a genuine three-way split is ambiguous.
            counts = defaultdict(list)
            for p in paths:
                counts[head[p]].append(p)
            if len(counts) == 1:
                # Unanimous: every copy of this repository is on the same revision.
                for p in paths:
                    ok.append(f"{os.path.relpath(p, ROOT)}: {head[p]} (agrees)")
                continue
            top = sorted(counts.items(), key=lambda kv: -len(kv[1]))
            if len(top) > 1 and len(top[0][1]) > len(top[1][1]):
                want, holders = top[0]
                for p in paths:
                    rel = os.path.relpath(p, ROOT)
                    if head[p] == want:
                        ok.append(f"{rel}: {head[p]} (majority)")
                    else:
                        drift.append(f"{rel}: {head[p]}  !=  {want} ({os.path.relpath(holders[0], ROOT)})")
            else:
                ambiguous.append((repo, paths, head))
            continue
        want = head[originals[0]]
        for p in paths:
            rel = os.path.relpath(p, ROOT)
            if p == originals[0]:
                ok.append(f"{rel}: {head[p]} (original)")
            elif head[p] == want:
                ok.append(f"{rel}: {head[p]} (matches)")
            else:
                drift.append(f"{rel}: {head[p]}  !=  {want} ({os.path.relpath(originals[0], ROOT)})")

    for line in ok:
        print("ok   " + line)
    for repo, paths, head in ambiguous:
        print(f"AMBIGUOUS {repo}: " + ", ".join(f"{os.path.relpath(p, ROOT)}={head[p]}" for p in paths))
    for line in drift:
        print("DRIFT    " + line)

    if drift:
        print(f"\n{len(drift)} vendored checkout(s) are a different revision than their original.")
        return 1
    if ambiguous:
        print(f"\n{len(ambiguous)} repository pair(s) could not be paired unambiguously.")
        return 1
    print("\nno vendored drift")
    return 0


if __name__ == "__main__":
    sys.exit(main())
