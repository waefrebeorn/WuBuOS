#!/usr/bin/env bash
# fetch_mbedtls_devroot.sh -- reproducible, repo-local mbedTLS dev root.
#
# Downloads the pinned Ubuntu noble amd64 mbedTLS 2.28.8-1 .deb packages,
# verifies each against a SHA-256 recorded in this script, and extracts them
# into a cache directory INSIDE the repository.  Opt-in crypto gates then
# stop depending on ~/.hermes/cache/scratch or on any developer's shell env.
#
# Usage:
#   tools/fetch_mbedtls_devroot.sh [DEST]
#   make mbedtls-devroot
#
# The default DEST is <repo>/.cache/mbedtls-devroot (gitignored).  Already
# complete roots are reused without network access (idempotent/reproducible).
# Overrides: MBEDTLS_DEVROOT (destination), MBEDTLS_DEB_BASE (space-separated
# mirror URLs), MBEDTLS_DEB_VERSION, MBEDTLS_DEB_ARCH.  No secrets are used.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
DEST=${1:-${MBEDTLS_DEVROOT:-$ROOT/.cache/mbedtls-devroot}}
BASE=${MBEDTLS_DEB_BASE:-https://archive.ubuntu.com/ubuntu}
VER=${MBEDTLS_DEB_VERSION:-2.28.8-1}
ARCH=${MBEDTLS_DEB_ARCH:-amd64}
POOL=pool/universe/m/mbedtls

# name sha256 -- pinned from the Ubuntu noble package index (2.28.8-1).
DEBS="
libmbedtls-dev_${VER}_${ARCH}.deb 9b73a681ed56f8572fcc951325bc1e9107c1fe7f3ee224f44270b9191af16e52
libmbedcrypto7t64_${VER}_${ARCH}.deb 1f0b795a2274f68edddaf2567d738fcee43383c2c4ba802d54ba3b710bbb3228
libmbedtls14t64_${VER}_${ARCH}.deb 64a732580a3b58ddc574a819505cf0e2de150da3d2fbf37b7e60e305de7b50bd
libmbedx509-1t64_${VER}_${ARCH}.deb 12362c7ac8693dadbb9e0aa1e0a5a5eaba1d5d47034c1624725e82c2a486813e
"

need() { command -v "$1" >/dev/null || { echo "fetch_mbedtls_devroot: missing tool $1" >&2; exit 2; }; }
need curl; need dpkg-deb; need sha256sum

# A complete root needs the dev headers, the static libs and the shared libs.
complete() {
    [ -f "$DEST/usr/include/mbedtls/version.h" ] &&
    [ -f "$DEST/usr/include/mbedtls/sha512.h" ] &&
    [ -f "$DEST/usr/include/mbedtls/bignum.h" ] &&
    [ -f "$DEST/usr/lib/x86_64-linux-gnu/libmbedcrypto.a" ] &&
    [ -f "$DEST/usr/lib/x86_64-linux-gnu/libmbedtls.a" ] &&
    [ -f "$DEST/usr/lib/x86_64-linux-gnu/libmbedx509.a" ] &&
    [ -f "$DEST/usr/lib/x86_64-linux-gnu/libmbedcrypto.so" ] &&
    grep -q "MBEDTLS_VERSION_STRING_FULL.*Mbed TLS 2.28" \
        "$DEST/usr/include/mbedtls/version.h" 2>/dev/null
}

if complete; then
    echo "mbedTLS dev root already present: $DEST"
    exit 0
fi

mkdir -p "$ROOT/.cache"
TMP=$(mktemp -d "$ROOT/.cache/.mbedtls-fetch.XXXXXX")
trap 'rm -rf "$TMP"' EXIT

echo "fetch_mbedtls_devroot: fetching mbedTLS $VER ($ARCH) into $DEST"
while read -r name sha; do
    [ -n "${name:-}" ] || continue
    got=""
    for base in $BASE; do
        base=${base%/}
        if curl -fsSL --retry 3 --connect-timeout 15 --max-time 180 \
                -o "$TMP/$name" "$base/$POOL/$name"; then
            got=$base
            break
        fi
    done
    [ -n "$got" ] || { echo "fetch_mbedtls_devroot: could not download $name from: $BASE" >&2; exit 1; }
    printf '%s  %s\n' "$sha" "$TMP/$name" | sha256sum -c - >/dev/null || {
        echo "fetch_mbedtls_devroot: SHA-256 mismatch for $name (from $got)" >&2
        exit 1
    }
    echo "  verified $name"
done <<< "$DEBS"

mkdir -p "$DEST"
while read -r name sha; do
    [ -n "${name:-}" ] || continue
    dpkg-deb -x "$TMP/$name" "$DEST"
done <<< "$DEBS"

complete || { echo "fetch_mbedtls_devroot: extraction incomplete at $DEST" >&2; exit 1; }
printf 'mbedTLS %s (%s) fetched from %s, SHA-256 verified, extracted here.\n' \
    "$VER" "$ARCH" "$BASE" > "$DEST/SOURCE"
echo "mbedTLS dev root ready: $DEST"
