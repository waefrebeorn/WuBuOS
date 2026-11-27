/* wubu_aead.c -- see wubu_aead.h. RFC 8439: ChaCha20, Poly1305, the AEAD composition. */
#include "wubu_aead.h"
#include <string.h>

static uint32_t load32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static void store32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
static uint32_t rotl32(uint32_t v, int n) { return (v << n) | (v >> (32 - n)); }

#define QR(a, b, c, d) do { \
    a += b; d ^= a; d = rotl32(d, 16); \
    c += d; b ^= c; b = rotl32(b, 12); \
    a += b; d ^= a; d = rotl32(d, 8);  \
    c += d; b ^= c; b = rotl32(b, 7);  \
} while (0)

static void chacha_block(uint8_t out[64], const uint8_t key[32], const uint8_t nonce[12], uint32_t counter) {
    uint32_t s[16], x[16];
    s[0] = 0x61707865u; s[1] = 0x3320646eu; s[2] = 0x79622d32u; s[3] = 0x6b206574u;
    for (int i = 0; i < 8; i++) s[4 + i] = load32(key + i * 4);
    s[12] = counter;
    s[13] = load32(nonce + 0);
    s[14] = load32(nonce + 4);
    s[15] = load32(nonce + 8);
    memcpy(x, s, sizeof x);
    for (int i = 0; i < 10; i++) {
        QR(x[0], x[4], x[8],  x[12]);
        QR(x[1], x[5], x[9],  x[13]);
        QR(x[2], x[6], x[10], x[14]);
        QR(x[3], x[7], x[11], x[15]);
        QR(x[0], x[5], x[10], x[15]);
        QR(x[1], x[6], x[11], x[12]);
        QR(x[2], x[7], x[8],  x[13]);
        QR(x[3], x[4], x[9],  x[14]);
    }
    for (int i = 0; i < 16; i++) store32(out + i * 4, x[i] + s[i]);
}

void wubu_chacha20_xor(uint8_t *out, const uint8_t *in, size_t len,
                       const uint8_t key[WUBU_AEAD_KEY], const uint8_t nonce[WUBU_AEAD_NONCE],
                       uint32_t counter) {
    uint8_t ks[64];
    while (len > 0) {
        chacha_block(ks, key, nonce, counter++);
        size_t take = len < 64 ? len : 64;
        for (size_t i = 0; i < take; i++) out[i] = (uint8_t)(in[i] ^ ks[i]);
        out += take; in += take; len -= take;
    }
    memset(ks, 0, sizeof ks);
}

/* Poly1305 as a context: one implementation of the block loop and one of the
 * finalisation, used by both the one-shot API and the AEAD MAC. Two copies of this
 * arithmetic is how a tag disagrees with itself. */
typedef struct {
    uint32_t h[5], r[5], s[4];
    uint8_t  buf[16];
    size_t   n;
} Poly;

static void poly_init(Poly *p, const uint8_t key[32]) {
    memset(p, 0, sizeof *p);
    p->r[0] = load32(key + 0) & 0x3ffffffu;
    p->r[1] = (load32(key + 3) >> 2) & 0x3ffff03u;
    p->r[2] = (load32(key + 6) >> 4) & 0x3ffc0ffu;
    p->r[3] = (load32(key + 9) >> 6) & 0x3f03fffu;
    p->r[4] = (load32(key + 12) >> 8) & 0x00fffffu;
    p->s[0] = p->r[1] * 5; p->s[1] = p->r[2] * 5; p->s[2] = p->r[3] * 5; p->s[3] = p->r[4] * 5;
}

