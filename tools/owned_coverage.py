#!/usr/bin/env python3
"""owned_coverage.py -- every gap in the benchmark list must name its owner, and
every import must be declared. One source of truth for both.

WHY THIS EXISTS
    The benchmark doc (docs/compendium/04-roadmap/daily-driver-benchmarks.md) grades
    414 workflows. Some of them are "missing" only in the sense that the code lives
    in a repository we own and have not imported. Without this map, "missing" and
    "not imported" look identical from inside WuBuOS -- and that is how a real
    capability gets rebuilt by mistake.

WHAT IT CHECKS (fails loudly)
    1. every import this map declares is really in .gitmodules (or is the compiler
       symlink src/compiler -> ../wubunos), and really exists on disk;
    2. every submodule in .gitmodules is claimed by an entry here -- no orphan
       imports that nobody can explain;
    3. every gap family in the benchmark document is claimed by at least one owned
       repo, or is listed in UNOWNED with a reason.

WHAT IT PRINTS (informational, not a failure)
    the families whose owner is NOT imported yet: those are integration legs, not
    product legs.

Usage: tools/owned_coverage.py [--table]
Exit: 0 clean, 1 drift.
"""
import json
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BENCH = ROOT / "docs/compendium/04-roadmap/daily-driver-benchmarks.md"

# repo: (purpose, import path inside WuBuOS or None, local checkout or None, gap families it owns)
OWNED = {
    "WuBuOS":         ("Body -- the operating system", None, "wubuos", []),
    "WuBuWeb":        ("Web layer -- URL, HTTP/1.1, HTML to text", "src/web", "WuBuWeb", ["online"]),
    "wubuwizard":     ("Brain -- the AGI (inference/training, KV cache)", "src/brain", None, ["agentic", "agi"]),
    "WuBuNOS":        ("Compiler -- HolyD, MIR, 14 ISA backends", "src/compiler", "wubunos", ["live-compiler", "dev"]),
    "WuBuKernel":     ("Own C11 kernel, consumed by WuBuOS", "src/kernel/WuBuKernel", "WuBuKernel", ["system", "security"]),
    "wemu":           ("x86-64 emulator", "src/wemu", "wemu", ["dev", "gaming"]),
    "WuBuOffice":     ("Office suite -- OOXML/ODF from spec (25 apps)", "src/apps/wubuoffice", "opt/WuBuOffice", ["office", "a11y"]),
    "WuBuPad":        ("Code editor -- Notepad++ parity", "src/apps/wubupad", "opt/WuBuPad", ["dev"]),
    "WET":            ("Applet framework -- .net analogue", "src/gui/wet", "WET", ["live-gui", "day1"]),
    "VWS":            ("Forms designer -- Visual Basic analogue", "src/gui/vws", "VWS", ["live-gui", "dev"]),
    "WWW":            ("World-space engine", "src/lua/www", "opt/WWW", ["live-gui"]),
    "wubugrep":       ("From-scratch grep (SIMD matcher)", "src/tools/wubugrep", None, ["dev", "files"]),
    "big-mac":        ("Big Mac -- audio/media engine", "src/audio/bigmac", None, ["media"]),
    "wubucompositor": ("Own Wayland compositor -- the host seam", None, "wubucompositor", ["system", "live-gui"]),
    "wubulens":       ("GPU lens + task manager", None, "wubulens", ["system"]),
    "W-Code":         ("Agentic IDE, human + agent frontends", None, "W-Code", ["dev", "live-compiler", "agentic"]),
    "mindpalace":     ("Durable agent skill palace, prestige handoff", None, "mindpalace", ["agentic"]),
    "slermes":        ("The agent itself, one C11 binary", None, "opt/slermes", ["agentic"]),
    "WuBuHIVE":       ("Hive memory substrate -- stable-address state", None, "WuBuHIVE", ["agentic"]),
    "WuBuIntel":      ("The Bureau -- vigilance", None, None, ["edr"]),
    "WuBuCore":       ("Operations core -- citizenship, identity", None, None, ["security", "day1"]),
    "WuBuCitizen":    ("Registry of the First Ones", None, None, ["security"]),
    "WuBuGov":        ("Public digital state -- policy", None, None, ["privacy", "security"]),
    "WuBuEconomics":  ("Economics division -- licensing", None, None, ["admin"]),
    "WuBuMM":         ("Media engine -- omni-modal models, C11+Vulkan", None, None, ["media"]),
    "WuBuMedia":      ("Stream-native media layer", None, None, ["media"]),
    "wubucodec":      ("Video codec, 22/24 SOTA cats vs AV1", None, None, ["media"]),
    "wubuedit":       ("Video editor (SLERM)", None, None, ["media"]),
    "wuburvc":        ("Voice engine -- RVC in C11", None, None, ["media"]),
    "wubutv-network": ("WuBuTV -- media network", None, None, ["media"]),
    "wubusearch":     ("Universal embedding search over any media", None, None, ["files", "dev"]),
    "quantum_rng_audit": ("Secure RNG on quantum measurement simulation", None, None, ["security"]),
    "wubufw-tools":   ("Writes PE32+/FAT32 for WuBuFW by hand", None, None, ["system"]),
    "BearRL":         ("RL environment + proof engine", None, None, ["agentic"]),
    "WuBuMath":       ("C11 math + WUBQ codec", None, None, ["agentic"]),
    "distiller-c11":  ("Distiller One device path", None, None, ["system"]),
}

