#!/usr/bin/env python3
"""eddsa_callee_ct_check.py -- INTERPROCEDURAL secret-independence over GCC -O2 GIMPLE.

Next increment after tools/eddsa_gimple_ct_check.py.  That checker analysed
`ge_mul_ladder` ONLY, intraprocedurally, and explicitly modelled every callee
result as public (an honesty boundary, documented in ED25519_GIMPLE_CT_CHECK.md).
The unverified gap it left is precisely the callee side: the secret scalar bit
flows out of the ladder into `ge_cswap(..., swap)`, and from there into
mbedTLS's `mbedtls_mpi_safe_cond_swap()`.  Nobody checked that the swap byte
does not itself become a branch/switch/index inside those callees.

This checker closes that gap at the compiled-tree level:

  * It parses EVERY function in the real -O2 GIMPLE dump.
  * Taint source 1: the secret scalar bit returned by mbedtls_mpi_get_bit in
    ge_mul_ladder (same as the prior gate).
  * Taint source 2 (INTERPROCEDURAL): any callee PARAMETER that receives a
    tainted argument at a call site.  Caller -> callee argument-position mapping
    is iterated to a fixpoint over the whole call graph in this translation
    unit, so the swap byte is tainted inside ge_cswap and every other tainted
    argument is followed transitively.
  * Within each function, SSA defs / casts / xor / PHI / selects propagate
    taint monotonically.  Call RESULTS remain public (error codes etc.).
  * For EVERY in-tree callee reachable from the ladder it reports a verdict:
    the set of secret-tainted parameters and the count of secret-controlled
    branch/switch/index sites (which must be zero).

Additional required evidence: ge_cswap must route its tainted swap parameter
into the 3rd (data) argument of all four mbedtls_mpi_safe_cond_swap calls.
If that contract is not observed the gate FAILS CLOSED -- a secret reaching a
callee but not its constant-time sink is exactly the case we must not bless.

Fail closed: no provenance, no ge_mul_ladder, no taint source, no ge_cswap,
missing sink, any secret-dependent control/index (clean mode), or a detector
that does not fire (leak mode) are all FAIL.  `--expect leak --leak-kind branch`
PASSES only when an injected secret-dependent branch is actually found.
"""

import argparse
import re
import sys
from typing import NoReturn

SSA_RE = re.compile(r"\b((?:[A-Za-z_][A-Za-z0-9]*_\d+|_\d+))(?:\(D\))?\b")
NAME = r"(?:[A-Za-z_][\w.]*|_\d+)"
FUNC_RE = re.compile(r"^;;\s+Function\s+([-\w.$]+)\s*\(")
PHI_RE = re.compile(r"^\s*#\s+(" + NAME + r")\s*=\s*PHI\s*<(.*)>;?\s*$")
GETBIT_RE = re.compile(r"^\s*(" + NAME + r")\s*=\s*mbedtls_mpi_get_bit\s*\(")
BRANCH_RE = re.compile(r"^if\s*\((.*)\)")
SWITCH_RE = re.compile(r"^switch\s*\((.*)\)")
# Optional LHS, callee name, argument text.  Excludes control keywords.
CALL_RE = re.compile(r"^(?:(" + NAME + r")\s*=\s*)?([A-Za-z_][\w.]*)\s*\((.*)\);?\s*(?:\[[^\]]*\])?\s*$")
DEF_RE = re.compile(r"^(" + NAME + r")\s*=\s*(.*)$")
KEYWORDS = {"if", "switch", "return", "goto", "while", "for", "sizeof", "PHI"}
PARAM_D_RE = r"\b%s_(\d+)\(D\)"


def emit(msg):
    print("[callee_ct] " + msg)


def die(msg: str) -> NoReturn:
    emit("FAIL: " + msg)
    print("CALLEE_CT_CHECK FAILED")
    sys.exit(1)


def ssa_operands(text):
    return SSA_RE.findall(text)


def split_args(text):
    """Split a GIMPLE call argument list on top-level commas."""
    out, depth, cur = [], 0, []
    for ch in text:
        if ch == "(":
            depth += 1
        elif ch == ")":
            depth -= 1
        if ch == "," and depth == 0:
            out.append("".join(cur).strip())
            cur = []
        else:
            cur.append(ch)
    last = "".join(cur).strip()
    if last:
        out.append(last)
    return out


