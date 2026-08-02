#!/usr/bin/env python3
"""WuBuOS documentation compendium generator (programmatic docs).

Run via `make docs` (or directly) whenever the code changes.  Regenerates
docs/compendium/01-reference/*.md from the source tree.  The 00-philosophy,
02-architecture, 03-learned and 04-roadmap sections are HUMAN-WRITTEN
(addendum style) and are never overwritten.

Generated sections:
  01-reference/modules.md   - every module + purpose + dependencies
  01-reference/api.md       - public functions per module
  01-reference/symbols.md   - key kernel symbols (from nm)
  01-reference/tests.md     - test targets + status
  01-reference/build.md     - build targets
"""
import os, re, subprocess, sys
from datetime import datetime, timezone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "compendium", "01-reference")
SRC_DIRS = ["src/kernel", "src/firmware", "src/apps", "src/hosted"]
BANNER = ("<!-- GENERATED FILE -- do not edit by hand.\n"
          "     Run `make docs` (tools/gen_docs.py) to regenerate. -->\n\n")

FUNC_RE = re.compile(
    r'^(?:static\s+)?(?:[A-Za-z_][\w\s\*]*?)\s+'
    r'([a-zA-Z_]\w*)\s*\(([^;]*?)\)\s*\{?$')

def module_purpose(path):
    """First comment block of the file = the module's stated purpose."""
    try:
        txt = open(path, errors="replace").read(4000)
    except OSError:
        return ""
    m = re.search(r'/\*+(.*?)\*/', txt, re.S)
    if not m:
        return ""
    lines = [l.strip().lstrip('*').strip() for l in m.group(1).splitlines()]
    lines = [l for l in lines if l and not l.startswith('=')]
    return " ".join(lines[:4])[:200]

def public_functions(path):
    """Heuristic: top-level function definitions (not static, not macros)."""
    fns = []
    in_comment = False
    for line in open(path, errors="replace"):
        s = line.strip()
        if s.startswith("/*"):
            in_comment = True
            continue
        if in_comment:
            if "*/" in s:
                in_comment = False
            continue
        if s.startswith(("static ", "#", "typedef", "//", "struct ", "enum ",
                         "union ", "extern ", "}")):
            continue
        m = FUNC_RE.match(s)
        if m and not m.group(1).startswith("_"):
            args = re.sub(r'/\*.*?\*/', '', m.group(2), flags=re.S)
            fns.append((m.group(1), args.replace("\n", " ").strip()))
    return fns

def includes(path):
    inc = []
    for line in open(path, errors="replace"):
        m = re.match(r'\s*#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            inc.append(m.group(1).split("/")[-1])
    return sorted(set(inc))

def collect():
    mods = []
    for d in SRC_DIRS:
        dpath = os.path.join(ROOT, d)
        if not os.path.isdir(dpath):
            continue
        for fn in sorted(os.listdir(dpath)):
            if not fn.endswith(".c"):
                continue
            p = os.path.join(dpath, fn)
            n = sum(1 for _ in open(p, errors="replace"))
            mods.append({
                "path": f"{d}/{fn}", "name": fn[:-2], "lines": n,
                "purpose": module_purpose(p),
                "fns": public_functions(p), "incs": includes(p),
            })
    return mods

def gen_modules(mods):
    out = [BANNER, "# Modules\n",
           "> Auto-generated from the source tree.  Purpose = the module's "
           "own header comment.\n\n",
           "| Module | Lines | Public API | Depends on | Purpose |\n",
           "|--------|------:|-----------:|------------|---------|\n"]
    for m in mods:
        incs = ", ".join(i for i in m["incs"] if i != fn) if False else \
               ", ".join(m["incs"])
        out.append(f"| `{m['name']}` | {m['lines']} | {len(m['fns'])} | "
                   f"{incs[:90]} | {m['purpose'][:110]} |\n")
    return "".join(out)

def gen_api(mods):
    out = [BANNER, "# Public API\n",
           "> Auto-generated.  Heuristic extraction of top-level functions.\n\n"]
    for m in mods:
        if not m["fns"]:
            continue
        out.append(f"## `{m['name']}` ({m['path']})\n\n")
        for name, args in m["fns"]:
            out.append(f"- `{name}({args})`\n")
        out.append("\n")
    return "".join(out)

def gen_symbols():
    elf = os.path.join(ROOT, "src", "kernel", "kernel.elf")
    out = [BANNER, "# Kernel Symbols (nm)\n\n```\n"]
    if os.path.exists(elf):
        try:
            r = subprocess.run(["nm", "-n", elf], capture_output=True, text=True)
            lines = r.stdout.splitlines()
            keep = [l for l in lines
                    if re.search(r' [TtBbDd] (wubu_|g_|idt|isr|common|task|syscall|klog|vbe|pit|apic|pci|console|holyc|agi)', l)]
            out += keep[:160]
        except OSError as e:
            out.append(f"(nm failed: {e})")
    else:
        out.append("(kernel.elf not built -- run make kernel)")
    out.append("\n```\n")
    return "".join(out)

def gen_tests():
    mk = open(os.path.join(ROOT, "Makefile"), errors="replace").read()
    targets = re.findall(r'^([a-zA-Z0-9_\-]+):', mk, re.M)
    targets = [t for t in targets if "test" in t or t in ("kernel", "docs", "all")]
    out = [BANNER, "# Tests\n",
           "> Auto-generated list of test/build targets.\n\n"]
    for t in targets:
        out.append(f"- `make {t}`\n")
    out.append("\nRun the suite: `make test_all` (hosted) / `make test_hive` / "
               "`make test_agi_kernel` / metal: boot + console probes.\n")
    return "".join(out)

def gen_build():
    mk = open(os.path.join(ROOT, "Makefile"), errors="replace").read()
    targets = re.findall(r'^([a-zA-Z0-9_\-]+):(.*)$', mk, re.M)
    out = [BANNER, "# Build\n\n| Target | Recipe (first line) |\n|--------|---------------------|\n"]
    for t, recipe in targets[:40]:
        r = recipe.strip().replace("|", "/").split("\n")[0][:100]
        out.append(f"| `{t}` | {r} |\n")
    return "".join(out)

def main():
    os.makedirs(OUT, exist_ok=True)
    mods = collect()
    files = {
        "modules.md": gen_modules(mods),
        "api.md": gen_api(mods),
        "symbols.md": gen_symbols(),
        "tests.md": gen_tests(),
        "build.md": gen_build(),
    }
    for fn, content in files.items():
        with open(os.path.join(OUT, fn), "w") as f:
            f.write(content)
        print(f"wrote 01-reference/{fn} ({len(content)} bytes)")
    print(f"total modules: {len(mods)}")

if __name__ == "__main__":
    main()
