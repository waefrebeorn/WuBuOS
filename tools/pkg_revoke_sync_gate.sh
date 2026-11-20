#!/usr/bin/env bash
# pkg_revoke_sync_gate.sh -- opt-in differential gate for signed cross-device
# revocation-feed sync.  A loopback HTTP fixture serves an admin-signed feed
# and its detached signature sidecar plus tampered/wrong-key/unsigned/oversize/
# redirect variants.  Ephemeral Ed25519 keys are generated and destroyed on
# exit; no key material is committed and no external network is used.
#
# The negative control compiles the PRE-CHANGE operator CLI (from the leg base
# commit aadf13c, which has no `sync` command) against the same sources and
# shows `sync <url>` is refused there while it works after the change.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CTL="$ROOT/tools/wubu_pkg_key_ctl"
WUBU_MBEDTLS_ROOT="$("$ROOT/tools/mbedtls_devroot.sh")" || exit 2
export WUBU_MBEDTLS_ROOT
MB="$WUBU_MBEDTLS_ROOT/usr"
[ -f "$MB/include/mbedtls/version.h" ] || { echo "missing mbedTLS headers" >&2; exit 2; }
command -v openssl >/dev/null && command -v xxd >/dev/null && command -v python3 >/dev/null || exit 2
TMP=$(mktemp -d)
SRV_PID=""
cleanup() { [ -n "$SRV_PID" ] && kill "$SRV_PID" 2>/dev/null; rm -rf "$TMP"; }
trap cleanup EXIT

echo "[gate] mbedTLS: $(grep -m1 MBEDTLS_VERSION_STRING "$MB/include/mbedtls/version.h" | cut -d'"' -f2)"

# -- ephemeral Ed25519 keys ----------------------------------------------
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
wrong_seed=$(seed_hex "$TMP/wrong.pem")
t1=$(pub_hex "$TMP/t1.pem"); t2=$(pub_hex "$TMP/t2.pem")
t3=$(pub_hex "$TMP/t3.pem"); t4=$(pub_hex "$TMP/t4.pem")
for v in "$admin_seed" "$admin_pub" "$wrong_seed" "$t1" "$t2" "$t3" "$t4"; do
    [ "${#v}" -eq 64 ] || { echo "failed to extract 32-byte key material" >&2; exit 2; }
done
echo "[gate] generated 6 ephemeral Ed25519 keypairs (no key material in repo)"

# -- build operator CLI (post-change) ------------------------------------
echo "[gate] building operator CLI: make pkg_key_ctl"
make -C "$ROOT" pkg_key_ctl WUBU_MBEDTLS_ROOT="$WUBU_MBEDTLS_ROOT" >"$TMP/make.log" 2>&1 || {
    cat "$TMP/make.log" >&2; echo "[gate] FAIL: make pkg_key_ctl" >&2; exit 2;
}
[ -x "$CTL" ] || { echo "[gate] FAIL: no $CTL" >&2; exit 2; }
echo "[gate] built $CTL"

# -- build PRE-CHANGE negative-control CLI -------------------------------
CFLAGS="-w -O1 -g -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -DWUBU_HAVE_MBEDTLS -DWUBU_NO_LIBM"
INCLUDES="-I$ROOT -I$ROOT/include -I$ROOT/src -I$ROOT/src/gui -I$ROOT/src/runtime -I$ROOT/src/kernel -I$MB/include"
SRCS="$ROOT/src/runtime/wubu_crc32.c \
 $ROOT/src/gui/wubu_pkgmgr.c $ROOT/src/gui/wubu_pkgmgr_verify.c $ROOT/src/gui/wubu_pkg_sign.c $ROOT/src/gui/wubu_pkg_ed25519.c \
 $ROOT/src/gui/wubu_pkgmgr_revoke_feed.c $ROOT/src/gui/wubu_pkgmgr_revoke_sync.c \
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
LIBS="-L$MB/lib/x86_64-linux-gnu -lmbedtls -lmbedx509 -lmbedcrypto \
 -Wl,-rpath,$MB/lib/x86_64-linux-gnu -lsqlite3 -lzstd -lm"
BASE=aadf13c
git -C "$ROOT" show "$BASE:tools/wubu_pkg_key_ctl.c" >"$TMP/ctl_pre.c" || {
    echo "[gate] FAIL: cannot read pre-change CLI from $BASE" >&2; exit 2;
}
echo "[gate] compiling pre-change negative-control CLI from $BASE"
gcc $CFLAGS $INCLUDES -include "$ROOT/src/runtime/wubu_gnu_compat.h" $SRCS \
    "$TMP/ctl_pre.c" -o "$TMP/ctl_pre" $LIBS || {
    echo "[gate] FAIL: pre-change CLI build" >&2; exit 2;
}

# -- loopback HTTP fixture -----------------------------------------------
WWW="$TMP/www"; mkdir -p "$WWW"
cat >"$TMP/serve.py" <<'PY'
import http.server, socketserver, os, sys
class H(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=os.environ["SERVE_DIR"], **kw)
    def do_GET(self):
        if self.path == "/redirect":
            self.send_response(302)
            self.send_header("Location", "http://untrusted.invalid/feed")
            self.send_header("Content-Length", "0")
            self.end_headers()
            return
        return super().do_GET()
    def log_message(self, *a):
        pass
