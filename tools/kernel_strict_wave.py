#!/usr/bin/env python3
"""kernel_strict_wave.py — one wave of the gcc14 declaration-hygiene campaign.

Parses the latest `make kernel-strict` error log, maps each implicit
declaration to its owning header, inserts the include, rebuilds strict,
and reports what remains. Implicit declarations are mechanical; type
mismatches and undeclared symbols are listed for manual fixing.

Usage: python3 tools/kernel_strict_wave.py <error-log> [--dry]
The campaign ratchet is `make kernel-strict` (mk/targets.mk).
"""
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STRICT = ("-Iinclude -DMYSEED_METAL -DWUBU_BAREMETAL=1 -DWUBU_NO_LIBM "
          "-fno-jump-tables -Werror=incompatible-pointer-types "
          "-Werror=int-conversion -Werror=implicit-function-declaration")

LIBC = {
    "snprintf": "<stdio.h>", "printf": "<stdio.h>", "vsnprintf": "<stdio.h>",
    "fprintf": "<stdio.h>", "puts": "<stdio.h>", "fopen": "<stdio.h>",
    "memset": "<string.h>", "memcpy": "<string.h>", "memmove": "<string.h>",
    "memcmp": "<string.h>", "strcmp": "<string.h>", "strncmp": "<string.h>",
    "strlen": "<string.h>", "strncpy": "<string.h>", "strcpy": "<string.h>",
    "strchr": "<string.h>", "strstr": "<string.h>", "strdup": "<string.h>",
    "strcasecmp": "<strings.h>", "strncasecmp": "<strings.h>",
    "malloc": "<stdlib.h>", "calloc": "<stdlib.h>", "free": "<stdlib.h>",
    "realloc": "<stdlib.h>", "abs": "<stdlib.h>", "atoi": "<stdlib.h>",
    "strtol": "<stdlib.h>", "strtoul": "<stdlib.h>", "qsort": "<stdlib.h>",
    "exit": "<stdlib.h>", "getenv": "<stdlib.h>",
    "time": "<time.h>", "gmtime": "<time.h>", "localtime": "<time.h>",
    "strftime": "<time.h>", "clock_gettime": "<time.h>",
    "sqrt": "<math.h>", "sin": "<math.h>", "cos": "<math.h>",
    "pow": "<math.h>", "fabs": "<math.h>", "floor": "<math.h>",
    "ceil": "<math.h>", "log": "<math.h>", "exp": "<math.h>",
}

ERR_RE = re.compile(r"^(src/[^:]+):\d+:\d+: error: implicit declaration of function [‘'](\w+)[’']")


def header_dirs():
    dirs = []
    for base in ("src/kernel", "src", "include", "src/runtime", "src/kernel/WuBuKernel/include"):
        d = os.path.join(ROOT, base)
        if os.path.isdir(d):
            dirs.append(d)
    return dirs


def find_header(sym, srcfile):
    if sym in LIBC:
        return LIBC[sym]
    # prefer a header whose name shares the symbol's prefix
    pat = re.compile(r"\b" + re.escape(sym) + r"\s*\(")
    cands = []
    for d in header_dirs():
        for fn in os.listdir(d):
            if not fn.endswith(".h"):
                continue
            path = os.path.join(d, fn)
            try:
                text = open(path, errors="replace").read()
            except OSError:
                continue
            if pat.search(text):
                score = 0
                stem = fn[:-2]
                if sym.startswith(stem):
                    score += 10
                if os.path.dirname(os.path.join(ROOT, srcfile)) == d:
                    score += 3
                if "internal" in fn or "test" in fn:
                    score -= 5
                cands.append((score, path))
    if not cands:
        return None
    cands.sort(reverse=True)
    best = cands[0][1]
    rel = os.path.relpath(best, os.path.dirname(os.path.join(ROOT, srcfile)))
    if rel.startswith("src/") or rel.startswith("include/"):
        # reachable via -I flags: use the bare name if a -I dir covers it
        for inc in ("include", "src/kernel", "src"):
            p = os.path.join(ROOT, inc)
            if best.startswith(p + os.sep):
                return os.path.relpath(best, p)
        return rel
    return rel


def add_include(srcfile, header):
    path = os.path.join(ROOT, srcfile)
    s = open(path).read()
    if header in s:
        return False
    inc = '#include "%s"' % header if not header.startswith("<") else "#include %s" % header
    lines = s.split("\n")
    last = -1
    for i, ln in enumerate(lines[:80]):
        if ln.startswith("#include"):
            last = i
    if last < 0:
        return False
    lines.insert(last + 1, inc + "   /* gcc14 campaign: declare what you use */")
    open(path, "w").write("\n".join(lines))
    return True


def main():
    log = sys.argv[1]
    dry = "--dry" in sys.argv
    text = open(log, errors="replace").read()
    # strip CI log prefixes if present
    pairs = {}
    other = set()
    for line in text.split("\n"):
        line = re.sub(r"^.*?\d{4}-\d{2}-\d{2}T[\d:.]+Z\s*", "", line)
        m = ERR_RE.match(line)
        if m:
            pairs.setdefault(m.group(1), set()).add(m.group(2))
        elif "error:" in line and "implicit declaration" not in line:
            other.add(line.strip()[:160])
    fixed, manual = [], []
    for f, syms in sorted(pairs.items()):
        for sym in sorted(syms):
            h = find_header(sym, f)
            if h is None:
                manual.append("%s: %s (NO declaring header found)" % (f, sym))
                continue
            if dry:
                fixed.append("%s += %s  (%s)" % (f, h, sym))
            elif add_include(f, h):
                fixed.append("%s += %s  (%s)" % (f, h, sym))
    print("== wave fixes (%d) ==" % len(fixed))
    for x in fixed:
        print(" ", x)
    print("== manual (%d) ==" % (len(manual) + len(other)))
    for x in sorted(manual):
        print(" ", x)
    for x in sorted(other)[:20]:
        print(" ", x)
    if dry:
        return
    print("== rebuild strict ==")
    r = subprocess.run(
        ["make", "-B", "kernel", "-j24", "KERNEL_CFLAGS=" + STRICT],
        cwd=ROOT, capture_output=True, text=True, timeout=420)
    out = r.stdout + r.stderr
    open(os.path.join(ROOT, "kernel_strict_latest.txt"), "w").write(out)
    errs = [l for l in out.split("\n") if "error:" in l]
    print("strict errors remaining: %d (log: kernel_strict_latest.txt)" % len(errs))
    for l in errs[:12]:
        print(" ", l[:160])


if __name__ == "__main__":
    main()