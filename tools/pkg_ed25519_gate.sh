#!/usr/bin/env bash
# pkg_ed25519_gate.sh -- differential Ed25519 package-signature gate.
# The DEFAULT ladder is the self-contained bignum-free fixed-width module
# (src/gui/wubu_ed25519_fixed.c).  Set WUBU_EDDSA_FIXED=0 for the documented
# legacy override (the mbedTLS MPI ladder).
# WUBU_MBEDTLS_ROOT must point at an extracted mbedTLS dev package
# (headers under usr/include, libs under usr/lib/x86_64-linux-gnu).
# Keys are generated ephemerally in a temp dir and destroyed on exit: no
# key material is ever written into the repository.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
WUBU_MBEDTLS_ROOT="$("$ROOT/tools/mbedtls_devroot.sh")"
export WUBU_MBEDTLS_ROOT
MB="$WUBU_MBEDTLS_ROOT/usr"
for f in "$MB/include/mbedtls/sha512.h" "$MB/include/mbedtls/bignum.h" \
         "$MB/lib/x86_64-linux-gnu/libmbedcrypto.a"; do
    [ -f "$f" ] || { echo "missing mbedTLS component: $f" >&2; exit 2; }
done
command -v openssl >/dev/null && command -v xxd >/dev/null && command -v zstd >/dev/null
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

echo "[gate] mbedTLS: $(grep -m1 MBEDTLS_VERSION_STRING "$MB/include/mbedtls/version.h" | cut -d'"' -f2)"
echo "[gate] generating ephemeral Ed25519 keypairs"
openssl genpkey -algorithm ED25519 -out "$TMP/key.pem" 2>/dev/null
openssl genpkey -algorithm ED25519 -out "$TMP/other.pem" 2>/dev/null
openssl genpkey -algorithm ED25519 -out "$TMP/rot.pem" 2>/dev/null

seed_hex=$(openssl pkey -in "$TMP/key.pem" -text -noout \
    | awk '/^priv:/{f=1;next} /^pub:/{f=0} f{gsub(/[ :]/,"");printf "%s",$0}')
pub_hex=$(openssl pkey -in "$TMP/key.pem" -pubout -outform DER 2>/dev/null \
    | tail -c 32 | xxd -p -c 64 | tr -d '\n')
wrong_pub=$(openssl pkey -in "$TMP/other.pem" -pubout -outform DER 2>/dev/null \
    | tail -c 32 | xxd -p -c 64 | tr -d '\n')
rot_seed=$(openssl pkey -in "$TMP/rot.pem" -text -noout \
    | awk '/^priv:/{f=1;next} /^pub:/{f=0} f{gsub(/[ :]/,"");printf "%s",$0}')
rot_pub=$(openssl pkey -in "$TMP/rot.pem" -pubout -outform DER 2>/dev/null \
    | tail -c 32 | xxd -p -c 64 | tr -d '\n')
[ "${#seed_hex}" -eq 64 ] && [ "${#pub_hex}" -eq 64 ] && [ "${#wrong_pub}" -eq 64 ] \
    && [ "${#rot_seed}" -eq 64 ] && [ "${#rot_pub}" -eq 64 ] \
    || { echo "failed to extract 32-byte Ed25519 key material" >&2; exit 2; }
echo "[gate] seed/pubkey: 32 bytes each (raw hex, never stored in repo)"
echo "[gate] generated 3 keypairs: genuine / wrong / rotation"

mkdir -p "$TMP/src"
printf 'Ed25519 gate payload\n' > "$TMP/src/payload.txt"

