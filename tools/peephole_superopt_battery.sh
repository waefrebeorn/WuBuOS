#!/usr/bin/env bash
# peephole_superopt_battery.sh — regression gate for the peephole superoptimizer.
# Proves the discovery engine finds the minimal instruction sequences for known
# idioms (the same ones baked into jit_minic.c by hand). Each case asserts the
# tool REPORTS a program (shortest-first) for a target expression.
# Exit 0 only when all cases pass.
set -u
DIR="$(cd "$(dirname "$0")" && pwd)"
BIN="$DIR/peephole_superopt"
gcc -O2 -Wall -o "$BIN" "$DIR/peephole_superopt.c" 2>/dev/null || { echo "build failed"; exit 2; }

pass=0; fail=0
check() {
  local desc="$1"; shift
  local out
  out=$(timeout 30 "$BIN" "$@" 2>&1)
  if echo "$out" | grep -q "FOUND length"; then
    pass=$((pass+1)); echo "PASS: $desc -> $(echo "$out" | head -1)"
  else
    fail=$((fail+1)); echo "FAIL: $desc"
  fi
}
# Generalized (Hydra, #7): the found program must survive the 100k-random-seed
# verification, proving it is a general identity, not a seed-specific accident.
check_general() {
  local desc="$1"; shift
  local out
  out=$(timeout 30 "$BIN" "$@" 2>&1)
  if echo "$out" | grep -q "GENERAL (accepted)"; then
    pass=$((pass+1)); echo "PASS: $desc generalizes -> $(echo "$out" | head -1)"
  else
    fail=$((fail+1)); echo "FAIL: $desc did not generalize"
  fi
}

# x*3 == x+x+x (lea r,[r+r*2] analog)
check "x*3 (lea strength reduction)" "x*3" "MUL,ADD,SHL" 4 0 1 2 3 7 100 -7
# x*5 == x+x+x+x+x
check "x*5" "x*5" "MUL,ADD,SHL" 5 0 1 2 3 100 -7
# sign bit: (x>>63)&1 via SAR+NEG discovered
check "sign bit (x>>63)&1" "(x>>63)&1" "MUL,ADD,SHL,SHR,SAR,AND,OR,XOR,SUB,NEG,NOT" 4 0 1 -1 2 3 100 -7
# x*2 == x<<1 == x+x
check "x*2" "x*2" "MUL,ADD,SHL" 3 0 1 2 3 100 -7
# x*8 == x+x+... (8 adds; needs len 7 in the constant-free model). ADD-only so
# the search is 1^7 (instant); 11^7 with all ops would take too long.
check "x*8" "x*8" "ADD" 8 0 1 2 3 100 -7
# x|x == x (idempotent)
check "x|x idempotent" "x|x" "OR,AND,XOR" 2 0 1 -1 2 100
# x^0 == x via x^x^x
check "x^x^x == x" "x" "XOR" 3 0 1 -1 2 100
# x&-1 == x (mask)
check "x&-1" "x&-1" "AND,OR,NEG,NOT" 3 0 1 -1 2 100

# #7 Hydra: found programs must survive the 100k random-seed generalization
# check (proves they are general identities, not seed-specific accidents).
check_general "x*3 generalizes" "x*3" "ADD" 4 0 1 2 3 100 -7
check_general "x*5 generalizes" "x*5" "ADD" 5 0 1 2 3 100 -7
check_general "x^0==x generalizes" "x" "XOR" 3 0 1 -1 2 100

echo ""
echo "=== peephole_superopt_battery: $pass passed, $fail failed ==="
[ "$fail" -eq 0 ]