static void poly_block(uint32_t h[5], const uint32_t r[5], const uint32_t s[4],
                       const uint8_t m[16], uint32_t hibit) {
    uint64_t d0, d1, d2, d3, d4, c;
    h[0] += load32(m + 0) & 0x3ffffffu;
    h[1] += (load32(m + 3) >> 2) & 0x3ffffffu;
    h[2] += (load32(m + 6) >> 4) & 0x3ffffffu;
    h[3] += (load32(m + 9) >> 6) & 0x3ffffffu;
    h[4] += (load32(m + 12) >> 8) | hibit;
    d0 = (uint64_t)h[0] * r[0] + (uint64_t)h[1] * s[3] + (uint64_t)h[2] * s[2] + (uint64_t)h[3] * s[1] + (uint64_t)h[4] * s[0];
    d1 = (uint64_t)h[0] * r[1] + (uint64_t)h[1] * r[0] + (uint64_t)h[2] * s[3] + (uint64_t)h[3] * s[2] + (uint64_t)h[4] * s[1];
    d2 = (uint64_t)h[0] * r[2] + (uint64_t)h[1] * r[1] + (uint64_t)h[2] * r[0] + (uint64_t)h[3] * s[3] + (uint64_t)h[4] * s[2];
    d3 = (uint64_t)h[0] * r[3] + (uint64_t)h[1] * r[2] + (uint64_t)h[2] * r[1] + (uint64_t)h[3] * r[0] + (uint64_t)h[4] * s[3];
    d4 = (uint64_t)h[0] * r[4] + (uint64_t)h[1] * r[3] + (uint64_t)h[2] * r[2] + (uint64_t)h[3] * r[1] + (uint64_t)h[4] * r[0];
    c = d0 >> 26; h[0] = (uint32_t)d0 & 0x3ffffffu;
    d1 += c;      c = d1 >> 26; h[1] = (uint32_t)d1 & 0x3ffffffu;
    d2 += c;      c = d2 >> 26; h[2] = (uint32_t)d2 & 0x3ffffffu;
    d3 += c;      c = d3 >> 26; h[3] = (uint32_t)d3 & 0x3ffffffu;
    d4 += c;      c = d4 >> 26; h[4] = (uint32_t)d4 & 0x3ffffffu;
    h[0] += (uint32_t)c * 5u; c = h[0] >> 26; h[0] &= 0x3ffffffu;
    h[1] += (uint32_t)c;
}

static void poly_update(Poly *p, const void *data, size_t len) {
    const uint8_t *m = (const uint8_t *)data;
    while (len--) {
        p->buf[p->n++] = *m++;
        if (p->n == 16) { poly_block(p->h, p->r, p->s, p->buf, 1u << 24); p->n = 0; }
    }
}

static void poly_final(Poly *p, uint8_t tag[16], const uint8_t key[32]) {
    if (p->n) {
        p->buf[p->n] = 1;
        memset(p->buf + p->n + 1, 0, 16 - p->n - 1);
        poly_block(p->h, p->r, p->s, p->buf, 0);
    }
    uint32_t *h = p->h;
    uint32_t c = h[1] >> 26; h[1] &= 0x3ffffffu;
    h[2] += c; c = h[2] >> 26; h[2] &= 0x3ffffffu;
    h[3] += c; c = h[3] >> 26; h[3] &= 0x3ffffffu;
    h[4] += c; c = h[4] >> 26; h[4] &= 0x3ffffffu;
    h[0] += c * 5; c = h[0] >> 26; h[0] &= 0x3ffffffu; h[1] += c;

    /* h + 5, then keep whichever of (that - 2^130) and h is smaller */
    uint32_t g0 = h[0] + 5, t = g0 >> 26; g0 &= 0x3ffffffu;
    uint32_t g1 = h[1] + t; t = g1 >> 26; g1 &= 0x3ffffffu;
    uint32_t g2 = h[2] + t; t = g2 >> 26; g2 &= 0x3ffffffu;
    uint32_t g3 = h[3] + t; t = g3 >> 26; g3 &= 0x3ffffffu;
    uint32_t g4 = (h[4] + t) - (1u << 26);
    uint32_t mask = (g4 >> 31) - 1;
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h[0] = (h[0] & mask) | g0; h[1] = (h[1] & mask) | g1;
    h[2] = (h[2] & mask) | g2; h[3] = (h[3] & mask) | g3;
    h[4] = (h[4] & mask) | g4;

    uint32_t hh0 = (h[0] | (h[1] << 26)) & 0xffffffffu;
    uint32_t hh1 = ((h[1] >> 6) | (h[2] << 20)) & 0xffffffffu;
    uint32_t hh2 = ((h[2] >> 12) | (h[3] << 14)) & 0xffffffffu;
    uint32_t hh3 = ((h[3] >> 18) | (h[4] << 8)) & 0xffffffffu;
    uint64_t f = (uint64_t)hh0 + load32(key + 16); store32(tag + 0, (uint32_t)f); f >>= 32;
    f += (uint64_t)hh1 + load32(key + 20);        store32(tag + 4, (uint32_t)f); f >>= 32;
    f += (uint64_t)hh2 + load32(key + 24);        store32(tag + 8, (uint32_t)f); f >>= 32;
    f += (uint64_t)hh3 + load32(key + 28);        store32(tag + 12, (uint32_t)f);
}

