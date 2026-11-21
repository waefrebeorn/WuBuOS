#!/usr/bin/env python3
"""Inject a real secret-dependent branch into a TEMP COPY of the shipped
bignum-free ladder.  The working tree is never mutated.  This is the shared
negative-control fixture for the single-config and multi-config CT gates.

Usage: ed25519_fixed_ladder_inject.py <source.c> <out.c>
"""
import sys

OLD = "        ge_add(&dbl, &r0, &r0);"
NEW = (OLD + "\n"
       "        if (bit) { dbl.X.l[0] ^= 1u; }  /* INJECTED CT REGRESSION */")

def main():
    if len(sys.argv) != 3:
        print("usage: ed25519_fixed_ladder_inject.py <source.c> <out.c>",
              file=sys.stderr)
        return 2
    src = open(sys.argv[1], encoding="utf-8").read()
    if src.count(OLD) != 1:
        print("[inject] FAIL: injection anchor is not unique (count=%d)"
              % src.count(OLD), file=sys.stderr)
        return 2
    open(sys.argv[2], "w", encoding="utf-8").write(src.replace(OLD, NEW))
    return 0

if __name__ == "__main__":
    sys.exit(main())
