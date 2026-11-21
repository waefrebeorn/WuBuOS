#!/usr/bin/env bash
# eddsa_gimple_ct_gate.sh -- COMPILED-TREE constant-time gate for Ed25519 ladder.
#
# Next increment after tools/eddsa_ir_ct_gate.sh.  That gate needs clang
# -emit-llvm and, on this host (no clang), fails closed and can only exercise
# hand-authored .ll fixtures.  This gate uses the compiler we DO have: it asks
# a LOCAL gcc for the real `-fdump-tree-optimized` GIMPLE of the shipped ladder
# at -O2 and checks that no secret scalar-bit-derived SSA value controls a
# branch, switch or memory index.  Compiler output, not a model.
#
# HONESTY CONTRACT (fail closed, never a fake PASS):
#   * no local gcc with -fdump-tree-optimized/-O2 => FAIL, exit 1;
#   * no mbedTLS dev root / failed compile / no dump => FAIL, exit 1;
#   * dump without provenance, without GCC funcdef_no/PHI artifacts, without
#     @ge_mul_ladder, or without a taint source => the checker FAILs;
#   * the detector is proven non-vacuous by the `if (swap)` negative control
#     before the clean verdict is issued;
#   * no network, no install, no toolchain guessing.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
# shellcheck source=tools/eddsa_gimple_toolchain.sh
source "$ROOT/tools/eddsa_gimple_toolchain.sh"

TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-gimple-ct-gate.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT

fail_closed() {
    echo "[gimple_ct] FAIL: $*" >&2
    echo "GIMPLE_CT_GATE FAILED" >&2
    exit 1
}

echo "[gimple_ct] === GCC -O2 GIMPLE secret-dependence gate: ge_mul_ladder ==="

# 1) Prove the detector is not vacuous (needs no compiler for the fixture-free
#    parts, and compiles the injected regression when gcc is available).
echo '[gimple_ct] running injected `if (swap)` negative control first'
bash tools/eddsa_gimple_ct_negative_control.sh \
    || fail_closed "injected 'if (swap)' negative control / detector non-vacuity failed"

# 2) LOCAL toolchain discovery.
CC=$(find_gimple_gcc)
if [ -z "$CC" ]; then
    fail_closed "no local gcc with -fdump-tree-optimized/-O2; cannot produce real -O2 GIMPLE. NO PASS ASSERTED (fail closed)."
fi
echo "[gimple_ct] gcc: $($CC --version | head -1)"

# 3) Compile the SHIPPED source to a real -O2 GIMPLE dump.
NOTE="gcc $($CC --version | head -1) -O2 -fdump-tree-optimized shipped src/gui/wubu_pkg_ed25519.c"
emit_gimple_dump "$CC" "$ROOT/src/gui/wubu_pkg_ed25519.c" \
    "$TMP" "$NOTE" "$TMP/dump.gimple" \
    || fail_closed "could not emit a real -O2 GIMPLE dump from the shipped ladder source"
echo "[gimple_ct] emitted $(wc -l < "$TMP/dump.gimple") lines of real -O2 GIMPLE (compiler output, not a fixture)"

# 4) Analyze the compiled tree: no secret-derived branch / switch / index.
python3 tools/eddsa_gimple_ct_check.py "$TMP/dump.gimple" \
    || fail_closed "secret-dependent branch/switch/index survived gcc -O2 GIMPLE"

echo "[gimple_ct] PASS: no secret-derived branch/switch/index in real gcc -O2 GIMPLE of ge_mul_ladder"
