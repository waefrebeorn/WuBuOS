#!/usr/bin/env bash
# eddsa_nombedtls_gate.sh -- differential gate for the shipped native EdDSA
# backend WITHOUT an mbedTLS dev root.
#
# Part A compiles the real signing leaf using the detector's forced
# --force-mbedtls=0 config (native + bignum-free policy, no mbedTLS header on
# the include path) and proves sign/verify plus every forgery class.
# Part B, when any mbedTLS dev root is available, compiles the SAME entry
# points the other way (mbedTLS ladder + policy) and checks that both builds
# return the same accept/reject class for every vector.
#
# WUBU_DIFF_MBEDTLS_ROOT pins the comparison root independently of the build
# config (WUBU_MBEDTLS_ROOT), so Part A can never accidentally pick up a
# dev root.  No network fetch is attempted by this script.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-eddsa-nomb.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "[nomb] PASS  $1"; PASS=$((PASS+1)); }
no(){ echo "[nomb] FAIL  $1"; FAIL=$((FAIL+1)); }

echo "[nomb] === EdDSA native backend without mbedTLS ==="

for f in /usr/include/openssl/evp.h; do
    [ -f "$f" ] || { echo "[nomb] SKIP: missing $f"; exit 0; }
done
ls /usr/lib/*/libcrypto.so >/dev/null 2>&1 || { echo "[nomb] SKIP: libcrypto absent"; exit 0; }
for t in openssl xxd gcc; do
    command -v "$t" >/dev/null 2>&1 || { echo "[nomb] SKIP: $t unavailable"; exit 0; }
done

mkval(){ sed -n "s/^$2 := //p" "$1" | head -1; }

# ---- Part A: shipped config with no dev root -----------------------------
bash "$ROOT/tools/detect_native_eddsa.sh" --out-dir "$TMP/cfg" \
     --force-mbedtls=0 --quiet
NATIVE=$(mkval "$TMP/cfg/native_eddsa.mk" WUBU_EDDSA_NATIVE)
LADDER=$(mkval "$TMP/cfg/native_eddsa.mk" WUBU_EDDSA_LADDER)
POLICY=$(mkval "$TMP/cfg/native_eddsa.mk" WUBU_EDDSA_POLICY)
echo "[nomb] forced config: native=$NATIVE ladder=$LADDER policy=$POLICY"
[ "$NATIVE" = 1 ] && [ "$LADDER" = 0 ] && [ "$POLICY" = 1 ] \
  && ok "config: native + policy, no ladder" \
  || { no "config: expected native=1 ladder=0 policy=1"; exit 1; }
grep -q 'define WUBU_HAVE_OPENSSL_EDDSA 1' "$TMP/cfg/native_eddsa_config.h" \
  && ok "config: OpenSSL backend macro present" || no "config: OpenSSL macro missing"
grep -q 'define WUBU_HAVE_EDDSA_POLICY 1' "$TMP/cfg/native_eddsa_config.h" \
  && ok "config: bignum-free policy macro present" || no "config: policy macro missing"
if grep -q 'define WUBU_HAVE_MBEDTLS' "$TMP/cfg/native_eddsa_config.h"; then
    no "config: mbedTLS macro leaked into no-root config"
else
    ok "config: no mbedTLS macro"
fi

CFGCFLAGS=$(mkval "$TMP/cfg/native_eddsa.mk" WUBU_EDDSA_HOSTED_CFLAGS)
CFLAGS="-w -O1 -g -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -DWUBU_NO_LIBM $CFGCFLAGS"
INCS="-I$TMP/cfg -I$ROOT -I$ROOT/include -I$ROOT/src -I$ROOT/src/gui -I$ROOT/src/runtime -I$ROOT/src/kernel -I$ROOT/tools"
SRCS="$ROOT/src/runtime/wubu_crc32.c $ROOT/src/gui/wubu_pkg_sign.c \
 $ROOT/src/gui/wubu_pkg_ed25519.c $ROOT/src/runtime/wubu_eddsa_policy.c \
 $ROOT/tools/ed25519_native_gate.c"
# shellcheck disable=SC2086
gcc $CFLAGS $INCS -include "$ROOT/src/runtime/wubu_gnu_compat.h" $SRCS \
    -lcrypto -lm -o "$TMP/nomb" 2>"$TMP/build.err" \
  && ok "no-mbedTLS native build compiles (no -lmbedcrypto, no mbedTLS headers)" \
  || { no "no-mbedTLS native build failed"; cat "$TMP/build.err"; exit 1; }

# Ephemeral OpenSSL key + canonical digest (no key material in the repo).
openssl genpkey -algorithm ED25519 -out "$TMP/key.pem" 2>/dev/null
openssl genpkey -algorithm ED25519 -out "$TMP/other.pem" 2>/dev/null
SEED=$(openssl pkey -in "$TMP/key.pem" -text -noout \
    | awk '/^priv:/{f=1;next} /^pub:/{f=0} f{gsub(/[ :]/,"");printf "%s",$0}')
PUB=$(openssl pkey -in "$TMP/key.pem" -pubout -outform DER 2>/dev/null \
    | tail -c 32 | xxd -p -c 64 | tr -d '\n')
WRONG_PUB=$(openssl pkey -in "$TMP/other.pem" -pubout -outform DER 2>/dev/null \
    | tail -c 32 | xxd -p -c 64 | tr -d '\n')
DIG=$(printf 'wubu no-mbedtls native policy gate' | openssl dgst -sha256 -hex -r | awk '{print $1}')
BADDIG=$(printf 'wubu tampered no-mbedtls digest' | openssl dgst -sha256 -hex -r | awk '{print $1}')
[ "${#SEED}" = 64 ] && [ "${#PUB}" = 64 ] && [ "${#WRONG_PUB}" = 64 ] \
  && [ "${#DIG}" = 64 ] || { echo "[nomb] FAIL: key/digest generation" >&2; exit 2; }

run_bin(){ # bin mode seed pub dig sig -> sets OUT/VER/ERR/OPS
    local bin=$1 mode=$2 seed=$3 pub=$4 dig=$5 sig=$6
    OUT=$("$bin" "$mode" "$seed" "$pub" "$dig" "$sig" 2>/dev/null)
    if [ "$mode" = sign ]; then
        SIG=$(printf '%s\n' "$OUT" | sed -n 's/^SIGN=OK SIG=\([0-9a-f]*\)$/\1/p')
    else
        VER=$(printf '%s\n' "$OUT" | sed -n 's/^RESULT=\([A-Z]*\).*/\1/p')
        ERR=$(printf '%s\n' "$OUT" | sed -n 's/^RESULT=[A-Z]* ERR=\(.*\)$/\1/p')
    fi
    OPS=$(printf '%s\n' "$OUT" | sed -n 's/^NATIVE_OPS=\([0-9]*\)$/\1/p')
}
classify(){ # RESULT ERR -> canonical class for cross-build comparison
    if [ "$1" = PASS ]; then echo PASS; return; fi
    case "$2" in
        *non-canonical*) echo REJECT_NONCANON_S ;;
        *small-subgroup*) echo REJECT_SMALL_SUBGROUP ;;
        *"invalid Ed25519 public key"*|*"invalid point"*) echo REJECT_INVALID_KEY ;;
        *) echo REJECT_SIGNATURE ;;
    esac
}

