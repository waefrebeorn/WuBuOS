#!/usr/bin/env python3
"""eddsa_gimple_ct_check.py -- secret-dependence checker over GCC -O2 GIMPLE.

This is the next real increment after tools/eddsa_ir_ct_check.py.  That checker
needed clang -emit-llvm and, on a host without clang, could only run over
*hand-authored* .ll fixtures.  This checker consumes the ACTUAL compiler
output of the shipped ladder: the `-fdump-tree-optimized` GIMPLE dump produced
by a local GCC at -O2.  No clang, no z3, no LLVM bindings, no network, no
install -- just Python stdlib over text GCC really emitted for the source we
ship.

Machine-checkable claim
-----------------------
In the -O2 GIMPLE of `ge_mul_ladder`, no SSA value derived (by SSA data flow)
from the secret scalar bit returned by `mbedtls_mpi_get_bit` controls a
conditional branch, a switch, or a memory index.  That is the compiled-IR
counterpart of the source-level L1/L2/L3 guards; it closes the "a compiler
could reintroduce a data-dependent branch" gap recorded in
docs/ED25519_CONSTANT_TIME_EVIDENCE.md for THIS gcc/flags version.

Trust boundary (honesty, not an omission)
------------------------------------------
The analysis is deliberately INTRAPROCEDURAL over `ge_mul_ladder`:
  * the only taint source is the scalar bit call `mbedtls_mpi_get_bit`;
  * values flowing through SSA defs, casts, xor, phi and selects propagate
    taint monotonically to a fixpoint;
  * the RESULT of every other call (ge_cswap/ge_add/ge_copy/ge_grow/... and
    error codes) is modeled as PUBLIC.  These are the callee's contract: their
    constant-timeness is a separate obligation, already bounded at source by
    the C1 check in tools/eddsa_ladder_guard.py and by mbedTLS's documented
    safe_cond_swap assembly.  Without this boundary a legitimate constant-time
    cswap(..., swap) would taint its own return code and produce a false leak
    on every error check -- the detector must not cry wolf.

Fail closed
-----------
No provenance marker, missing function, a dump that is not recognisably GCC
GIMPLE, no taint source, or (with --expect leak) a detector that did not fire:
all are FAIL, never PASS.  Absence of evidence is not evidence of absence.

`--expect leak --leak-kind branch` inverts the verdict: it PASSES only when a
secret-dependent branch is actually found.  That is what makes the injected
`if (swap)` negative control non-vacuous.
"""

import argparse
import re
import sys
from typing import NoReturn

# GIMPLE SSA name: versioned (bit_37, swap_70, rc_40, i_73, n_35) or
# compiler-temporary (_1, _2, _71).  A trailing (D) marks a function parameter
# and is stripped so comparisons use the base name.
SSA_RE = re.compile(r"\b((?:[A-Za-z_][A-Za-z0-9]*_\d+|_\d+))(?:\(D\))?\b")
NAME = r"(?:[A-Za-z_][A-Za-z0-9_.]*|_\d+)"
FUNC_RE = re.compile(r"^;;\s+Function\s+([-\w.$]+)\s*\(")
PHI_RE = re.compile(r"^\s*#\s+(" + NAME + r")\s*=\s*PHI\s*<(.*)>;?\s*$")
SOURCE_RE = re.compile(
    r"^\s*(" + NAME + r")\s*=\s*(?:call\s+)?mbedtls_mpi_get_bit\s*\("
)
CALL_RE = re.compile(r"^\s*(" + NAME + r")\s*=\s*[A-Za-z_][\w.]*\s*\(")
DEF_RE = re.compile(r"^\s*(" + NAME + r")\s*=\s*(.*)$")
BRANCH_RE = re.compile(r"^\s*if\s*\(([^)]*)\)")
SWITCH_RE = re.compile(r"^\s*switch\s*\(([^)]*)\)")


def emit(msg):
    print("[gimple_ct] " + msg)


def die(msg: str) -> NoReturn:
    emit("FAIL: " + msg)
    print("GIMPLE_CT_CHECK FAILED")
    sys.exit(1)


def ssa_operands(text):
    """SSA value operands referenced in a GIMPLE expression string."""
    return SSA_RE.findall(text)


def extract_function(lines, name):
    """Return the {...} body lines of `;; Function <name> ...`."""
    start = None
    for i, line in enumerate(lines):
        m = FUNC_RE.match(line)
        if m and m.group(1) == name:
            start = i
            break
    if start is None:
        die("function @%s not found in GIMPLE dump (cannot verify)" % name)
    # The signature is followed by a lone '{'.  Skip the "Removing basic
    # block N" bookkeeping lines between the header and the body.
    open_i = None
    for j in range(start + 1, len(lines)):
        if lines[j].strip() == "{":
            open_i = j
            break
    if open_i is None:
        die("no opening brace for @%s in GIMPLE dump" % name)
    depth = 0
    for j in range(open_i, len(lines)):
        depth += lines[j].count("{") - lines[j].count("}")
        if depth == 0 and j > open_i:
            return lines[open_i + 1:j]
    die("unterminated definition of @%s" % name)


