/*
 * wubu_crypto_sha512.c -- SHA-512 / SHA-384 (FIPS 180-4), freestanding C11.
 *
 * GOAL (GOALS-1000 #778). Mirrors the structure of the proven wubu_sha256.c
 * (shipped green via test_wubu_audit): same Merkle-Damgard shape, same
 * Ch/Maj/big-sigma/small-sigma formulas, same unsigned-wraparound arithmetic,
 * only widened to 64-bit words, 128-byte blocks, 80 rounds.
 *
 * Constants are the FIPS 180-4 §4.2.3 / §5.3.5 values. They were derived as the
 * exact floor(frac(sqrt(prime)) * 2^64) and floor(frac(cbrt(prime)) * 2^64) via
 * math.isqrt (integer square root) and an integer Newton icbrt -- both exact --
 * and the derivation was cross-checked against the NIST FIPS 180-4 PDF
 * (H0=6a09e667f3bcc908, H1=bb67ae8584caa73b confirmed verbatim) before
 * hardcoding, so there is no hand-transcription drift on the IV table.
 */
#include "wubu_crypto_sha512.h"

#include <string.h>

struct wubu_sha512_ctx {
    uint64_t state[8];
    uint64_t bitlen;        /* total bits hashed so far (low 64 of 128-bit len) */
    uint8_t  buf[128];
    size_t   buflen;
};

_Static_assert(sizeof(struct wubu_sha512_ctx) == WUBU_SHA512_CTX_SZ,
               "wubu_sha512_ctx size drift vs WUBU_SHA512_CTX_SZ");

/* FIPS 180-4 §4.2.3: first 64 bits of the fractional parts of the cube roots
 * of the first 80 primes. (Proven-exact integer derivation; cross-checked
 * against the NIST PDF K[0]=428a2f98d728ae22, K[79]=6c44198c4a475817.) */
static const uint64_t K[80] = {
    0x428a2f98d728ae22, 0x7137449123ef65cd, 0xb5c0fbcfec4d3b2f, 0xe9b5dba58189dbbc,
    0x3956c25bf348b538, 0x59f111f1b605d019, 0x923f82a4af194f9b, 0xab1c5ed5da6d8118,
    0xd807aa98a3030242, 0x12835b0145706fbe, 0x243185be4ee4b28c, 0x550c7dc3d5ffb4e2,
    0x72be5d74f27b896f, 0x80deb1fe3b1696b1, 0x9bdc06a725c71235, 0xc19bf174cf692694,
    0xe49b69c19ef14ad2, 0xefbe4786384f25e3, 0x0fc19dc68b8cd5b5, 0x240ca1cc77ac9c65,
    0x2de92c6f592b0275, 0x4a7484aa6ea6e483, 0x5cb0a9dcbd41fbd4, 0x76f988da831153b5,
    0x983e5152ee66dfab, 0xa831c66d2db43210, 0xb00327c898fb213f, 0xbf597fc7beef0ee4,
    0xc6e00bf33da88fc2, 0xd5a79147930aa725, 0x06ca6351e003826f, 0x142929670a0e6e70,
    0x27b70a8546d22ffc, 0x2e1b21385c26c926, 0x4d2c6dfc5ac42aed, 0x53380d139d95b3df,
    0x650a73548baf63de, 0x766a0abb3c77b2a8, 0x81c2c92e47edaee6, 0x92722c851482353b,
    0xa2bfe8a14cf10364, 0xa81a664bbc423001, 0xc24b8b70d0f89791, 0xc76c51a30654be30,
    0xd192e819d6ef5218, 0xd69906245565a910, 0xf40e35855771202a, 0x106aa07032bbd1b8,
    0x19a4c116b8d2d0c8, 0x1e376c085141ab53, 0x2748774cdf8eeb99, 0x34b0bcb5e19b48a8,
    0x391c0cb3c5c95a63, 0x4ed8aa4ae3418acb, 0x5b9cca4f7763e373, 0x682e6ff3d6b2b8a3,
    0x748f82ee5defb2fc, 0x78a5636f43172f60, 0x84c87814a1f0ab72, 0x8cc702081a6439ec,
    0x90befffa23631e28, 0xa4506cebde82bde9, 0xbef9a3f7b2c67915, 0xc67178f2e372532b,
    0xca273eceea26619c, 0xd186b8c721c0c207, 0xeada7dd6cde0eb1e, 0xf57d4f7fee6ed178,
    0x06f067aa72176fba, 0x0a637dc5a2c898a6, 0x113f9804bef90dae, 0x1b710b35131c471b,
    0x28db77f523047d84, 0x32caab7b40c72493, 0x3c9ebe0a15c9bebc, 0x431d67c49c100d4c,
    0x4cc5d4becb3e42b6, 0x597f299cfc657e2a, 0x5fcb6fab3ad6faec, 0x6c44198c4a475817
};

