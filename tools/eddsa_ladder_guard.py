#!/usr/bin/env python3
"""eddsa_ladder_guard.py -- ROOT-INDEPENDENT structural constant-time guard
for the shipped Ed25519 Montgomery ladder.

This reads src/gui/wubu_pkg_ed25519.c as TEXT only.  It needs no compiler, no
mbedTLS headers, no dev root and no network: it can run in a completely
rootless build and therefore closes the blind spot left by the dudect/CT
evidence gates (which SKIP when no mbedTLS dev root is present).

It mechanically checks the structural invariants that make ge_mul_ladder
constant-time:

  L1  the loop trip count is the compile-time constant
      WUBU_ED25519_LADDER_BITS (never mbedtls_mpi_bitlen / secret data);
  L2  the secret scalar state (`swap` / `bit`) never selects a branch, a
      ternary or a memory index anywhere in the ladder;
  L3  every conditional-selection call is ge_cswap(..., swap) -- driven by the
      ladder state, not by a constant -- and is unconditional;
  C1  ge_cswap() selects all four projective coordinates through mbedTLS's
      mbedtls_mpi_safe_cond_swap() and contains no secret-dependent branch.

The negative control in tools/default_crypto_negative_control.sh injects an
`if (swap) ge_cswap(..., 1)` secret-dependent cswap.  Checks L2 and L3 both
catch it, so this guard goes RED with no root available.

This is structural evidence, NOT a formal constant-time proof.  It bounds the
C source shipped in this tree; it does not bound compiler codegen or the
upstream mbedTLS assembly (see docs/ED25519_CONSTANT_TIME_EVIDENCE.md).
"""
import re
import sys
from typing import NoReturn


def die(msg: str) -> NoReturn:
    print("[ladder_guard] FAIL: " + msg)
    print("LADDER_GUARD FAILED")
    sys.exit(1)


def strip_comments(s):
    out, i, n = [], 0, len(s)
    while i < n:
        if s[i:i + 2] == "/*":
            j = s.find("*/", i + 2)
            i = n if j < 0 else j + 2
            out.append(" ")
        elif s[i:i + 2] == "//":
            j = s.find("\n", i + 2)
            i = n if j < 0 else j + 1
            out.append("\n")
        else:
            out.append(s[i])
            i += 1
    return "".join(out)


def extract_function(src, name):
    """Return the {...} body of `name(...) { ... }` with comments stripped."""
    m = re.search(r"\b" + re.escape(name) + r"\s*\([^;{]*\)\s*\{", src)
    if not m:
        return None
    open_brace = m.end() - 1
    depth, i, n = 0, open_brace, len(src)
    while i < n:
        c = src[i]
        if c == "/" and i + 1 < n and src[i + 1] == "*":
            j = src.find("*/", i + 2)
            i = n if j < 0 else j + 2
            continue
        if c == "/" and i + 1 < n and src[i + 1] == "/":
            j = src.find("\n", i + 2)
            i = n if j < 0 else j + 1
            continue
        if c in ("\"", "'"):
            q, i = c, i + 1
            while i < n:
                if src[i] == "\\":
                    i += 2
                    continue
                if src[i] == q:
                    i += 1
                    break
                i += 1
            continue
        if c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return strip_comments(src[open_brace:i + 1])
        i += 1
    return None


def main():
    if len(sys.argv) != 2:
        die("usage: eddsa_ladder_guard.py <path-to-wubu_pkg_ed25519.c>")
    path = sys.argv[1]
    try:
        with open(path, encoding="utf-8") as fh:
            src = fh.read()
    except OSError as exc:
        die("cannot read %s: %s" % (path, exc))

    print("[ladder_guard] source: %s (text-only, no mbedTLS root needed)" % path)

    ladder = extract_function(src, "ge_mul_ladder")
    if ladder is None:
        die("ge_mul_ladder() definition not found")

    # L1 -- constant trip count, no secret-dependent iteration count.
    mdef = re.search(r"^\s*#\s*define\s+WUBU_ED25519_LADDER_BITS\s+([0-9]+)u?\b",
                     src, re.M)
    if not mdef:
        die("WUBU_ED25519_LADDER_BITS is not a numeric compile-time constant")
    bits = int(mdef.group(1))
    if not re.search(r"\bfor\s*\([^)]*WUBU_ED25519_LADDER_BITS", ladder):
        die("ladder loop is not bounded by WUBU_ED25519_LADDER_BITS")
    if "mbedtls_mpi_bitlen" in ladder:
        die("secret-dependent trip count (mbedtls_mpi_bitlen) in the ladder")
    print("[ladder_guard] L1 PASS: trip count = compile-time constant %d, no bitlen" % bits)

    # L2 -- the secret state may only flow through swap ^= bit, never into a
    # branch, a ternary or a memory index.
    branch = re.search(r"\b(if|while|for|switch)\s*\([^)]*\b(swap|bit)\b[^)]*\)", ladder)
    if branch:
        die("secret-dependent branch in ladder: %s" % branch.group(0).strip())
    if re.search(r"\b(swap|bit)\b\s*\?", ladder) or \
       re.search(r"\?\s*[^:;]*\b(swap|bit)\b\s*:", ladder):
        die("secret-dependent ternary in ladder")
    if re.search(r"\[[^]]*\b(swap|bit)\b[^]]*\]", ladder):
        die("secret-indexed memory access in ladder")
    print("[ladder_guard] L2 PASS: no secret-dependent branch / ternary / index in ladder")

    # L3 -- conditional selection must be ge_cswap(..., swap), unconditional.
    all_calls = re.findall(r"\bge_cswap\s*\(", ladder)
    swap_calls = re.findall(r"\bge_cswap\s*\([^)]*,\s*swap\s*\)", ladder)
    if not all_calls:
        die("ladder makes no ge_cswap() conditional-selection call")
    if len(all_calls) != len(swap_calls):
        die("a ge_cswap() call is not driven by `swap` (%d of %d are)" %
            (len(swap_calls), len(all_calls)))
    print("[ladder_guard] L3 PASS: %d/%d ge_cswap calls driven by `swap` (unconditional)" %
          (len(swap_calls), len(all_calls)))

    # C1 -- the selection primitive itself must be constant-time for all four
    # projective coordinates and free of secret branches.
    cswap = extract_function(src, "ge_cswap")
    if cswap is None:
        die("ge_cswap() definition not found")
    safe = re.findall(r"\bmbedtls_mpi_safe_cond_swap\s*\(", cswap)
    if len(safe) < 4:
        die("ge_cswap() has %d safe_cond_swap calls, expected >= 4" % len(safe))
    for coord in ("x", "y", "z", "t"):
        if "&a->%s" % coord not in cswap or "&b->%s" % coord not in cswap:
            die("ge_cswap() does not select coordinate %s" % coord)
    if re.search(r"\b(if|while|switch)\s*\([^)]*\bswap\b[^)]*\)", cswap):
        die("secret-dependent branch inside ge_cswap()")
    if re.search(r"\bswap\b\s*\?", cswap):
        die("secret-dependent ternary inside ge_cswap()")
    print("[ladder_guard] C1 PASS: ge_cswap selects x,y,z,t via safe_cond_swap, no secret branch")

    print("[ladder_guard] PASS: root-independent structural ladder guard held")
    print("LADDER_GUARD PASSED")
    return 0


if __name__ == "__main__":
    sys.exit(main())