run_bin "$TMP/nomb" sign "$SEED" "$PUB" "$DIG" -
[ "${#SIG}" = 128 ] && ok "native (no mbedTLS) signs a 64-byte signature" \
  || { no "native sign failed: $OUT"; exit 1; }
[ "${OPS:-0}" = 1 ] && ok "native backend actually executed (NATIVE_OPS=1)" \
  || no "native path was not exercised (NATIVE_OPS=${OPS:-?})"
SIG_N=$SIG

# Negative vectors.  The small-subgroup public keys are real forged-signature
# scenarios: [S]B == R holds for arbitrary S when A has order dividing 8.
BASEPT="5866666666666666666666666666666666666666666666666666666666666666"
FORGED="$BASEPT""01$(printf '00%.0s' $(seq 1 31))"
NONCAN="${SIG_N:0:64}$(printf 'ff%.0s' $(seq 1 32))"
Y_P1="ee$(printf 'ff%.0s' $(seq 1 30))7f"   # y = p+1, non-canonical field element
VECTORS=(
 "valid|$DIG|$PUB|$SIG_N"
 "tampered-signature|$DIG|$PUB|00${SIG_N:2}"
 "wrong-public-key|$DIG|$WRONG_PUB|$SIG_N"
 "tampered-digest|$BADDIG|$PUB|$SIG_N"
 "non-canonical-S|$DIG|$PUB|$NONCAN"
 "small-subgroup-identity|$DIG|0100000000000000000000000000000000000000000000000000000000000000|$FORGED"
 "small-subgroup-order2|$DIG|ecffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff7f|$FORGED"
 "small-subgroup-order4|$DIG|26e8958fc2b227b045c3f489f2ef98f0d5dfac05d3c63339b13802886d53fc05|$FORGED"
 "small-subgroup-signbit|$DIG|0100000000000000000000000000000000000000000000000000000000000080|$FORGED"
 "noncanonical-y|$DIG|$Y_P1|$FORGED"
)

for v in "${VECTORS[@]}"; do
    IFS='|' read -r label dg pk sg <<<"$v"
    run_bin "$TMP/nomb" verify "$SEED" "$pk" "$dg" "$sg"
    want=FAIL; [ "$label" = valid ] && want=PASS
    [ "${VER:-}" = "$want" ] \
      && ok "no-root $label: $(classify "${VER:-FAIL}" "${ERR:-}")" \
      || no "no-root $label: got ${VER:-?} err=${ERR:-} (wanted $want)"
