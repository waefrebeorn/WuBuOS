#!/bin/bash
# wuburuntime_demo.sh — THE WUBURUNTIME, COMPLETE (one command).
#
# Builds and runs the full wuburuntime story end-to-end:
#   1. test_runtime        — the registry + personality oracles
#   2. holyc CLI           — the broker flags (-space, -personality,
#                            -i_make_shit_code -space)
#   3. -brainfuck          — the meme flag, compiled for real
#   4. persistence         — spaces survive process exit (the
#                            "nothing left in the dust" guarantee)
#   5. -spaces             — the disorganization, solved
#
# Usage:  tools/wuburuntime_demo.sh
cd "$(dirname "$0")/.." || exit 1
set -e

echo "================ 1. test_runtime (registry + personalities) ================"
make -s test_runtime 2>&1 | tail -3

echo ""
echo "================ 2. build the holyc CLI ================"
make -s holyc 2>&1 | grep -cE " error" || true

HOLYC=src/compiler/holyc
echo '1+2*3;' > /tmp/hc_demo.c
echo 'print("python but we ballin"); 42;' > /tmp/py_demo.py
rm -f /tmp/wuburuntime.spaces

echo ""
echo "================ 3. the compiler flags (the joke, shipped) ================"
echo "--- C11 (the sacred tongue) ---"
"$HOLYC" /tmp/hc_demo.c
echo "--- -brainfuck (the meme) ---"
"$HOLYC" -brainfuck '++++++++[>++++[>++>+++>+++>+<<<<-]>+>+>->>+[<]<-]>>.>---.+++++++..+++.>>.<-.<.+++.------.--------.>>+.>++.'
echo ""

echo ""
echo "================ 4. the broker (-space / -personality) ================"
"$HOLYC" -space java-jvm-21 /tmp/hc_demo.c
"$HOLYC" -space dotnet-clr-9 -personality posix /tmp/hc_demo.c
"$HOLYC" -i_make_shit_code -space wasm-instance-1 /tmp/py_demo.py

echo ""
echo "================ 5. persistence: a NEW invocation sees the same spaces ================"
"$HOLYC" -spaces

echo ""
echo "=== wuburuntime: COMPLETE ==="
