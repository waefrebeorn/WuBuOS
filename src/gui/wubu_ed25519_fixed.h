#ifndef WUBU_ED25519_FIXED_H
#define WUBU_ED25519_FIXED_H
/*
 * wubu_ed25519_fixed.h -- bignum-free, fixed-width Ed25519 scalar multiply.
 *
 * Self-contained C11: no mbedTLS (or any other) bignum type or call appears
 * in this module.  Field elements are four 64-bit little-endian limbs modulo
 * q = 2^255 - 19; the scalar ladder runs a compile-time-fixed 256 iterations
 * and uses mask-only conditional swaps.  This is the complete arithmetic
 * substrate of the default fixed-width package-signing path: scalar mod-L
 * arithmetic, point addition and point encode/decode are all done here with
 * fixed-width 64-bit limbs, never with a bignum library.  SHA-512 (in
 * wubu_ed25519_fixed_sign.c) is the only mbedTLS primitive still used by the
 * fixed path, and it is a hash, not a bignum.
 */
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define WUBU_ED25519_FIXED_LADDER_BITS 256u

/* [scalar] point, all 32-byte little-endian RFC 8032 encodings.
 * scalar: 32-byte little-endian scalar (bits above L are absorbed by group
 * order).  point: encoded Edwards point.  out: encoded result.
 * Returns 0 on success, -1 on an invalid/non-canonical point. */
int wubu_ed25519_fixed_scalarmult(uint8_t out[32],
                                  const uint8_t scalar[32],
                                  const uint8_t point[32]);

/* [scalar]B using the RFC 8032 base point. No point decoding required. */
int wubu_ed25519_fixed_base_scalarmult(uint8_t out[32],
                                       const uint8_t scalar[32]);

/* Point addition of two RFC 8032 encoded Edwards points.  Uses the complete
 * a = -1 addition formulas, so it also handles doubling and the identity.
 * Returns 0 on success, -1 if either input is not a canonical encoding. */
int wubu_ed25519_fixed_add(uint8_t out[32], const uint8_t a[32],
                           const uint8_t b[32]);

/* Fixed-width scalar arithmetic modulo the RFC 8032 group order L = 2^252 +
 * 27742317777372353535851937790883648493.  All reductions run a fixed number
 * of bit steps and use mask-only conditional subtraction.  `n` is the number
 * of input bytes (32 or 64); the result is a canonical 32-byte little-endian
 * scalar in [0, L). */
int wubu_ed25519_fixed_scalar_reduce(uint8_t out[32], const uint8_t in[64],
                                     size_t n);

/* S = (k * a + r) mod L.  All three operands are canonical mod-L scalars. */
int wubu_ed25519_fixed_scalar_mul_add(uint8_t out[32], const uint8_t k[32],
                                      const uint8_t a[32], const uint8_t r[32]);

/* 1 iff the 32-byte little-endian scalar is canonical (s < L). */
int wubu_ed25519_fixed_scalar_ltL(const uint8_t s[32]);

#ifdef __cplusplus
}
#endif
#endif /* WUBU_ED25519_FIXED_H */
