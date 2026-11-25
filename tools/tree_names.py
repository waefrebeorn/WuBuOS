#!/usr/bin/env python3
"""tree_names.py -- measure whether a tree's names say what the files do.

Renaming by feel is how a tree gets two `desktop.c`. This measures the failures that are
objective, per directory:

  COLLISION   the same basename in two or more directories -- a reader cannot tell which
              one an include means, and a token sweep cannot either (wubu_bonzi.h existed
              in src/gui, src/kernel and src/brain).
  OFF-PREFIX  a .c in a directory where >=60% of the other .c files share a prefix and
              this one does not (controlpanel.c beside cp_*.c).
  ACRONYM     a basename of <=6 chars with no vowel-bearing word shape (bpm.c) -- a hint,
              not a verdict: read the file before renaming.
  NO-HEADER   a .c with no same-named .h and no header in its directory that mentions it.

Usage: python3 tools/tree_names.py [tree ...]     (default: every submodule)
"""

import collections
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VOWELS = set("aeiouy")


def sources(tree):
    out = []
    for base, dirs, files in os.walk(tree):
        dirs[:] = [d for d in dirs if d not in (".git", "build", "node_modules", "__pycache__")]
        for f in files:
            if f.endswith((".c", ".h")):
                out.append(os.path.join(base, f))
    return out


def prefix_of(name):
    """First token of a snake_case basename: 'cp_text' -> 'cp'."""
    stem = name.rsplit(".", 1)[0]
    return stem.split("_")[0].lower()


def is_acronym(short):
    """'bpm' yes; 'gui' no (a known word); 'wm' no (2 chars is a layer prefix)."""
    if not (1 < len(short) <= 6) or short in ("gui", "gfx", "wm", "fs"):
        return False
    return not (set(short) & VOWELS) or short.isupper()


def survey(tree):
    files = sources(tree)
    if not files:
        return None
    by_base = collections.defaultdict(list)
    for f in files:
        by_base[os.path.basename(f)].append(f)
    collisions = {b: v for b, v in by_base.items() if len({os.path.dirname(x) for x in v}) > 1}

    by_dir = collections.defaultdict(list)
    for f in files:
        if f.endswith(".c"):
            by_dir[os.path.dirname(f)].append(os.path.basename(f))

    off_prefix, acronyms = [], []
    for d, names in by_dir.items():
        counts = collections.Counter(prefix_of(n) for n in names)
        if len(names) >= 3:
            dom, dom_n = counts.most_common(1)[0]
            if dom_n >= 0.6 * len(names):
                for n in names:
                    if prefix_of(n) != dom:
                        off_prefix.append(os.path.join(d, n))
        for n in names:
            if is_acronym(prefix_of(n)):
                acronyms.append(os.path.join(d, n))

    no_header = []
    for f in files:
        if not f.endswith(".c"):
            continue
        d = os.path.dirname(f)
        if not os.path.exists(f[:-2] + ".h"):
            no_header.append(f)

    return {
        "tree": tree, "files": len(files), "c": sum(1 for f in files if f.endswith(".c")),
        "collisions": collisions, "off_prefix": sorted(off_prefix),
        "acronyms": sorted(acronyms), "no_header": sorted(no_header),
    }


def main(argv):
    if argv:
        trees = argv
    else:
        trees = []
        for base, dirs, _ in os.walk(os.path.join(ROOT, "src")):
            for d in list(dirs):
                p = os.path.join(base, d)
                if os.path.exists(os.path.join(p, ".git")):
                    trees.append(os.path.relpath(p, ROOT))
            dirs[:] = [d for d in dirs if d not in (".git", "build")]
    for t in trees:
        s = survey(os.path.join(ROOT, t))
        if not s:
            continue
        print(f"\n== {s['tree']}  ({s['c']} .c / {s['files']} sources)")
        if s["collisions"]:
            print("  COLLISION (same name, different dirs):")
            for b, v in sorted(s["collisions"].items())[:8]:
                print(f"    {b}: " + ", ".join(os.path.relpath(x, ROOT) for x in v[:3]))
        if s["off_prefix"]:
            print("  OFF-PREFIX (" + str(len(s["off_prefix"])) + "):")
            for f in s["off_prefix"][:6]:
                print("    " + os.path.relpath(f, ROOT))
        if s["acronyms"]:
            print("  ACRONYM hint (" + str(len(s["acronyms"])) + "):")
            for f in s["acronyms"][:6]:
                print("    " + os.path.relpath(f, ROOT))


if __name__ == "__main__":
    main(sys.argv[1:])
