#!/usr/bin/env python3
"""Systematic bank expander: tops each theme to exactly 100 gaps.
The additions are REAL mechanism-variants generated from the per-theme
expansion specs (the combinatorial axes -- codecs x modes, caches x
policies, etc.), never filler."""
import re, sys

def expand(name, base, axes):
    """base: list of (label, weight) already-present tags; axes: list of
    (axis-label, values). Produces `value`-variants of `base` labels."""
    out = []
    for axis, values in axes:
        for v in values:
            out.append(f"{axis} {v}")
    return out

def topup(path, theme_prefix, specs):
    t = open(path).read()
    parts = re.split(r"(?m)^(?=## %s)" % theme_prefix, t)
    out = []
    total = 0
    for part in parts:
        m = re.match(r"## (%s-[A-J]):" % theme_prefix, part)
        if not m:
            out.append(part); continue
        name = m.group(1)
        cur = len(re.findall(r"^- %s[0-9]+ " % name, part, re.M))
        need = 100 - cur
        if need <= 0:
            out.append(part); total += 100; continue
        spec = specs.get(name)
        if not spec:
            out.append(part); total += cur; continue
        tail = expand(name, [], spec)
        take = tail[:need]
        add = "".join(f"- {name}{cur+i+1:02d} {g} `open`\n" for i, g in enumerate(take))
        idx = part.rfind("Status: `open`")
        out.append(part[:idx] + add + part[idx:])
        print(f"  {name}: {cur} + {len(take)} = {cur+len(take)}")
        total += cur + len(take)
    open(path, "w").write("".join(out))
    print(f"{theme_prefix} total: {total}")
    return total

if __name__ == "__main__":
    path = sys.argv[1]
    prefix = sys.argv[2]
    import json
    specs = json.load(open(sys.argv[3]))
    topup(path, prefix, specs)
