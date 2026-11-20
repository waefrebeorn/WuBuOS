#!/usr/bin/env bash
# pkg_revoke_sync_gate.sh -- opt-in differential gate for signed cross-device
# revocation-feed sync.  A loopback HTTP fixture AND a loopback HTTPS fixture
# (with an ephemeral, locally generated CA) serve admin-signed feeds with a
# signed monotonic generation line, plus tampered/wrong-key/unsigned/oversize/
# redirect variants.  Ephemeral Ed25519 keys and the CA are generated and
# destroyed on exit; no key material is committed and no external network is
# used.
#
# Differential properties asserted here:
#   * signed seq N applies; replay N-1 (and exact N) is REFUSED with no DB change;
#     signed seq N+1 then applies;
#   * HTTPS with the trusted local CA succeeds; an untrusted CA and a
#     wrong-host URL are REFUSED;
#   * trusted_sync_url stored through the GUI settings store survives
#     save/load and is the sole trust source on a fresh process (no --trusted,
#     no config file, no env var);
#   * the PRE-CHANGE operator CLI from aadf13c (no `sync` command) still refuses.
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
CTL="$ROOT/tools/wubu_pkg_key_ctl"
WUBU_MBEDTLS_ROOT="$("$ROOT/tools/mbedtls_devroot.sh")" || exit 2
export WUBU_MBEDTLS_ROOT
MB="$WUBU_MBEDTLS_ROOT/usr"
[ -f "$MB/include/mbedtls/version.h" ] || { echo "missing mbedTLS headers" >&2; exit 2; }
command -v openssl >/dev/null && command -v xxd >/dev/null && command -v python3 >/dev/null || exit 2
TMP=$(mktemp -d)
SRV_PID=""; HTTPS_PID=""
cleanup() {
    [ -n "$SRV_PID" ] && kill "$SRV_PID" 2>/dev/null
    [ -n "$HTTPS_PID" ] && kill "$HTTPS_PID" 2>/dev/null
    rm -rf "$TMP"
}
trap cleanup EXIT

echo "[gate] mbedTLS: $(grep -m1 MBEDTLS_VERSION_STRING "$MB/include/mbedtls/version.h" | cut -d'"' -f2)"

# -- ephemeral Ed25519 keys ----------------------------------------------
for n in admin wrong t1 t2 t3 t4 t5 t6; do
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
t5=$(pub_hex "$TMP/t5.pem"); t6=$(pub_hex "$TMP/t6.pem")
for v in "$admin_seed" "$admin_pub" "$wrong_seed" "$t1" "$t2" "$t3" "$t4" "$t5" "$t6"; do
    [ "${#v}" -eq 64 ] || { echo "failed to extract 32-byte key material" >&2; exit 2; }
done
echo "[gate] generated 9 ephemeral Ed25519 keypairs (no key material in repo)"

# -- ephemeral local CA + server cert (DNS SAN localhost) -----------------
# mbedTLS 2.28 verifies DNS SAN/CN; an IP-literal CN is not accepted, so the
# loopback cert is issued for "localhost" (which resolves to 127.0.0.1).
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$TMP/ca.key" -out "$TMP/ca.pem" \
    -days 1 -subj "/CN=WuBu RevSync Test CA" \
    -addext "basicConstraints=critical,CA:TRUE" 2>/dev/null || { echo "[gate] FAIL: CA generation" >&2; exit 2; }
openssl req -newkey rsa:2048 -nodes -keyout "$TMP/srv.key" -out "$TMP/srv.csr" \
    -subj "/CN=localhost" 2>/dev/null || { echo "[gate] FAIL: server key generation" >&2; exit 2; }
printf 'basicConstraints=CA:FALSE\nkeyUsage=digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=DNS:localhost\n' >"$TMP/srv.ext"
openssl x509 -req -in "$TMP/srv.csr" -CA "$TMP/ca.pem" -CAkey "$TMP/ca.key" -CAcreateserial \
    -out "$TMP/srv.pem" -days 1 -extfile "$TMP/srv.ext" 2>/dev/null || { echo "[gate] FAIL: server cert signing" >&2; exit 2; }