#define ROTR64(x, n) (((x) >> (n)) | ((x) << (64 - (n))))
static inline uint64_t rotr64(uint64_t x, unsigned n) { return ROTR64(x, n); }

static inline uint64_t Ch64(uint64_t x, uint64_t y, uint64_t z) {
    return (x & y) ^ (~x & z);
}
static inline uint64_t Maj64(uint64_t x, uint64_t y, uint64_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}
/* FIPS 180-4 §4.1.3: SHA-512 round functions (verified vs the NIST PDF). */
static inline uint64_t S0_512(uint64_t x) { return rotr64(x, 28) ^ rotr64(x, 34) ^ rotr64(x, 39); }
static inline uint64_t S1_512(uint64_t x) { return rotr64(x, 14) ^ rotr64(x, 18) ^ rotr64(x, 41); }
static inline uint64_t s0_512(uint64_t x) { return rotr64(x, 1)  ^ rotr64(x, 8)  ^ (x >> 7);  }
static inline uint64_t s1_512(uint64_t x) { return rotr64(x, 19) ^ rotr64(x, 61) ^ (x >> 6); }

static inline uint64_t be64(const uint8_t *p) {
    return ((uint64_t)p[0] << 56) | ((uint64_t)p[1] << 48) |
           ((uint64_t)p[2] << 40) | ((uint64_t)p[3] << 32) |
           ((uint64_t)p[4] << 24) | ((uint64_t)p[5] << 16) |
           ((uint64_t)p[6] << 8)  |  (uint64_t)p[7];
}

static void
sha512_compress(struct wubu_sha512_ctx *c, const uint8_t block[128])
{
    uint64_t w[80];
    uint64_t a, b, cc, d, e, f, g, hh;

    for (int i = 0; i < 16; i++)
        w[i] = be64(block + 8 * i);
    for (int i = 16; i < 80; i++)
        w[i] = s1_512(w[i - 2]) + w[i - 7] + s0_512(w[i - 15]) + w[i - 16];

    a = c->state[0]; b = c->state[1]; cc = c->state[2]; d = c->state[3];
    e = c->state[4]; f = c->state[5]; g = c->state[6]; hh = c->state[7];

    for (int i = 0; i < 80; i++) {
        uint64_t t1 = hh + S1_512(e) + Ch64(e, f, g) + K[i] + w[i];
        uint64_t t2 = S0_512(a) + Maj64(a, b, cc);
        hh = g; g = f; f = e; e = d + t1;
        d = cc; cc = b; b = a; a = t1 + t2;
    }

    c->state[0] += a; c->state[1] += b; c->state[2] += cc; c->state[3] += d;
    c->state[4] += e; c->state[5] += f; c->state[6] += g; c->state[7] += hh;
}

void
wubu_sha512_init(struct wubu_sha512_ctx *c)
{
    if (!c) return;
    /* FIPS 180-4 §5.3.5: initial hash value H(0). */
    static const uint64_t H0[8] = {
        0x6a09e667f3bcc908, 0xbb67ae8584caa73b, 0x3c6ef372fe94f82b, 0xa54ff53a5f1d36f1,
        0x510e527fade682d1, 0x9b05688c2b3e6c1f, 0x1f83d9abfb41bd6b, 0x5be0cd19137e2179
    };
    for (int i = 0; i < 8; i++) c->state[i] = H0[i];
    c->bitlen = 0;
    c->buflen = 0;
}

