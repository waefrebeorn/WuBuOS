#!/usr/bin/env bash
# sync_panel_gate.sh -- differential gate for the Control Panel revocation-sync
# widget and its bounded scheduled loop.
#
# Drives the REAL applet key/mouse callbacks in a hosted GUI test binary
# (src/gui/test_sync_panel.c) against a loopback HTTP fixture serving admin-
# signed feeds. Asserts: the widget saves trusted_sync_url through the GUI
# settings store; Sync Now revokes keys that persist across a fresh DB load;
# bad-scheme and bad-signature URLs are REFUSED with no DB change; the
# scheduled loop is OFF by default, runs at most once per interval, and
# applies a ROTATED signed feed at the same URL after the interval elapses.
# Epochal keys are generated on exit; no key material is committed.
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

# -- ephemeral Ed25519 keys ------------------------------------------------
for n in admin wrong t1 t2 t3 t4 t5 t6; do
    openssl genpkey -algorithm ED25519 -out "$TMP/$n.pem" 2>/dev/null
done
seed_hex() { openssl pkey -in "$1" -text -noout | awk '/^priv:/{f=1;next} /^pub:/{f=0} f{gsub(/[ :]/,"");printf "%s",$0}'; }
pub_hex()  { openssl pkey -in "$1" -pubout -outform DER 2>/dev/null | tail -c 32 | xxd -p -c 64 | tr -d '\n'; }
admin_seed=$(seed_hex "$TMP/admin.pem"); admin_pub=$(pub_hex "$TMP/admin.pem")
wrong_seed=$(seed_hex "$TMP/wrong.pem")
k1=$(pub_hex "$TMP/t1.pem"); k2=$(pub_hex "$TMP/t2.pem")
k3=$(pub_hex "$TMP/t3.pem"); k4=$(pub_hex "$TMP/t4.pem"); kt=$(pub_hex "$TMP/t5.pem")
k6=$(pub_hex "$TMP/t6.pem")
for v in "$admin_seed" "$admin_pub" "$wrong_seed" "$k1" "$k2" "$k3" "$k4" "$kt" "$k6"; do
    [ "${#v}" -eq 64 ] || { echo "failed to extract 32-byte key material" >&2; exit 2; }
done
echo "[gate] generated 9 ephemeral Ed25519 keypairs (no key material in repo)"

echo "[gate] building operator CLI + hosted GUI test: make pkg_key_ctl"
make -C "$ROOT" pkg_key_ctl WUBU_MBEDTLS_ROOT="$WUBU_MBEDTLS_ROOT" >"$TMP/make.log" 2>&1 || {
    cat "$TMP/make.log" >&2; echo "[gate] FAIL: make pkg_key_ctl" >&2; exit 2;
}
[ -x "$CTL" ] || { echo "[gate] FAIL: no $CTL" >&2; exit 2; }

# -- build the hosted GUI test binary -------------------------------------
CFLAGS="-w -O1 -g -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -DWUBU_HAVE_MBEDTLS -DWUBU_NO_LIBM -DVBE_HOSTED"
INCLUDES="-I$ROOT -I$ROOT/include -I$ROOT/src -I$ROOT/src/gui -I$ROOT/src/runtime -I$ROOT/src/kernel -I$MB/include"
SRCS="$ROOT/src/runtime/wubu_crc32.c \
 $ROOT/src/gui/wubu_pkgmgr.c $ROOT/src/gui/wubu_pkgmgr_verify.c $ROOT/src/gui/wubu_pkg_sign.c $ROOT/src/gui/wubu_pkg_ed25519.c \
 $ROOT/src/gui/wubu_pkgmgr_revoke_feed.c $ROOT/src/gui/wubu_pkgmgr_revoke_sync.c \
 $ROOT/src/gui/wubu_pkgmgr_resolve.c $ROOT/src/gui/wubu_pkgmgr_manifest.c \
 $ROOT/src/gui/wubu_pkgmgr_db.c $ROOT/src/gui/wubu_pkgmgr_pkg.c \
 $ROOT/src/gui/wubu_pkgmgr_install.c $ROOT/src/gui/wubu_pkgmgr_txn.c \
 $ROOT/src/gui/wubu_pkgmgr_remote.c $ROOT/src/gui/wubu_pkgmgr_fetch.c \
 $ROOT/src/gui/wubu_pkgmgr_sync_panel.c \
 $ROOT/src/gui/wubu_settings.c $ROOT/src/gui/wubu_settings_defaults.c $ROOT/src/gui/wubu_settings_io.c \
 $ROOT/src/gui/wubu_theme.c \
 $ROOT/src/runtime/wubu_pkg.c $ROOT/src/runtime/wubu_json.c $ROOT/src/runtime/wubu_palette.c \
 $ROOT/src/runtime/styx_names.c $ROOT/src/runtime/styx_enc.c $ROOT/src/runtime/styx_serve.c \
 $ROOT/src/runtime/styx_parse.c $ROOT/src/runtime/styx_fid.c $ROOT/src/runtime/styxfs_vfs.c \
 $ROOT/src/runtime/styxfs_callbacks.c $ROOT/src/runtime/styxfs_posix.c $ROOT/src/runtime/styxfs_path.c \
 $ROOT/src/runtime/styxfs_host.c $ROOT/src/runtime/styxfs_util.c \
 $ROOT/src/runtime/wubu_guest_root.c $ROOT/src/runtime/wubu_container.c \
 $ROOT/src/runtime/oci/oci_http_client.c $ROOT/src/kernel/wubu_math.c $ROOT/src/kernel/vbe.c"