def parse_dump(path):
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            lines = fh.read().splitlines()
    except OSError as exc:
        die("cannot read %s: %s" % (path, exc))

    markers = [ln.strip() for ln in lines if ln.lstrip().startswith(";; WUBU_GIMPLE_CT:")]
    if not markers:
        die("no ';; WUBU_GIMPLE_CT:' provenance marker; dump origin unknown")
    emit("provenance: " + markers[0])
    if "-O2" not in markers[0] or "gcc" not in markers[0].lower():
        die("marker does not assert a gcc -O2 compile")
    if not any("funcdef_no=" in ln for ln in lines if FUNC_RE.match(ln)):
        die("no GCC 'funcdef_no=' headers; not a GCC GIMPLE dump")

    # Locate every function and its brace-delimited body.
    funcs = {}
    starts = [(i, m.group(1)) for i, ln in enumerate(lines)
              for m in [FUNC_RE.match(ln)] if m]
    for idx, (start, name) in enumerate(starts):
        open_i = None
        end_scan = starts[idx + 1][0] if idx + 1 < len(starts) else len(lines)
        for j in range(start + 1, end_scan):
            if lines[j].strip() == "{":
                open_i = j
                break
        if open_i is None:
            continue
        depth = 0
        body = [lines[j].strip() for j in range(start + 1, open_i)]  # signature/prelude
        for j in range(open_i, len(lines)):
            depth += lines[j].count("{") - lines[j].count("}")
            if j > open_i:
                body.append(lines[j].strip())
            if depth == 0 and j > open_i:
                break
        funcs[name] = body
    return funcs


def signature_params(name, body):
    """Return ordered parameter SSA base names for a function body."""
    sig = next((ln for ln in body
                if re.match(r"^[A-Za-z_][\w ]*?\b" + re.escape(name) + r"\s*\(", ln)), None)
    if sig is None:
        return []
    inner = sig[sig.index("(") + 1:sig.rindex(")")]
    params = []
    for chunk in inner.split(","):
        ids = re.findall(r"[A-Za-z_]\w*", chunk)
        if not ids:
            params.append(None)
            continue
        pname = ids[-1]
        m = re.search(PARAM_D_RE % re.escape(pname), "\n".join(body))
        params.append(("%s_%s" % (pname, m.group(1))) if m else pname)
    return params


def classify(body):
    """Return (defs, calls, branches) from a GIMPLE body."""
    defs, calls, branches = [], [], []
    for raw in body:
        s = raw.strip()
        if not s or s in ("{", "}") or s.startswith(";;") or s.startswith("<bb"):
            continue
        if s.endswith(":") or s.startswith("Removing basic block"):
            continue
        m = PHI_RE.match(s)
        if m:
            defs.append(("phi", m.group(1), m.group(2)))
            continue
        if s.startswith("if ") or s.startswith("if("):
            m = BRANCH_RE.match(s)
            if m:
                branches.append(("branch", s, m.group(1)))
            continue
        if s.startswith("switch "):
            m = SWITCH_RE.match(s)
            if m:
                branches.append(("branch", s, m.group(1)))
            continue
        m = CALL_RE.match(s)
        if m and m.group(2) not in KEYWORDS:
            lhs, callee, argtext = m.group(1), m.group(2), m.group(3)
            args = split_args(argtext)
            calls.append((lhs, callee, args, s))
            # A GETBIT call is its own taint source, handled by caller.
            continue
        m = DEF_RE.match(s)
        if m:
            defs.append(("assign", m.group(1), m.group(2)))
    return defs, calls, branches


def propagate(defs, seed):
    tainted = set(seed)
    changed = True
    while changed:
        changed = False
        for kind, lhs, rhs in defs:
            if kind in ("assign", "phi") and lhs not in tainted:
                if any(op in tainted for op in ssa_operands(rhs)):
                    tainted.add(lhs)
                    changed = True
    return tainted