# Legacy tree emits many unrelated format/unused warnings; keep gate output readable.
CFLAGS="-w -O1 -g -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -DWUBU_HAVE_MBEDTLS -DWUBU_NO_LIBM"
# WUBU_EDDSA_NATIVE=1 selects the opt-in OpenSSL EVP_PKEY_ED25519 core.  The
# policy checks and every tamper/negative-control case below stay identical.
NATIVE_FLAG=""
NATIVE_LINK=""
if [ "${WUBU_EDDSA_NATIVE:-0}" = "1" ]; then
    [ -f /usr/include/openssl/evp.h ] && ls /usr/lib/*/libcrypto.so >/dev/null 2>&1 \
        || { echo "[gate] native backend needs OpenSSL dev headers + libcrypto.so" >&2; exit 2; }
    NATIVE_FLAG="-DWUBU_HAVE_OPENSSL_EDDSA"
    NATIVE_LINK="-lcrypto"
    echo "[gate] backend: OpenSSL EVP_PKEY_ED25519 (opt-in native)"
else
    echo "[gate] backend: in-repo hardened Montgomery ladder (mbedTLS)"
fi
# DEFAULT: the self-contained fixed-width ladder (no mbedTLS bignum).  The
# documented LEGACY override is WUBU_EDDSA_FIXED=0, which restores the mbedTLS
# MPI ladder.  Hashing, scalar mod-L arithmetic and point encode/decode stay
# on the mbedTLS path in BOTH modes, so the differential checks below still
# exercise the real package-signing flow.
FIXED_FLAG="-DWUBU_EDDSA_FIXED_LADDER"
FIXED_SRC="$ROOT/src/gui/wubu_ed25519_fixed.c"
if [ "${WUBU_EDDSA_FIXED:-1}" = "1" ]; then
    echo "[gate] ladder: fixed-width bignum-free (DEFAULT; WUBU_EDDSA_FIXED_LADDER)"
else
    FIXED_FLAG=""
    FIXED_SRC=""
    echo "[gate] ladder: LEGACY mbedTLS MPI (override: WUBU_EDDSA_FIXED=0)"
fi
INCLUDES="-I$ROOT -I$ROOT/include -I$ROOT/src -I$ROOT/src/gui -I$ROOT/src/runtime -I$ROOT/src/kernel -I$MB/include"
SRCS="$ROOT/src/runtime/wubu_crc32.c \
 $ROOT/src/gui/wubu_pkgmgr.c $ROOT/src/gui/wubu_pkgmgr_verify.c $ROOT/src/gui/wubu_pkg_sign.c $ROOT/src/gui/wubu_pkg_ed25519.c \
 $ROOT/src/gui/wubu_pkgmgr_revoke_feed.c \
 $ROOT/src/gui/wubu_pkgmgr_resolve.c $ROOT/src/gui/wubu_pkgmgr_manifest.c \
 $ROOT/src/gui/wubu_pkgmgr_db.c $ROOT/src/gui/wubu_pkgmgr_pkg.c \
 $ROOT/src/gui/wubu_pkgmgr_install.c $ROOT/src/gui/wubu_pkgmgr_txn.c \
 $ROOT/src/gui/wubu_pkgmgr_remote.c $ROOT/src/gui/wubu_pkgmgr_fetch.c \
 $ROOT/src/gui/wubu_settings.c $ROOT/src/gui/wubu_settings_defaults.c $ROOT/src/gui/wubu_settings_io.c \
 $ROOT/src/gui/wubu_theme.c $ROOT/src/gui/wubu_pkg_ed25519_gate.c \
 $ROOT/src/runtime/wubu_pkg.c $ROOT/src/runtime/wubu_json.c $ROOT/src/runtime/wubu_palette.c \
 $ROOT/src/runtime/styx_names.c $ROOT/src/runtime/styx_enc.c $ROOT/src/runtime/styx_serve.c \
 $ROOT/src/runtime/styx_parse.c $ROOT/src/runtime/styx_fid.c $ROOT/src/runtime/styxfs_vfs.c \
 $ROOT/src/runtime/styxfs_callbacks.c $ROOT/src/runtime/styxfs_posix.c $ROOT/src/runtime/styxfs_path.c \
 $ROOT/src/runtime/styxfs_host.c $ROOT/src/runtime/styxfs_util.c \
 $ROOT/src/runtime/wubu_guest_root.c $ROOT/src/runtime/wubu_container.c \
 $ROOT/src/runtime/oci/oci_http_client.c $ROOT/src/kernel/wubu_math.c"

echo "[gate] compiling mbedTLS-enabled pkgmgr harness"
# shellcheck disable=SC2086
gcc $CFLAGS $NATIVE_FLAG $FIXED_FLAG $INCLUDES -include "$ROOT/src/runtime/wubu_gnu_compat.h" $SRCS $FIXED_SRC \
    -o "$TMP/probe" \
    -L"$MB/lib/x86_64-linux-gnu" -lmbedtls -lmbedx509 -lmbedcrypto \
    -Wl,-rpath,"$MB/lib/x86_64-linux-gnu" -lsqlite3 -lzstd -lm $NATIVE_LINK

echo "[gate] running differential checks: record process (persist revocation)"
mkdir -p "$TMP/db"
export LD_LIBRARY_PATH="$MB/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
ARGS=(record "$TMP/db" "$TMP/gate.wubu" "$TMP/src" \
      "$seed_hex" "$pub_hex" "$wrong_pub" "$rot_seed" "$rot_pub")
"$TMP/probe" "${ARGS[@]}"
echo "[gate] restarting verifier in a FRESH process against the same DB"
ARGS[0]=check
"$TMP/probe" "${ARGS[@]}"
