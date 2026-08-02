#!/usr/bin/env python3
"""WuBuOS documentation compendium generator -- NEW GENERATION SCANNERS.

Run via `make docs`. Regenerates docs/compendium/01-reference/*.md from the
SOURCE TREE + the LIVE SYSTEM. Every generated file is the output of a
SCANNER that executes something (source walk, test runs, boot probes, build
inspection) -- the docs prove the system's state at generation time.

Scanners:
  modules  -> modules.md   recursive src/ walk (ALL dirs), header-sourced API
  symbols  -> symbols.md   all built ELFs (kernel + hosted)
  tests    -> tests.md     RUNS the host tests, records PASS/FAIL + time
  state    -> state.md     boots QEMU, records the live AGI-OS probes
  parity   -> parity.md    hosted legs + OS portability matrix (PARITY)

The 00-philosophy / 02-architecture / 03-learned / 04-roadmap sections are
HUMAN-WRITTEN (addendum style) and are never overwritten.
"""
import os, re, subprocess, sys
from datetime import datetime, timezone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "docs", "compendium", "01-reference")
BANNER = ("<!-- GENERATED FILE -- do not edit by hand.\n"
          "     Run `make docs` (tools/gen_docs.py) to regenerate. -->\n\n")
GEN_AT = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")

FUNC_RE = re.compile(
    r'^(?:static\s+)?(?:[A-Za-z_][\w\s\*]*?)\s+'
    r'([a-zA-Z_]\w*)\s*\(([^;]*?)\)\s*\{?$')
PROTO_RE = re.compile(
    r'^(?:extern\s+)?(?:[A-Za-z_][\w\s\*]*?)\s+'
    r'([a-zA-Z_]\w*)\s*\(([^;]*?)\)\s*;')
SKIP_DIRS = {"build", ".git", "__pycache__", "vendor", "third_party"}


def walk_src():
    """All .c and .h files under src/, grouped by top-level dir."""
    src = os.path.join(ROOT, "src")
    groups = {}
    for root, dirs, files in os.walk(src):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        rel = os.path.relpath(root, src)
        top = rel.split(os.sep)[0]
        for fn in files:
            if fn.endswith((".c", ".h")):
                groups.setdefault(top, []).append(os.path.join(root, fn))
    return groups


def module_purpose(path):
    """Purpose: a 'Purpose:' line, else the first comment block, else name."""
    try:
        txt = open(path, errors="replace").read(6000)
    except OSError:
        return ""
    m = re.search(r'(?im)^\s*[#*]+\s*(?:purpose|what)\s*[:\-]?\s*(.+)$',
                  txt)
    if m:
        return m.group(1).strip()[:200]
    m = re.search(r'/\*+(.*?)\*/', txt, re.S)
    if m:
        lines = [l.strip().lstrip('*').strip() for l in m.group(1).splitlines()]
        lines = [l for l in lines if l and not l.startswith('=') and
                 'Copyright' not in l and 'License' not in l]
        return " ".join(lines[:3])[:200]
    return ""


def header_api(path):
    """Public API from a header: top-level prototypes."""
    apis = []
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
        if s.startswith(("#", "typedef", "struct ", "enum ", "union ",
                         "static ", "//", "}")):
            continue
        m = PROTO_RE.match(s)
        if m and not m.group(1).startswith("_"):
            args = re.sub(r'/\*.*?\*/', '', m.group(2), flags=re.S)
            apis.append(f"{m.group(1)}({args.replace(chr(10), ' ').strip()})")
    return apis


def includes(path):
    inc = []
    for line in open(path, errors="replace"):
        m = re.match(r'\s*#include\s*[<"]([^>"]+)[>"]', line)
        if m:
            inc.append(m.group(1).split("/")[-1])
    return sorted(set(inc))


# ---------------------------------------------------------------- modules
def scan_modules():
    groups = walk_src()
    rows = []
    for top in sorted(groups):
        for path in sorted(groups[top]):
            fn = os.path.basename(path)
            if fn.endswith(".h"):
                continue
            n = sum(1 for _ in open(path, errors="replace"))
            base = fn[:-2]
            rows.append((top, base, n, module_purpose(path),
                         ", ".join(includes(path))))
    out = [BANNER, f"# Modules\n> Generated {GEN_AT} -- recursive src/ walk, "
                   f"{len(rows)} modules.\n\n",
           "| Tree | Module | Lines | Depends on | Purpose |\n",
           "|------|--------|------:|------------|---------|\n"]
    for top, base, n, purp, incs in rows:
        out.append(f"| `{top}/` | `{base}` | {n} | {incs[:80]} | "
                   f"{purp[:100]} |\n")
    return "".join(out)