# A DIFFERENT CA: a client trusting this one must refuse our server cert.
openssl req -x509 -newkey rsa:2048 -nodes -keyout "$TMP/other.key" -out "$TMP/other.pem" \
    -days 1 -subj "/CN=WuBu RevSync Untrusted CA" \
    -addext "basicConstraints=critical,CA:TRUE" 2>/dev/null || exit 2
echo "[gate] generated ephemeral local CA + IP:127.0.0.1 server cert (no secrets in repo)"

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

# -- loopback HTTP + HTTPS fixtures --------------------------------------
WWW="$TMP/www"; mkdir -p "$WWW" "$TMP/cfghome"
cat >"$TMP/serve.py" <<'PY'
import http.server, socketserver, os, ssl
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
if os.environ.get("SERVE_SSL") == "1":
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(os.environ["CERT"], os.environ["KEY"])
    s.socket = ctx.wrap_socket(s.socket, server_side=True)
with open(os.environ["PORT_FILE"], "w") as f:
    f.write(str(s.server_address[1]))
s.serve_forever()
PY
SERVE_DIR="$WWW" PORT_FILE="$TMP/port" python3 "$TMP/serve.py" >"$TMP/serve.log" 2>&1 &
SRV_PID=$!
SERVE_SSL=1 CERT="$TMP/srv.pem" KEY="$TMP/srv.key" SERVE_DIR="$WWW" PORT_FILE="$TMP/sport" \
    python3 "$TMP/serve.py" >"$TMP/serve_https.log" 2>&1 &
HTTPS_PID=$!
for _ in $(seq 1 100); do [ -s "$TMP/port" ] && [ -s "$TMP/sport" ] && break; sleep 0.1; done
[ -s "$TMP/port" ] && [ -s "$TMP/sport" ] || {
    echo "[gate] FAIL: loopback servers not up" >&2
    cat "$TMP/serve.log" "$TMP/serve_https.log" >&2; exit 2;
}
PORT=$(cat "$TMP/port"); SPORT=$(cat "$TMP/sport")
U="http://127.0.0.1:$PORT"; HU="https://localhost:$SPORT"
HW="https://127.0.0.1:$SPORT"; HPLAIN="https://127.0.0.1:$PORT"
DEAD="http://127.0.0.1:1/feed"
echo "[gate] loopback HTTP fixture on $U; HTTPS fixture on $HU"

# -- signed, generation-bearing feed variants ----------------------------
# Every feed carries {"seq":N} as its first meaningful line; the detached
# signature covers those exact bytes, so the generation cannot be edited.
printf '{"seq":5}\n{"op":"revoke","key":"%s"}\n{"op":"revoke","key":"%s"}\n' "$t1" "$t2" >"$WWW/feed5"
printf '{"seq":4}\n{"op":"revoke","key":"%s"}\n' "$t3" >"$WWW/feed4"
printf '{"seq":6}\n{"op":"revoke","key":"%s"}\n' "$t3" >"$WWW/feed6"
printf '{"seq":7}\n{"op":"revoke","key":"%s"}\n' "$t4" >"$WWW/feed7"
printf '{"seq":8}\n{"op":"revoke","key":"%s"}\n' "$t6" >"$WWW/feed8"
for f in feed5 feed4 feed6 feed7 feed8; do
    "$CTL" sign-feed "$WWW/$f" --seed "$admin_seed" >"$WWW/$f.sig" 2>>"$TMP/ctl.err"
