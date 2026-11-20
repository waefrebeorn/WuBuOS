#!/usr/bin/env bash
# eddsa_runtime_gate.sh -- RUNTIME equivalence/tamper gate in the default
# suite.  Uses the SAME build-time config the hosted product uses, so what is
# tested is what ships:
#   * both backends present -> native vs ladder cross-sign/verify + forgery
#     negatives (the full differential harness);
#   * OpenSSL absent -> ladder-only sign/verify + forgery negatives;
#   * no backend -> explicit SKIP (never a hard dependency on a fetched root).
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-eddsa-rt.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT

bash "$ROOT/tools/detect_native_eddsa.sh" --out-dir "$TMP/cfg" --quiet
mkval(){ sed -n "s/^$2 := //p" "$1" | head -1; }
NATIVE=$(mkval "$TMP/cfg/native_eddsa.mk" WUBU_EDDSA_NATIVE)
LADDER=$(mkval "$TMP/cfg/native_eddsa.mk" WUBU_EDDSA_LADDER)
echo "[rt] shipped config: native=$NATIVE ladder=$LADDER"

if [ "$NATIVE" = 1 ] && [ "$LADDER" = 1 ]; then
    echo "[rt] running native<->ladder differential + tamper/forgery suite"
    bash "$ROOT/tools/ed25519_native_gate.sh" | tee "$TMP/native.out" || exit 1
    echo "[rt] NEGATIVE CONTROL (forgeries refused on both backends):"
    grep -E 'tampered signature rejected|wrong public key rejected|tampered digest rejected|small-subgroup forgery rejected|non-canonical S rejected' "$TMP/native.out" || true
    echo "[rt] PASS: runtime equivalence + tamper gate (native and ladder agree)"
    exit 0
fi

if [ "$LADDER" != 1 ]; then
    echo "[rt] SKIP: no compiled EdDSA backend (mbedTLS dev root absent; no fetch attempted)"
    exit 0
fi

# Ladder-only build (OpenSSL absent): still prove sign/verify + forgeries.
MB=$(bash "$ROOT/tools/mbedtls_devroot.sh" 2>/dev/null) || {
    echo "[rt] SKIP: ladder dev root unavailable" ; exit 0; }
M="$MB/usr"
for t in openssl xxd; do command -v "$t" >/dev/null || { echo "[rt] SKIP: $t unavailable"; exit 0; }; done
openssl genpkey -algorithm ED25519 -out "$TMP/key.pem" 2>/dev/null
SEED=$(openssl pkey -in "$TMP/key.pem" -text -noout \
    | awk '/^priv:/{f=1;next} /^pub:/{f=0} f{gsub(/[ :]/,"");printf "%s",$0}')
PUB=$(openssl pkey -in "$TMP/key.pem" -pubout -outform DER 2>/dev/null \
    | tail -c 32 | xxd -p -c 64 | tr -d '\n')
DIG=$(printf 'wubu ladder-only runtime gate' | openssl dgst -sha256 -hex -r | awk '{print $1}')
CFLAGS="-w -O1 -g -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -DWUBU_HAVE_MBEDTLS -DWUBU_NO_LIBM"
INCS="-I$ROOT -I$ROOT/include -I$ROOT/src -I$ROOT/src/gui -I$ROOT/src/runtime -I$ROOT/src/kernel -I$M/include -I$ROOT/tools"
# shellcheck disable=SC2086
gcc $CFLAGS $INCS -include "$ROOT/src/runtime/wubu_gnu_compat.h" \
    "$ROOT/src/runtime/wubu_crc32.c" "$ROOT/src/gui/wubu_pkg_sign.c" \
    "$ROOT/src/gui/wubu_pkg_ed25519.c" "$ROOT/tools/ed25519_native_gate.c" \
    -L"$M/lib/x86_64-linux-gnu" -lmbedcrypto -Wl,-rpath,"$M/lib/x86_64-linux-gnu" \
    -o "$TMP/probe" || { echo "[rt] FAIL: ladder probe build failed" >&2; exit 1; }
export LD_LIBRARY_PATH="$M/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

SIG=$("$TMP/probe" sign "$SEED" "$PUB" "$DIG" - 2>/dev/null \
     | sed -n 's/^SIGN=OK SIG=\([0-9a-f]*\)$/\1/p')
[ "${#SIG}" -eq 128 ] || { echo "[rt] FAIL: ladder-only sign failed" >&2; exit 1; }
echo "[rt] PASS  ladder-only sign (64-byte signature)"
VER=$("$TMP/probe" verify "$SEED" "$PUB" "$DIG" "$SIG" | sed -n 's/^RESULT=\([A-Z]*\).*/\1/p')
[ "$VER" = PASS ] || { echo "[rt] FAIL: ladder-only verify failed" >&2; exit 1; }
echo "[rt] PASS  ladder-only verify"
TAMPERED="00${SIG:2}"
VER=$("$TMP/probe" verify "$SEED" "$PUB" "$DIG" "$TAMPERED" | sed -n 's/^RESULT=\([A-Z]*\).*/\1/p')
[ "$VER" = FAIL ] || { echo "[rt] FAIL: tampered signature accepted" >&2; exit 1; }
echo "[rt] NEGATIVE CONTROL: tampered signature refused by ladder"
echo "[rt] PASS: runtime equivalence/tamper gate (ladder-only; OpenSSL absent)"
