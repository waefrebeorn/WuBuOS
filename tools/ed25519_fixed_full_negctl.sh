#!/usr/bin/env bash
# ed25519_fixed_full_negctl.sh -- prove the zero-bignum gate is not vacuous.
# Injects a real mbedTLS integer symbol into TEMP COPIES only (the working tree
# is never mutated), requires both checker modes to go RED, then requires the
# shipped fixed path to stay GREEN.  Fails closed if a tool is missing.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-fixed-full-negctl.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT
CHECKER="$ROOT/tools/ed25519_fixed_bignum_check.py"
SRC_SIGN="$ROOT/src/gui/wubu_ed25519_fixed_sign.c"
SRC_PKG="$ROOT/src/gui/wubu_pkg_ed25519.c"

echo "[negctl] === zero-bignum gate negative control ==="
python3 - "$SRC_SIGN" "$TMP/injected_sign.c" <<'PY' || exit 2
import sys
src = open(sys.argv[1], encoding="utf-8").read()
anchor = "int wubu_ed25519_fixed_sign(const uint8_t digest[32], const uint8_t seed[32],\n"
needle = anchor + "                            uint8_t out_sig[64]) {\n"
inject = needle + "    mbedtls_mpi injected_bignum;  /* INJECTED BIGNUM SYMBOL */\n    (void)injected_bignum;\n"
if src.count(needle) != 1:
    print("[negctl] injection anchor not unique", file=sys.stderr); sys.exit(2)
open(sys.argv[2], "w", encoding="utf-8").write(src.replace(needle, inject))
PY
grep -q "INJECTED BIGNUM SYMBOL" "$TMP/injected_sign.c" || { echo "[negctl] FAIL: file injection missing"; exit 1; }
echo "[negctl] injected mbedtls_mpi into a temp copy of wubu_ed25519_fixed_sign.c"
if python3 "$CHECKER" files \
    "$ROOT/src/gui/wubu_ed25519_fixed.c" \
    "$ROOT/src/gui/wubu_ed25519_fixed.h" \
    "$TMP/injected_sign.c" \
    "$ROOT/src/gui/wubu_ed25519_fixed_sign.h" >"$TMP/files.log" 2>&1; then
    echo "[negctl] FAIL: files checker ACCEPTED an injected bignum symbol" >&2
    cat "$TMP/files.log"; exit 1
fi
grep -q "banned symbol .*mbedtls_mpi" "$TMP/files.log" || {
    echo "[negctl] FAIL: files checker failed for a reason other than the injected symbol" >&2
    cat "$TMP/files.log"; exit 1
}
echo "[negctl] files checker RED as required:"
grep -E "FAIL|BIGNUM_CHECK FAILED" "$TMP/files.log" | head -3

# A second, independent injection into the fixed-mode dispatcher source: the
# preprocessed fixed branch must not contain any integer symbol either.
mkdir -p "$TMP/injected"
python3 - "$SRC_PKG" "$TMP/injected/wubu_pkg_ed25519.c" <<'PY' || exit 2
import sys
src = open(sys.argv[1], encoding="utf-8").read()
anchor = "        wubu_pkg_sign_set_err(err, errsz, \"Ed25519 sign failed (fixed-width bignum-free path)\");\n"
inject = "    mbedtls_mpi injected;  /* INJECTED BIGNUM SYMBOL */\n    (void)injected;\n" + anchor
if src.count(anchor) != 1:
    print("[negctl] pkg injection anchor not unique", file=sys.stderr); sys.exit(2)
open(sys.argv[2], "w", encoding="utf-8").write(src.replace(anchor, inject))
PY
grep -q "INJECTED BIGNUM SYMBOL" "$TMP/injected/wubu_pkg_ed25519.c" || { echo "[negctl] FAIL: pkg injection missing"; exit 1; }
echo "[negctl] injected mbedtls_mpi into a temp copy of wubu_pkg_ed25519.c (fixed branch)"

command -v gcc >/dev/null || { echo "[negctl] FAIL: no gcc; cannot prove pp non-vacuity" >&2; exit 1; }
MB="$(bash "$ROOT/tools/mbedtls_devroot.sh")" || exit 2
INCLUDES="-I$ROOT -I$ROOT/include -I$ROOT/src -I$ROOT/src/gui -I$ROOT/src/runtime -I$ROOT/src/kernel -I$MB/usr/include"
gcc -E -std=c11 -DWUBU_HAVE_MBEDTLS -DWUBU_EDDSA_FIXED_LADDER -DWUBU_NO_LIBM \
    -I"$TMP/injected" $INCLUDES -include "$ROOT/src/runtime/wubu_gnu_compat.h" \
    "$TMP/injected/wubu_pkg_ed25519.c" -o "$TMP/injected_pp.c" || exit 2
if python3 "$CHECKER" pp "$TMP/injected_pp.c" >"$TMP/pp.log" 2>&1; then
    echo "[negctl] FAIL: pp checker ACCEPTED an injected bignum symbol" >&2
    cat "$TMP/pp.log"; exit 1
fi
grep -q "banned symbol .*mbedtls_mpi" "$TMP/pp.log" || {
    echo "[negctl] FAIL: pp checker failed for a reason other than the injected symbol" >&2
    cat "$TMP/pp.log"; exit 1
}
echo "[negctl] pp checker RED as required:"
grep -E "FAIL|BIGNUM_CHECK FAILED" "$TMP/pp.log" | head -3

echo "[negctl] restoring SHIPPED fixed path and requiring GREEN again"
python3 "$CHECKER" files \
    "$ROOT/src/gui/wubu_ed25519_fixed.c" \
    "$ROOT/src/gui/wubu_ed25519_fixed.h" \
    "$SRC_SIGN" \
    "$ROOT/src/gui/wubu_ed25519_fixed_sign.h" >/dev/null || { echo "[negctl] FAIL: shipped files not green" >&2; exit 1; }
gcc -E -std=c11 -DWUBU_HAVE_MBEDTLS -DWUBU_EDDSA_FIXED_LADDER -DWUBU_NO_LIBM \
    $INCLUDES -include "$ROOT/src/runtime/wubu_gnu_compat.h" \
    "$SRC_PKG" -o "$TMP/shipped_pp.c" || exit 2
python3 "$CHECKER" pp "$TMP/shipped_pp.c" >/dev/null || { echo "[negctl] FAIL: shipped pp not green" >&2; exit 1; }
echo "[negctl] PASS: detectors red on injected bignum, green on shipped fixed path"
echo "FIXED_FULL_NEGCTL PASSED"
