#ifndef WUBU_ED25519_FIXED_SIGN_H
#define WUBU_ED25519_FIXED_SIGN_H
/*
 * wubu_ed25519_fixed_sign.h -- RFC 8032 sign/verify on the bignum-free path.
 *
 * This is the orchestration layer above wubu_ed25519_fixed.c: SHA-512 (the
 * only mbedTLS primitive still used here -- a hash, not a bignum), fixed-width
 * scalar mod-L arithmetic, and encoded-point addition/scalar multiplication.
 * No mbedTLS integer type or call appears in this module.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 0 on success, -1 on invalid input or an invalid point. */
int wubu_ed25519_fixed_sign(const uint8_t digest[32], const uint8_t seed[32],
                            uint8_t out_sig[64]);

/* 0 on a valid signature, -1 on invalid/canonicality or small-subgroup
 * rejection, or a malformed point.  Policy (non-canonical S and small-order
 * A) is enforced here, never delegated. */
int wubu_ed25519_fixed_verify(const uint8_t digest[32], const uint8_t pub[32],
                              const uint8_t sig[64]);

#ifdef __cplusplus
}
#endif
#endif /* WUBU_ED25519_FIXED_SIGN_H */
