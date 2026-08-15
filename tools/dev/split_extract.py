#!/usr/bin/env python3
"""Brace-safe monolith splitter for WuBuOS.

Modes:
  single:   split_extract.py <src> <new> <start> <end> [incs...]
  multi:    split_extract.py <src> --multi 'new:s:e:inc1,inc2;new2:s:e:inc3'
  --fn:     split_extract.py <src> <new> <fnname> [incs...]   (1 fn by brace-match)
  --fngroup:split_extract.py <src> <new> <firstFn>:<lastFn> [incs...] (fn group)

All modes strip #include lines from the extracted block by default (rely on
the listed includes) unless --keep-inc is passed. Coordinates for --fn/--fngroup
are computed by brace-matching, so no off-by-one drift.
"""
import sys, os, re

def emit_includes(incs):
    if not incs:
        return ""
    out = []
    for i in incs:
        if i.endswith(".h") and not i.startswith("<"):
            out.append(f'#include "{i}"')
        else:
            out.append(f'#include {i}')
    return "\n".join(out) + "\n\n"

def find_fn_span(lines, name):
    start = None
    for i, l in enumerate(lines):
        if re.search(r'\b' + re.escape(name) + r'\s*\(', l) and '(' in l:
            start = i
            break
    if start is None:
        raise SystemExit(f"function '{name}' not found")
    depth = 0; started = False; end = None
    for j in range(start, len(lines)):
        for ch in lines[j]:
            if ch == '{':
                depth += 1; started = True
            elif ch == '}':
                depth -= 1
                if started and depth == 0:
                    return (start + 1, j + 1)
    raise SystemExit(f"no matching }} for '{name}'")

def write_new(newf, incs, block):
    os.makedirs(os.path.dirname(newf) or ".", exist_ok=True)
    with open(newf, "w") as f:
        f.write("/*\n * WuBuOS -- extracted module (auto-split, C11, opaque-safe)\n */\n\n")
        f.write(emit_includes(incs))
        f.write("\n".join(block))
        if not block or not block[-1].endswith("\n"):
            f.write("\n")

def main():
    args = sys.argv[1:]
    keep_inc = "--keep-inc" in args
    if keep_inc:
        args = [a for a in args if a != "--keep-inc"]

    if "--multi" in args:
        args = [a for a in args if a != "--multi"]
        src = args[0]
        blocks = []
        for spec in args[1].split(";"):
            parts = spec.split(":")
            newf, s, e = parts[0], int(parts[1]), int(parts[2])
            incs = [x for x in parts[3].split(",") if x]
            blocks.append((newf, s, e, incs))
        blocks.sort(key=lambda b: b[1], reverse=True)
        lines = open(src, errors="ignore").read().split("\n")
        orig_n = len(lines)
        removed = 0
        for newf, s, e, incs in blocks:
            block = lines[s-1:e]
            if not keep_inc:
                block = [l for l in block if not re.match(r'\s*#\s*include\b', l)]
            write_new(newf, incs, block)
            lines = lines[:s-1] + lines[e:]
            removed += (e - s + 1)
            print(f"  -> {newf}: lines {s}-{e} extracted")
        with open(src, "w") as f:
            f.write("\n".join(lines))
        print(f"[multi] {src}: {orig_n} -> {len(lines)} lines ({removed} removed)")
        return

    if "--fn" in args:
        args = [a for a in args if a != "--fn"]
        src, newf, fnname = args[0], args[1], args[2]
        incs = args[3:]
        lines = open(src, errors="ignore").read().split("\n")
        s1, e1 = find_fn_span(lines, fnname)
        block = lines[s1-1:e1]
        if not keep_inc:
            block = [l for l in block if not re.match(r'\s*#\s*include\b', l)]
        write_new(newf, incs, block)
        lines = lines[:s1-1] + lines[e1:]
        with open(src, "w") as f:
            f.write("\n".join(lines))
        print(f"[fn] {newf}: {fnname} lines {s1}-{e1}; {src} now {len(lines)} lines")
        return

    if "--fngroup" in args:
        args = [a for a in args if a != "--fngroup"]
        src, newf = args[0], args[1]
        first, last = args[2].split(":")
        incs = args[3:]
        lines = open(src, errors="ignore").read().split("\n")
        s1, _ = find_fn_span(lines, first)
        _, e2 = find_fn_span(lines, last)
        block = lines[s1-1:e2]
        if not keep_inc:
            block = [l for l in block if not re.match(r'\s*#\s*include\b', l)]
        write_new(newf, incs, block)
        lines = lines[:s1-1] + lines[e2:]
        with open(src, "w") as f:
            f.write("\n".join(lines))
        print(f"[fngroup] {newf}: {first}..{last} lines {s1}-{e2}; {src} now {len(lines)} lines")
        return

    # single range
    src, newf, s, e = args[0], args[1], int(args[2]), int(args[3])
    incs = args[4:]
    lines = open(src, errors="ignore").read().split("\n")
    block = lines[s-1:e]
    if not keep_inc:
        block = [l for l in block if not re.match(r'\s*#\s*include\b', l)]
    write_new(newf, incs, block)
    lines = lines[:s-1] + lines[e:]
    with open(src, "w") as f:
        f.write("\n".join(lines))
    print(f"[single] {newf}: {s}-{e}; {src} now {len(lines)} lines")

if __name__ == "__main__":
    main()
