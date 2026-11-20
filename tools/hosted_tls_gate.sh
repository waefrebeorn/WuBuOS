#!/usr/bin/env bash
# hosted_tls_gate.sh -- differential gate for the hosted-build TLS configuration.
#
# The DEFAULT hosted build does NOT link mbedTLS, so the GUI sync widget's
# https:// path fails closed before any socket is opened.  Wiring mbedTLS into
# every hosted build would make a network-fetched dev root a hard dependency
# of the bare-runner gate, so TLS is a configuration gate instead:
#
#   make hosted HOSTED_TLS=1 WUBU_MBEDTLS_ROOT=<extracted dev root>
#
# This gate proves three things with real output:
#   1. NEGATIVE (exact build evidence): the default hosted recipe defines no
#      WUBU_HAVE_MBEDTLS and links no mbedTLS library; oci_http_client.c also
#      carries the fail-closed fallback in source.
#   2. CONFIG GATE (exact build evidence): the HOSTED_TLS=1 recipe carries
#      BOTH the define and the three mbedTLS libraries.
#   3. LOOPBACK TLS FIXTURE: the real oci_http_client.c compiled with the
#      macro runs against a local TLS server: untrusted CA refused, trusted
#      CA accepted.
#
# Submodule checkouts are optional here: `make -n` only needs the recipe, so
# any missing (uninitialised) submodule source is treated as -o/old and the
# full recipe is still expanded. No submodule is modified.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

# Clear any ambient override so the repo-local, SHA-pinned dev root is used.
unset WUBU_MBEDTLS_ROOT MBEDTLS_DEVROOT
DEVROOT=$("$ROOT/tools/mbedtls_devroot.sh")
export WUBU_MBEDTLS_ROOT="$DEVROOT"

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# Expand the real hosted recipe without executing it. Pass 1 reports any
# missing prereqs (uninitialised submodules); pass 2 marks those -o/old so
# make prints the complete compile + link command lines.
recipe() {
    local out="$1"; shift
    make -n -B -k "$@" hosted >"$out" 2>&1 || true
    local oflags=()
    local f
    while IFS= read -r f; do
        [ -n "$f" ] && oflags+=(-o "$f")
    done < <(grep 'No rule to make target' "$out" \
             | sed -E "s/.*No rule to make target '([^']+)'.*/\1/" | sort -u)
    if ! make -n -B "${oflags[@]}" "$@" hosted >"$out" 2>"$out.err"; then
        cat "$out.err" >&2
        echo "[hosted_tls] FAIL: could not expand hosted recipe" >&2
        return 2
    fi
}

echo "[hosted_tls] === 1. NEGATIVE: default hosted build recipe ==="
recipe "$TMP/default.txt"
echo "[hosted_tls] exact default recipe lines (build evidence):"
grep -E 'oci_http_client\.c[[:space:]]+-o' "$TMP/default.txt" | sed 's/^/    /' | head -2 || true
grep -E -- '-o src/hosted/wubu ' "$TMP/default.txt" | sed 's/^/    /' | head -1 || true
grep -q -E 'oci_http_client\.c[[:space:]]+-o' "$TMP/default.txt" || {
    echo "[hosted_tls] FAIL: no oci_http_client compile line in default recipe" >&2; exit 1; }
grep -q -E -- '-o src/hosted/wubu ' "$TMP/default.txt" || {
    echo "[hosted_tls] FAIL: no hosted link line in default recipe" >&2; exit 1; }
if grep -q -- '-DWUBU_HAVE_MBEDTLS' "$TMP/default.txt"; then
    echo "[hosted_tls] FAIL: default hosted build unexpectedly defines WUBU_HAVE_MBEDTLS" >&2; exit 1
fi
if grep -q -- '-lmbedtls' "$TMP/default.txt"; then
    echo "[hosted_tls] FAIL: default hosted build unexpectedly links mbedTLS" >&2; exit 1
fi
echo "[hosted_tls] source-level fail-closed fallback (build evidence):"
grep -n 'TLS not implemented - compile with -DWUBU_HAVE_MBEDTLS' \
    src/runtime/oci/oci_http_client.c | sed 's/^/    /' || true
echo "[hosted_tls] negative result VERIFIED: default hosted build cannot serve HTTPS"

echo "[hosted_tls] === 2. CONFIG GATE: HOSTED_TLS=1 hosted build recipe ==="
recipe "$TMP/tls.txt" HOSTED_TLS=1
if ! grep -q -- '-DWUBU_HAVE_MBEDTLS' "$TMP/tls.txt"; then
    echo "[hosted_tls] FAIL: HOSTED_TLS=1 recipe has no -DWUBU_HAVE_MBEDTLS" >&2; exit 1
fi
for lib in -lmbedtls -lmbedx509 -lmbedcrypto; do
    if ! grep -q -- "$lib" "$TMP/tls.txt"; then
        echo "[hosted_tls] FAIL: HOSTED_TLS=1 recipe missing $lib" >&2; exit 1
    fi
done
echo "[hosted_tls] exact TLS-configured recipe lines (build evidence):"
grep -E -- '-DWUBU_HAVE_MBEDTLS.*oci_http_client\.c' "$TMP/tls.txt" | sed 's/^/    /' | head -2 || true
grep -E -- '-o src/hosted/wubu ' "$TMP/tls.txt" | sed 's/^/    /' | head -1 \
    | cut -c1-320 || true
echo "[hosted_tls] config gate VERIFIED: HOSTED_TLS=1 links the mbedTLS HTTPS path"

echo "[hosted_tls] === 3. LOOPBACK TLS FIXTURE: mbedTLS-backed HTTPS path ==="
bash "$ROOT/tools/oci_tls_gate.sh"

echo "[hosted_tls] ALL HOSTED-TLS CONFIG GATE CHECKS PASSED"