# -------------------------------------------------------------------- api
def scan_api():
    """Header-sourced public API: prototypes per module (the real contracts)."""
    out = [BANNER, f"# Public API (header-sourced)\n> Generated {GEN_AT} -- "
                   f"prototypes extracted from the headers = the real "
                   f"interface contracts.\n\n"]
    count = 0
    for root, dirs, files in os.walk(os.path.join(ROOT, "src")):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for fn in sorted(files):
            if not fn.endswith(".h"):
                continue
            p = os.path.join(root, fn)
            apis = header_api(p)
            if not apis:
                continue
            rel = os.path.relpath(p, ROOT)
            out.append(f"## `{rel}`\n\n")
            # Gap J5: no per-header cap -- the api reference lists every
            # prototype (the previous 40/header cap silently truncated).
            for a in apis:
                out.append(f"- `{a}`\n")
                count += 1
            out.append("\n")
    out.insert(2, f"> {count} prototypes across the tree.\n\n")
    return "".join(out)


# ---------------------------------------------------------------- symbols
def scan_symbols():
    out = [BANNER, f"# Symbols\n> Generated {GEN_AT} -- all built ELFs.\n\n"]
    for root, dirs, files in os.walk(os.path.join(ROOT, "src")):
        dirs[:] = [d for d in dirs if d not in SKIP_DIRS]
        for fn in files:
            if not (fn.endswith(".elf") or fn.endswith(".o")):
                continue
            if fn.endswith(".o"):
                continue
            p = os.path.join(root, fn)
            out.append(f"\n## {os.path.relpath(p, ROOT)}\n\n```\n")
            try:
                r = subprocess.run(["nm", "-n", p], capture_output=True,
                                   text=True, timeout=30)
                keep = [l for l in r.stdout.splitlines()
                        if re.search(r' [TtBbDd] (wubu_|g_|idt|isr|common|'
                                     r'task|syscall|klog|vbe|pit|apic|pci|'
                                     r'console|holyc|agi|dosgui)', l)]
                out += keep[:120]
            except OSError:
                out.append("(nm unavailable)")
            out.append("\n```\n")
    return "".join(out)


# ------------------------------------------------------------- tests (RUNS)
CURATED_TESTS = ["test_hive", "test_agi_kernel", "test_theme_hid",
                 "test_verifier", "test_sync", "test_vmm", "test_sha256",
                 "test_rtc", "test_lfn"]


def scan_tests():
    out = [BANNER, f"# Tests\n> Generated {GEN_AT} -- the scanner RUNS the "
                   f"curated host tests and records the results.\n\n"]
    for t in CURATED_TESTS:
        try:
            r = subprocess.run(["make", "-s", t], cwd=ROOT,
                               capture_output=True, text=True, timeout=300)
            last = [l for l in (r.stdout + r.stderr).splitlines()
                    if "PASS" in l or "FAIL" in l or "green" in l]
            status = "PASS" if r.returncode == 0 else "FAIL"
            out.append(f"- `make {t}` -> **{status}**"
                       f"{'  (' + last[-1].strip()[:60] + ')' if last else ''}\n")
        except subprocess.TimeoutExpired:
            out.append(f"- `make {t}` -> **TIMEOUT**\n")
    out.append("\nMetal: boot + console probes (`test_agi_metal`); "
               "docs: `make docs`.\n")
    return "".join(out)


# ------------------------------------------------------------------ commands
def scan_commands():
    """Console command list (gap J3): parsed from wubu_console.c's dispatch
    table -- the REAL command set, never a stale hand list."""
    out = [BANNER, f"# Console Commands\n> Generated {GEN_AT} -- parsed from "
                   f"wubu_console.c's dispatch table.\n\n"]
    src = os.path.join(ROOT, "src", "kernel", "wubu_console.c")
    if not os.path.exists(src):
        return "".join(out) + "(wubu_console.c not found)\n"
    cmds = []
    for line in open(src, errors="replace"):
        m = re.search(r'strcmp\(argv\[0\],\s*"([a-z]+)"\)', line)
        if m:
            cmds.append(m.group(1))
    for c in sorted(set(cmds)):
        out.append(f"- `{c}`\n")
    return "".join(out)


