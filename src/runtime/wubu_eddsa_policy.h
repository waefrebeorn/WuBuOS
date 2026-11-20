/*
 * wubu_eddsa_policy.h -- bignum-free RFC 8032 verification policy.
 *
 * Two checks must run BEFORE an Ed25519 signature is accepted, whatever
 * primitive supplies the group math (in-repo ladder or OpenSSL EVP):
 *
 *   1. S (the high half of the 64-byte signature, a 256-bit little-endian
 *      integer) must be strictly less than the subgroup order L.
 *   2. the compressed public key A must not lie in the cofactor-8 subgroup
 *      ([8]A != identity), and must be a canonical field encoding.
 *
 * This module implements both with fixed-width byte comparisons only -- no
 * bignum library, no heap, no third-party code.  It is therefore available
 * in a native (OpenSSL) build even when no mbedTLS dev root is installed.
 * The mbedTLS ladder keeps its own bignum-based checks; both produce the
 * same accept/reject decisions (see tools/eddsa_nombedtls_gate.sh).
 */
#ifndef WUBU_EDDSA_POLICY_H
#define WUBU_EDDSA_POLICY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* RFC 8032 subgroup order L = 2^252 + 27742317777372353535851937790883648493,
 * 32-byte little-endian.  Exposed for differential tooling. */
extern const uint8_t wubu_eddsa_L_le[32];

/* RFC 8032 field prime p = 2^255 - 19, 32-byte little-endian. */
extern const uint8_t wubu_eddsa_p_le[32];

/* True iff S = sig[32..64) interpreted as a 256-bit little-endian integer is
 * in [0, L).  S == L and S > L are both non-canonical. */
bool wubu_eddsa_s_is_canonical(const uint8_t sig[64]);

/* 0  = A is not a small-order point (accept the encoding);
 * 1  = A is an order-1/2/4/8 point (reject: small subgroup);
 * -1 = the encoding is not a canonical curve point (reject: invalid key).
 * Both 1 and -1 must be refused before the signature primitive is called. */
int wubu_eddsa_public_key_policy(const uint8_t pub[32]);

/* Combined gate used by the native backend.  Writes a human-readable reason
 * into err on failure and returns false; returns true with err cleared if
 * both policy checks pass. */
bool wubu_eddsa_policy_check(const uint8_t pub[32], const uint8_t sig[64],
                             char *err, size_t errsz);

#endif /* WUBU_EDDSA_POLICY_H */