void wubu_poly1305(uint8_t tag[WUBU_AEAD_TAG], const uint8_t *msg, size_t len, const uint8_t key[32]) {
    Poly p;
    poly_init(&p, key);
    poly_update(&p, msg, len);
    poly_final(&p, tag, key);
    memset(&p, 0, sizeof p);
}

static void poly_pad(Poly *p, size_t n) {
    static const uint8_t zeros[16] = { 0 };
    if (n % 16) poly_update(p, zeros, 16 - (n % 16));
}

/* RFC 8439 section 2.8: the tag covers AAD, ciphertext, and BOTH lengths, in that
 * order, so a message cannot be re-split between the two fields. */
static void aead_mac(uint8_t tag[WUBU_AEAD_TAG],
                     const uint8_t *ct, size_t ct_len,
                     const uint8_t *aad, size_t aad_len,
                     const uint8_t key[WUBU_AEAD_KEY], const uint8_t nonce[WUBU_AEAD_NONCE]) {
    uint8_t block0[64], otk[32], lens[16];
    chacha_block(block0, key, nonce, 0);
    memcpy(otk, block0, sizeof otk);
    Poly p;
    poly_init(&p, otk);
    if (aad_len) { poly_update(&p, aad, aad_len); poly_pad(&p, aad_len); }
    if (ct_len)  { poly_update(&p, ct, ct_len);   poly_pad(&p, ct_len); }
    uint64_t al = (uint64_t)aad_len, cl = (uint64_t)ct_len;
    for (int i = 0; i < 8; i++) lens[i] = (uint8_t)(al >> (8 * i));
    for (int i = 0; i < 8; i++) lens[8 + i] = (uint8_t)(cl >> (8 * i));
    poly_update(&p, lens, sizeof lens);
    poly_final(&p, tag, otk);
    memset(&p, 0, sizeof p);
    memset(otk, 0, sizeof otk);
    memset(block0, 0, sizeof block0);
}

static int ct_equal(const uint8_t *a, const uint8_t *b, size_t n) {
    uint8_t d = 0;
    for (size_t i = 0; i < n; i++) d |= (uint8_t)(a[i] ^ b[i]);
    return d == 0;
}

int wubu_aead_seal(uint8_t *ct, uint8_t tag[WUBU_AEAD_TAG],
                   const uint8_t *pt, size_t pt_len,
                   const uint8_t *aad, size_t aad_len,
                   const uint8_t key[WUBU_AEAD_KEY], const uint8_t nonce[WUBU_AEAD_NONCE]) {
    if (!ct || !tag || !key || !nonce) return WUBU_AEAD_ARGS;
    if (!pt && pt_len) return WUBU_AEAD_ARGS;
    if (pt_len) wubu_chacha20_xor(ct, pt, pt_len, key, nonce, 1);
    aead_mac(tag, ct, pt_len, aad, aad_len, key, nonce);
    return WUBU_AEAD_OK;
}

int wubu_aead_open(uint8_t *pt, size_t pt_cap,
                   const uint8_t *ct, size_t ct_len, const uint8_t tag[WUBU_AEAD_TAG],
                   const uint8_t *aad, size_t aad_len,
                   const uint8_t key[WUBU_AEAD_KEY], const uint8_t nonce[WUBU_AEAD_NONCE]) {
    if (!pt || !ct || !tag || !key || !nonce) return WUBU_AEAD_ARGS;
    if (pt_cap < ct_len) return WUBU_AEAD_SMALL;
    uint8_t want[WUBU_AEAD_TAG];
    aead_mac(want, ct, ct_len, aad, aad_len, key, nonce);
    int ok = ct_equal(want, tag, WUBU_AEAD_TAG);
    memset(want, 0, sizeof want);
    if (!ok) return WUBU_AEAD_TAG_BAD;   /* nothing written: no unauthenticated bytes */
    if (ct_len) wubu_chacha20_xor(pt, ct, ct_len, key, nonce, 1);
    return WUBU_AEAD_OK;
}

const char *wubu_aead_reason(int err) {
    switch (err) {
        case WUBU_AEAD_OK:      return "ok";
        case WUBU_AEAD_ARGS:    return "bad arguments";
        case WUBU_AEAD_TAG_BAD: return "authentication failed";
        case WUBU_AEAD_SMALL:   return "output buffer too small";
        default:                return "invalid";
    }
}
