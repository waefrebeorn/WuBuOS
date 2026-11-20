#!/usr/bin/env bash
# oci_tls_gate.sh — opt-in TLS verification gate.
# WUBU_MBEDTLS_ROOT must point at an extracted mbedTLS dev package
# (headers under usr/include, libs under usr/lib/x86_64-linux-gnu).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
: "${WUBU_MBEDTLS_ROOT:?set WUBU_MBEDTLS_ROOT to an extracted mbedTLS dev root}"
MB="$WUBU_MBEDTLS_ROOT/usr"
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

openssl req -x509 -newkey rsa:2048 -nodes -days 2 \
    -subj '/CN=localhost' -addext 'subjectAltName=DNS:localhost' \
    -keyout "$TMP/key.pem" -out "$TMP/cert.pem" >/dev/null 2>&1

gcc -O1 -g -D_GNU_SOURCE -DWUBU_HAVE_MBEDTLS \
    -I"$ROOT/src" -I"$ROOT/src/runtime" -I"$ROOT/include" -I"$MB/include" \
    -include "$ROOT/src/runtime/wubu_gnu_compat.h" \
    "$ROOT/src/runtime/oci/oci_tls_test.c" \
    "$ROOT/src/runtime/oci/oci_http_client.c" \
    -o "$TMP/probe" \
    -L"$MB/lib/x86_64-linux-gnu" -lmbedtls -lmbedx509 -lmbedcrypto \
    -Wl,-rpath,"$MB/lib/x86_64-linux-gnu" -lm

LD_LIBRARY_PATH="$MB/lib/x86_64-linux-gnu${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
    "$TMP/probe" "$TMP/cert.pem" "$TMP/key.pem"
