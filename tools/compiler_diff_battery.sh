#!/bin/bash
# compiler_diff_battery.sh — THE BUG-BANK ENGINE (one command).
#
# Runs the full differential battery: every expression is compiled by
# OUR HolyC compiler AND by gcc, run, and compared. Any divergence is
# a FINDING — either our compiler is wrong (fix it) or gcc is (rare).
#
# The 33 expressions cover: arithmetic, precedence, nested parens,
# shifts, comparisons, ternaries, bitwise ops, logical ops. They are
# the regression net that caught the 6-bug rdi-clobber binop family
# (2026-08-04).
#
# Usage:  tools/compiler_diff_battery.sh
# Needs:  src/compiler/compiler_diff built (make test_holyc builds it,
#         or run the recipe in the wubuos-holyc-compiler skill).
cd "$(dirname "$0")/.." || exit 1
BIN=src/compiler/compiler_diff
if [ ! -x "$BIN" ]; then
    echo "compiler_diff not built — build it first (see wubuos-holyc-compiler skill)" >&2
    exit 1
fi
pass=0; fail=0
run() {
  out=$("$BIN" "$1" "$2" 2>&1)
  if echo "$out" | grep -q "FINDING"; then
    echo "FAIL: $1 (expected $2)"; echo "$out" | grep "ours"; fail=$((fail+1))
  elif echo "$out" | grep -q "agree"; then
    pass=$((pass+1))
  else
    echo "??? $1"; fail=$((fail+1))
  fi
}
run "1+2" 3
run "7*6" 42
run "(1<<4)|3" 19
run "100/7" 14
run "-5+10" 5
run "3>2 ? 1 : 0" 1
run "1<<2+1" 8
run "-2*-3" 6
run "5^3" 6
run "-7%3" -1
run "1|2&4" 1
run "((2+3)*4-6)/2" 7
run "1<2==1" 1
run "(1+2)*3" 9
run "(1|2)" 3
run "(5&3)" 1
run "1+(2*3)" 7
run "10-(3*2)" 4
run "1+(2+3)" 6
run "(1+2)+(3+4)" 10
run "5-(2-1)" 4
run "1<<(1+2)" 8
run "16>>(1+1)" 4
run "1<(2-1)" 0
run "(1+2)==(2+1)" 1
run "3>(1+1)" 1
run "2+3*4" 14
run "(2+3)*4" 20
run "1|2^3" 1
run "8>>1+1" 2
run "~0" -1
run "1&&0" 0
run "1||0" 1
echo "=== PASS: $pass FAIL: $fail ==="
[ "$fail" -eq 0 ]
