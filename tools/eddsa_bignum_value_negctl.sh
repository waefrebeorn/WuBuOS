#!/usr/bin/env bash
# eddsa_bignum_value_negctl.sh -- non-vacuity control for the bignum VALUE
# auditor.  Compiles the same local callee fixture twice at -O2: once with a
# branch-free mask select, once with an INJECTED secret-dependent branch on an
# mpi limb value.  The auditor must accept the first and refute the second.
# The fixture is compiled locally; nothing is fetched and no shipped artifact
# is modified.  If the compiler is absent, the control FAILS CLOSED.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
# shellcheck source=tools/eddsa_gimple_toolchain.sh
source "$ROOT/tools/eddsa_gimple_toolchain.sh"

checker="$ROOT/tools/eddsa_bignum_value_check.py"
fixture="$ROOT/tests/fixtures/value_ct/callee_value_probe.c"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-bignum-negctl.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT

fail() {
    echo "[bnval_negctl] FAIL: $*" >&2
    echo "BIGNUM_NEGCTL FAILED" >&2
    exit 1
}

echo "[bnval_negctl] === injected secret-dependent-branch negative control (callee) ==="
CC=$(find_gimple_gcc)
[ -n "$CC" ] || fail "no local -O2 gcc; NO detector PASS asserted (fail closed)"
command -v objdump >/dev/null 2>&1 || fail "objdump absent"
command -v sha256sum >/dev/null 2>&1 || fail "sha256sum absent"
echo "[bnval_negctl] gcc: $($CC --version | head -1)"

marker() {
    local obj=$1 out=$2 note=$3 sha
    sha=$(sha256sum "$obj" | cut -d' ' -f1)
    { printf ';; WUBU_BIGNUM_VALUE: %s object_sha256=%s\n' "$note" "$sha"
      objdump -d -M intel "$obj"; } > "$out"
}

# --- clean callee: branch-free mask select, must NOT be flagged ---
"$CC" -O2 -c "$fixture" -o "$TMP/clean.o" || fail "clean fixture compile failed"
marker "$TMP/clean.o" "$TMP/clean.asm" "local gcc -O2 fixture CLEAN" || \
    fail "clean fixture disassembly failed"
if python3 "$checker" "$TMP/clean.asm" \
        --func bn_ct_limb_select --func bn_branchy_limb_select \
        --seed-ptr rdi,rsi,rdx --expect clean --min-insns 3 --min-jcc 0 \
        --allow-branchfree --max-print 0 >"$TMP/clean.log" 2>&1; then
    grep -E '^\[bnval\] (function|TOTAL)' "$TMP/clean.log" | tail -4
    echo "[bnval_negctl] CLEAN: branch-free callee accepted (0 secret-governed sites)"
else
    cat "$TMP/clean.log"
    fail "branch-free callee was flagged (blanket-false-positive auditor)"
fi

# --- injected callee: secret limb value governs a jcc, MUST be flagged ---
"$CC" -O2 -DWUBU_INJECT_VALUE_BRANCH -c "$fixture" -o "$TMP/injected.o" || \
    fail "injected fixture compile failed"
marker "$TMP/injected.o" "$TMP/injected.asm" \
    "local gcc -O2 fixture INJECTED secret-dependent branch" || \
    fail "injected fixture disassembly failed"
if python3 "$checker" "$TMP/injected.asm" \
        --func bn_ct_limb_select --func bn_branchy_limb_select \
        --seed-ptr rdi,rsi,rdx --expect leak --min-insns 3 --min-jcc 0 \
        --allow-branchfree --max-print 4 >"$TMP/injected.log" 2>&1; then
    grep -E '^\[bnval\]   bn_branchy_limb_select @.*JCC.*SECRET_GOVERNED' \
        "$TMP/injected.log" | head -2 || fail "detector fired for the wrong function"
    echo "[bnval_negctl] INJECTED: secret-dependent branch in the callee was DETECTED"
else
    cat "$TMP/injected.log"
    fail "injected secret-dependent branch was NOT detected (vacuous auditor)"
fi

echo "[bnval_negctl] PASS: clean callee green, injected secret-value branch red; detector is non-vacuous"
