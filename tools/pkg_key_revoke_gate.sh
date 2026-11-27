#!/usr/bin/env bash
# pkg_key_revoke_gate.sh -- opt-in differential gate for the authorized
# operator CLI.  WUBU_MBEDTLS_ROOT must point at an extracted mbedTLS dev
# package.  Keys are generated ephemerally and destroyed on exit.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CTL="$ROOT/tools/wubu_pkg_key_ctl"
WUBU_MBEDTLS_ROOT="$("$ROOT/tools/mbedtls_devroot.sh")" || exit 2
export WUBU_MBEDTLS_ROOT
MB="$WUBU_MBEDTLS_ROOT/usr"
[ -f "$MB/include/mbedtls/version.h" ] || { echo "missing mbedTLS headers" >&2; exit 2; }
command -v openssl >/dev/null && command -v xxd >/dev/null
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

echo "[gate] mbedTLS: $(grep -m1 MBEDTLS_VERSION_STRING "$MB/include/mbedtls/version.h" | cut -d'"' -f2)"

# Ephemeral Ed25519 keys: admin (authorized), wrong (unauthorized), and three
# target keys used only as 64-hex identifiers in signed commands.
for n in admin wrong t1 t2 t3 t4; do
    openssl genpkey -algorithm ED25519 -out "$TMP/$n.pem" 2>/dev/null
done
seed_hex() {
    openssl pkey -in "$1" -text -noout | awk '/^priv:/{f=1;next} /^pub:/{f=0} f{gsub(/[ :]/,"");printf "%s",$0}'
}
pub_hex() {
    openssl pkey -in "$1" -pubout -outform DER 2>/dev/null | tail -c 32 | xxd -p -c 64 | tr -d '\n'
}
admin_seed=$(seed_hex "$TMP/admin.pem"); admin_pub=$(pub_hex "$TMP/admin.pem")
wrong_seed=$(seed_hex "$TMP/wrong.pem"); wrong_pub=$(pub_hex "$TMP/wrong.pem")
t1=$(pub_hex "$TMP/t1.pem"); t2=$(pub_hex "$TMP/t2.pem")
t3=$(pub_hex "$TMP/t3.pem"); t4=$(pub_hex "$TMP/t4.pem")
for v in "$admin_seed" "$admin_pub" "$wrong_seed" "$wrong_pub" "$t1" "$t2" "$t3" "$t4"; do
    [ "${#v}" -eq 64 ] || { echo "failed to extract 32-byte key material" >&2; exit 2; }
done
echo "[gate] generated 6 ephemeral Ed25519 keypairs (no key material in repo)"

echo "[gate] building operator CLI: make pkg_key_ctl"
make -C "$ROOT" pkg_key_ctl WUBU_MBEDTLS_ROOT="$WUBU_MBEDTLS_ROOT" >"$TMP/make.log" 2>&1 || {
    cat "$TMP/make.log" >&2; echo "[gate] FAIL: make pkg_key_ctl" >&2; exit 2;
}
[ -x "$CTL" ] || { echo "[gate] FAIL: no $CTL" >&2; exit 2; }
echo "[gate] built $CTL"

# Build the test-only negative-control harness against the same sources.
CFLAGS="-w -O1 -g -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -DWUBU_HAVE_MBEDTLS -DWUBU_NO_LIBM"
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
 $ROOT/src/runtime/wubu_pkg.c $ROOT/src/runtime/wubu_json.c src/runtime/wubu_utf8.c $ROOT/src/runtime/wubu_palette.c \
 $ROOT/src/runtime/styx_names.c $ROOT/src/runtime/styx_enc.c $ROOT/src/runtime/styx_serve.c \
 $ROOT/src/runtime/styx_parse.c $ROOT/src/runtime/styx_fid.c $ROOT/src/runtime/styxfs_vfs.c \
 $ROOT/src/runtime/styxfs_callbacks.c $ROOT/src/runtime/styxfs_posix.c $ROOT/src/runtime/styxfs_path.c \
 $ROOT/src/runtime/styxfs_host.c $ROOT/src/runtime/styxfs_util.c \
 $ROOT/src/runtime/wubu_guest_root.c $ROOT/src/runtime/wubu_container.c \
 $ROOT/src/runtime/oci/oci_http_client.c $ROOT/src/kernel/wubu_math.c"
