/*
 * wubu_pkg_sign.h -- single owner of .wubu package signing and verification.
 *
 * Canonical signed message:
 *     SHA-256( header with the 64-byte signature field zeroed
 *            || compressed manifest
 *            || compressed payload )
 * Ed25519 (RFC 8032) signs/verifies that 32-byte digest.  The mbedTLS build
 * supplies SHA-512 + bignum for the Ed25519 group arithmetic; the extracted
 * mbedTLS 2.28 dev package has no MBEDTLS_EDDSA_C module, so RFC 8032 is
 * implemented here rather than through mbedtls_pk (which cannot parse an
 * Ed25519 key in that build).  With WUBU_HAVE_MBEDTLS undefined the crypto
 * entry points refuse to operate: there is no fake "verified" path.
 *
 * Key encoding (no secret literals in the repo):
 *   private seed: 64 hex chars = 32-byte RFC 8032 seed
 *   public key:   64 hex chars = 32-byte raw Ed25519 public key
 */
#ifndef WUBU_PKG_SIGN_H
#define WUBU_PKG_SIGN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
    WUBU_PKG_SIGN_DIGEST_LEN = 32,
    WUBU_PKG_SIGNATURE_LEN   = 64
};

/* Stream the canonical SHA-256 digest from already-loaded parts.  The bytes
 * at [sig_offset, sig_offset+sig_len) of `header` are hashed as zero, so the
 * caller may pass the header before or after the signature was written. */
bool wubu_pkg_sign_digest(const uint8_t *header, size_t header_len,
                          size_t sig_offset, size_t sig_len,
                          const uint8_t *manifest, size_t manifest_len,
                          const char *payload_path,
                          uint8_t out_digest[WUBU_PKG_SIGN_DIGEST_LEN]);

/* Same digest straight from an assembled .wubu file. */
bool wubu_pkg_sign_digest_file(const char *pkg_path, size_t header_len,
                               size_t sig_offset, size_t sig_len,
                               size_t manifest_len,
                               uint8_t out_digest[WUBU_PKG_SIGN_DIGEST_LEN]);

/* Ed25519 sign/verify over the 32-byte digest.  err is human-readable on
 * failure.  Both fail (with a message) when mbedTLS is not compiled in. */
bool wubu_pkg_sign_ed25519(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                           const char *seed_hex,
                           uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN],
                           char *err, size_t errsz);
bool wubu_pkg_sign_verify_ed25519(const uint8_t digest[WUBU_PKG_SIGN_DIGEST_LEN],
                                  const char *pubkey_hex,
                                  const uint8_t sig[WUBU_PKG_SIGNATURE_LEN],
                                  char *err, size_t errsz);

/* Fail-closed high-level gate: read the package, reject absent signatures,
 * recompute the digest and verify Ed25519 with pubkey_hex. */
bool wubu_pkg_sign_verify_package(const char *pkg_path, size_t header_len,
                                  size_t sig_offset, size_t sig_len,
                                  size_t manifest_len, const char *pubkey_hex,
                                  char *err, size_t errsz);

/* Sign/verify an arbitrary command string (operator revocation CLI).  The
 * command bytes are SHA-256-hashed to the 32-byte Ed25519 message, so any
 * length is supported.  Domain separation is the caller's responsibility
 * (see wubu_pkgmgr_key_command_string).  Both fail closed without mbedTLS. */
bool wubu_pkg_sign_command(const char *command, const char *seed_hex,
                           uint8_t out_sig[WUBU_PKG_SIGNATURE_LEN],
                           char *err, size_t errsz);
bool wubu_pkg_verify_command(const char *command, const char *pubkey_hex,
                             const uint8_t sig[WUBU_PKG_SIGNATURE_LEN],
                             char *err, size_t errsz);

#endif /* WUBU_PKG_SIGN_H */