LIBS="-L$MB/lib/x86_64-linux-gnu -lmbedtls -lmbedx509 -lmbedcrypto \
 -Wl,-rpath,$MB/lib/x86_64-linux-gnu -lsqlite3 -lzstd -lm"
echo "[gate] compiling hosted GUI test binary"
gcc $CFLAGS $INCLUDES -include "$ROOT/src/runtime/wubu_gnu_compat.h" $SRCS \
    "$ROOT/src/gui/test_sync_panel.c" -o "$TMP/test_sync_panel" $LIBS || {
    echo "[gate] FAIL: test_sync_panel build" >&2; exit 2;
}

# -- loopback HTTP fixture ------------------------------------------------
WWW="$TMP/www"; mkdir -p "$WWW" "$TMP/db" "$TMP/cfghome"
cat >"$TMP/serve.py" <<'PY'
import http.server, socketserver, os
class H(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **kw):
        super().__init__(*a, directory=os.environ["SERVE_DIR"], **kw)
    def log_message(self, *a): pass
class S(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True
s = S(("127.0.0.1", 0), H)
with open(os.environ["PORT_FILE"], "w") as f: f.write(str(s.server_address[1]))
s.serve_forever()
PY
SERVE_DIR="$WWW" PORT_FILE="$TMP/port" python3 "$TMP/serve.py" >"$TMP/serve.log" 2>&1 &
SRV_PID=$!
for _ in $(seq 1 100); do [ -s "$TMP/port" ] && break; sleep 0.1; done
[ -s "$TMP/port" ] || { echo "[gate] FAIL: loopback server not up" >&2; cat "$TMP/serve.log" >&2; exit 2; }
PORT=$(cat "$TMP/port"); U="http://127.0.0.1:$PORT"
echo "[gate] loopback HTTP fixture on $U"

# -- signed feeds: initial (seq 6, k1+k2), rotated (seq 7, k3), recovery (seq 8, k4) ----
printf '{"seq":6}\n{"op":"revoke","key":"%s"}\n{"op":"revoke","key":"%s"}\n' "$k1" "$k2" >"$TMP/feed_a"
printf '{"seq":7}\n{"op":"revoke","key":"%s"}\n' "$k3" >"$TMP/feed_b"
printf '{"seq":8}\n{"op":"revoke","key":"%s"}\n' "$k4" >"$TMP/feed_c"
printf '{"seq":9}\n{"op":"revoke","key":"%s"}\n' "$k6" >"$TMP/feed_d"
"$CTL" sign-feed "$TMP/feed_a" --seed "$admin_seed" >"$TMP/feed_a.sig" 2>>"$TMP/ctl.err"
"$CTL" sign-feed "$TMP/feed_b" --seed "$admin_seed" >"$TMP/feed_b.sig" 2>>"$TMP/ctl.err"
"$CTL" sign-feed "$TMP/feed_c" --seed "$admin_seed" >"$TMP/feed_c.sig" 2>>"$TMP/ctl.err"
"$CTL" sign-feed "$TMP/feed_d" --seed "$admin_seed" >"$TMP/feed_d.sig" 2>>"$TMP/ctl.err"
cp "$TMP/feed_a" "$WWW/feed_a"; cp "$TMP/feed_a.sig" "$WWW/feed_a.sig"
cp "$TMP/feed_b" "$WWW/feed_b"; cp "$TMP/feed_b.sig" "$WWW/feed_b.sig"
cp "$TMP/feed_c" "$WWW/feed_c"; cp "$TMP/feed_c.sig" "$WWW/feed_c.sig"
cp "$TMP/feed_d" "$WWW/feed_d"; cp "$TMP/feed_d.sig" "$WWW/feed_d.sig"
ln -s feed_a "$WWW/live"       # widget URL; gate rotates the symlink to feed_b
ln -s feed_a.sig "$WWW/live.sig"  # detached sidecar for the same URL
ln -s feed_d "$WWW/live2"      # second trusted URL (bounded multi-URL list)
ln -s feed_d.sig "$WWW/live2.sig"
# Bad signature: signed over k3, bytes swapped to kt, sidecar reused.
printf '{"seq":8}\n{"op":"revoke","key":"%s"}\n' "$k3" >"$WWW/tampered"
"$CTL" sign-feed "$WWW/tampered" --seed "$admin_seed" >"$WWW/tampered.sig" 2>>"$TMP/ctl.err"
sed "s/$k3/$kt/" "$WWW/tampered" >"$WWW/tampered_bad"
cp "$WWW/tampered.sig" "$WWW/tampered_bad.sig"

export LD_LIBRARY_PATH="$MB/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export XDG_CONFIG_HOME="$TMP/cfghome"
unset WUBU_PKGMGR_SYNC_URLS

echo "[gate] --- driving the widget in a hosted GUI test ---"
WUBU_SYNC_DB="$TMP/db" WUBU_SYNC_ADMIN="$admin_pub" \
WUBU_SYNC_URL="$U/live" WUBU_SYNC_URL2="$U/live2" WUBU_SYNC_BADSIG_URL="$U/tampered_bad" \
WUBU_SYNC_BAD_URL="ftp://127.0.0.1:$PORT/live" WUBU_SYNC_SERVE_DIR="$WWW" \
WUBU_SYNC_KEY1="$k1" WUBU_SYNC_KEY2="$k2" WUBU_SYNC_KEY3="$k3" WUBU_SYNC_KEY4="$k4" \
WUBU_SYNC_TAMPER_KEY="$kt" WUBU_SYNC_KEY6="$k6" \
    "$TMP/test_sync_panel" >"$TMP/test.out" 2>&1
rc=$?
cat "$TMP/test.out"

echo "[gate] --- persisted settings.conf (new keys) ---"
grep -E 'trusted_sync_url|sync_auto_enabled|sync_interval_secs|sync_backoff' "$TMP/cfghome/wubu/settings.conf" \
    | sed 's/^/    /'
for key in sync_backoff_initial_secs sync_backoff_max_secs sync_backoff_jitter_pct; do
    grep -q "$key" "$TMP/cfghome/wubu/settings.conf" || {
        echo "[gate] FAIL: $key not persisted" >&2; exit 1; }
done

# -- negative control: pre-change tree has no widget / scheduled keys -----
if git -C "$ROOT" cat-file -e 780bdc7:src/gui/wubu_pkgmgr_sync_panel.h 2>/dev/null; then
    echo "[gate] FAIL: pre-change tree unexpectedly has the sync widget" >&2; exit 1
fi
if git -C "$ROOT" show 780bdc7:src/gui/wubu_settings.h | grep -q sync_auto_enabled; then
    echo "[gate] FAIL: pre-change tree unexpectedly has scheduled-sync settings" >&2; exit 1
fi
if git -C "$ROOT" show 780bdc7:src/gui/wubu_settings.h | grep -q sync_backoff; then
    echo "[gate] FAIL: pre-change tree unexpectedly has backoff settings" >&2; exit 1
fi
echo "[gate] negative control: pre-change 780bdc7 has no sync widget, loop or backoff keys"
# This leg's pre-change base a6c46b0: no jitter key, no bounded-list API, and
# HOSTED_TLS defaults to 0. The differential is the code under test, not the
# loopback fixture.
BASE=a6c46b03fd281d1decd8d7f92381cc558add9564
if git -C "$ROOT" show "$BASE:src/gui/wubu_settings.h" | grep -q sync_backoff_jitter_pct; then
    echo "[gate] FAIL: pre-change tree unexpectedly has a jitter setting" >&2; exit 1
fi
if git -C "$ROOT" show "$BASE:src/gui/wubu_pkgmgr_sync_panel.h" | grep -q cp_pkg_sync_backoff_delay_jittered; then
    echo "[gate] FAIL: pre-change tree unexpectedly has jittered backoff" >&2; exit 1
fi
if git -C "$ROOT" show "$BASE:src/gui/wubu_pkgmgr_sync_panel.c" | grep -q cp_pkg_sync_url_add; then
    echo "[gate] FAIL: pre-change tree unexpectedly has the multi-URL API" >&2; exit 1
fi
if ! git -C "$ROOT" show "$BASE:mk/vars.mk" | grep -q 'HOSTED_TLS ?= 0'; then
    echo "[gate] FAIL: pre-change tree did not have the fail-closed TLS default" >&2; exit 1
fi
echo "[gate] negative control: base $BASE has no jitter, no multi-URL list and HOSTED_TLS ?= 0"
grep -q '0 failures' "$TMP/test.out" || { echo "[gate] FAIL: test reported failures" >&2; exit 1; }
[ "$rc" -eq 0 ] || { echo "[gate] FAIL: test_sync_panel exit $rc" >&2; exit 1; }
echo "[gate] ALL SYNC-PANEL GUI CHECKS PASSED (bad URL/signature refused, bounded backoff, resets on success)"
