#!/usr/bin/env python3
"""hardcode_census.py -- the ~WuBu~ org-wide hardcoding inventory.

WHY
  The de-hardcoding effort (docs/compendium/04-roadmap/dehardcode-workflow.md)
  started in the GUI's governed surfaces. The owner directive 2026-11-19 went
  wider: every repo in the ~WuBu~ family, every layer of the body. This tool
  measures the hardcoding that lives OUTSIDE the GUI token scale, in five
  tightly-scoped categories. Narrow beats comprehensive: a tree-wide "no
  numbers" rule is noise that trains people to ignore the tool.

CATEGORIES
  dup_defines  object-like #define names defined in 2+ first-party files
               (mirrors = same value, conflicts = different values). The
               cheapest, highest-value dedup class: one owner per constant.
  paths        filesystem path literals in C strings (/dev, /sys, /tmp, /run,
               ~/..., C:\\...). Candidates for a path owner or settings.
  env          getenv("NAME") call sites -- the env-knob registry. Every knob
               should be documented and, where it bounds an allocation,
               revolver-clamped (src/runtime/wubu_revolver.h).
  alloc        malloc/calloc/realloc with a LITERAL size (not sizeof) --
               allocation bounds that should be named or revolver-driven.
  sleep        usleep/sleep with a literal argument -- timing constants that
               should be named (and usually monotonic-clock-driven).

USAGE
  tools/hardcode_census.py [--root DIR] [--json] [--quiet]
  tools/hardcode_census.py --baseline FILE        ratchet: fail on GROWTH
  tools/hardcode_census.py --write-baseline FILE  record the frontier

EXIT CODES
  0 clean / warnings only   1 growth past baseline (or hits, no baseline)
  2 usage error

First-party only: vendored trees, build dirs and submodule mounts are other
repos' property -- they get their OWN census run with --root, never edits
from here.
"""

import argparse
import json
import os
import re
import sys

EXCLUDED_DIRS = {".git", "reference"}
EXCLUDED_SUBSTRINGS = (
    "third_party/", "thirdparty/", "/~WuBu~/thirdparty/",
)


def submodule_mounts(root):
    mounts = []
    gm = os.path.join(root, ".gitmodules")
    if os.path.exists(gm):
        for line in open(gm, errors="replace"):
            m = re.match(r"\s*path\s*=\s*(\S+)", line)
            if m:
                mounts.append(m.group(1).rstrip("/") + "/")
    return mounts


def is_first_party(rel, mounts):
    if any(s in rel for s in EXCLUDED_SUBSTRINGS):
        return False
    if any(rel.startswith(m) for m in mounts):
        return False
    return True


def collect(root):
    mounts = submodule_mounts(root)
    out = []
    for dirpath, dirnames, filenames in os.walk(root):
        dirnames[:] = [d for d in dirnames
                       if d not in EXCLUDED_DIRS and not d.startswith("build")]
        for fn in filenames:
            if fn.endswith((".c", ".h")):
                full = os.path.join(dirpath, fn)
                rel = os.path.relpath(full, root)
                if is_first_party(rel, mounts):
                    out.append(rel)
    return sorted(out)


def read(root, rel):
    try:
        with open(os.path.join(root, rel), errors="replace") as f:
            return f.read()
    except OSError:
        return ""


def strip_comments(text):
    # crude but honest: drop // tails and /* */ blocks for literal scanning
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.S)
    text = re.sub(r"//[^\n]*", "", text)
    return text


DEFINE_RE = re.compile(r"^\s*#\s*define\s+([A-Za-z_][A-Za-z0-9_]*)\s+(\S[^\n]*)",
                       re.M)
PATH_RE = re.compile(r'"((?:/(?:dev|sys|tmp|run|proc|apps|n|etc|usr|var|home|opt|mnt|media)/|~/|\.\./|[A-Z]:\\)[^"\n]{1,120})"')
ENV_RE = re.compile(r'\bgetenv\s*\(\s*"([A-Za-z_][A-Za-z0-9_]*)"')
ALLOC_RE = re.compile(r'\b(malloc|calloc|realloc)\s*\(\s*([0-9][0-9xXa-fA-F]*)\s*[,)]')
SLEEP_RE = re.compile(r'\b(usleep|sleep)\s*\(\s*([0-9]+)\s*\)')


