#!/usr/bin/env bash
# ed25519_fixed_timing_negctl.sh -- standalone negative control for the shipped
# fixed-path timing gate.  Rebuilds the same self-contained probe and requires
# that the detector FLAGS the deliberately secret-dependent double-and-add
# control (Welch |t| > 10).  A vacuous detector -- one that cannot see a real
# leak when one is present -- makes this FAIL.  Fail-closed: no toolchain, no
# sources, or build failure => non-zero, never a fabricated PASS.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-fixed-timing-negctl.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT

fail() { echo "[fixed_timing_negctl] FAIL: $*" >&2; echo "FIXED_TIMING_NEGCTL FAILED" >&2; exit 1; }

CC=${CC:-cc}
OPT=${EDDSA_TIMING_OPT:--O1}
command -v "$CC" >/dev/null 2>&1 || fail "no C compiler ($CC); negative control fails closed"
[ -f "$ROOT/tools/ed25519_fixed_timing_probe.c" ] || fail "probe source missing"
[ -f "$ROOT/src/gui/wubu_ed25519_fixed.c" ]        || fail "shipped fixed path missing"
[ -f "$ROOT/src/gui/wubu_ed25519_fixed.h" ]        || fail "shipped fixed header missing"

CFLAGS="-w $OPT -g -std=c11 -D_POSIX_C_SOURCE=200809L"
echo "[fixed_timing_negctl] compiler: $($CC --version 2>/dev/null | head -1) [$OPT]"
if ! $CC $CFLAGS -I"$ROOT/src/gui" \
        "$ROOT/tools/ed25519_fixed_timing_probe.c" -o "$TMP/probe" -lm \
        >"$TMP/build.log" 2>&1; then
    cat "$TMP/build.log" >&2
    fail "probe build failed; toolchain/harness incomplete"
fi

echo "[fixed_timing_negctl] --- detector must flag the secret-dependent control ---"
if ! "$TMP/probe" --negctl | tee "$TMP/probe.out"; then
    fail "probe --negctl returned non-zero"
fi
grep -q 'FIXED_TIMING_NEGCTL PASSED' "$TMP/probe.out" || {
    cat "$TMP/probe.out" >&2
    fail "no FIXED_TIMING_NEGCTL PASSED marker"
}
echo "[fixed_timing_negctl] PASS: detector flags the deliberately secret-dependent path (non-vacuous)."
