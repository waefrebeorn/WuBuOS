#!/usr/bin/env bash
# eddsa_bignum_value_gate.sh -- VALUE-INDEPENDENCE gate for the bignum callees
# used by the Ed25519 ladder (mbedtls_mpi_add/sub/mul/mod/cmp and their
# in-archive helpers).  This is the next real increment after the selection-bit
# callee-CT gate, which explicitly modelled bignum VALUES as public.
#
# Evidence legs, all from LOCALLY present artifacts (no fetch, no install):
#   1. injected secret-dependent-branch negative control in a locally compiled
#      callee fixture -- run FIRST (eddsa_bignum_value_negctl.sh);
#   2. bounded abstract interpretation of the SHIPPED bignum machine code from
#      the local libmbedcrypto.a (bignum.c.o), with a fixpoint secret-taint
#      lattice seeded on the secret-derived mpi * arguments and limb values;
#   3. an EXACT inventory of every non-public conditional jump / memory index.
#
# HONESTY CONTRACT: if the shipped object cannot be disassembled, a required
# function is missing, the provenance marker is absent, the auditor fails, or
# the negative control does not fire, the gate FAILS CLOSED.  If the audit
# finds non-public control/address sites (as the pinned 2.28.8 build does), the
# gate records the exact negative and REFUSES TO ASSERT A PASS (exit 1).
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
# shellcheck source=tools/eddsa_callee_toolchain.sh
source "$ROOT/tools/eddsa_callee_toolchain.sh"
# shellcheck source=tools/eddsa_bignum_toolchain.sh
source "$ROOT/tools/eddsa_bignum_toolchain.sh"

checker="$ROOT/tools/eddsa_bignum_value_check.py"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-bignum-value.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT

fail() {
    echo "[bignum_value] FAIL: $*" >&2
    echo "BIGNUM_VALUE_GATE FAILED" >&2
    exit 1
}

echo "[bignum_value] === EdDSA bignum VALUE-independence gate ==="

# 1) Non-vacuity first.
echo "[bignum_value] running injected secret-dependent-branch negative control"
bash "$ROOT/tools/eddsa_bignum_value_negctl.sh" \
    || fail "injected secret-dependent-branch negative control failed"

# 2) Local toolchain / devroot, no network.
command -v objdump >/dev/null 2>&1 || fail "objdump absent"
MB=$(resolve_mbedtls_root)
[ -n "$MB" ] || fail "no local mbedTLS devroot/libmbedcrypto.a; cannot audit shipped bignum machine code"
echo "[bignum_value] mbedTLS devroot: $MB"
disasm_bignum_object "$TMP/bignum.asm" \
    || fail "could not disassemble shipped bignum.c.o from local libmbedcrypto.a"
echo "[bignum_value] disassembled $(grep -c ':' "$TMP/bignum.asm") lines of REAL bignum.c.o ($(grep -m1 '^;; WUBU_BIGNUM_VALUE:' "$TMP/bignum.asm" | cut -d' ' -f4))"

# 3) Bounded abstract interpretation, one tailored seed set per shipped function.
#    format: function|secret mpi-pointer args|secret scalar args
AUDIT_SET=(
    "mbedtls_mpi_cmp_mpi|rdi,rsi|"
    "mbedtls_mpi_cmp_abs|rdi,rsi|"
    "add_sub_mpi|rdi,rsi,rdx|"
    "mbedtls_mpi_mul_mpi|rdi,rsi,rdx|"
    "mpi_mul_hlp|rsi,rdx|rdi,rcx"
    "mbedtls_mpi_div_mpi|rdi,rsi,rdx,rcx|"
    "mbedtls_mpi_mod_mpi|rdi,rsi,rdx|"
)

: > "$TMP/all.evidence"
total_secret=0
total_unver=0
total_bad=0
funcs=0
for spec in "${AUDIT_SET[@]}"; do
    fn=${spec%%|*}
    rest=${spec#*|}
    ptrs=${rest%%|*}
    vals=${rest#*|}
    args=( "$TMP/bignum.asm" --func "$fn" --expect negative
           --evidence-out "$TMP/$fn.ev" --max-print 0 )
    [ -n "$ptrs" ] && args+=( --seed-ptr "$ptrs" )
    [ -n "$vals" ] && args+=( --seed-val "$vals" )
    python3 "$checker" "${args[@]}" > "$TMP/$fn.log" 2>&1
    rc=$?
    case $rc in
        0|2) ;;
        *) cat "$TMP/$fn.log" >&2
           fail "auditor failed on $fn (rc=$rc)" ;;
    esac
    funcs=$((funcs + 1))
    grep -E '^\[bnval\] function ' "$TMP/$fn.log" | head -1
    counts=$(grep -oE 'secret_governed=[0-9]+ unverified=[0-9]+' "$TMP/$fn.log" | head -1)
    [ -n "$counts" ] || fail "no verdict counts emitted for $fn (vacuous audit)"
    s=${counts#secret_governed=}; s=${s%% *}
    u=${counts##*unverified=}
    total_secret=$((total_secret + s))
    total_unver=$((total_unver + u))
    total_bad=$((total_bad + s + u))
    [ -f "$TMP/$fn.ev" ] || fail "no evidence file written for $fn"
done

{ echo "[bnval] EXACT NON-PUBLIC INSTRUCTION LIST ($total_bad):"
  for spec in "${AUDIT_SET[@]}"; do
      fn=${spec%%|*}
      tail -n +2 "$TMP/$fn.ev"
  done
} > "$TMP/all.evidence"

echo "[bignum_value] audited $funcs shipped bignum functions; non-public sites: $total_bad (secret-governed=$total_secret, unverified=$total_unver)"

EVIDENCE_OUT=${WUBU_BIGNUM_EVIDENCE_OUT:-"$TMP/exact_negative.txt"}
if [ "$EVIDENCE_OUT" != "$TMP/exact_negative.txt" ]; then
    cp "$TMP/all.evidence" "$EVIDENCE_OUT" || fail "cannot write evidence file $EVIDENCE_OUT"
fi
echo "[bignum_value] FULL_EVIDENCE=$EVIDENCE_OUT"
echo "[bignum_value] exact non-public instructions (first 12 of $total_bad):"
sed -n '2,13p' "$TMP/all.evidence"

if [ "$total_bad" -eq 0 ]; then
    echo "[bignum_value] PASS: no secret-governed or unverified control/address sites in the audited shipped bignum functions"
    echo "BIGNUM_VALUE_GATE PASS"
    exit 0
fi

echo "[bignum_value] VERIFIED NEGATIVE: value-independence of the shipped mbedTLS bignum callees CANNOT be established from headers + libmbedcrypto.a."
echo "[bignum_value] $total_bad exact non-public instructions are listed in $EVIDENCE_OUT; they are NOT blessed as constant-time."
echo "[bignum_value] NO PASS ASSERTED. (Discharging them needs the mbedTLS C source / a value-range proof of the ladder preconditions, neither present in this devroot.)"
echo "BIGNUM_VALUE_GATE VERIFIED_NEGATIVE"
exit 1
