#!/usr/bin/env bash
# pkg_key_policy_gate.sh -- opt-in differential gate for the three remaining
# key-policy legs: (1) trust-anchor expiry with fail-closed verifier skip,
# (2) admin rotation/recovery signed by the current admin key, and (3) M-of-N
# quorum for feed apply with all-or-nothing semantics.
#
# WUBU_MBEDTLS_ROOT must point at an extracted mbedTLS dev root. All Ed25519
# keys are generated ephemerally in a temp dir and destroyed on exit; no
# secret material ever enters the repository. Both phases share one on-disk
# pkgmgr DB so the fresh-process `check` phase proves DB persistence.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
WUBU_MBEDTLS_ROOT="$("$ROOT/tools/mbedtls_devroot.sh")" || exit 2
export WUBU_MBEDTLS_ROOT
MB="$WUBU_MBEDTLS_ROOT/usr"
[ -f "$MB/include/mbedtls/version.h" ] || { echo "missing mbedTLS headers" >&2; exit 2; }
command -v openssl >/dev/null && command -v xxd >/dev/null && command -v zstd >/dev/null
TMP=$(mktemp -d)
if [ "${POLICY_GATE_KEEP:-0}" = "1" ]; then
    trap 'echo "[gate] kept workspace: $TMP"' EXIT
else
    trap 'rm -rf "$TMP"' EXIT
fi

echo "[gate] mbedTLS: $(grep -m1 MBEDTLS_VERSION_STRING "$MB/include/mbedtls/version.h" | cut -d'"' -f2)"

# Eight ephemeral Ed25519 keypairs: signer/anchor, wrong, old admin, new admin,
# two quorum/escrow members, revocation target, and the post-recovery admin.
for n in sign wrong admin new q1 q2 target rec; do
    openssl genpkey -algorithm ED25519 -out "$TMP/$n.pem" 2>/dev/null
done
seed_hex() {
    openssl pkey -in "$1" -text -noout | awk '/^priv:/{f=1;next} /^pub:/{f=0} f{gsub(/[ :]/,"");printf "%s",$0}'
}
pub_hex() {
    openssl pkey -in "$1" -pubout -outform DER 2>/dev/null | tail -c 32 | xxd -p -c 64 | tr -d '\n'
}
sign_seed=$(seed_hex "$TMP/sign.pem");  sign_pub=$(pub_hex "$TMP/sign.pem")
wrong_seed=$(seed_hex "$TMP/wrong.pem"); wrong_pub=$(pub_hex "$TMP/wrong.pem")
admin_seed=$(seed_hex "$TMP/admin.pem"); admin_pub=$(pub_hex "$TMP/admin.pem")
new_seed=$(seed_hex "$TMP/new.pem");     new_pub=$(pub_hex "$TMP/new.pem")
q1_seed=$(seed_hex "$TMP/q1.pem");       q1_pub=$(pub_hex "$TMP/q1.pem")
q2_seed=$(seed_hex "$TMP/q2.pem");       q2_pub=$(pub_hex "$TMP/q2.pem")
target=$(pub_hex "$TMP/target.pem")
rec_seed=$(seed_hex "$TMP/rec.pem");     rec_pub=$(pub_hex "$TMP/rec.pem")
for v in "$sign_seed" "$sign_pub" "$wrong_seed" "$wrong_pub" \
         "$admin_seed" "$admin_pub" "$new_seed" "$new_pub" \
         "$q1_seed" "$q1_pub" "$q2_seed" "$q2_pub" "$target" \
         "$rec_seed" "$rec_pub"; do
    [ "${#v}" -eq 64 ] || { echo "failed to extract 32-byte key material" >&2; exit 2; }
done
echo "[gate] generated 8 ephemeral Ed25519 keypairs (no key material in repo)"

mkdir -p "$TMP/src"
printf 'Key policy gate payload\n' > "$TMP/src/payload.txt"

