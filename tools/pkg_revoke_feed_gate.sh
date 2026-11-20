#!/usr/bin/env bash
# pkg_revoke_feed_gate.sh -- opt-in differential gate for the AUTHENTICATED
# revocation-feed import.  WUBU_MBEDTLS_ROOT must point at an extracted
# mbedTLS dev root.  Ed25519 keys are generated ephemerally and destroyed on
# exit; no key material is stored in the repository.
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

# Ephemeral Ed25519 keys: admin (authorized), wrong (unauthorized), and four
# target keys used only as 64-hex identifiers in feed entries.
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

# Build the test-only negative-control harness against the SAME sources.
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
 $ROOT/src/runtime/wubu_pkg.c $ROOT/src/runtime/wubu_json.c $ROOT/src/runtime/wubu_palette.c \
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
        printf '[gate] %-58s PASS\n' "$label"; pass=$((pass+1))
    else
        printf '[gate] %-58s FAIL (got=%s want=%s)\n' "$label" "$got" "$want"; fail=$((fail+1))
    fi
}
ctl() { "$CTL" --db "$DB" "$@" 2>>"$TMP/ctl.err"; }
outcome() { ctl "$@" | sed -n '1s/^\([^ ]*\).*/\1/p'; }
count() { ctl list | sed -n 's/^COUNT //p'; }
has_key() { ctl list | grep -Fxq "$1" && echo yes || echo no; }
admin_of() { ctl list | sed -n 's/^ADMIN //p'; }
sign_feed() { "$CTL" sign-feed "$1" --seed "$2" 2>>"$TMP/ctl.err"; }
feed_signed() { ctl feed "$1" --sig "$2" | sed -n '1s/^\([^ ]*\).*/\1/p'; }
feed_unsigned() { ctl feed "$1" | sed -n '1s/^\([^ ]*\).*/\1/p'; }

echo "[gate] --- one-time admin bootstrap ---"
expect "bootstrap admin key" "$(outcome bootstrap "$admin_pub")" "OK"
expect "re-bootstrap with a different key refused" "$(outcome bootstrap "$wrong_pub")" "REFUSED"
expect "admin key unchanged after refused re-bootstrap" "$(admin_of)" "$admin_pub"
expect "unsigned feed on missing file refused" "$(feed_unsigned "$TMP/nope")" "REFUSED"

echo "[gate] --- valid signed feed revokes 2 keys, persists across processes ---"
cat >"$TMP/f_valid.feed" <<EOF
{"op":"revoke","key":"$t1"}
{"op":"revoke","key":"$t2"}
EOF
sig_valid=$(sign_feed "$TMP/f_valid.feed" "$admin_seed")
expect "feed signature is 128 hex chars" "${#sig_valid}" "128"
expect "valid signed feed applies" "$(feed_signed "$TMP/f_valid.feed" "$sig_valid")" "OK"
expect "revoked t1 listed" "$(has_key "$t1")" "yes"
expect "revoked t2 listed" "$(has_key "$t2")" "yes"
c2=$(count); expect "revoked count after feed" "$c2" "2"
expect "fresh process: t1 still listed" "$(has_key "$t1")" "yes"
expect "fresh process: count persists" "$(count)" "$c2"

echo "[gate] --- idempotent re-application is a no-op ---"
expect "re-applying same signed feed succeeds" "$(feed_signed "$TMP/f_valid.feed" "$sig_valid")" "OK"
expect "idempotent re-apply leaves count unchanged" "$(count)" "$c2"

echo "[gate] --- unsigned / wrong-key / tampered feeds refused before any DB write ---"
expect "unsigned feed refused" "$(feed_unsigned "$TMP/f_valid.feed")" "REFUSED"
expect "unsigned feed: no DB change" "$(count)" "$c2"
sig_wrong=$(sign_feed "$TMP/f_valid.feed" "$wrong_seed")
expect "wrong-key signed feed refused" "$(feed_signed "$TMP/f_valid.feed" "$sig_wrong")" "REFUSED"
expect "wrong-key feed: no DB change" "$(count)" "$c2"
sed "s/$t1/$t3/" "$TMP/f_valid.feed" >"$TMP/f_tampered.feed"
expect "single-byte tampered feed refused (original sig)" "$(feed_signed "$TMP/f_tampered.feed" "$sig_valid")" "REFUSED"
expect "tampered feed: no DB change" "$(count)" "$c2"
expect "tampered feed: t3 not revoked" "$(has_key "$t3")" "no"
expect "tampered feed: t1 still revoked" "$(has_key "$t1")" "yes"

