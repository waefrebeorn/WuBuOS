#!/usr/bin/env python3
"""eddsa_ir_ct_check.py -- secret-dependence checker over LLVM IR text.

This is the compiler-IR increment after tools/eddsa_ladder_guard.py: the source
guard bounds the C text, this checker bounds the *compiled* -O2 LLVM IR.  It
reads a .ll file, taints the scalar-bit source (mbedtls_mpi_get_bit / an
explicit WUBU_CT_TAINT_SOURCE marker), propagates taint through SSA data flow
to a fixpoint, and fails on any secret-derived control flow (br/switch) or
secret-derived memory index/pointer (getelementptr / load / store).

Design rules that matter for honesty:
  * It FAILS CLOSED: no provenance marker, no target function, or no taint
    source is a FAIL, never a PASS.  Absence of evidence is not evidence of
    absence.
  * A secret `select` is permitted at IR level (data selection, usually a
    cmov).  It taints its RESULT but is not itself reported as a branch; the
    back-end lowering is out of scope here and is covered by the recorded
    disassembly evidence instead.
  * `--expect leak` inverts the verdict: it PASSES only when the requested
    violation is actually detected.  This is what makes the negative control
    non-vacuous -- a checker that never flags anything fails the control.

No compiler, no LLVM Python bindings, no network.  Pure stdlib IR-text parse.
"""


import argparse
import re
import sys
from typing import NoReturn

VALUE_RE = re.compile(r"%([-\w.$]+)")
DEF_RE = re.compile(r"^%([-\w.$]+)\s*=\s*(.*)$")
SOURCE_RE = re.compile(
    r"^%([-\w.$]+)\s*=\s*call\b.*?@(?:mbedtls_mpi_get_bit|WUBU_CT_TAINT_SOURCE)\s*\("
)
FUNC_RE = re.compile(r"^define\s+[^@]*@([-\w.$]+)\s*\(")


def emit(msg):
    print("[ir_ct_check] " + msg)


def die(msg: str) -> NoReturn:
    emit("FAIL: " + msg)
    print("IR_CT_CHECK FAILED")
    sys.exit(1)


def operands(instr):
    """SSA value operands of an instruction (the defining %dst excluded)."""
    body = instr
    if " = " in body:
        body = body.split(" = ", 1)[1]
    return VALUE_RE.findall(body)


def extract_function(lines, name):
    start = None
    for i, line in enumerate(lines):
        m = FUNC_RE.match(line.strip())
        if m and m.group(1) == name:
            start = i
            break
    if start is None:
        die("function @%s not found in IR (cannot verify)" % name)
    depth = 0
    for i in range(start, len(lines)):
        depth += lines[i].count("{") - lines[i].count("}")
        if depth == 0 and i > start:
            return [(start + j + 1, lines[start + j]) for j in range(i - start + 1)]
    die("unterminated definition of @%s" % name)


def analyze(path, fn, expect, leak_kind, flag_secret_select=False):
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            lines = fh.read().splitlines()
    except OSError as exc:
        die("cannot read %s: %s" % (path, exc))

    markers = [ln.strip() for ln in lines if ln.lstrip().startswith("; WUBU_IR_CT:")]
    if not markers:
        die("no '; WUBU_IR_CT:' provenance marker; optimization level unknown")
    emit("provenance: " + markers[0])
    if "O2" not in markers[0]:
        die("marker does not assert -O2; refusing to claim a compiled result")

    flines = extract_function(lines, fn)
    emit("function: @%s (%d IR lines)" % (fn, len(flines)))

    tainted = set()
    for _, raw in flines:
        m = SOURCE_RE.match(raw.strip())
        if m:
            tainted.add(m.group(1))
    if not tainted:
        die("no secret source (mbedtls_mpi_get_bit/WUBU_CT_TAINT_SOURCE) in @%s" % fn)
    emit("secret sources: " + ", ".join(sorted("%" + t for t in tainted)))

    # Monotone SSA taint propagation to a fixpoint.  Any value derived from a
    # tainted operand is tainted (conservative: includes call results, phi,
    # select, loads of tainted pointers, and GEP results).
    changed = True
    rounds = 0
    while changed:
        changed = False
        rounds += 1
        for _, raw in flines:
            s = raw.strip()
            m = DEF_RE.match(s)
            if not m:
                continue
            dst = m.group(1)
            if dst in tainted:
                continue
            if any(op in tainted for op in operands(s)):
                tainted.add(dst)
                changed = True
    emit("taint fixpoint: %d iteration(s), %d tainted SSA value(s)" % (
        rounds, len(tainted)))

    violations = []  # (kind, lineno, text, detail)
    for lineno, raw in flines:
        s = raw.strip()
        if not s or s.startswith(";"):
            continue
        # secret-derived data selection (opt-in only: a select/cmov is the
        # legitimate CT primitive, so the production gate leaves it alone;
        # the negative control enables this to catch `if (swap)` lowered to
        # select by the optimiser instead of a br).
        if flag_secret_select and " = select " in s:
            cond = VALUE_RE.search(s.split(" = select ", 1)[1])
            if cond and cond.group(1) in tainted:
                violations.append(("branch", lineno, s, "select cond %" + cond.group(1)))
            continue
        # secret-derived control flow
        if s.startswith("br i1 ") or s.startswith("switch "):
            m = VALUE_RE.search(s)
            if m and m.group(1) in tainted:
                violations.append(("branch", lineno, s, "%" + m.group(1)))
            continue
        # secret-derived memory index / pointer
        if "getelementptr" in s:
            bad = [op for op in operands(s) if op in tainted]
            if bad:
                violations.append(("index", lineno, s, ", ".join("%" + b for b in bad)))
            continue
        m = re.search(r"\bptr\s+(%[-\w.$]+)", s)
        if m and m.group(1) in tainted and (
                s.startswith("load ") or s.startswith("store ")):
            violations.append(("index", lineno, s, "%" + m.group(1)))

    if expect == "clean":
        if violations:
            for kind, lineno, text, detail in violations:
                emit("LEAK %s at line %d: %s (tainted: %s)" % (kind, lineno, text, detail))
            die("%d secret-dependent control/index site(s) survived -O2" % len(violations))
        emit("no secret-derived branch or memory index in @%s" % fn)
        print("IR_CT_CHECK PASS: clean -O2 IR; no secret-dependent branch or index")
        return

    # expect == leak: the detector must actually fire on the requested kind.
    if leak_kind == "any":
        hits = violations
    else:
        hits = [v for v in violations if v[0] == leak_kind]
    if not hits:
        die("--expect leak/%s but no such violation was detected (vacuous checker)"
            % leak_kind)
    for kind, lineno, text, detail in hits:
        emit("DETECTED %s at line %d: %s (tainted: %s)" % (kind, lineno, text, detail))
    print("IR_CT_CHECK PASS: leak detector fired (%d %s violation(s))"
          % (len(hits), leak_kind))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("irfile")
    ap.add_argument("--fn", default="ge_mul_ladder")
    ap.add_argument("--expect", choices=["clean", "leak"], default="clean")
    ap.add_argument("--leak-kind", choices=["any", "branch", "index"], default="any")
    ap.add_argument("--flag-secret-select", action="store_true",
                    help="report a secret-conditioned select as a violation (negative controls only)")
    args = ap.parse_args()
    analyze(args.irfile, args.fn, args.expect, args.leak_kind,
            args.flag_secret_select)


if __name__ == "__main__":
    main()