def census(root):
    sources = collect(root)
    defines = {}   # name -> [(rel, lineno, value)]
    paths, envs, allocs, sleeps = [], [], [], []
    for rel in sources:
        text = read(root, rel)
        for m in DEFINE_RE.finditer(text):
            name, val = m.group(1), m.group(2).strip()
            if val.startswith(("(",)) and "(" in val.split()[0]:
                continue  # function-like macro: not a constant
            # Compare VALUES, not trailing comments: 0x20 /* a */ and
            # 0x20 /* b */ are mirrors, not conflicts.
            val = re.split(r"/\*|//", val)[0].strip()
            lineno = text[:m.start()].count("\n") + 1
            defines.setdefault(name, []).append((rel, lineno, val))
        code = strip_comments(text)
        for m in PATH_RE.finditer(code):
            lineno = code[:m.start()].count("\n") + 1
            paths.append((rel, lineno, m.group(1)))
        for m in ENV_RE.finditer(code):
            lineno = code[:m.start()].count("\n") + 1
            envs.append((rel, lineno, m.group(1)))
        for m in ALLOC_RE.finditer(code):
            lineno = code[:m.start()].count("\n") + 1
            allocs.append((rel, lineno, m.group(1) + "(" + m.group(2) + ")"))
        for m in SLEEP_RE.finditer(code):
            lineno = code[:m.start()].count("\n") + 1
            sleeps.append((rel, lineno, m.group(1) + "(" + m.group(2) + ")"))

    dups = []
    for name, sites in sorted(defines.items()):
        files = {s[0] for s in sites}
        if len(files) < 2:
            continue
        vals = {s[2] for s in sites}
        kind = "mirror" if len(vals) == 1 else "CONFLICT"
        dups.append({"name": name, "kind": kind,
                     "sites": [f"{r}:{l} = {v}" for r, l, v in sites]})
    return {
        "files_scanned": len(sources),
        "dup_defines": dups,
        "paths": [{"file": r, "line": l, "value": v} for r, l, v in paths],
        "env": [{"file": r, "line": l, "value": v} for r, l, v in envs],
        "alloc": [{"file": r, "line": l, "value": v} for r, l, v in allocs],
        "sleep": [{"file": r, "line": l, "value": v} for r, l, v in sleeps],
    }


def keys(rep):
    return {
        "dup": sorted(f"{d['name']}:{d['kind']}" for d in rep["dup_defines"]),
        "paths": sorted(f"{h['file']}:{h['line']}" for h in rep["paths"]),
        "env": sorted(f"{h['file']}:{h['line']}" for h in rep["env"]),
        "alloc": sorted(f"{h['file']}:{h['line']}" for h in rep["alloc"]),
        "sleep": sorted(f"{h['file']}:{h['line']}" for h in rep["sleep"]),
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--json", action="store_true")
    ap.add_argument("--quiet", action="store_true")
    ap.add_argument("--baseline")
    ap.add_argument("--write-baseline")
    args = ap.parse_args()
    root = os.path.abspath(args.root)
    rep = census(root)
    now = keys(rep)

    if args.write_baseline:
        with open(args.write_baseline, "w") as f:
            json.dump(now, f, indent=2, sort_keys=True)
        print("baseline written: " +
              ", ".join(f"{k}={len(v)}" for k, v in now.items()))
        return 0

    baseline = None
    if args.baseline and os.path.exists(args.baseline):
        try:
            baseline = json.load(open(args.baseline))
        except (OSError, ValueError):
            baseline = None

    grew = {}
    if baseline is not None:
        for k in now:
            new = set(now[k]) - set(baseline.get(k) or [])
            if new:
                grew[k] = sorted(new)
    hard = bool(grew) if baseline is not None else any(now.values())

    if args.json:
        print(json.dumps(rep, indent=2))
        return 1 if hard else 0

    conflicts = [d for d in rep["dup_defines"] if d["kind"] == "CONFLICT"]
    mirrors = [d for d in rep["dup_defines"] if d["kind"] == "mirror"]
    print(f"census {root}: files={rep['files_scanned']} "
          f"dup_conflicts={len(conflicts)} dup_mirrors={len(mirrors)} "
          f"paths={len(rep['paths'])} env={len(rep['env'])} "
          f"alloc={len(rep['alloc'])} sleep={len(rep['sleep'])}")
    if not args.quiet:
        for d in conflicts[:25]:
            print(f"  CONFLICT {d['name']}")
            for s in d["sites"][:4]:
                print(f"    {s}")
        for cat in ("paths", "env", "alloc", "sleep"):
            print(f"{cat}: {len(rep[cat])} sites")
        if grew:
            for k, v in grew.items():
                print(f"HARD: {len(v)} NEW {k} sites")
                for entry in v[:10]:
                    print(f"  {entry}")
        print("FAIL: growth past baseline" if hard and baseline is not None
              else ("FAIL: hardcoding present (no baseline)" if hard
                    else "clean"))
    return 1 if hard else 0


if __name__ == "__main__":
    sys.exit(main())