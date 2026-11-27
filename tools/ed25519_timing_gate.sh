#!/usr/bin/env bash
# ed25519_timing_gate.sh -- differential timing gate for Ed25519 scalar
# multiplication.  Builds the SAME probe twice against the SAME mbedTLS:
#   probe_new : production build, ge_mul == constant-time Montgomery ladder
#   probe_old : -DWUBU_ED25519_LEGACY_MUL, ge_mul == pre-change double-and-add
# The probe times both implementations directly over controlled secret
# scalars of fixed bit length and varying Hamming weight, checks that the
# ladder is functionally identical to the old code, and exercises the public
# sign/verify API over many distinct keys.
#
# Statistical differential, NOT a formal constant-time proof.  Keys are
# ephemeral and generated in-process; no key material is written to the repo.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
WUBU_MBEDTLS_ROOT="$("$ROOT/tools/mbedtls_devroot.sh")"
export WUBU_MBEDTLS_ROOT
MB="$WUBU_MBEDTLS_ROOT/usr"
for f in "$MB/include/mbedtls/sha512.h" "$MB/include/mbedtls/bignum.h" \
         "$MB/lib/x86_64-linux-gnu/libmbedcrypto.a"; do
    [ -f "$f" ] || { echo "missing mbedTLS component: $f" >&2; exit 2; }
done
command -v openssl >/dev/null
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

echo "[timing] mbedTLS: $(grep -m1 MBEDTLS_VERSION_STRING "$MB/include/mbedtls/version.h" | cut -d'"' -f2)"
echo "[timing] compiling probe (new = hardened ladder, old = legacy double-and-add)"

CFLAGS="-w -O1 -g -std=c11 -D_GNU_SOURCE -D_POSIX_C_SOURCE=200809L -DWUBU_HAVE_MBEDTLS -DWUBU_NO_LIBM"
INCLUDES="-I$ROOT -I$ROOT/include -I$ROOT/src -I$ROOT/src/gui -I$ROOT/src/runtime -I$ROOT/src/kernel -I$MB/include"
# The probe #includes wubu_pkg_ed25519.c, so that file is NOT linked separately
# here (doing so would duplicate every Ed25519 symbol).
SRCS="$ROOT/src/runtime/wubu_crc32.c \
 $ROOT/src/gui/wubu_pkgmgr.c $ROOT/src/gui/wubu_pkgmgr_verify.c $ROOT/src/gui/wubu_pkg_sign.c \
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
 $ROOT/src/runtime/oci/oci_http_client.c $ROOT/src/kernel/wubu_math.c \
 $ROOT/tools/ed25519_timing_probe.c"

LINK="-L$MB/lib/x86_64-linux-gnu -lmbedtls -lmbedx509 -lmbedcrypto -Wl,-rpath,$MB/lib/x86_64-linux-gnu -lsqlite3 -lzstd -lm"
# shellcheck disable=SC2086
gcc $CFLAGS $INCLUDES -include "$ROOT/src/runtime/wubu_gnu_compat.h" $SRCS -o "$TMP/probe_new" $LINK
# shellcheck disable=SC2086
gcc $CFLAGS -DWUBU_ED25519_LEGACY_MUL $INCLUDES -include "$ROOT/src/runtime/wubu_gnu_compat.h" $SRCS -o "$TMP/probe_old" $LINK

export LD_LIBRARY_PATH="$MB/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

echo "[timing] --- NEGATIVE CONTROL: pre-change double-and-add (public ge_mul) ---"
"$TMP/probe_old" | tee "$TMP/old.log"
echo "[timing] --- NEW: hardened Montgomery ladder (public ge_mul) ---"
"$TMP/probe_new" | tee "$TMP/new.log"

fail=0
grep -q 'TIMING_GATE PASSED' "$TMP/old.log" || { echo "[timing] FAILED: legacy probe did not pass its own gate"; fail=1; }
grep -q 'TIMING_GATE PASSED' "$TMP/new.log" || { echo "[timing] FAILED: hardened probe gate failed"; fail=1; }
echo "[timing] negative control (legacy): $(grep '^old_ns_per_mul=' "$TMP/old.log")"
echo "[timing] hardened (new):            $(grep '^new_ns_per_mul=' "$TMP/new.log")"

if [ "$fail" -eq 0 ]; then
    echo "[timing] ALL CHECKS PASSED -- legacy leaks (r>0.80, |t|>2.074, ratio>1.50); hardened ladder removes the baseline leak (delta_r>=0.25, t_new<=5% of t_old, ratio<1.25)"
else
    echo "[timing] GATE FAILED"
fi
exit "$fail"