# Build the mbedTLS-enabled harness against the SAME sources as the product.
CFLAGS="${POLICY_GATE_CFLAGS:--w -O1 -g -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -DWUBU_HAVE_MBEDTLS -DWUBU_NO_LIBM}"
INCLUDES="-I$ROOT -I$ROOT/include -I$ROOT/src -I$ROOT/src/gui -I$ROOT/src/runtime -I$ROOT/src/kernel -I$MB/include"
SRCS="$ROOT/src/runtime/wubu_crc32.c \
 $ROOT/src/gui/wubu_pkgmgr.c $ROOT/src/gui/wubu_pkgmgr_verify.c $ROOT/src/gui/wubu_pkg_sign.c $ROOT/src/gui/wubu_pkg_ed25519.c \
 $ROOT/src/gui/wubu_pkgmgr_revoke_feed.c \
 $ROOT/src/gui/wubu_pkgmgr_resolve.c $ROOT/src/gui/wubu_pkgmgr_manifest.c \
 $ROOT/src/gui/wubu_pkgmgr_db.c $ROOT/src/gui/wubu_pkgmgr_pkg.c \
 $ROOT/src/gui/wubu_pkgmgr_install.c $ROOT/src/gui/wubu_pkgmgr_txn.c \
 $ROOT/src/gui/wubu_pkgmgr_remote.c $ROOT/src/gui/wubu_pkgmgr_fetch.c \
 $ROOT/src/gui/wubu_settings.c $ROOT/src/gui/wubu_settings_defaults.c $ROOT/src/gui/wubu_settings_io.c \
 $ROOT/src/gui/wubu_theme.c \
 $ROOT/src/runtime/wubu_pkg.c $ROOT/src/runtime/wubu_json.c $ROOT/src/runtime/wubu_palette.c \
 $ROOT/src/runtime/styx_names.c $ROOT/src/runtime/styx_enc.c $ROOT/src/runtime/styx_serve.c \
 $ROOT/src/runtime/styx_parse.c $ROOT/src/runtime/styx_fid.c $ROOT/src/runtime/styxfs_vfs.c \
 $ROOT/src/runtime/styxfs_callbacks.c $ROOT/src/runtime/styxfs_posix.c $ROOT/src/runtime/styxfs_path.c \
 $ROOT/src/runtime/styxfs_host.c $ROOT/src/runtime/styxfs_util.c \
 $ROOT/src/runtime/wubu_guest_root.c $ROOT/src/runtime/wubu_container.c \
 $ROOT/src/runtime/oci/oci_http_client.c $ROOT/src/kernel/wubu_math.c"

echo "[gate] compiling mbedTLS-enabled key-policy harness"
# shellcheck disable=SC2086
gcc $CFLAGS $INCLUDES -include "$ROOT/src/runtime/wubu_gnu_compat.h" $SRCS \
    "$ROOT/tools/pkg_key_policy_gate.c" -o "$TMP/probe" \
    -L"$MB/lib/x86_64-linux-gnu" -lmbedtls -lmbedx509 -lmbedcrypto \
    -Wl,-rpath,"$MB/lib/x86_64-linux-gnu" -lsqlite3 -lzstd -lm || {
    echo "[gate] FAIL: harness build" >&2; exit 2;
}

DB="$TMP/db"; mkdir -p "$DB"
export LD_LIBRARY_PATH="$MB/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
ARGS=(record "$DB" "$TMP/gate.wubu" "$TMP/src" \
      "$sign_seed" "$sign_pub" "$wrong_seed" "$wrong_pub" \
      "$admin_seed" "$admin_pub" "$new_seed" "$new_pub" \
      "$q1_seed" "$q1_pub" "$q2_seed" "$q2_pub" "$target" \
      "$rec_seed" "$rec_pub")

echo "[gate] === record phase (fresh DB) ==="
"$TMP/probe" "${ARGS[@]}"
rc_record=$?
echo "[gate] === check phase (fresh process, same DB) ==="
ARGS[0]=check
"$TMP/probe" "${ARGS[@]}"
rc_check=$?

echo "[gate] record rc=$rc_record check rc=$rc_check"
[ "$rc_record" -eq 0 ] && [ "$rc_check" -eq 0 ] || exit 1
echo "[gate] ALL KEY-POLICY CHECKS PASSED"
