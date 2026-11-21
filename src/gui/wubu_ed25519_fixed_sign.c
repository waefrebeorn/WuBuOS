/*
 * wubu_ed25519_fixed_sign.c -- RFC 8032 sign/verify with zero bignum calls.
 *
 * The default package-signing gate selects WUBU_EDDSA_FIXED_LADDER and this
 * module.  Group arithmetic and scalar mod-L arithmetic live entirely in
 * wubu_ed25519_fixed.c as fixed-width 64-bit-limb code; the only mbedTLS
 * primitive used here is SHA-512 (a hash, not a bignum).  No mbedtls_mpi
 * symbol, no bignum header and no heap are used on this path.
 *
 * Policy is enforced locally before the group equation is accepted:
 *   - S must be canonical (S < L);
 *   - [8]A must not be the identity (small-subgroup rejection).
 */
#include "wubu_ed25519_fixed_sign.h"
#include "wubu_ed25519_fixed.h"

#include <mbedtls/sha512.h>

#include <string.h>

/* SHA-512 over a single buffer, and over the two/three concatenations used
 * by RFC 8032 (r = H(prefix || M), k = H(R || A || M)). */
static void sha512_one(const uint8_t *m, size_t n, uint8_t out[64]) {
    mbedtls_sha512_context ctx;
    mbedtls_sha512_init(&ctx);
    mbedtls_sha512_starts_ret(&ctx, 0);
    mbedtls_sha512_update_ret(&ctx, m, n);
    mbedtls_sha512_finish_ret(&ctx, out);
    mbedtls_sha512_free(&ctx);
}

static void sha512_two(const uint8_t *a, size_t an, const uint8_t *b, size_t bn,
                       uint8_t out[64]) {
    mbedtls_sha512_context ctx;
    mbedtls_sha512_init(&ctx);
    mbedtls_sha512_starts_ret(&ctx, 0);
    mbedtls_sha512_update_ret(&ctx, a, an);
    mbedtls_sha512_update_ret(&ctx, b, bn);
    mbedtls_sha512_finish_ret(&ctx, out);
    mbedtls_sha512_free(&ctx);
}

static void sha512_challenge(const uint8_t r[32], const uint8_t a[32],
                             const uint8_t m[32], uint8_t out[64]) {
    mbedtls_sha512_context ctx;
    mbedtls_sha512_init(&ctx);
    mbedtls_sha512_starts_ret(&ctx, 0);
    mbedtls_sha512_update_ret(&ctx, r, 32);
    mbedtls_sha512_update_ret(&ctx, a, 32);
    mbedtls_sha512_update_ret(&ctx, m, 32);
    mbedtls_sha512_finish_ret(&ctx, out);
    mbedtls_sha512_free(&ctx);
}

int wubu_ed25519_fixed_sign(const uint8_t digest[32], const uint8_t seed[32],
                            uint8_t out_sig[64]) {
    if (!digest || !seed || !out_sig) return -1;

    uint8_t h[64], rh[64], kh[64];
    uint8_t avec[32], rbuf[32], kbuf[32];
    uint8_t renc[32], aenc[32], sbuf[32];

    sha512_one(seed, 32, h);
    h[0] &= 248;
    h[31] &= 63;
    h[31] |= 64;
    if (wubu_ed25519_fixed_scalar_reduce(avec, h, 32) != 0) return -1;

    sha512_two(h + 32, 32, digest, 32, rh);
    if (wubu_ed25519_fixed_scalar_reduce(rbuf, rh, 64) != 0) return -1;

    if (wubu_ed25519_fixed_base_scalarmult(renc, rbuf) != 0) return -1;
    if (wubu_ed25519_fixed_base_scalarmult(aenc, avec) != 0) return -1;

    sha512_challenge(renc, aenc, digest, kh);
    if (wubu_ed25519_fixed_scalar_reduce(kbuf, kh, 64) != 0) return -1;
    if (wubu_ed25519_fixed_scalar_mul_add(sbuf, kbuf, avec, rbuf) != 0) return -1;

    memcpy(out_sig, renc, 32);
    memcpy(out_sig + 32, sbuf, 32);
    return 0;
}

/* The RFC 8032 identity encodes as y=1, x=0, sign bit 0. */
static const uint8_t IDENTITY_ENC[32] = {
    1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int wubu_ed25519_fixed_verify(const uint8_t digest[32], const uint8_t pub[32],
                              const uint8_t sig[64]) {
    if (!digest || !pub || !sig) return -1;

    /* Canonical S: a non-reduced S must never be accepted, even though the
     * group equation would still hold mod L. */
    if (!wubu_ed25519_fixed_scalar_ltL(sig + 32)) return -1;

    uint8_t eight[32];
    memset(eight, 0, sizeof(eight));
    eight[0] = 8;

    /* [8]A decodes A (rejecting non-canonical encodings) and rejects the
     * small-subgroup, including A = identity. */
    uint8_t eightA[32];
    if (wubu_ed25519_fixed_scalarmult(eightA, eight, pub) != 0) return -1;
    if (memcmp(eightA, IDENTITY_ENC, 32) == 0) return -1;

    uint8_t kh[64], kbuf[32], ka[32], rhs[32], lhs[32];
    sha512_challenge(sig, pub, digest, kh);
    if (wubu_ed25519_fixed_scalar_reduce(kbuf, kh, 64) != 0) return -1;

    if (wubu_ed25519_fixed_scalarmult(ka, kbuf, pub) != 0) return -1;
    if (wubu_ed25519_fixed_add(rhs, sig, ka) != 0) return -1;
    if (wubu_ed25519_fixed_base_scalarmult(lhs, sig + 32) != 0) return -1;

    return memcmp(lhs, rhs, 32) == 0 ? 0 : -1;
}
