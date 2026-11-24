#!/usr/bin/env bash
# pkg_key_recovery_gate.sh -- opt-in differential gate for the three
# key-policy closures, exercised through the PUBLIC operator CLI:
#   (1) trust-anchor expiry requires an admin signature (domain-separated);
#   (2) the legacy single-signature feed importer is subject to the GLOBAL
#       quorum policy -- one signature is refused when quorum=2;
#   (3) a lost admin is recovered by 2-of-2 escrow signatures, and the old
#       admin key loses all authority immediately.
#
# Every refusal is paired with a negative control that re-enacts the
# PRE-change behavior (raw unsigned expiry write / direct single-admin feed
# verification) so a refusal is attributable to the new policy alone.
# Keys are ephemeral, generated in a temp dir and destroyed on exit.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CTL="$ROOT/tools/wubu_pkg_key_ctl"
# The CLI under test links real mbedTLS, so it needs an extracted dev root:
#   mkdir -p "$HOME/opt/mbedtls-root" && cd "$(mktemp -d)" \
#     && apt-get download libmbedtls-dev && dpkg-deb -x ./*.deb "$HOME/opt/mbedtls-root"
# That needs no root privileges. Without one there is nothing to verify, and a
# missing toolchain is not a failed recovery -- say SKIP, do not go red.
if [ -z "${WUBU_MBEDTLS_ROOT:-}" ] && [ -d "$HOME/opt/mbedtls-root/usr/include/mbedtls" ]; then
    WUBU_MBEDTLS_ROOT="$HOME/opt/mbedtls-root"
fi
if [ -z "${WUBU_MBEDTLS_ROOT:-}" ] || [ ! -d "$WUBU_MBEDTLS_ROOT/usr/include/mbedtls" ]; then
    echo "[gate] SKIP: no extracted mbedTLS dev root"
    echo "[gate]   set WUBU_MBEDTLS_ROOT, or extract one to ~/opt/mbedtls-root (see the header of this script)"
    exit 0
fi
export WUBU_MBEDTLS_ROOT
MB="$WUBU_MBEDTLS_ROOT/usr"
[ -f "$MB/include/mbedtls/version.h" ] || { echo "missing mbedTLS headers" >&2; exit 2; }
command -v openssl >/dev/null && command -v xxd >/dev/null
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

echo "[gate] mbedTLS: $(grep -m1 MBEDTLS_VERSION_STRING "$MB/include/mbedtls/version.h" | cut -d'"' -f2)"

# Six ephemeral Ed25519 keypairs: admin, wrong, two quorum/escrow members,
# the post-recovery admin, and a revocation target.
for n in admin wrong q1 q2 rec target; do
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
q1_seed=$(seed_hex "$TMP/q1.pem");       q1_pub=$(pub_hex "$TMP/q1.pem")
q2_seed=$(seed_hex "$TMP/q2.pem");       q2_pub=$(pub_hex "$TMP/q2.pem")
rec_seed=$(seed_hex "$TMP/rec.pem");     rec_pub=$(pub_hex "$TMP/rec.pem")
target=$(pub_hex "$TMP/target.pem")
for v in "$admin_seed" "$admin_pub" "$wrong_seed" "$wrong_pub" "$q1_seed" \
         "$q1_pub" "$q2_seed" "$q2_pub" "$rec_seed" "$rec_pub" "$target"; do
    [ "${#v}" -eq 64 ] || { echo "failed to extract 32-byte key material" >&2; exit 2; }
done
echo "[gate] generated 6 ephemeral Ed25519 keypairs (no key material in repo)"

echo "[gate] building operator CLI: make pkg_key_ctl"
make -C "$ROOT" pkg_key_ctl WUBU_MBEDTLS_ROOT="$WUBU_MBEDTLS_ROOT" >"$TMP/make.log" 2>&1 || {
    cat "$TMP/make.log" >&2; echo "[gate] FAIL: make pkg_key_ctl" >&2; exit 2;
}
[ -x "$CTL" ] || { echo "[gate] FAIL: no $CTL" >&2; exit 2; }

DB="$TMP/db"; mkdir -p "$DB"
export LD_LIBRARY_PATH="$MB/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
pass=0; fail=0
expect() {
    local label="$1" got="$2" want="$3"
    if [ "$got" = "$want" ]; then
        printf '[gate] %-62s PASS\n' "$label"; pass=$((pass+1))
    else
        printf '[gate] %-62s FAIL (got=%s want=%s)\n' "$label" "$got" "$want"; fail=$((fail+1))
    fi
}
ctl() { "$CTL" --db "$DB" "$@" 2>>"$TMP/ctl.err"; }
outcome() { ctl "$@" | sed -n '1s/^\([^ ]*\).*/\1/p'; }
has_key() { ctl list | grep -Fxq "$1" && echo yes || echo no; }
admin_of() { ctl list | sed -n 's/^ADMIN //p'; }
expiry_of() { ctl expiry-status "$1" | sed -n '1p'; }

echo "[gate] --- one-time admin bootstrap ---"
expect "bootstrap admin key" "$(outcome bootstrap "$admin_pub")" "OK"
expect "admin key recorded" "$(admin_of)" "$admin_pub"