# Families with no owned repo AND no in-tree implementation: say why, so a future
# session does not go looking for a repo that does not exist.
UNOWNED = {
    "comms":   "no mail/chat/call client in any owned repo",
    "gaming":  "covered in-tree (Proton executor, game library), no separate repo",
    "a11y":    "partial owner: WuBuOffice carries the a11y modules; the shell has no owner",
    "privacy": "WuBuGov claims policy, nothing implements it yet",
    "admin":   "no fleet/MDM repo; WuBuEconomics is licensing only",
}

FAMILIES = ["online", "comms", "office", "media", "files", "system", "security", "a11y",
            "dev", "gaming", "day1", "admin", "agentic", "agi", "edr",
            "live-compiler", "live-gui", "privacy"]


def gitmodules():
    path = ROOT / ".gitmodules"
    if not path.exists():
        return {}
    text = path.read_text()
    out = {}
    for block in re.split(r"\[submodule", text)[1:]:
        m_path = re.search(r"path\s*=\s*(\S+)", block)
        m_url = re.search(r"url\s*=\s*(\S+)", block)
        if m_path:
            out[m_path.group(1)] = m_url.group(1) if m_url else ""
    return out


def main():
    mods = gitmodules()
    problems, notes = [], []

    # 1. every declared import is real
    for repo, (purpose, imp, local, fams) in OWNED.items():
        if not imp:
            continue
        if imp == "src/compiler":
            if not (ROOT / "src/compiler").exists():
                problems.append(f"{repo}: compiler symlink src/compiler is missing")
            continue
        if imp not in mods:
            problems.append(f"{repo}: import path {imp} is not in .gitmodules")
        if not (ROOT / imp).exists():
            problems.append(f"{repo}: import path {imp} does not exist on disk")

    # 2. no orphan submodules
    claimed = {imp for (_p, imp, _l, _f) in OWNED.values() if imp and imp != "src/compiler"}
    for path in mods:
        if path not in claimed:
            problems.append(f".gitmodules path {path} is not claimed by any owned repo in this map")

    # 3. every family has an owner (repo or an explicit reason)
    owned_fams = {f for (_p, _i, _l, fams) in OWNED.values() for f in fams}
    for fam in FAMILIES:
        if fam not in owned_fams and fam not in UNOWNED:
            problems.append(f"gap family '{fam}' has no owner and no reason in UNOWNED")

    # 4. the benchmark document must still talk about those families
    if BENCH.exists():
        text = BENCH.read_text()
        for fam in FAMILIES:
            if fam not in text:
                notes.append(f"benchmark doc never mentions family '{fam}'")

    if "--table" in sys.argv:
        print(f"{'repo':22s} {'imported':28s} {'local':22s} owns")
        for repo, (purpose, imp, local, fams) in sorted(OWNED.items()):
            print(f"{repo:22s} {(imp or '-'):28s} {(local or '-'):22s} {','.join(fams) or '-'}")
        print()
        not_imported = [(r, f) for r, (_p, i, _l, f) in OWNED.items() if f and not i]
        print(f"owned repos that own a gap family but are NOT imported: {len(not_imported)}")
        for r, fams in sorted(not_imported):
            print(f"  {r:20s} -> {','.join(fams)}")
        print()
        for fam in FAMILIES:
            owners = [r for r, (_p, i, _l, f) in OWNED.items() if fam in f]
            state = "owned+imported" if any(OWNED[r][1] for r in owners) else (
                    "owned, NOT imported" if owners else "no owner")
            print(f"  family {fam:15s} {state:20s} {','.join(owners) or UNOWNED.get(fam, '-')}")

    if problems:
        print("DRIFT:")
        for p in problems:
            print("  -", p)
    for n in notes:
        print("NOTE:", n)
    if not problems:
        print(f"owned_coverage: clean -- {len(OWNED)} owned repos, {len(mods)} submodules, "
              f"{len(FAMILIES)} gap families")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
