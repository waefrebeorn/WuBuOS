#ifndef WUBU_ED25519_FIXED_H
#define WUBU_ED25519_FIXED_H
/*
 * wubu_ed25519_fixed.h -- bignum-free, fixed-width Ed25519 scalar multiply.
 *
 * Self-contained C11: no mbedTLS (or any other) bignum type or call appears
 * in this module.  Field elements are four 64-bit little-endian limbs modulo
 * q = 2^255 - 19; the scalar ladder runs a compile-time-fixed 256 iterations
 * and uses mask-only conditional swaps.  It is the opt-in fast path for the
 * scalar multiplication used by package signing; the mbedTLS MPI ladder in
 * wubu_pkg_ed25519.c remains the default fallback.
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

#ifdef __cplusplus
}
#endif
#endif /* WUBU_ED25519_FIXED_H */
