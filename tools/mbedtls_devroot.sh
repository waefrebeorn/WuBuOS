#!/usr/bin/env bash
# mbedtls_devroot.sh -- resolve the mbedTLS dev root for the opt-in crypto gates.
# Prints the absolute path on success; exits non-zero with setup instructions
# otherwise.  Resolution order:
#   1. $WUBU_MBEDTLS_ROOT (explicit override, still validated)
#   2. <repo>/.cache/mbedtls-devroot  -- created by `make mbedtls-devroot`
# This script deliberately knows nothing about ~/.hermes/cache/scratch.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
root=${WUBU_MBEDTLS_ROOT:-$ROOT/.cache/mbedtls-devroot}
if [ ! -f "$root/usr/include/mbedtls/version.h" ] || \
   [ ! -f "$root/usr/lib/x86_64-linux-gnu/libmbedcrypto.a" ]; then
    echo "mbedTLS dev root is missing or incomplete: $root" >&2
    echo "  Run: make mbedtls-devroot" >&2
    echo "  Or set WUBU_MBEDTLS_ROOT to an extracted mbedTLS dev root." >&2
    exit 2
fi
printf '%s\n' "$root"