void
wubu_sha384_init(struct wubu_sha512_ctx *c)
{
    if (!c) return;
    /* FIPS 180-4 §5.3.4: SHA-384 initial hash value H(0). */
    static const uint64_t H0[8] = {
        0xcbbb9d5dc1059ed8, 0x629a292a367cd507, 0x9159015a3070dd17, 0x152fecd8f70e5939,
        0x67332667ffc00b31, 0x8eb44a8768581511, 0xdb0c2e0d64f98fa7, 0x47b5481dbefa4fa4
    };
    for (int i = 0; i < 8; i++) c->state[i] = H0[i];
    c->bitlen = 0;
    c->buflen = 0;
}

void
wubu_sha512_update(struct wubu_sha512_ctx *c, const void *data, size_t len)
{
    if (!c || !data) return;
    const uint8_t *p = (const uint8_t *)data;
    c->bitlen += (uint64_t)len * 8;

    if (c->buflen) {
        size_t need = 128 - c->buflen;
        if (len < need) {
            memcpy(c->buf + c->buflen, p, len);
            c->buflen += len;
            return;
        }
        memcpy(c->buf + c->buflen, p, need);
        sha512_compress(c, c->buf);
        p += need;
        len -= need;
        c->buflen = 0;
    }
    while (len >= 128) {
        sha512_compress(c, p);
        p += 128;
        len -= 128;
    }
    if (len) {
        memcpy(c->buf, p, len);
        c->buflen = len;
    }
}

/*
 * FIPS 180-4 §5.1.1 padding: a single 0x80 byte, zero padding to the last
 * 16 bytes of the block, then the 128-bit big-endian bit length. The high
 * 64 bits of the length are zero (all test vectors have < 2^64 bits), matching
 * the uint64_t bitlen counter -- the same 64-bit-length simplification sha256.c
 * already ships with (DRIVERS/Security).
 */
void
wubu_sha512_final(struct wubu_sha512_ctx *c, uint8_t out[WUBU_SHA512_SZ])
{
    if (!c) return;
    uint64_t bitlen = c->bitlen;
    uint8_t  pad = 0x80;
    wubu_sha512_update(c, &pad, 1);

    uint8_t zeros[128];
    memset(zeros, 0, sizeof(zeros));
    size_t rem = 128 - c->buflen;
    if (rem < 16) rem += 128;      /* not enough room for the length: pad */
    if (rem > 16) wubu_sha512_update(c, zeros, rem - 16);

    uint8_t lenb[16];
    memset(lenb, 0, sizeof(lenb)); /* high 64 bits of the 128-bit length */
    for (int i = 0; i < 8; i++)
        lenb[8 + i] = (uint8_t)(bitlen >> (56 - 8 * i));
    wubu_sha512_update(c, lenb, 16);

    if (out) {
        for (int i = 0; i < 8; i++)
            for (int j = 0; j < 8; j++)
                out[i * 8 + j] = (uint8_t)(c->state[i] >> (56 - 8 * j));
    }
}

void
wubu_sha384_final(struct wubu_sha512_ctx *c, uint8_t out[WUBU_SHA384_SZ])
{
    uint8_t full[WUBU_SHA512_SZ];
    wubu_sha512_final(c, full);                 /* identical padding + rounds */
    if (out)
        memcpy(out, full, WUBU_SHA384_SZ);      /* truncate to 48 bytes */
}

int
wubu_sha512(const void *data, size_t len, uint8_t out[WUBU_SHA512_SZ])
{
    struct wubu_sha512_ctx ctx;
    wubu_sha512_init(&ctx);
    wubu_sha512_update(&ctx, data, len);
    wubu_sha512_final(&ctx, out);
    return 0;
}

int
wubu_sha384(const void *data, size_t len, uint8_t out[WUBU_SHA384_SZ])
{
    struct wubu_sha512_ctx ctx;
    wubu_sha384_init(&ctx);
    wubu_sha512_update(&ctx, data, len);
    wubu_sha384_final(&ctx, out);
    return 0;
}
