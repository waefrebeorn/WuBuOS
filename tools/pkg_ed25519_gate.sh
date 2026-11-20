#!/usr/bin/env bash
# pkg_ed25519_gate.sh -- opt-in differential Ed25519 package-signature gate.
# WUBU_MBEDTLS_ROOT must point at an extracted mbedTLS dev package
# (headers under usr/include, libs under usr/lib/x86_64-linux-gnu).
# Keys are generated ephemerally in a temp dir and destroyed on exit: no
# key material is ever written into the repository.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
: "${WUBU_MBEDTLS_ROOT:?set WUBU_MBEDTLS_ROOT to an extracted mbedTLS dev root}"
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

seed_hex=$(openssl pkey -in "$TMP/key.pem" -text -noout \
    | awk '/^priv:/{f=1;next} /^pub:/{f=0} f{gsub(/[ :]/,"");printf "%s",$0}')
pub_hex=$(openssl pkey -in "$TMP/key.pem" -pubout -outform DER 2>/dev/null \
    | tail -c 32 | xxd -p -c 64 | tr -d '\n')
wrong_pub=$(openssl pkey -in "$TMP/other.pem" -pubout -outform DER 2>/dev/null \
    | tail -c 32 | xxd -p -c 64 | tr -d '\n')
[ "${#seed_hex}" -eq 64 ] && [ "${#pub_hex}" -eq 64 ] && [ "${#wrong_pub}" -eq 64 ] \
    || { echo "failed to extract 32-byte Ed25519 key material" >&2; exit 2; }
echo "[gate] seed/pubkey: 32 bytes each (raw hex, never stored in repo)"

mkdir -p "$TMP/src"
printf 'Ed25519 gate payload\n' > "$TMP/src/payload.txt"

# Legacy tree emits many unrelated format/unused warnings; keep gate output readable.
CFLAGS="-w -O1 -g -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -DWUBU_HAVE_MBEDTLS -DWUBU_NO_LIBM"
INCLUDES="-I$ROOT -I$ROOT/include -I$ROOT/src -I$ROOT/src/gui -I$ROOT/src/runtime -I$ROOT/src/kernel -I$MB/include"
SRCS="$ROOT/src/runtime/wubu_crc32.c \
 $ROOT/src/gui/wubu_pkgmgr.c $ROOT/src/gui/wubu_pkgmgr_verify.c $ROOT/src/gui/wubu_pkg_sign.c $ROOT/src/gui/wubu_pkg_ed25519.c \
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
gcc $CFLAGS $INCLUDES -include "$ROOT/src/runtime/wubu_gnu_compat.h" $SRCS \
    -o "$TMP/probe" \
    -L"$MB/lib/x86_64-linux-gnu" -lmbedtls -lmbedx509 -lmbedcrypto \
    -Wl,-rpath,"$MB/lib/x86_64-linux-gnu" -lsqlite3 -lzstd -lm

echo "[gate] running differential checks"
LD_LIBRARY_PATH="$MB/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    "$TMP/probe" "$TMP/gate.wubu" "$TMP/src" "$seed_hex" "$pub_hex" "$wrong_pub"