class S(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True
s = S(("127.0.0.1", 0), H)
with open(os.environ["PORT_FILE"], "w") as f:
    f.write(str(s.server_address[1]))
s.serve_forever()
PY
SERVE_DIR="$WWW" PORT_FILE="$TMP/port" python3 "$TMP/serve.py" >"$TMP/serve.log" 2>&1 &
SRV_PID=$!
for _ in $(seq 1 100); do [ -s "$TMP/port" ] && break; sleep 0.1; done
[ -s "$TMP/port" ] || { echo "[gate] FAIL: loopback server not up" >&2; cat "$TMP/serve.log" >&2; exit 2; }
PORT=$(cat "$TMP/port")
U="http://127.0.0.1:$PORT"; HU="https://127.0.0.1:$PORT"; DEAD="http://127.0.0.1:1/feed"
echo "[gate] loopback HTTP fixture on $U"

# -- feed variants --------------------------------------------------------
cat >"$WWW/feed" <<EOF
{"op":"revoke","key":"$t1"}
{"op":"revoke","key":"$t2"}
EOF
"$CTL" sign-feed "$WWW/feed" --seed "$admin_seed" >"$WWW/feed.sig" 2>>"$TMP/ctl.err"
sed "s/$t1/$t3/" "$WWW/feed" >"$WWW/tampered"
cp "$WWW/feed.sig" "$WWW/tampered.sig"          # original sig over changed bytes
cp "$WWW/feed" "$WWW/wrongkey"
"$CTL" sign-feed "$WWW/wrongkey" --seed "$wrong_seed" >"$WWW/wrongkey.sig" 2>>"$TMP/ctl.err"
cp "$WWW/feed" "$WWW/unsigned"                   # no .sig sidecar -> 404
python3 -c "import sys; sys.stdout.write('{\"op\":\"revoke\",\"key\":\"$t3\"}\n'*3000)" >"$WWW/oversized"
cp "$WWW/feed.sig" "$WWW/oversized.sig"          # refused by byte cap first
cp "$WWW/feed" "$WWW/untrusted"; cp "$WWW/feed.sig" "$WWW/untrusted.sig"
TRUSTED="$U/feed,$U/tampered,$U/wrongkey,$U/unsigned,$U/oversized,$U/redirect,$DEAD,$HU/feed"
# /untrusted is deliberately NOT in TRUSTED.

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
sync_outcome() { ctl sync "$1" --trusted "$TRUSTED" | sed -n '1s/^\([^ ]*\).*/\1/p'; }

echo "[gate] --- one-time admin bootstrap ---"
expect "bootstrap admin key" "$(outcome bootstrap "$admin_pub")" "OK"
expect "admin key recorded" "$(admin_of)" "$admin_pub"

echo "[gate] --- valid signed feed via loopback HTTP sync ---"
expect "sync valid signed feed" "$(sync_outcome "$U/feed")" "OK"
expect "revoked t1 listed" "$(has_key "$t1")" "yes"
expect "revoked t2 listed" "$(has_key "$t2")" "yes"
c2=$(count); expect "revoked count after sync" "$c2" "2"
echo "[gate] --- fresh process (new ctl invocation) still sees both revocations ---"
expect "fresh process: t1 still listed" "$(has_key "$t1")" "yes"
expect "fresh process: t2 still listed" "$(has_key "$t2")" "yes"
expect "fresh process: count persists" "$(count)" "$c2"
expect "idempotent re-sync is a no-op" "$(sync_outcome "$U/feed")" "OK"
expect "idempotent re-sync leaves count unchanged" "$(count)" "$c2"

echo "[gate] --- tampered / wrong-key / unsigned feeds refused, no DB change ---"
expect "tampered feed (original sig) refused" "$(sync_outcome "$U/tampered")" "REFUSED"
expect "tampered feed: no DB change" "$(count)" "$c2"
expect "tampered feed: t3 not revoked" "$(has_key "$t3")" "no"
expect "tampered feed: t1 still revoked" "$(has_key "$t1")" "yes"
expect "wrong-key signed feed refused" "$(sync_outcome "$U/wrongkey")" "REFUSED"
expect "wrong-key feed: no DB change" "$(count)" "$c2"
expect "unsigned feed (no sidecar) refused" "$(sync_outcome "$U/unsigned")" "REFUSED"
expect "unsigned feed: no DB change" "$(count)" "$c2"

echo "[gate] --- oversize / redirect / untrusted / TLS / unreachable refused ---"
expect "oversized feed refused" "$(sync_outcome "$U/oversized")" "REFUSED"
expect "oversized feed: no DB change" "$(count)" "$c2"
expect "redirect to untrusted host refused" "$(sync_outcome "$U/redirect")" "REFUSED"
expect "redirect: no DB change" "$(count)" "$c2"
expect "untrusted URL refused" "$(ctl sync "$U/untrusted" --trusted "$TRUSTED" | sed -n '1s/^\([^ ]*\).*/\1/p')" "REFUSED"
expect "untrusted URL: no DB change" "$(count)" "$c2"
expect "https to plain listener (TLS fail) refused" "$(sync_outcome "$HU/feed")" "REFUSED"
expect "TLS failure: no DB change" "$(count)" "$c2"
expect "unreachable endpoint refused" "$(sync_outcome "$DEAD")" "REFUSED"
expect "unreachable: no DB change" "$(count)" "$c2"

echo "[gate] --- negative control: pre-change CLI has no sync command ---"
"$TMP/ctl_pre" --db "$DB" sync "$U/feed" --trusted "$TRUSTED" >"$TMP/pre.out" 2>"$TMP/pre.err"
pre_rc=$?
expect "pre-change sync exits nonzero" "$([ "$pre_rc" -ne 0 ] && echo yes || echo no)" "yes"
expect "pre-change sync prints usage" "$(grep -q usage "$TMP/pre.err" && echo yes || echo no)" "yes"
expect "pre-change sync: no DB change" "$(count)" "$c2"
expect "negative-control key never revoked by sync" "$(has_key "$t4")" "no"

echo "[gate] final list on $DB:"
ctl list | sed 's/^/    /'
echo "[gate] $pass passed, $fail failed"
[ "$fail" -eq 0 ] || exit 1
