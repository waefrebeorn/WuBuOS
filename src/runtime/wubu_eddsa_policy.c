/*
 * wubu_eddsa_policy.c -- bignum-free Ed25519 verification policy.
 *
 * No general-purpose integer arithmetic is used.  A full-width comparison of
 * two 32-byte little-endian values is a byte loop; the small-subgroup check is
 * a constant-time (over the table) comparison against the complete, finite
 * set of encodings that decode into the order-8 cofactor subgroup.  Ed25519's
 * small subgroup has exactly eight points (k = 8), so enumerating their
 * compressed encodings is complete -- it is not a heuristic blacklist.
 *
 * Derivation of the table (offline, verified independently of this file):
 *   for a curve point P, [L]P lies in the order-8 subgroup; recovering [L]P
 *   for many P yields exactly the eight points, whose canonical encodings
 *   have y in {1, p-1, 0, 26e8958f..53fc05, c7176a70..ac037a}.  Both sign
 *   bits are listed: flipping the sign of x maps a small-order point to its
 *   negation, which is also small-order (and OpenSSL's ref10 decoder accepts
 *   the non-canonical y=1 / y=p-1 sign bit too).  The y >= p encodings are
 *   refused separately by the canonical-field check below, matching the
 *   mbedTLS ladder's ge_frombytes() rejection.
 */
#include "wubu_eddsa_policy.h"

#include <string.h>

const uint8_t wubu_eddsa_L_le[32] = {
    0xed, 0xd3, 0xf5, 0x5c, 0x6e, 0x31, 0x26, 0x58,
    0x65, 0xcd, 0x79, 0xa2, 0xf7, 0x9d, 0xde, 0x14,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10
};

const uint8_t wubu_eddsa_p_le[32] = {
    0xed, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f
};

/* Complete set of 32-byte little-endian encodings whose Ed25519 decode lands
 * in the order-8 subgroup.  The first four are the x == 0 points (y = 1 and
 * y = p-1, each with both sign bits); mbedTLS treats sign=1 there as an
 * invalid encoding, so we classify those as -1 rather than 1. */
static const uint8_t small_order[10][32] = {
    {0x01,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x00},
    {0x01,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x80},
    {0xec,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
     0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
     0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
     0xff,0xff,0xff,0xff,0xff,0xff,0xff,0x7f},
    {0xec,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
     0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
     0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
     0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
     0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x80},
    {0x26,0xe8,0x95,0x8f,0xc2,0xb2,0x27,0xb0,
     0x45,0xc3,0xf4,0x89,0xf2,0xef,0x98,0xf0,
     0xd5,0xdf,0xac,0x05,0xd3,0xc6,0x33,0x39,
     0xb1,0x38,0x02,0x88,0x6d,0x53,0xfc,0x05},
    {0x26,0xe8,0x95,0x8f,0xc2,0xb2,0x27,0xb0,
     0x45,0xc3,0xf4,0x89,0xf2,0xef,0x98,0xf0,
     0xd5,0xdf,0xac,0x05,0xd3,0xc6,0x33,0x39,
     0xb1,0x38,0x02,0x88,0x6d,0x53,0xfc,0x85},
    {0xc7,0x17,0x6a,0x70,0x3d,0x4d,0xd8,0x4f,
     0xba,0x3c,0x0b,0x76,0x0d,0x10,0x67,0x0f,
     0x2a,0x20,0x53,0xfa,0x2c,0x39,0xcc,0xc6,
     0x4e,0xc7,0xfd,0x77,0x92,0xac,0x03,0x7a},
    {0xc7,0x17,0x6a,0x70,0x3d,0x4d,0xd8,0x4f,
     0xba,0x3c,0x0b,0x76,0x0d,0x10,0x67,0x0f,
     0x2a,0x20,0x53,0xfa,0x2c,0x39,0xcc,0xc6,
     0x4e,0xc7,0xfd,0x77,0x92,0xac,0x03,0xfa}
};

static void policy_err(char *err, size_t errsz, const char *msg) {
    if (!err || !errsz) return;
    size_t n = strlen(msg);
    if (n >= errsz) n = errsz - 1;
    memcpy(err, msg, n);
    err[n] = '\0';
}

bool wubu_eddsa_s_is_canonical(const uint8_t sig[64]) {
    if (!sig) return false;
    /* Big endian of the little-endian value: compare most significant byte
     * first.  No borrow/wider type is needed for a 256-bit compare. */
    for (int i = 31; i >= 0; --i) {
        uint8_t a = sig[32 + (size_t)i];
        uint8_t b = wubu_eddsa_L_le[i];
        if (a < b) return true;
        if (a > b) return false;
    }
    return false;  /* S == L is outside [0, L) */
}

/* y is the 32-byte little-endian field element with the sign bit cleared. */
static bool y_not_canonical(const uint8_t y[32]) {
    for (int i = 31; i >= 0; --i) {
        if (y[i] > wubu_eddsa_p_le[i]) return true;
        if (y[i] < wubu_eddsa_p_le[i]) return false;
    }
    return true;  /* y == p is not a canonical field element */
}

int wubu_eddsa_public_key_policy(const uint8_t pub[32]) {
    if (!pub) return -1;
    uint8_t y[32];
    memcpy(y, pub, sizeof(y));
    unsigned sign = y[31] >> 7;
    y[31] &= 0x7f;
    if (y_not_canonical(y)) return -1;  /* matches ladder ge_frombytes() */

    int result = 0;
    for (size_t i = 0; i < sizeof(small_order) / sizeof(small_order[0]); ++i) {
        uint8_t diff = 0;
        for (size_t j = 0; j < 32; ++j) diff |= (uint8_t)(pub[j] ^ small_order[i][j]);
        if (diff != 0) continue;
        /* y=1 and y=p-1 have x == 0 and both are non-canonical with sign=1;
         * the ladder refuses those as an invalid point before the cofactor
         * check, so report the same class here. */
        result = (sign && i < 4) ? -1 : 1;
        break;
    }
    return result;
}

bool wubu_eddsa_policy_check(const uint8_t pub[32], const uint8_t sig[64],
                             char *err, size_t errsz) {
    if (err) policy_err(err, errsz, "");
    if (!sig) {
        policy_err(err, errsz, "missing Ed25519 signature");
        return false;
    }
    if (!wubu_eddsa_s_is_canonical(sig)) {
        policy_err(err, errsz, "non-canonical Ed25519 S");
        return false;
    }
    int pk = wubu_eddsa_public_key_policy(pub);
    if (pk == 1) {
        policy_err(err, errsz, "small-subgroup public key rejected");
        return false;
    }
    if (pk < 0) {
        policy_err(err, errsz, "invalid Ed25519 public key encoding");
        return false;
    }
    return true;
}