echo "[gate] --- partially-invalid / duplicate / conflicting / oversized feeds ---"
cat >"$TMP/f_partial.feed" <<EOF
{"op":"revoke","key":"$t3"}
{"op":"revoke","key":"deadbeef"}
EOF
sig_partial=$(sign_feed "$TMP/f_partial.feed" "$admin_seed")
expect "partially-invalid feed refused (atomic all-or-none)" "$(feed_signed "$TMP/f_partial.feed" "$sig_partial")" "REFUSED"
expect "partially-invalid feed: no DB change" "$(count)" "$c2"
expect "partially-invalid feed: t3 absent (no partial apply)" "$(has_key "$t3")" "no"
cat >"$TMP/f_dup.feed" <<EOF
{"op":"revoke","key":"$t3"}
{"op":"revoke","key":"$t3"}
EOF
sig_dup=$(sign_feed "$TMP/f_dup.feed" "$admin_seed")
expect "duplicate-entry feed refused" "$(feed_signed "$TMP/f_dup.feed" "$sig_dup")" "REFUSED"
expect "duplicate feed: no DB change" "$(count)" "$c2"
cat >"$TMP/f_conflict.feed" <<EOF
{"op":"revoke","key":"$t3"}
{"op":"unrevoke","key":"$t3"}
EOF
sig_conflict=$(sign_feed "$TMP/f_conflict.feed" "$admin_seed")
expect "conflicting-entry feed refused" "$(feed_signed "$TMP/f_conflict.feed" "$sig_conflict")" "REFUSED"
expect "conflicting feed: no DB change" "$(count)" "$c2"
python3 -c "import sys; sys.stdout.write('{\"op\":\"revoke\",\"key\":\"$t3\"}\n'*3000)" >"$TMP/f_big.feed"
sig_big=$(sign_feed "$TMP/f_big.feed" "$admin_seed" 2>/dev/null)
[ -n "$sig_big" ] || sig_big="$sig_valid"
expect "oversized feed refused" "$(feed_signed "$TMP/f_big.feed" "$sig_big")" "REFUSED"
expect "oversized feed: no DB change" "$(count)" "$c2"

echo "[gate] --- signed feed with revoke+unrevoke applies atomically ---"
cat >"$TMP/f_mixed.feed" <<EOF
{"op":"revoke","key":"$t3"}
{"op":"unrevoke","key":"$t1"}
EOF
sig_mixed=$(sign_feed "$TMP/f_mixed.feed" "$admin_seed")
expect "mixed revoke/unrevoke feed applies" "$(feed_signed "$TMP/f_mixed.feed" "$sig_mixed")" "OK"
expect "mixed feed: t3 now revoked" "$(has_key "$t3")" "yes"
expect "mixed feed: t1 unrevoked" "$(has_key "$t1")" "no"
expect "mixed feed: count stays 2" "$(count)" "2"

echo "[gate] --- negative control: pre-change unsigned direct API still writes ---"
NCDB="$TMP/negdb"; mkdir -p "$NCDB"
nc=$("$TMP/negctl" legacy_direct "$NCDB" "$t4" 2>>"$TMP/ctl.err" | sed -n '1p')
expect "pre-change direct revoke needs no signature (negative control)" "$nc" "LEGACY_OK"
cat >"$TMP/f_nc.feed" <<EOF
{"op":"revoke","key":"$t4"}
EOF
expect "same unsigned feed refused by authorized importer" "$(feed_unsigned "$TMP/f_nc.feed")" "REFUSED"
expect "negative-control key absent from authorized DB" "$(has_key "$t4")" "no"

echo "[gate] final list on $DB:"
ctl list | sed 's/^/    /'
echo "[gate] $pass passed, $fail failed"
[ "$fail" -eq 0 ] || exit 1
