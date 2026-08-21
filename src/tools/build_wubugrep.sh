#!/bin/sh
# Build WuBuGrep with maximum performance for this machine.
# -O3 + LTO + PGO + -march=native. Pure C11, zero third-party deps.
# Usage: sh tools/build_wubugrep.sh   (run from repo root)
set -e
cd "$(dirname "$0")"
echo "==> PGO instrumented build"
cc -O3 -flto -fprofile-generate -march=native -std=c11 -I. -o /tmp/wg_pgo_gen wubugrep.c wubre.c
# Train on representative workloads (create a small sample if no big corpus)
if [ -f /tmp/corpus.txt ]; then CORPUS=/tmp/corpus.txt; else CORPUS=/tmp/wg_train.txt; head -50000 /usr/share/dict/words >"$CORPUS" 2>/dev/null || printf 'a b c d e f g\n' >"$CORPUS"; fi
/tmp/wg_pgo_gen -F 'error' "$CORPUS" >/dev/null 2>&1 || true
/tmp/wg_pgo_gen -E '[a-z]+tion' "$CORPUS" >/dev/null 2>&1 || true
/tmp/wg_pgo_gen -G 'a*b' "$CORPUS" >/dev/null 2>&1 || true
echo "==> PGO optimized build -> wubugrep"
cc -O3 -flto -fprofile-use -fprofile-correction -march=native -std=c11 -I. -o wubugrep wubugrep.c wubre.c
rm -f /tmp/wg_pgo_gen /tmp/wg_pgo_gen-*.gcda /tmp/wg_pgo_gen-*.gcno wubugrep-*.gcda wubugrep-*.gcno
echo "==> built ./wubugrep ($(wc -c < wubugrep) bytes)"
echo "==> self-test"
cc -O2 -std=c11 -I. -o wubre_test wubre.c wubre_test.c && ./wubre_test | tail -1