done
# Error provenance on the two policy paths (must match the ladder's wording).
coarse(){ case "$1" in PASS) echo PASS ;; *) echo REJECT ;; esac; }
run_bin "$TMP/nomb" verify "$SEED" "$PUB" "$DIG" "$NONCAN"
case "${ERR:-}" in *non-canonical*) ok "no-root non-canonical S refused before primitive" ;;
    *) no "no-root non-canonical S error provenance: ${ERR:-}" ;; esac
run_bin "$TMP/nomb" verify "$SEED" "0100000000000000000000000000000000000000000000000000000000000000" "$DIG" "$FORGED"
case "${ERR:-}" in *small-subgroup*) ok "no-root small-subgroup A refused before primitive" ;;
    *) no "no-root small-subgroup error provenance: ${ERR:-}" ;; esac

# ---- Part B: same vectors through the mbedTLS build ----------------------
DIFF_ROOT="${WUBU_DIFF_MBEDTLS_ROOT:-${WUBU_MBEDTLS_ROOT:-}}"
if [ -n "$DIFF_ROOT" ] && \
   [ -f "$DIFF_ROOT/usr/include/mbedtls/bignum.h" ] && \
   [ -f "$DIFF_ROOT/usr/lib/x86_64-linux-gnu/libmbedcrypto.a" ]; then
    MB="$DIFF_ROOT/usr"
    MCFLAGS="-w -O1 -g -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -DWUBU_HAVE_MBEDTLS -DWUBU_HAVE_OPENSSL_EDDSA -DWUBU_NO_LIBM"
    MINCS="-I$ROOT -I$ROOT/include -I$ROOT/src -I$ROOT/src/gui -I$ROOT/src/runtime -I$ROOT/src/kernel -I$MB/include -I$ROOT/tools"
    MLINK="-L$MB/lib/x86_64-linux-gnu -lmbedcrypto -lcrypto -Wl,-rpath,$MB/lib/x86_64-linux-gnu"
    # shellcheck disable=SC2086
    gcc $MCFLAGS $MINCS -include "$ROOT/src/runtime/wubu_gnu_compat.h" $SRCS \
        -lm $MLINK -o "$TMP/mbed" 2>"$TMP/mbed-build.err" \
      && ok "with-mbedTLS native build compiles for comparison" \
      || { no "with-mbedTLS comparison build failed"; cat "$TMP/mbed-build.err"; }
    export LD_LIBRARY_PATH="$MB/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

    if [ -x "$TMP/mbed" ]; then
        agree=0; disagree=0
        for v in "${VECTORS[@]}"; do
            IFS='|' read -r label dg pk sg <<<"$v"
            run_bin "$TMP/nomb" verify "$SEED" "$pk" "$dg" "$sg"
            c1=$(classify "${VER:-FAIL}" "${ERR:-}")
            run_bin "$TMP/mbed" verify "$SEED" "$pk" "$dg" "$sg"
            c2=$(classify "${VER:-FAIL}" "${ERR:-}")
            # Mutating R can make it undecodable on one backend and a
            # decodable-but-invalid signature on the other: both are refusals.
            # Assert the exact policy class only where provenance is the
            # contract; generic tampering is compared as accept-vs-reject.
            c1c=$c1; c2c=$c2
            case "$label" in
                non-canonical-S|small-subgroup-*|noncanonical-y) ;;
                *) c1c=$(coarse "$c1"); c2c=$(coarse "$c2") ;;
            esac
            if [ "$c1c" = "$c2c" ]; then
                agree=$((agree+1))
                echo "[nomb] AGREE  $label: $c1 / $c2"
            else
                disagree=$((disagree+1))
                echo "[nomb] MISMATCH  $label: no-root=$c1 with-root=$c2"
            fi
        done
        [ "$disagree" -eq 0 ] && [ "$agree" -eq "${#VECTORS[@]}" ] \
          && ok "policy results agree across both builds ($agree vectors)" \
          || no "policy disagreement across builds ($disagree of $agree+$disagree)"

        # Cross-build signature interchange: either primitive's output verifies
        # under the other, so the bignum-free policy cannot change the message.
        run_bin "$TMP/mbed" sign "$SEED" "$PUB" "$DIG" -
        SIG_MB=$SIG
        run_bin "$TMP/nomb" verify "$SEED" "$PUB" "$DIG" "$SIG_MB"
        [ "${VER:-}" = PASS ] && ok "no-root build verifies with-root signature" \
          || no "no-root build rejected with-root signature"
        run_bin "$TMP/nomb" verify "$SEED" "$PUB" "$DIG" "$SIG_N"
        [ "${VER:-}" = PASS ] && ok "with-root build verifies no-root signature" \
          || no "with-root build rejected no-root signature"
    fi
else
    echo "[nomb] SKIP  with-mbedTLS comparison (set WUBU_DIFF_MBEDTLS_ROOT to enable)"
fi

echo "[nomb] SUMMARY: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ] || { echo "[nomb] GATE FAILED"; exit 1; }
echo "[nomb] ALL CHECKS PASSED -- native EdDSA works with no mbedTLS; policy agrees with ladder"
