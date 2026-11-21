#!/usr/bin/env bash
# ed25519_fixed_full_gate.sh -- fully bignum-free fixed EdDSA path gate.
#
# Asserts, with real tool output and no network:
#   1. zero mbedTLS integer/bignum symbols in the shipped fixed-path sources;
#   2. zero such symbols in the PREPROCESSED default fixed-mode dispatcher
#      (gcc -E with -DWUBU_EDDSA_FIXED_LADDER -- the mbedTLS bignum header and
#      the whole legacy MPI backend must be compiled out);
#   3. detector non-vacuity: injected bignum symbols go RED (negctl), shipped
#      sources go GREEN;
#   4. RFC 8032 cross-check against OpenSSL: our signatures verify under EVP,
#      EVP signatures verify on our path, both match byte-for-byte, and the
#      canonical-S / small-subgroup / tamper negatives hold.
# Fails closed (exit 1, never a fake PASS) on any missing tool or obligation.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-fixed-full-gate.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT

fail_closed() {
    echo "[fixed_full] FAIL: $*" >&2
    echo "FIXED_FULL_GATE FAILED" >&2
    exit 1
}

echo "[fixed_full] === bignum-free fixed EdDSA path gate ==="
for t in python3 gcc; do command -v "$t" >/dev/null || fail_closed "no $t"; done
CHECKER="$ROOT/tools/ed25519_fixed_bignum_check.py"
FILES=(
    "$ROOT/src/gui/wubu_ed25519_fixed.c"
    "$ROOT/src/gui/wubu_ed25519_fixed.h"
    "$ROOT/src/gui/wubu_ed25519_fixed_sign.c"
    "$ROOT/src/gui/wubu_ed25519_fixed_sign.h"
)
for f in "${FILES[@]}"; do [ -f "$f" ] || fail_closed "missing $f"; done

echo "[fixed_full] (1/4) shipped fixed-path sources: zero bignum symbols"
python3 "$CHECKER" files "${FILES[@]}" || fail_closed "bignum symbol in shipped fixed path"

echo "[fixed_full] (2/4) preprocessed fixed-mode dispatcher: zero bignum symbols"
MB="$(bash "$ROOT/tools/mbedtls_devroot.sh")" || fail_closed "no mbedTLS dev root"
[ -f "$MB/usr/include/mbedtls/sha512.h" ] || fail_closed "no mbedTLS SHA-512 header"
INCLUDES="-I$ROOT -I$ROOT/include -I$ROOT/src -I$ROOT/src/gui -I$ROOT/src/runtime -I$ROOT/src/kernel -I$MB/usr/include"
gcc -E -std=c11 -DWUBU_HAVE_MBEDTLS -DWUBU_EDDSA_FIXED_LADDER -DWUBU_NO_LIBM \
    $INCLUDES -include "$ROOT/src/runtime/wubu_gnu_compat.h" \
    "$ROOT/src/gui/wubu_pkg_ed25519.c" -o "$TMP/fixed_pp.c" \
    || fail_closed "could not preprocess the fixed-mode dispatcher"
python3 "$CHECKER" pp "$TMP/fixed_pp.c" || fail_closed "bignum code compiled into the fixed-mode dispatcher"

echo "[fixed_full] (3/4) negative control: injected bignum must go RED"
bash "$ROOT/tools/ed25519_fixed_full_negctl.sh" || fail_closed "negative control did not prove detector non-vacuity"

echo "[fixed_full] (4/4) RFC 8032 cross-check against OpenSSL EVP"
[ -f /usr/include/openssl/evp.h ] || fail_closed "no OpenSSL EVP headers"
gcc -O2 -std=c11 -I"$ROOT/src/gui" -I"$MB/usr/include" \
    "$ROOT/tests/fixtures/fixed_full/probe.c" \
    "$ROOT/src/gui/wubu_ed25519_fixed.c" \
    "$ROOT/src/gui/wubu_ed25519_fixed_sign.c" \
    -o "$TMP/probe" \
    -L"$MB/usr/lib/x86_64-linux-gnu" -lmbedcrypto -lcrypto \
    -Wl,-rpath,"$MB/usr/lib/x86_64-linux-gnu" \
    || fail_closed "could not build the RFC 8032 cross-check probe"
LD_LIBRARY_PATH="$MB/usr/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    "$TMP/probe" || fail_closed "RFC 8032 cross-check failed"

echo "[fixed_full] PASS: fixed signing path is bignum-free, detector non-vacuous, RFC 8032 correct"
echo "FIXED_FULL_GATE PASSED"
