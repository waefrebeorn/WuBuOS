#!/usr/bin/env bash
# eddsa_callee_ct_gate.sh -- COMPILED + MACHINE-CODE constant-time gate for the
# EdDSA ladder CALLEES (the next real increment after the intraprocedural
# GIMPLE gate).  Three evidence legs, all from LOCALLY present artifacts:
#
#   1. injected secret-dependent-cswap negative control (both detectors proven
#      non-vacuous) -- run FIRST;
#   2. real gcc -O2 GIMPLE of the shipped ladder, interprocedural secret-taint
#      across @ge_mul_ladder and EVERY reachable callee, plus the ge_cswap ->
#      safe_cond_swap(data arg) sink contract (tools/eddsa_callee_ct_check.py);
#   3. the shipped mbedTLS 2.28.8 mbedtls_mpi_safe_cond_swap MACHINE CODE from
#      the local libmbedcrypto.a, register-taint audited for secret-governed
#      conditional jumps (tools/eddsa_callee_asm_check.py), plus the LOCAL
#      gcc -O2 lowering of a constant-time select to cmov (same auditor).
#
# HONESTY CONTRACT (fail closed, never a fake PASS): missing gcc/objdump/mbedTLS
# archive, a failed compile, a disassembly without the real function, a missing
# taint source/sink, a vacuous detector, or any secret-governed control site:
# all FAIL with exit 1.  No network, no install, no toolchain guessing.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
# shellcheck source=tools/eddsa_gimple_toolchain.sh
source "$ROOT/tools/eddsa_gimple_toolchain.sh"
# shellcheck source=tools/eddsa_callee_toolchain.sh
source "$ROOT/tools/eddsa_callee_toolchain.sh"

TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-callee-ct-gate.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT

fail_closed() {
    echo "[callee_ct] FAIL: $*" >&2
    echo "CALLEE_CT_GATE FAILED" >&2
    exit 1
}

echo "[callee_ct] === EdDSA ladder CALLEE secret-independence gate ==="

# 1) Non-vacuity first.
echo "[callee_ct] running injected secret-dependent-cswap negative control first"
bash tools/eddsa_callee_ct_negative_control.sh \
    || fail_closed "injected secret-dependent-cswap negative control failed"

# 2) LOCAL toolchain discovery.
CC=$(find_gimple_gcc)
[ -n "$CC" ] || fail_closed "no local gcc with -fdump-tree-optimized/-O2; NO PASS asserted (fail closed)"
command -v objdump >/dev/null 2>&1 || fail_closed "objdump absent; cannot audit shipped machine code"
echo "[callee_ct] gcc: $($CC --version | head -1)"

# 3) Real -O2 GIMPLE, interprocedural over the whole call graph.
NOTE="gcc $($CC --version | head -1) -O2 -fdump-tree-optimized shipped src/gui/wubu_pkg_ed25519.c"
emit_gimple_dump "$CC" "$ROOT/src/gui/wubu_pkg_ed25519.c" \
    "$TMP" "$NOTE" "$TMP/dump.gimple" \
    || fail_closed "could not emit a real -O2 GIMPLE dump from the shipped ladder source"
echo "[callee_ct] emitted $(wc -l < "$TMP/dump.gimple") lines of real -O2 GIMPLE (compiler output, not a fixture)"
python3 tools/eddsa_callee_ct_check.py "$TMP/dump.gimple" \
    || fail_closed "secret-dependent control/index across ladder/callees in real -O2 GIMPLE"

# 4) Shipped mbedTLS safe_cond_swap MACHINE CODE from the local static archive.
MB=$(resolve_mbedtls_root)
[ -n "$MB" ] || fail_closed "no local mbedTLS devroot/libmbedcrypto.a; cannot audit shipped callee machine code"
echo "[callee_ct] mbedTLS devroot: $MB ($(grep -m1 'MBEDTLS_VERSION_STRING ' "$MB/usr/include/mbedtls/version.h" | tr -d '\r'))"
disasm_safe_cond_swap "$TMP/mbedtls.asm" \
    || fail_closed "could not disassemble shipped mbedtls_mpi_safe_cond_swap from local libmbedcrypto.a"
echo "[callee_ct] disassembled $(grep -c ':' "$TMP/mbedtls.asm") lines of real mbedtls_mpi_safe_cond_swap object code"
python3 tools/eddsa_callee_asm_check.py "$TMP/mbedtls.asm" --taint-reg edx \
    --label "mbedtls_mpi_safe_cond_swap (shipped libmbedcrypto.a)" \
    || fail_closed "shipped safe_cond_swap has a secret-governed conditional jump"

# 5) LOCAL gcc -O2 lowering of a constant-time select -> cmov (branch-free).
gcc -O2 -c -o "$TMP/select.o" tests/fixtures/callee_ct/ct_select_probe.c \
    || fail_closed "could not compile the cmov lowering probe with local gcc -O2"
objdump -d -M intel "$TMP/select.o" > "$TMP/select.asm" \
    || fail_closed "could not disassemble the cmov lowering probe"
python3 tools/eddsa_callee_asm_check.py "$TMP/select.asm" --taint-reg edi \
    --require-cmov --label "gcc -O2 ct_select branch-free lowering" \
    || fail_closed "gcc -O2 did not lower the constant-time select to a branch-free cmov/setcc"

echo "[callee_ct] PASS: ladder + all reachable callees secret-independent in -O2 GIMPLE; shipped safe_cond_swap machine code has no secret-governed jcc; gcc -O2 select lowers to cmov"