def analyze(path, fn, expect, leak_kind):
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            lines = fh.read().splitlines()
    except OSError as exc:
        die("cannot read %s: %s" % (path, exc))

    # Provenance: the gate prepends a marker asserting the real -O2 compile.
    markers = [ln.strip() for ln in lines if ln.lstrip().startswith(";; WUBU_GIMPLE_CT:")]
    if not markers:
        die("no ';; WUBU_GIMPLE_CT:' provenance marker; dump origin unknown")
    emit("provenance: " + markers[0])
    if "-O2" not in markers[0] or "gcc" not in markers[0].lower():
        die("marker does not assert a gcc -O2 compile; refusing to claim compiled evidence")

    # The dump must be recognisably GCC GIMPLE, not a hand-authored model.
    if not any("funcdef_no=" in ln for ln in lines if FUNC_RE.match(ln)):
        die("no GCC 'funcdef_no=' function headers; this is not a GCC GIMPLE dump")

    body = extract_function(lines, fn)
    emit("function: @%s (%d GIMPLE lines)" % (fn, len(body)))
    if not any("PHI <" in ln for ln in body):
        die("no SSA PHI nodes in @%s; dump is not an optimised GIMPLE SSA body" % fn)

    # ---- classify definitions -------------------------------------------
    defs = []  # (kind, lhs, rhs) ; kind in source/phi/call/assign
    for raw in body:
        s = raw.strip()
        if not s or s.startswith("#") and "PHI" not in s:
            continue
        m = SOURCE_RE.match(s)
        if m:
            defs.append(("source", m.group(1), s))
            continue
        m = PHI_RE.match(s)
        if m:
            defs.append(("phi", m.group(1), m.group(2)))
            continue
        m = CALL_RE.match(s)
        if m:
            # Call RESULT is public (see trust boundary in module docstring).
            defs.append(("call", m.group(1), None))
            continue
        m = DEF_RE.match(s)
        if m:
            defs.append(("assign", m.group(1), m.group(2)))

    tainted = set(lhs for kind, lhs, _ in defs if kind == "source")
    if not tainted:
        die("no secret source (mbedtls_mpi_get_bit) in @%s" % fn)
    emit("secret sources: " + ", ".join(sorted("@" + t for t in tainted)))

    # Monotone SSA taint propagation to a fixpoint.
    changed, rounds = True, 0
    while changed:
        changed, rounds = False, rounds + 1
        for kind, lhs, rhs in defs:
            if kind not in ("assign", "phi") or lhs in tainted:
                continue
            if any(op in tainted for op in ssa_operands(rhs)):
                tainted.add(lhs)
                changed = True
    emit("taint fixpoint: %d iteration(s), %d tainted SSA value(s)" % (
        rounds, len(tainted)))
    emit("tainted: " + ", ".join(sorted(tainted)))

    # Constant-trip-count evidence (L1 at compiled level): the loop PHI should
    # be seeded with the literal 256 (WUBU_ED25519_LADDER_BITS).  This is
    # evidence, not a hard gate: gcc may print the constant differently, and
    # the taint checks below already fail on a secret-dependent back edge.
    seed = [ln.strip() for ln in body if "PHI <" in ln and re.search(r"\b256\b", ln)]
    if seed:
        emit("trip-count evidence: " + seed[0])
    else:
        emit("trip-count evidence: literal 256 not observed in a PHI at this -O2 "
             "(optimization-dependent; back-edge taint check still applies)")

    # ---- violations ------------------------------------------------------
    violations = []  # (kind, text, detail)
    for raw in body:
        s = raw.strip()
        if not s:
            continue
        m = BRANCH_RE.match(s)
        if m:
            bad = [op for op in ssa_operands(m.group(1)) if op in tainted]
            if bad:
                violations.append(("branch", s, ", ".join(bad)))
            continue
        m = SWITCH_RE.match(s)
        if m:
            bad = [op for op in ssa_operands(m.group(1)) if op in tainted]
            if bad:
                violations.append(("branch", s, ", ".join(bad)))
            continue
        # secret-derived memory index / pointer offset
        for seg in re.findall(r"\[([^\]]*)\]", s):
            bad = [op for op in ssa_operands(seg) if op in tainted]
            if bad:
                violations.append(("index", s, ", ".join(bad)))

    claim = ("CLAIM: in gcc -O2 GIMPLE of @%s, no mbedtls_mpi_get_bit-derived "
             "SSA value controls a branch, switch or memory index" % fn)

    if expect == "clean":
        if violations:
            for kind, text, detail in violations:
                emit("LEAK %s: %s (tainted: %s)" % (kind, text, detail))
            die("%d secret-dependent control/index site(s) survived -O2 GIMPLE" % len(violations))
        emit(claim)
        print("GIMPLE_CT_CHECK PASS: clean -O2 GIMPLE; no secret-dependent branch/switch/index")
        return

    # expect == leak: the detector must actually fire on the requested kind.
    hits = violations if leak_kind == "any" else [v for v in violations if v[0] == leak_kind]
    if not hits:
        die("--expect leak/%s but no such violation was detected (vacuous checker)" % leak_kind)
    for kind, text, detail in hits:
        emit("DETECTED %s: %s (tainted: %s)" % (kind, text, detail))
    print("GIMPLE_CT_CHECK PASS: leak detector fired (%d %s violation(s))"
          % (len(hits), leak_kind))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("dump")
    ap.add_argument("--fn", default="ge_mul_ladder")
    ap.add_argument("--expect", choices=["clean", "leak"], default="clean")
    ap.add_argument("--leak-kind", choices=["any", "branch", "index"], default="any")
    args = ap.parse_args()
    analyze(args.dump, args.fn, args.expect, args.leak_kind)


if __name__ == "__main__":
    main()
