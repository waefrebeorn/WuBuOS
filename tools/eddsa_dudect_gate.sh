#!/usr/bin/env bash
# eddsa_dudect_gate.sh -- dudect-style, NON-FORMAL statistical leakage gate
# for the Ed25519 signing scalar multiply.  Member of the DEFAULT test suite
# (`make test_ed25519_dudect`, wired into test_medium_other).
#
# One process, one clock, one scalar set.  The probe samples the SHIPPED
# Montgomery ladder and the legacy double-and-add negative control over two
# secret classes (fixed 256-bit length, low vs high Hamming weight), interleaved
# with a deterministic shuffle.  It computes Welch's t statistic for each and
# applies FIXED thresholds with NO retries:
#   |t_ladder| <= 10  -> shipped path shows no significant leak
#   |t_legacy|  > 10  -> the control proves the harness can detect a leak
# The legacy path is compiled into the PROBE ONLY (two-macro gate in the
# product source); it is never part of a shipped signing build.
#
# Like test_ed25519_ct_evidence, the mbedTLS dev root is required to build the
# real scalar-multiply core.  When no root is available the target SKIPs
# without fetching anything; it does not fabricate a PASS.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-eddsa-dudect.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT

if ! MB=$(bash "$ROOT/tools/mbedtls_devroot.sh" 2>/dev/null); then
    echo "[dudect] SKIP: mbedTLS dev root unavailable (no fetch); leakage gate not run."
    exit 0
fi
M="$MB/usr"
[ -f "$M/include/mbedtls/version.h" ] || { echo "[dudect] FAIL: missing mbedTLS headers" >&2; exit 1; }
CC=${CC:-cc}
CFLAGS="-w -O1 -g -std=c11 -D_POSIX_C_SOURCE=200809L -DWUBU_HAVE_MBEDTLS -DWUBU_NO_LIBM"
INCS="-I$ROOT/include -I$ROOT/src/gui -I$ROOT/src/runtime -I$ROOT/src/kernel -I$M/include"

echo "[dudect] mbedTLS: $(grep -m1 MBEDTLS_VERSION_STRING "$M/include/mbedtls/version.h" | cut -d'"' -f2)"
echo "[dudect] compiling single-run probe (shipped ladder + legacy control)"
# The probe #includes wubu_pkg_ed25519.c, so that file is NOT linked separately.
# shellcheck disable=SC2086
if ! $CC $CFLAGS $INCS -include "$ROOT/src/runtime/wubu_gnu_compat.h" \
        "$ROOT/tools/eddsa_dudect_probe.c" \
        -L"$M/lib/x86_64-linux-gnu" -lmbedcrypto -lm \
        -Wl,-rpath,"$M/lib/x86_64-linux-gnu" -o "$TMP/probe" >"$TMP/build.log" 2>&1; then
    cat "$TMP/build.log" >&2
    echo "[dudect] FAIL: probe build" >&2
    exit 1
fi

echo "[dudect] --- same-run baseline, fixed thresholds, no retries ---"
if ! LD_LIBRARY_PATH="$M/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
        "$TMP/probe" | tee "$TMP/probe.out"; then
    echo "[dudect] FAIL: probe reported a failure" >&2
    exit 1
fi
grep -q 'DUDECT_GATE PASSED' "$TMP/probe.out" || {
    echo "[dudect] FAIL: no DUDECT_GATE PASSED marker" >&2; exit 1; }
echo "[dudect] PASS: statistical leakage gate held (non-formal; see ED25519_CONSTANT_TIME_EVIDENCE.md)"
