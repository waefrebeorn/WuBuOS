#!/usr/bin/env bash
# eddsa_config_gate.sh -- BUILD-TIME config gate (member of the default suite).
# Asserts that tools/detect_native_eddsa.sh:
#   * matches independently probed OpenSSL / mbedTLS availability;
#   * emits native+ladder when both are present, ladder only when OpenSSL is
#     forced absent, and a valid fail-closed config when the dev root is
#     forced absent;
#   * NEVER attempts a network fetch (PATH shims for curl/wget/apt abort);
#   * emits CFLAGS that actually compile a translation unit using the config.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
TMP=$(mktemp -d "${TMPDIR:-/tmp}/wubu-eddsa-cfg.XXXXXX") || exit 2
trap 'rm -rf "$TMP"' EXIT
PASS=0; FAIL=0
ok(){ echo "[cfg] PASS  $1"; PASS=$((PASS+1)); }
no(){ echo "[cfg] FAIL  $1"; FAIL=$((FAIL+1)); }

det(){ bash "$ROOT/tools/detect_native_eddsa.sh" "$@"; }
mkval(){ local f=$1 k=$2; sed -n "s/^$k := //p" "$f" | head -1; }

echo "[cfg] === build-time EdDSA configuration gate ==="

# Independent expectation (does not use the detector).
exp_ssl=0
if [ -f /usr/include/openssl/evp.h ] && ls /usr/lib/*/libcrypto.so >/dev/null 2>&1 \
   && command -v cc >/dev/null 2>&1; then exp_ssl=1; fi
exp_mb=0
root="${WUBU_MBEDTLS_ROOT:-$ROOT/.cache/mbedtls-devroot}"
if [ -f "$root/usr/include/mbedtls/bignum.h" ] && \
   [ -f "$root/usr/include/mbedtls/sha512.h" ] && \
   [ -f "$root/usr/lib/x86_64-linux-gnu/libmbedcrypto.a" ]; then exp_mb=1; fi
exp_native=0; [ "$exp_ssl" = 1 ] && exp_native=1
echo "[cfg] probed independently: openssl=$exp_ssl mbedtls=$exp_mb native=$exp_native"

# 1. Real detection.
det --out-dir "$TMP/real" --quiet || { echo "[cfg] detector failed" >&2; exit 2; }
[ "$(mkval "$TMP/real/native_eddsa.mk" WUBU_EDDSA_NATIVE)" = "$exp_native" ] \
  && ok "native flag matches independent probe ($exp_native)" \
  || no "native flag mismatch"
[ "$(mkval "$TMP/real/native_eddsa.mk" WUBU_EDDSA_LADDER)" = "$exp_mb" ] \
  && ok "ladder flag matches independent probe ($exp_mb)" \
  || no "ladder flag mismatch"

# 2. OpenSSL forced absent -> ladder still selected, no -lcrypto.
det --out-dir "$TMP/nossl" --force-openssl=0 --quiet
[ "$(mkval "$TMP/nossl/native_eddsa.mk" WUBU_EDDSA_NATIVE)" = 0 ] \
  && ok "OpenSSL absent: native disabled" || no "OpenSSL absent: native still on"
case "$(mkval "$TMP/nossl/native_eddsa.mk" WUBU_EDDSA_HOSTED_LIBS)" in
    *-lcrypto*) no "OpenSSL absent: -lcrypto still in link line" ;;
    *) ok "OpenSSL absent: -lcrypto not linked" ;;
esac
if [ "$exp_mb" = 1 ]; then
    grep -q 'define WUBU_HAVE_MBEDTLS 1' "$TMP/nossl/native_eddsa_config.h" \
      && ok "OpenSSL absent: ladder fallback compiled in" \
      || no "OpenSSL absent: ladder fallback missing"
    echo "[cfg] OPENSSL-ABSENT STATE: still builds, backend=ladder"
fi

# 3. Dev root forced absent -> zero backend, valid config, NO fetch.
mkdir -p "$TMP/fakebin"
for tool in curl wget apt-get apt; do
    printf '#!/bin/sh\necho "NETWORK FETCH ATTEMPT: %s" >> "%s/fetch.log"\nexit 99\n' \
        "$tool" "$TMP" > "$TMP/fakebin/$tool"
    chmod +x "$TMP/fakebin/$tool"
done
PATH="$TMP/fakebin:$PATH" det --out-dir "$TMP/nomb" --force-mbedtls=0 --quiet
if [ -f "$TMP/fetch.log" ]; then
    no "network-fetch negative control: fetcher invoked"
    cat "$TMP/fetch.log"
else
    ok "NEGATIVE CONTROL: no curl/wget/apt invoked by build-time detection"
fi
[ "$(mkval "$TMP/nomb/native_eddsa.mk" WUBU_EDDSA_LADDER)" = 0 ] \
  && ok "dev root absent: ladder flag 0 (no fetch attempted)" \
  || no "dev root absent: ladder flag nonzero"
[ -s "$TMP/nomb/native_eddsa_config.h" ] \
  && ok "dev root absent: still emits a valid config header (build not blocked)" \
  || no "dev root absent: no config header emitted"

# The native backend must now key off OpenSSL alone: the canonical-S and
# small-subgroup policy is bignum-free and lives in the repo, not in the dev
# root.  With no dev root the config must enable native, leave the ladder off,
# and must NOT reference mbedTLS headers or libraries.
[ "$(mkval "$TMP/nomb/native_eddsa.mk" WUBU_EDDSA_NATIVE)" = "$exp_native" ] \
  && ok "dev root absent: native flag follows OpenSSL ($exp_native)" \
  || no "dev root absent: native flag does not follow OpenSSL"
if [ "$exp_native" = 1 ]; then
    grep -q 'define WUBU_HAVE_OPENSSL_EDDSA 1' "$TMP/nomb/native_eddsa_config.h" \
      && ok "dev root absent: native feature macro emitted" \
      || no "dev root absent: native feature macro missing"
    if grep -q 'define WUBU_HAVE_MBEDTLS' "$TMP/nomb/native_eddsa_config.h"; then
        no "dev root absent: mbedTLS macro leaked into config"
    else
        ok "dev root absent: no mbedTLS macro in config"
    fi
    grep -q 'define WUBU_HAVE_EDDSA_POLICY 1' "$TMP/nomb/native_eddsa_config.h" \
      && ok "dev root absent: bignum-free policy macro emitted" \
      || no "dev root absent: bignum-free policy macro missing"
    case "$(mkval "$TMP/nomb/native_eddsa.mk" WUBU_EDDSA_HOSTED_LIBS)" in
        *-lmbedcrypto*) no "dev root absent: -lmbedcrypto still linked" ;;
        *) ok "dev root absent: -lmbedcrypto not linked" ;;
    esac
fi

# 4. Emitted CFLAGS must compile TUs against BOTH generated headers.  The
# invariant is "any backend implies a policy", not "native implies mbedTLS".
cat > "$TMP/tu.c" <<'EOF'
#include "native_eddsa_config.h"
#if (defined(WUBU_HAVE_OPENSSL_EDDSA) || defined(WUBU_HAVE_MBEDTLS)) && \
    !defined(WUBU_HAVE_EDDSA_POLICY)
#error "every crypto backend requires the shared verification policy layer"
#endif
int main(void) {
#if defined(WUBU_HAVE_OPENSSL_EDDSA) && !defined(WUBU_HAVE_MBEDTLS)
    return (WUBU_EDDSA_CFG_NATIVE == 1 && WUBU_EDDSA_CFG_LADDER == 0 &&
            WUBU_EDDSA_CFG_POLICY == 1) ? 0 : 1;
#else
    return 0;
#endif
}
EOF
for cfg in nossl nomb; do
    CFLAGS=$(mkval "$TMP/$cfg/native_eddsa.mk" WUBU_EDDSA_HOSTED_CFLAGS)
    # shellcheck disable=SC2086
    if cc $CFLAGS "$TMP/tu.c" -o "$TMP/tu-$cfg" 2>"$TMP/cc-$cfg.err" && "$TMP/tu-$cfg"; then
        ok "generated $cfg CFLAGS compile and respect backend=>policy invariant"
    else
        no "generated $cfg CFLAGS failed to compile"
        cat "$TMP/cc-$cfg.err"
    fi
done

echo "[cfg] SUMMARY: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ] || { echo "[cfg] GATE FAILED"; exit 1; }
echo "[cfg] ALL BUILD-TIME CONFIG CHECKS PASSED (no network fetch)"
