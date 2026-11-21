#!/usr/bin/env bash
# ed25519_fixed_timing_gate.sh -- dudect-style, NON-FORMAL timing gate for the
# SHIPPED DEFAULT fixed-width Ed25519 signing path (WUBU_EDDSA_FIXED_LADDER,
# src/gui/wubu_ed25519_fixed.c).  Builds a SELF-CONTAINED probe -- no mbedTLS
# bignum, no network fetch -- that times the real shipped ladder against a
# deliberately secret-dependent control in one interleaved same-run pass with
# fixed Welch-t thresholds and no retries.  FAILS CLOSED if the compiler,
# sources, or harness are absent: no SKIP, no fabricated PASS.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-fixed-timing.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT

fail() { echo "[fixed_timing] FAIL: $*" >&2; exit 1; }

CC=${CC:-cc}
OPT=${EDDSA_TIMING_OPT:--O1}
command -v "$CC" >/dev/null 2>&1 || fail "no C compiler ($CC); timing gate fails closed"
[ -f "$ROOT/tools/ed25519_fixed_timing_probe.c" ] || fail "probe source missing"
[ -f "$ROOT/src/gui/wubu_ed25519_fixed.c" ]        || fail "shipped fixed path missing"
[ -f "$ROOT/src/gui/wubu_ed25519_fixed.h" ]        || fail "shipped fixed header missing"

CFLAGS="-w $OPT -g -std=c11 -D_POSIX_C_SOURCE=200809L"
echo "[fixed_timing] compiler: $($CC --version 2>/dev/null | head -1) [$OPT]"
echo "[fixed_timing] building self-contained probe (shipped bignum-free ladder + variable control)"
if ! $CC $CFLAGS -I"$ROOT/src/gui" \
        "$ROOT/tools/ed25519_fixed_timing_probe.c" -o "$TMP/probe" -lm \
        >"$TMP/build.log" 2>&1; then
    cat "$TMP/build.log" >&2
    fail "probe build failed; toolchain/harness incomplete"
fi

echo "[fixed_timing] --- same-run interleaved pass, fixed thresholds, no retries ---"
if ! "$TMP/probe" | tee "$TMP/probe.out"; then
    fail "probe returned non-zero"
fi
grep -q 'FIXED_TIMING_GATE PASSED' "$TMP/probe.out" || {
    cat "$TMP/probe.out" >&2
    fail "no FIXED_TIMING_GATE PASSED marker"
}
echo "[fixed_timing] PASS: shipped fixed path flat across secret classes AND variable control flagged (non-vacuous)."
echo "[fixed_timing] Non-formal empirical evidence only -- see docs/ED25519_FIXED_TIMING.md."