done
# Fresh-generation tampering: valid signature over t3, bytes changed to t5.
printf '{"seq":21}\n{"op":"revoke","key":"%s"}\n' "$t3" >"$WWW/tampered"
"$CTL" sign-feed "$WWW/tampered" --seed "$admin_seed" >"$WWW/tampered.sig" 2>>"$TMP/ctl.err"
sed "s/$t3/$t5/" "$WWW/tampered" >"$WWW/tampered_bad"
cp "$WWW/tampered.sig" "$WWW/tampered_bad.sig"
# Fresh-generation wrong-key signature.
printf '{"seq":22}\n{"op":"revoke","key":"%s"}\n' "$t3" >"$WWW/wrongkey"
"$CTL" sign-feed "$WWW/wrongkey" --seed "$wrong_seed" >"$WWW/wrongkey.sig" 2>>"$TMP/ctl.err"
printf '{"seq":23}\n{"op":"revoke","key":"%s"}\n' "$t3" >"$WWW/unsigned"  # no sidecar -> 404
{ printf '{"seq":24}\n'; python3 -c "print('{\"op\":\"revoke\",\"key\":\"$t3\"}' * 3000)"; } >"$WWW/oversized"
cp "$WWW/feed5.sig" "$WWW/oversized.sig"                       # byte cap first
cp "$WWW/feed5" "$WWW/untrusted"; cp "$WWW/feed5.sig" "$WWW/untrusted.sig"
TRUSTED="$U/feed5,$U/feed4,$U/feed6,$U/tampered_bad,$U/wrongkey,$U/unsigned,$U/oversized,$U/redirect,$DEAD,$HPLAIN/feed5,$HU/feed7,$HW/feed7"
# /untrusted is deliberately NOT in TRUSTED.

DB="$TMP/db"; mkdir -p "$DB"
export LD_LIBRARY_PATH="$MB/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# GUI settings store lives under an isolated XDG_CONFIG_HOME for this gate.
export XDG_CONFIG_HOME="$TMP/cfghome"
unset WUBU_PKGMGR_SYNC_URLS

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
tls_outcome() { WUBU_CA_BUNDLE="$1" ctl sync "$2" --trusted "$TRUSTED" | sed -n '1s/^\([^ ]*\).*/\1/p'; }
sync_settings() { ctl sync "$1" | sed -n '1s/^\([^ ]*\).*/\1/p'; }   # no --trusted

echo "[gate] --- one-time admin bootstrap ---"
expect "bootstrap admin key" "$(outcome bootstrap "$admin_pub")" "OK"
expect "admin key recorded" "$(admin_of)" "$admin_pub"

echo "[gate] --- signed generation N applies; N-1 and N replay refused; N+1 applies ---"
expect "sync signed seq 5" "$(sync_outcome "$U/feed5")" "OK"
expect "seq 5: t1 revoked" "$(has_key "$t1")" "yes"
expect "seq 5: t2 revoked" "$(has_key "$t2")" "yes"
c5=$(count); expect "count after seq 5" "$c5" "2"
expect "replay seq 4 (older) REFUSED" "$(sync_outcome "$U/feed4")" "REFUSED"
expect "replay seq 4: no DB change" "$(count)" "$c5"
expect "replay seq 4: t3 NOT revoked" "$(has_key "$t3")" "no"
expect "replay exact seq 5 REFUSED" "$(sync_outcome "$U/feed5")" "REFUSED"
expect "replay seq 5: no DB change" "$(count)" "$c5"
expect "fresh process still sees seq 5 state" "$(has_key "$t1")" "yes"
expect "sync signed seq 6 (N+1)" "$(sync_outcome "$U/feed6")" "OK"
expect "seq 6: t3 revoked" "$(has_key "$t3")" "yes"
c6=$(count); expect "count after seq 6" "$c6" "3"
expect "replay seq 6 REFUSED" "$(sync_outcome "$U/feed6")" "REFUSED"
expect "replay seq 6: no DB change" "$(count)" "$c6"

echo "[gate] --- tampered / wrong-key / unsigned feeds refused, no DB change ---"
expect "tampered feed (fresh seq, bad sig) refused" "$(sync_outcome "$U/tampered_bad")" "REFUSED"
expect "tampered feed: no DB change" "$(count)" "$c6"
expect "tampered feed: t5 not revoked" "$(has_key "$t5")" "no"
expect "wrong-key signed feed refused" "$(sync_outcome "$U/wrongkey")" "REFUSED"
expect "wrong-key feed: no DB change" "$(count)" "$c6"
expect "unsigned feed (no sidecar) refused" "$(sync_outcome "$U/unsigned")" "REFUSED"
expect "unsigned feed: no DB change" "$(count)" "$c6"

