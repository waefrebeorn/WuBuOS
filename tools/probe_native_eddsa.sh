#!/usr/bin/env bash
# probe_native_eddsa.sh -- record which native constant-time EdDSA primitives
# are actually usable by this build, before deciding to harden the in-repo
# RFC 8032 implementation.  This is a SURVEY, not a gate: it always exits 0.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
MB="${WUBU_MBEDTLS_ROOT:-}/usr"

echo "=== native EdDSA primitive survey (wubu_pkg_ed25519 timing leg) ==="

# 1) libsodium: needs BOTH the dev header and a linkable dev library.
if [ -f /usr/include/sodium.h ] && ls /usr/lib/*/libsodium.so /usr/lib/*/libsodium.a >/dev/null 2>&1; then
    echo "libsodium:        AVAILABLE (dev header + linkable lib)"
else
    echo "libsodium:        NOT USABLE (dev header present? $([ -f /usr/include/sodium.h ] && echo yes || echo no);"
    echo "                  runtime only: $(ls /usr/lib/*/libsodium.so.* 2>/dev/null | head -1 || echo none))"
fi

# 2) mbedTLS in the pinned dev root: 2.28 has no EdDSA/PSA EVP_pkey path.
if [ -f "$MB/include/mbedtls/version.h" ]; then
    ver=$(grep -m1 MBEDTLS_VERSION_STRING "$MB/include/mbedtls/version.h" | cut -d'"' -f2)
    eddsa=$(grep -m1 'define MBEDTLS_EDDSA_C' "$MB/include/mbedtls/config.h" 2>/dev/null || true)
    echo "mbedTLS:          $ver (MBEDTLS_EDDSA_C: ${eddsa:-absent})"
    if nm "$MB/lib/x86_64-linux-gnu/libmbedcrypto.a" 2>/dev/null | grep -qi 'mbedtls_eddsa'; then
        echo "                  EdDSA symbols present"
    else
        echo "                  no mbedtls_eddsa_* symbols: RFC 8032 must stay in-repo"
    fi
else
    echo "mbedTLS:          dev root not set (WUBU_MBEDTLS_ROOT)"
fi

# 3) OpenSSL 3 EVP_PKEY_ED25519: present on the HOST only.
if [ -f /usr/include/openssl/evp.h ] && openssl list -public-key-algorithms 2>/dev/null | grep -qi 'ED25519'; then
    echo "openssl EVP:      AVAILABLE on host ($(openssl version)) -- ED25519 in EVP_PKEY list"
    echo "                  NOTE: host-only libcrypto; cannot be linked by the in-OS"
    echo "                  package-signer, and replacing the verifier would drop the"
    echo "                  small-subgroup / non-canonical-S policy checks in"
    echo "                  wubu_pkg_sign_verify_ed25519()."
else
    echo "openssl EVP:      not available"
fi

echo "DECISION: no native primitive is adopt-able for the shipped in-OS signing"
echo "          path -> harden the in-repo scalar multiplication with a fixed-"
echo "          schedule ladder + constant-time conditional selection."
echo "=== end survey ==="