# ------------------------------------------------------- state (boots QEMU)
def scan_state():
    out = [BANNER, f"# Live State\n> Generated {GEN_AT} -- boots the metal "
                   f"kernel in QEMU and records the console probes.\n\n"]
    probe = os.path.join(ROOT, "tools", "probe_metal.py")
    if os.path.exists(probe):
        try:
            r = subprocess.run(["python3", probe], capture_output=True,
                               text=True, timeout=240)
            out.append("```\n" + r.stdout[-1200:] + "\n```\n")
        except (subprocess.TimeoutExpired, OSError) as e:
            out.append(f"(probe failed: {e})\n")
    else:
        out.append("(tools/probe_metal.py not present -- install the boot "
                   "probe for live state)\n")
    return "".join(out)


# ------------------------------------------------- parity (build matrix)
def scan_parity():
    hosted = os.path.join(ROOT, "src", "hosted")
    legs = sorted(fn for fn in os.listdir(hosted)
                  if fn.startswith("wubu_metal_") and fn.endswith(".c"))
    core = []
    for fn in ("wubu_metal.c", "wubu_metal.h"):
        if os.path.exists(os.path.join(hosted, fn)):
            core.append(fn)
    mk = open(os.path.join(ROOT, "Makefile"), errors="replace").read()
    cc = re.search(r'^CC\s*[:?]?=\s*(.+)$', mk, re.M)
    # EVIDENCE: build the portable core targets on this host
    build_status = "?"
    try:
        r = subprocess.run(["make", "-s", "runtime", "tools"], cwd=ROOT,
                           capture_output=True, text=True, timeout=600)
        build_status = "PASS" if r.returncode == 0 else "FAIL"
    except (subprocess.TimeoutExpired, OSError) as e:
        build_status = f"ERROR ({e})"
    out = [BANNER, f"# Parity (compiled binaries across OSes)\n> Generated "
                   f"{GEN_AT} -- the PARITY project: the hosted layer is the "
                   f"scaffold that must run on Linux/Windows/macOS.\n\n",
           f"## Hosted core (portable abstraction)\n{', '.join(core)}\n\n",
           f"## OS legs (per-platform backends)\n"]
    for l in legs:
        out.append(f"- `{l}`\n")
    out.append("\n## Toolchain\n")
    out.append(f"- CC: `{cc.group(1).strip() if cc else '?'}`\n")
    out.append("\n## Portability matrix (evidence of record)\n")
    out.append("| Platform | Build | Runtime legs | Status |\n")
    out.append("|----------|-------|--------------|--------|\n")
    out.append(f"| Linux (this host) | `make runtime tools` -> "
               f"**{build_status}** | "
               f"{', '.join(l[:-2] for l in legs[:4]) or 'core'} | "
               f"{'VERIFIED' if build_status == 'PASS' else 'CHECK'} |\n")
    out.append("| Windows (WSL host / native) | cross build (see "
               "00-philosophy/cross-platform-build.md, gap K3) | core + "
               "win32 leg (planned) | CONFIG |\n")
    out.append("| macOS | cross build (see "
               "00-philosophy/cross-platform-build.md, gap K4) | core + "
               "metal leg (see wubuos-macos-leg-proof) | CONFIG |\n")
    return "".join(out)


def main():
    os.makedirs(OUT, exist_ok=True)
    files = {
        "modules.md": scan_modules(),
        "api.md": scan_api(),
        "commands.md": scan_commands(),
        "symbols.md": scan_symbols(),
        "tests.md": scan_tests(),
        "state.md": scan_state(),
        "parity.md": scan_parity(),
    }
    for fn, content in files.items():
        with open(os.path.join(OUT, fn), "w") as f:
            f.write(content)
        print(f"wrote 01-reference/{fn} ({len(content)} bytes)")


if __name__ == "__main__":
    main()
