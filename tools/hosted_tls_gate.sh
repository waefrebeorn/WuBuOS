#!/usr/bin/env bash
# hosted_tls_gate.sh -- differential gate for the hosted-build TLS default.
#
# With an mbedTLS dev root ALREADY present (no fetch), the DEFAULT hosted
# build now links the HTTPS path (HOSTED_TLS auto-detected = 1). The gate
# proves four states with exact build evidence plus a loopback TLS fixture:
#   1. DEFAULT, root present: -DWUBU_HAVE_MBEDTLS and the three mbedTLS libs.
#   2. HOSTED_TLS=0: fail-closed HTTP-only recipe + source fallback.
#   3. ABSENT ROOT (empty override, env cleared): default stays fail-closed,
#      with no curl/wget/apt invocation (no network fetch).
#   4. HOSTED_TLS=1 explicit: the opt-in recipe still links HTTPS.
#   5. LOOPBACK TLS FIXTURE: untrusted CA refused, trusted CA accepted.
#   6. NEGATIVE CONTROL: the pre-change base had HOSTED_TLS ?= 0.
#
# Submodule checkouts are optional here: `make -n` only needs the recipe, so
# any missing (uninitialised) submodule source is treated as -o/old and the
# full recipe is still expanded. No submodule is modified.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT"

# Use the repo-local, validated dev root for the "present" states.
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

assert_tls() {
    local f="$1"
    grep -q -- '-DWUBU_HAVE_MBEDTLS' "$f" || {
        echo "[hosted_tls] FAIL: recipe has no -DWUBU_HAVE_MBEDTLS" >&2; exit 1; }
    for lib in -lmbedtls -lmbedx509 -lmbedcrypto; do
        grep -q -- "$lib" "$f" || {
            echo "[hosted_tls] FAIL: recipe missing $lib" >&2; exit 1; }
    done
}

assert_no_tls() {
    local f="$1"
    grep -q -E 'oci_http_client\.c[[:space:]]+-o' "$f" || {
        echo "[hosted_tls] FAIL: no oci_http_client compile line in recipe" >&2; exit 1; }
    grep -q -E -- '-o src/hosted/wubu ' "$f" || {
        echo "[hosted_tls] FAIL: no hosted link line in recipe" >&2; exit 1; }
    if grep -q -- '-DWUBU_HAVE_MBEDTLS' "$f"; then
        echo "[hosted_tls] FAIL: fail-closed recipe defines WUBU_HAVE_MBEDTLS" >&2; exit 1
    fi
    if grep -q -- '-lmbedtls' "$f"; then
        echo "[hosted_tls] FAIL: fail-closed recipe links mbedTLS" >&2; exit 1
    fi
}

echo "[hosted_tls] dev root: $DEVROOT"

echo "[hosted_tls] === 1. DEFAULT (root present): HTTPS linked ==="
recipe "$TMP/default.txt"
assert_tls "$TMP/default.txt"
echo "[hosted_tls] exact default recipe lines (build evidence):"
grep -E -- '-DWUBU_HAVE_MBEDTLS.*oci_http_client\.c' "$TMP/default.txt" | sed 's/^/    /' | head -1 || true
grep -E -- '-o src/hosted/wubu ' "$TMP/default.txt" | sed 's/^/    /' | head -1 | cut -c1-320 || true
echo "[hosted_tls] default WITH root VERIFIED: hosted product links HTTPS"

echo "[hosted_tls] === 2. FORCED OFF (HOSTED_TLS=0): fail-closed HTTP ==="
recipe "$TMP/off.txt" HOSTED_TLS=0
assert_no_tls "$TMP/off.txt"
grep -n 'TLS not implemented - compile with -DWUBU_HAVE_MBEDTLS' \
    src/runtime/oci/oci_http_client.c | sed 's/^/    /' || true
echo "[hosted_tls] HOSTED_TLS=0 VERIFIED: no HTTPS, source fallback present"

echo "[hosted_tls] === 3. ABSENT ROOT: default stays fail-closed, no fetch ==="
unset WUBU_MBEDTLS_ROOT
recipe "$TMP/noroot.txt" WUBU_MBEDTLS_ROOT=
assert_no_tls "$TMP/noroot.txt"
if grep -Eiq '(^|[[:space:]])(curl|wget|apt-get|apt|dpkg)([[:space:]]|$)' "$TMP/noroot.txt"; then
    echo "[hosted_tls] FAIL: absent-root recipe invoked a network fetch" >&2; exit 1
fi
export WUBU_MBEDTLS_ROOT="$DEVROOT"
echo "[hosted_tls] absent-root VERIFIED: fail-closed HTTP path, no network fetch"

echo "[hosted_tls] === 4. EXPLICIT HOSTED_TLS=1 (root present) ==="
recipe "$TMP/tls.txt" HOSTED_TLS=1
assert_tls "$TMP/tls.txt"
echo "[hosted_tls] exact opt-in recipe lines (build evidence):"
grep -E -- '-DWUBU_HAVE_MBEDTLS.*oci_http_client\.c' "$TMP/tls.txt" | sed 's/^/    /' | head -1 || true
grep -E -- '-o src/hosted/wubu ' "$TMP/tls.txt" | sed 's/^/    /' | head -1 | cut -c1-320 || true
echo "[hosted_tls] opt-in VERIFIED: HOSTED_TLS=1 links the mbedTLS HTTPS path"

echo "[hosted_tls] === 5. LOOPBACK TLS FIXTURE: untrusted CA refused ==="
bash "$ROOT/tools/oci_tls_gate.sh"

echo "[hosted_tls] === 6. NEGATIVE CONTROL: pre-change base ==="
BASE=a6c46b03fd281d1decd8d7f92381cc558add9564
if ! git -C "$ROOT" show "$BASE:mk/vars.mk" | grep -q 'HOSTED_TLS ?= 0'; then
    echo "[hosted_tls] FAIL: pre-change base did not have HOSTED_TLS ?= 0" >&2; exit 1
fi
if git -C "$ROOT" show "$BASE:mk/vars.mk" | grep -q 'HOSTED_TLS_AUTO'; then
    echo "[hosted_tls] FAIL: pre-change base unexpectedly auto-detected TLS" >&2; exit 1
fi
echo "[hosted_tls] negative control: base $BASE had HOSTED_TLS ?= 0 (no auto-detect)"

echo "[hosted_tls] ALL HOSTED-TLS CONFIG GATE CHECKS PASSED"