echo "[gate] compiling pre-change negative-control harness"
gcc $CFLAGS $INCLUDES -include "$ROOT/src/runtime/wubu_gnu_compat.h" $SRCS \
    "$ROOT/tools/pkg_key_revoke_negctl.c" -o "$TMP/negctl" \
    -L"$MB/lib/x86_64-linux-gnu" -lmbedtls -lmbedx509 -lmbedcrypto \
    -Wl,-rpath,"$MB/lib/x86_64-linux-gnu" -lsqlite3 -lzstd -lm || {
    echo "[gate] FAIL: negctl build" >&2; exit 2;
}

DB="$TMP/db"; mkdir -p "$DB"
export LD_LIBRARY_PATH="$MB/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

pass=0; fail=0
expect() {
    local label="$1" got="$2" want="$3"
    if [ "$got" = "$want" ]; then
        printf '[gate] %-56s PASS\n' "$label"; pass=$((pass+1))
    else
        printf '[gate] %-56s FAIL (got=%s want=%s)\n' "$label" "$got" "$want"; fail=$((fail+1))
    fi
}
ctl() { "$CTL" --db "$DB" "$@" 2>>"$TMP/ctl.err"; }
outcome() { ctl "$@" | sed -n '1s/^\([^ ]*\).*/\1/p'; }
count() { ctl list | sed -n 's/^COUNT //p'; }
has_key() { ctl list | grep -Fxq "$1" && echo yes || echo no; }
admin_of() { ctl list | sed -n 's/^ADMIN //p'; }
sign_cmd() { "$CTL" sign "$1" "$2" --seed "$3" 2>>"$TMP/ctl.err"; }

echo "[gate] --- one-time admin bootstrap ---"
expect "bootstrap admin key" "$(outcome bootstrap "$admin_pub")" "OK"
expect "re-bootstrap with a different key refused" "$(outcome bootstrap "$wrong_pub")" "REFUSED"
expect "admin key unchanged after refused re-bootstrap" "$(admin_of)" "$admin_pub"

echo "[gate] --- signed revoke succeeds and persists across processes ---"
sig_ok=$(sign_cmd revoke "$t1" "$admin_seed")
expect "signature is 128 hex chars" "${#sig_ok}" "128"
expect "signed revoke succeeds" "$(outcome revoke "$t1" --sig "$sig_ok")" "OK"
expect "revoked key appears in list" "$(has_key "$t1")" "yes"
c1=$(count); expect "revoked count after signed revoke" "$c1" "1"
expect "fresh process: revoked key still listed" "$(has_key "$t1")" "yes"
expect "fresh process: count persists" "$(count)" "$c1"

echo "[gate] --- unsigned / wrong-key / tampered commands refused before any DB write ---"
expect "unsigned revoke refused" "$(outcome revoke "$t2")" "REFUSED"
expect "unsigned revoke: no DB change" "$(count)" "$c1"
sig_wrong=$(sign_cmd revoke "$t2" "$wrong_seed")
expect "wrong-key signature refused" "$(outcome revoke "$t2" --sig "$sig_wrong")" "REFUSED"
expect "wrong-key revoke: no DB change" "$(count)" "$c1"
sig_t2=$(sign_cmd revoke "$t2" "$admin_seed")
expect "tampered target key refused" "$(outcome revoke "$t3" --sig "$sig_t2")" "REFUSED"
expect "tampered target key: no DB change" "$(count)" "$c1"
sig_un_t1=$(sign_cmd unrevoke "$t1" "$admin_seed")
expect "tampered operation (unrevoke sig as revoke) refused" \
       "$(outcome revoke "$t1" --sig "$sig_un_t1")" "REFUSED"
expect "tampered operation: target still revoked" "$(has_key "$t1")" "yes"
expect "tampered operation: no DB change" "$(count)" "$c1"

echo "[gate] --- authorized unrevoke restores state ---"
expect "authorized unrevoke succeeds" "$(outcome unrevoke "$t1" --sig "$sig_un_t1")" "OK"
expect "revoked count after unrevoke" "$(count)" "0"
expect "unrevoked key absent from list" "$(has_key "$t1")" "no"

echo "[gate] --- negative control: pre-change no-auth path still writes DB ---"
NCDB="$TMP/negdb"; mkdir -p "$NCDB"
nc=$("$TMP/negctl" legacy_direct "$NCDB" "$t4" 2>>"$TMP/ctl.err" | sed -n '1p')
expect "pre-change direct revoke needs no signature (negative control)" "$nc" "LEGACY_OK"
expect "same unsigned command refused by authorized CLI" "$(outcome revoke "$t4")" "REFUSED"
expect "negative-control key absent from authorized DB" "$(has_key "$t4")" "no"

echo "[gate] final list on $DB:"
ctl list | sed 's/^/    /'
echo "[gate] $pass passed, $fail failed"
[ "$fail" -eq 0 ] || exit 1
