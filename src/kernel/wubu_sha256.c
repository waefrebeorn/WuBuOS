/*
 * wubu_sha256.c -- SHA-256 (FIPS 180-4), freestanding C11.
 *
 * Self-contained: no malloc, no hosted APIs, no third party. The standard
 * 64-round Merkle-Damgard construction with the FIPS constants. Used for
 * the runtime PCR extension (attestation chain), and available to the
 * verifier / AGI as the kernel's own hash.
 */
#include "wubu_sha256.h"

#include <string.h>

struct wubu_sha256_ctx {
    uint32_t state[8];
    uint64_t bitlen;        /* total bits hashed so far */
    uint8_t  buf[64];
    size_t   buflen;
};

_Static_assert(sizeof(wubu_sha256_ctx) == WUBU_SHA256_CTX_SZ,
               "wubu_sha256_ctx size drift vs WUBU_SHA256_CTX_SZ");

/* FIPS 180-4 section 4.2.2 */
static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

#define ROTR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static inline uint32_t rotr32(uint32_t x, unsigned n) { return ROTR(x, n); }
static inline uint32_t Ch(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (~x & z);
}
static inline uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}
static inline uint32_t S0(uint32_t x) {
    return rotr32(x, 2) ^ rotr32(x, 13) ^ rotr32(x, 22);
}
static inline uint32_t S1(uint32_t x) {
    return rotr32(x, 6) ^ rotr32(x, 11) ^ rotr32(x, 25);
}
static inline uint32_t s0(uint32_t x) {
    return rotr32(x, 7) ^ rotr32(x, 18) ^ (x >> 3);
}
static inline uint32_t s1(uint32_t x) {
    return rotr32(x, 17) ^ rotr32(x, 19) ^ (x >> 10);
}

static inline uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8)  |  (uint32_t)p[3];
}

static void sha256_compress(wubu_sha256_ctx *c, const uint8_t block[64])
{
    uint32_t w[64];
    uint32_t a, b, cc, d, e, f, g, h;

    for (int i = 0; i < 16; i++)
        w[i] = be32(block + 4 * i);
    for (int i = 16; i < 64; i++)
        w[i] = s1(w[i - 2]) + w[i - 7] + s0(w[i - 15]) + w[i - 16];

    a = c->state[0]; b = c->state[1]; cc = c->state[2]; d = c->state[3];
    e = c->state[4]; f = c->state[5]; g = c->state[6]; h = c->state[7];

    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + S1(e) + Ch(e, f, g) + K[i] + w[i];
        uint32_t t2 = S0(a) + Maj(a, b, cc);
        h = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }

    c->state[0] += a; c->state[1] += b; c->state[2] += cc; c->state[3] += d;
    c->state[4] += e; c->state[5] += f; c->state[6] += g; c->state[7] += h;
}

void wubu_sha256_init(wubu_sha256_ctx *c)
{
    if (!c) return;
    /* FIPS 180-4 section 5.3.3 */
    c->state[0] = 0x6a09e667; c->state[1] = 0xbb67ae85;
    c->state[2] = 0x3c6ef372; c->state[3] = 0xa54ff53a;
    c->state[4] = 0x510e527f; c->state[5] = 0x9b05688c;
    c->state[6] = 0x1f83d9ab; c->state[7] = 0x5be0cd19;
    c->bitlen = 0;
    c->buflen = 0;
}

void wubu_sha256_update(wubu_sha256_ctx *c, const void *data, size_t len)
{
    if (!c || !data) return;
    const uint8_t *p = (const uint8_t *)data;
    c->bitlen += (uint64_t)len * 8;

    if (c->buflen) {
        size_t need = 64 - c->buflen;
        if (len < need) {
            memcpy(c->buf + c->buflen, p, len);
            c->buflen += len;
            return;
        }
        memcpy(c->buf + c->buflen, p, need);
        sha256_compress(c, c->buf);
        p += need;
        len -= need;
        c->buflen = 0;
    }
    while (len >= 64) {
        sha256_compress(c, p);
        p += 64;
        len -= 64;
    }
    if (len) {
        memcpy(c->buf, p, len);
        c->buflen = len;
    }
}

void wubu_sha256_final(wubu_sha256_ctx *c, uint8_t out[WUBU_SHA256_SZ])
{
    if (!c) return;
    /* FIPS 180-4 section 5.1.1: append 0x80, zeros, then the 64-bit
     * big-endian bit length. */
    uint64_t bitlen = c->bitlen;
    uint8_t  pad = 0x80;
    wubu_sha256_update(c, &pad, 1);

    uint8_t zeros[64];
    memset(zeros, 0, sizeof(zeros));
    size_t rem = 64 - c->buflen;
    if (rem < 8) rem += 64;      /* not enough room for the length: pad */
    if (rem > 8) wubu_sha256_update(c, zeros, rem - 8);

    uint8_t lenb[8];
    for (int i = 0; i < 8; i++)
        lenb[i] = (uint8_t)(bitlen >> (56 - 8 * i));
    wubu_sha256_update(c, lenb, 8);

    if (out) {
        for (int i = 0; i < 8; i++) {
            out[4 * i + 0] = (uint8_t)(c->state[i] >> 24);
            out[4 * i + 1] = (uint8_t)(c->state[i] >> 16);
            out[4 * i + 2] = (uint8_t)(c->state[i] >> 8);
            out[4 * i + 3] = (uint8_t)(c->state[i]);
        }
    }
}

int wubu_sha256(const void *data, size_t len, uint8_t out[WUBU_SHA256_SZ])
{
    wubu_sha256_ctx ctx;
    wubu_sha256_init(&ctx);
    wubu_sha256_update(&ctx, data, len);
    wubu_sha256_final(&ctx, out);
    return 0;
}