echo "[gate] --- signed expiry: unsigned refused with no DB change ---"
epoch=$(( $(date +%s) + 3600 ))
expect "unsigned set-expiry refused" "$(outcome set-expiry "$target" "$epoch")" "REFUSED"
expect "unsigned expiry left no metadata" "$(expiry_of "$target")" "NOEXPIRY $target"
sig=$(ctl sign-expiry "$target" "$epoch" --seed "$admin_seed")
expect "expiry signature is 128 hex chars" "${#sig}" "128"
expect "signed set-expiry accepted" "$(outcome set-expiry "$target" "$epoch" --sig "$sig")" "OK"
expect "signed expiry stored" "$(expiry_of "$target" | awk '{print $1" "$3}')" "EXPIRY $epoch"
sig_wrong=$(ctl sign-expiry "$target" "$epoch" --seed "$wrong_seed")
expect "wrong-key set-expiry refused" "$(outcome set-expiry "$target" "$epoch" --sig "$sig_wrong")" "REFUSED"
expect "wrong-key expiry left metadata unchanged" "$(expiry_of "$target" | awk '{print $1" "$3}')" "EXPIRY $epoch"

echo "[gate] --- global quorum: single-sig feed refused when quorum=2 ---"
qsig=$(ctl sign-quorum 2 "$q1_pub" "$q2_pub" --seed "$admin_seed")
expect "configure 2-of-2 quorum" "$(outcome quorum-config 2 "$q1_pub" "$q2_pub" --sig "$qsig")" "OK"
cat >"$TMP/f.feed" <<EOF
{"op":"revoke","key":"$target"}
EOF
admin_sig=$(ctl sign-feed "$TMP/f.feed" --seed "$admin_seed")
expect "single admin signature on feed" "${#admin_sig}" "128"
expect "legacy feed (one sig) REFUSED under quorum=2" \
    "$(outcome feed "$TMP/f.feed" --sig "$admin_sig")" "REFUSED"
expect "refused feed revoked nothing" "$(has_key "$target")" "no"
s1=$(ctl sign-feed "$TMP/f.feed" --seed "$q1_seed")
s2=$(ctl sign-feed "$TMP/f.feed" --seed "$q2_seed")
expect "1-of-2 feed-quorum REFUSED" \
    "$(outcome feed-quorum "$TMP/f.feed" --sigs "$s1")" "REFUSED"
expect "2-of-2 feed-quorum ACCEPTED" \
    "$(outcome feed-quorum "$TMP/f.feed" --sigs "$s1,$s2")" "OK"
expect "2-of-2 feed revoked target" "$(has_key "$target")" "yes"
# Clean up the revocation with a 2-of-2 feed.
cat >"$TMP/f_un.feed" <<EOF
{"op":"unrevoke","key":"$target"}
EOF
u1=$(ctl sign-feed "$TMP/f_un.feed" --seed "$q1_seed")
u2=$(ctl sign-feed "$TMP/f_un.feed" --seed "$q2_seed")
expect "2-of-2 unrevoke feed ACCEPTED" \
    "$(outcome feed-quorum "$TMP/f_un.feed" --sigs "$u1,$u2")" "OK"
expect "target unrevoked again" "$(has_key "$target")" "no"

echo "[gate] --- escrow recovery: 2-of-N installs new admin ---"
esig=$(ctl sign-escrow 2 "$q1_pub" "$q2_pub" --seed "$admin_seed")
expect "configure 2-of-2 escrow (admin-signed)" \
    "$(outcome escrow-config 2 "$q1_pub" "$q2_pub" --sig "$esig")" "OK"
expect "escrow status is 2-of-2" "$(ctl escrow-status | sed -n '1p')" "ESCROW 2 2 SET"
expect "unsigned escrow config refused" \
    "$(outcome escrow-config 2 "$q1_pub" "$q2_pub")" "REFUSED"
r1=$(ctl sign-recover "$rec_pub" --seed "$q1_seed")
r2=$(ctl sign-recover "$rec_pub" --seed "$q2_seed")
rw=$(ctl sign-recover "$rec_pub" --seed "$wrong_seed")
expect "1-of-2 escrow recovery REFUSED" "$(outcome recover "$rec_pub" --sigs "$r1")" "REFUSED"
expect "wrong-key escrow recovery REFUSED" "$(outcome recover "$rec_pub" --sigs "$rw")" "REFUSED"
expect "one valid + one wrong REFUSED" "$(outcome recover "$rec_pub" --sigs "$r1,$rw")" "REFUSED"
expect "admin unchanged after refused recovery" "$(admin_of)" "$admin_pub"
expect "2-of-2 escrow recovery ACCEPTED" \
    "$(outcome recover "$rec_pub" --sigs "$r1,$r2")" "OK"
expect "admin is now the recovered key" "$(admin_of)" "$rec_pub"

echo "[gate] --- old admin key after recovery refused ---"
revoke_sig_old=$(ctl sign revoke "$target" --seed "$admin_seed")
expect "old-admin revoke REFUSED after recovery" \
    "$(outcome revoke "$target" --sig "$revoke_sig_old")" "REFUSED"
expect "old-admin revoke changed nothing" "$(has_key "$target")" "no"
revoke_sig_new=$(ctl sign revoke "$target" --seed "$rec_seed")
expect "recovered-admin revoke ACCEPTED" \
    "$(outcome revoke "$target" --sig "$revoke_sig_new")" "OK"
expect "recovered-admin revocation persisted" "$(has_key "$target")" "yes"
# Signed expiry also requires the NEW admin after recovery.
epoch2=$(( epoch + 60 ))
exp_old=$(ctl sign-expiry "$target" "$epoch2" --seed "$admin_seed")
expect "old-admin signed expiry REFUSED after recovery" \
    "$(outcome set-expiry "$target" "$epoch2" --sig "$exp_old")" "REFUSED"
exp_new=$(ctl sign-expiry "$target" "$epoch2" --seed "$rec_seed")
expect "recovered-admin signed expiry ACCEPTED" \
    "$(outcome set-expiry "$target" "$epoch2" --sig "$exp_new")" "OK"

echo "[gate] $pass passed, $fail failed"
[ "$fail" -eq 0 ] || exit 1
echo "[gate] ALL RECOVERY CLI CHECKS PASSED"