echo "[gate] --- oversize / redirect / untrusted / TLS-fail / unreachable refused ---"
expect "oversized feed refused" "$(sync_outcome "$U/oversized")" "REFUSED"
expect "oversized feed: no DB change" "$(count)" "$c6"
expect "redirect to untrusted host refused" "$(sync_outcome "$U/redirect")" "REFUSED"
expect "redirect: no DB change" "$(count)" "$c6"
expect "untrusted URL refused" "$(ctl sync "$U/untrusted" --trusted "$TRUSTED" | sed -n '1s/^\([^ ]*\).*/\1/p')" "REFUSED"
expect "untrusted URL: no DB change" "$(count)" "$c6"
expect "https to plain listener (TLS fail) refused" "$(sync_outcome "$HPLAIN/feed5")" "REFUSED"
expect "TLS failure: no DB change" "$(count)" "$c6"
expect "unreachable endpoint refused" "$(sync_outcome "$DEAD")" "REFUSED"
expect "unreachable: no DB change" "$(count)" "$c6"

echo "[gate] --- loopback HTTPS trusted-CA success + untrusted/wrong-host refusal ---"
expect "HTTPS trusted local CA accepted" "$(tls_outcome "$TMP/ca.pem" "$HU/feed7")" "OK"
expect "HTTPS: t4 revoked" "$(has_key "$t4")" "yes"
c7=$(count); expect "count after HTTPS sync" "$c7" "4"
expect "untrusted CA refused" "$(tls_outcome "$TMP/other.pem" "$HU/feed7")" "REFUSED"
expect "untrusted CA: no DB change" "$(count)" "$c7"
expect "wrong-host cert refused" "$(tls_outcome "$TMP/ca.pem" "$HW/feed7")" "REFUSED"
expect "wrong-host: no DB change" "$(count)" "$c7"

echo "[gate] --- GUI settings store read path survives save/load ---"
expect "store trusted_sync_url via settings" "$(outcome set-sync-url "$U/feed8")" "OK"
expect "settings save/load round-trip" "$(ctl get-sync-url | sed -n '1s/^SYNCURL //p')" "$U/feed8"
sync_conf="$XDG_CONFIG_HOME/wubu/settings.conf"
expect "settings.conf persisted key" "$(grep -q '"trusted_sync_url"' "$sync_conf" && echo yes || echo no)" "yes"
expect "sync trusts URL from settings only (no --trusted)" "$(sync_settings "$U/feed8")" "OK"
expect "settings-sourced sync: t6 revoked" "$(has_key "$t6")" "yes"
c8=$(count); expect "count after settings sync" "$c8" "5"
expect "URL absent from settings is refused" "$(sync_settings "$U/untrusted")" "REFUSED"
expect "absent URL: no DB change" "$(count)" "$c8"

echo "[gate] --- negative control: pre-change CLI has no sync command ---"
"$TMP/ctl_pre" --db "$DB" sync "$U/feed5" --trusted "$TRUSTED" >"$TMP/pre.out" 2>"$TMP/pre.err"
pre_rc=$?
expect "pre-change sync exits nonzero" "$([ "$pre_rc" -ne 0 ] && echo yes || echo no)" "yes"
expect "pre-change sync prints usage" "$(grep -q usage "$TMP/pre.err" && echo yes || echo no)" "yes"
expect "pre-change sync: no DB change" "$(count)" "$c8"
expect "negative-control key never revoked by sync" "$(has_key "$t5")" "no"

echo "[gate] final list on $DB:"
ctl list | sed 's/^/    /'
echo "[gate] $pass passed, $fail failed"
[ "$fail" -eq 0 ] || exit 1
