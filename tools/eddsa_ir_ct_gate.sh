#!/usr/bin/env bash
# eddsa_ir_ct_gate.sh -- COMPILER-IR constant-time gate for the Ed25519 ladder.
#
# This is the next increment after tools/eddsa_ladder_guard.sh: the source
# guard bounds the C text; this gate bounds what the compiler actually emits.
# It compiles the shipped ladder source to LLVM IR at -O2 with a LOCAL clang
# only, then runs tools/eddsa_ir_ct_check.py over @ge_mul_ladder: taint the
# scalar bit from mbedtls_mpi_get_bit, propagate through SSA data flow, and
# fail on any secret-derived branch or memory index that survived -O2.
#
# HONESTY CONTRACT (fail closed, never a fake PASS):
#   * no local clang capable of -emit-llvm -O2 => explicit FAIL, exit 1;
#   * no mbedTLS dev root / failed compile => explicit FAIL, exit 1;
#   * IR without a provenance -O2 marker, missing @ge_mul_ladder, or no
#     taint source => the checker FAILs;
#   * opt is optional and only used to -verify the IR; its absence is printed
#     and never relaxes the verdict.
#   No network, no install, no toolchain guessing.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
# shellcheck source=tools/eddsa_ir_toolchain.sh
source "$ROOT/tools/eddsa_ir_toolchain.sh"

TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-ir-ct-gate.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT

fail_closed() {
    echo "[ir_ct] FAIL: $*" >&2
    echo "IR_CT_GATE FAILED" >&2
    exit 1
}

echo "[ir_ct] === compiler-IR secret-dependence gate: ge_mul_ladder @ -O2 ==="

# 1) Prove the detector is not vacuous.  This part needs no compiler and is
#    also available as the standalone `test_ed25519_ir_ct_negctl` target.
echo '[ir_ct] running analyzer self-test + `if (swap)` negative control'
bash tools/eddsa_ir_ct_negative_control.sh || fail_closed "negative control / analyzer self-test failed"

# 2) Toolchain discovery -- LOCAL only.
CLANG=$(find_ir_clang)
OPT=$(find_ir_opt)
if [ -z "$CLANG" ]; then
    fail_closed "no local clang with -emit-llvm/-O2 is available; cannot produce real -O2 IR. NO PASS ASSERTED (fail closed)."
fi
echo "[ir_ct] clang: $($CLANG --version | head -1)"
echo "[ir_ct] opt: ${OPT:-not found (optional; -verify skipped, verdict unchanged)}"

# 3) Compile the shipped source to LLVM IR at -O2.
if ! emit_ladder_ir "$CLANG" "$ROOT/src/gui/wubu_pkg_ed25519.c" \
        "$TMP/ladder.ll" "clang -O2 shipped src/gui/wubu_pkg_ed25519.c"; then
    fail_closed "could not emit -O2 LLVM IR from the shipped ladder source"
fi
echo "[ir_ct] emitted $(wc -l < "$TMP/ladder.ll") lines of -O2 IR"

if [ -n "$OPT" ]; then
    if ! "$OPT" -verify "$TMP/ladder.ll" -o /dev/null; then
        fail_closed "opt -verify rejected the emitted IR"
    fi
    echo "[ir_ct] opt -verify: OK"
fi

# 4) Analyze the compiled IR: no secret-derived branch or memory index.
python3 tools/eddsa_ir_ct_check.py "$TMP/ladder.ll" \
    || fail_closed "secret-dependent branch or memory index survived -O2 compilation"

echo "[ir_ct] PASS: no secret-derived branch or memory index in -O2 IR of ge_mul_ladder"