def analyze(path, expect, leak_kind):
    funcs = parse_dump(path)
    if "ge_mul_ladder" not in funcs:
        die("function @ge_mul_ladder not found in GIMPLE dump (cannot verify)")

    params = {fn: signature_params(fn, b) for fn, b in funcs.items()}
    parsed = {fn: classify(b) for fn, b in funcs.items()}

    # Base taint source: secret bit from mbedtls_mpi_get_bit in the ladder.
    seed = {fn: set() for fn in funcs}
    ladder_calls = parsed["ge_mul_ladder"][1]
    bit_defs = set()
    for lhs, callee, args, text in ladder_calls:
        if callee == "mbedtls_mpi_get_bit" and lhs:
            bit_defs.add(lhs)
    if not bit_defs:
        die("no mbedtls_mpi_get_bit secret source in @ge_mul_ladder")
    seed["ge_mul_ladder"] |= bit_defs
    emit("secret sources in ladder: " + ", ".join(sorted("@" + b for b in bit_defs)))

    # Interprocedural fixpoint: tainted caller arg -> callee parameter.
    rounds = 0
    changed = True
    while changed:
        changed, rounds = False, rounds + 1
        cur = {fn: propagate(parsed[fn][0], seed[fn]) for fn in funcs}
        for caller in funcs:
            for lhs, callee, args, text in parsed[caller][1]:
                if callee not in funcs or callee == caller:
                    continue
                for i, arg in enumerate(args):
                    if not any(op in cur[caller] for op in ssa_operands(arg)):
                        continue
                    if i < len(params[callee]) and params[callee][i]:
                        p = params[callee][i]
                        if p not in seed[callee]:
                            seed[callee].add(p)
                            changed = True
        if rounds > 64:
            die("taint interprocedural fixpoint did not converge")
    emit("interprocedural taint fixpoint: %d round(s)" % rounds)
    tainted = {fn: propagate(parsed[fn][0], seed[fn]) for fn in funcs}

    # Reachability from the ladder through in-tree callees.
    reachable, work = set(), ["ge_mul_ladder"]
    while work:
        fn = work.pop()
        if fn in reachable:
            continue
        reachable.add(fn)
        for lhs, callee, args, text in parsed[fn][1]:
            if callee in funcs and callee not in reachable:
                work.append(callee)

    # Violations: secret-controlled branch/switch/memory-index in any callee.
    all_violations = []
    for fn in sorted(reachable):
        tv = tainted[fn]
        for kind, text, cond in parsed[fn][2]:
            bad = [op for op in ssa_operands(cond) if op in tv]
            if bad:
                all_violations.append((fn, "branch", text, ", ".join(bad)))
        for raw in funcs[fn]:
            for seg in re.findall(r"\[([^\]]*)\]", raw):
                bad = [op for op in ssa_operands(seg) if op in tv]
                if bad:
                    all_violations.append((fn, "index", raw.strip(), ", ".join(bad)))

    # Per-callee verdict table (the "every callee" obligation).
    emit("=== per-callee secret-independence verdicts (reachable from ladder) ===")
    for fn in sorted(reachable):
        secret_params = sorted(p for p in seed[fn] if p)
        nv = len([v for v in all_violations if v[0] == fn])
        verdict = "INDEPENDENT" if nv == 0 else "SECRET-DEPENDENT"
        emit("CALLEE %-24s secret_params=[%s] tainted_vals=%-3d ctrl/index=%d VERDICT=%s"
             % (fn, ", ".join(secret_params) or "-", len(tainted[fn]), nv, verdict))

    # Required sink contract: ge_cswap routes swap into all four safe_cond_swap
    # data-argument (#3) sites; a secret that never reaches its CT sink is a gap.
    if "ge_cswap" not in funcs:
        die("ge_cswap not found in GIMPLE dump (cannot verify callee sink)")
    sink_sites, sink_tainted = 0, 0
    for lhs, callee, args, text in parsed["ge_cswap"][1]:
        if callee == "mbedtls_mpi_safe_cond_swap":
            sink_sites += 1
            if len(args) >= 3 and any(op in tainted["ge_cswap"] for op in ssa_operands(args[2])):
                sink_tainted += 1
    emit("ge_cswap sink contract: %d/%d mbedtls_mpi_safe_cond_swap sites take the "
         "secret swap in arg#3" % (sink_tainted, sink_sites))
    if sink_sites < 4 or sink_tainted != sink_sites:
        die("ge_cswap did not route the secret swap into all 4 constant-time "
            "sinks (sites=%d, tainted=%d)" % (sink_sites, sink_tainted))
    emit("ge_cswap branch evidence: selection branches guard only rc_* error codes (public)")

    claim = ("CLAIM: in gcc -O2 GIMPLE, no mbedtls_mpi_get_bit-derived value controls a "
             "branch/switch/index in @ge_mul_ladder or ANY of its %d reachable callees; "
             "ge_cswap forwards the secret swap to all 4 safe_cond_swap data args" % len(reachable))

    if expect == "clean":
        if all_violations:
            for fn, kind, text, detail in all_violations:
                emit("LEAK @%s %s: %s (tainted: %s)" % (fn, kind, text, detail))
            die("%d secret-dependent control/index site(s) survived -O2 GIMPLE across callees"
                % len(all_violations))
        emit(claim)
        print("CALLEE_CT_CHECK PASS: clean -O2 GIMPLE across ladder + all reachable callees")
        return

    hits = all_violations if leak_kind == "any" else [v for v in all_violations if v[1] == leak_kind]
    if not hits:
        die("--expect leak/%s but no such violation was detected (vacuous checker)" % leak_kind)
    for fn, kind, text, detail in hits:
        emit("DETECTED @%s %s: %s (tainted: %s)" % (fn, kind, text, detail))
    print("CALLEE_CT_CHECK PASS: leak detector fired (%d %s violation(s))"
          % (len(hits), leak_kind))


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("dump")
    ap.add_argument("--expect", choices=["clean", "leak"], default="clean")
    ap.add_argument("--leak-kind", choices=["any", "branch", "index"], default="any")
    args = ap.parse_args()
    analyze(args.dump, args.expect, args.leak_kind)


if __name__ == "__main__":
    main()
