/* wubu_blake2b.c -- see wubu_blake2b.h. RFC 7693, twelve rounds, little-endian. */
#include "wubu_blake2b.h"
#include <string.h>

static const uint64_t IV[8] = {
    0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
    0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL
};

static const uint8_t SIGMA[12][16] = {
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15},
    {14,10, 4, 8, 9,15,13, 6, 1,12, 0, 2,11, 7, 5, 3},
    {11, 8,12, 0, 5, 2,15,13,10,14, 3, 6, 7, 1, 9, 4},
    { 7, 9, 3, 1,13,12,11,14, 2, 6, 5,10, 4, 0,15, 8},
    { 9, 0, 5, 7, 2, 4,10,15,14, 1,11,12, 6, 8, 3,13},
    { 2,12, 6,10, 0,11, 8, 3, 4,13, 7, 5,15,14, 1, 9},
    {12, 5, 1,15,14,13, 4,10, 0, 7, 6, 3, 9, 2, 8,11},
    {13,11, 7,14,12, 1, 3, 9, 5, 0,15, 4, 8, 6, 2,10},
    { 6,15,14, 9,11, 3, 0, 8,12, 2,13, 7, 1, 4,10, 5},
    {10, 2, 8, 4, 7, 6, 1, 5,15,11, 9,14, 3,12,13, 0},
    { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,15},
    {14,10, 4, 8, 9,15,13, 6, 1,12, 0, 2,11, 7, 5, 3}
};

static uint64_t load64(const uint8_t *p) {
    uint64_t v = 0;
    for (int i = 7; i >= 0; i--) v = (v << 8) | p[i];
    return v;
}
static void store64(uint8_t *p, uint64_t v) {
    for (int i = 0; i < 8; i++) { p[i] = (uint8_t)v; v >>= 8; }
}
static uint64_t rotr64(uint64_t v, int n) { return (v >> n) | (v << (64 - n)); }

#define G(a, b, c, d, x, y) do { \
    v[a] = v[a] + v[b] + (x); v[d] = rotr64(v[d] ^ v[a], 32); \
    v[c] = v[c] + v[d];       v[b] = rotr64(v[b] ^ v[c], 24); \
    v[a] = v[a] + v[b] + (y); v[d] = rotr64(v[d] ^ v[a], 16); \
    v[c] = v[c] + v[d];       v[b] = rotr64(v[b] ^ v[c], 63); \
} while (0)

static void compress(WubuBlake2b *c, const uint8_t block[WUBU_BLAKE2B_BLOCK], int last) {
    uint64_t v[16], m[16];
    for (int i = 0; i < 16; i++) m[i] = load64(block + i * 8);
    for (int i = 0; i < 8; i++) v[i] = c->h[i];
    for (int i = 0; i < 8; i++) v[i + 8] = IV[i];
    v[12] ^= c->t[0];
    v[13] ^= c->t[1];
    if (last) v[14] = ~v[14];
    for (int r = 0; r < 12; r++) {
        const uint8_t *s = SIGMA[r];
        G(0, 4,  8, 12, m[s[0]],  m[s[1]]);
        G(1, 5,  9, 13, m[s[2]],  m[s[3]]);
        G(2, 6, 10, 14, m[s[4]],  m[s[5]]);
        G(3, 7, 11, 15, m[s[6]],  m[s[7]]);
        G(0, 5, 10, 15, m[s[8]],  m[s[9]]);
        G(1, 6, 11, 12, m[s[10]], m[s[11]]);
        G(2, 7,  8, 13, m[s[12]], m[s[13]]);
        G(3, 4,  9, 14, m[s[14]], m[s[15]]);
    }
    for (int i = 0; i < 8; i++) c->h[i] ^= v[i] ^ v[i + 8];
}

static void add_counter(WubuBlake2b *c, uint64_t n) {
    c->t[0] += n;
    if (c->t[0] < n) c->t[1]++;
}

void wubu_blake2b_init(WubuBlake2b *c, size_t outlen, const void *key, size_t keylen) {
    if (!c) return;
    memset(c, 0, sizeof *c);
    if (outlen == 0 || outlen > WUBU_BLAKE2B_OUT) outlen = WUBU_BLAKE2B_OUT;
    /* RFC 7693 caps the key at 64 bytes (the digest size), NOT at the 128-byte block.
     * Accepting 65..128 reached memcpy() with whatever pointer the caller had -- a
     * null dereference when the caller passed NULL and a length, which is exactly how
     * the gate crashed. A key we cannot use is no key: fall back to unkeyed. */
    if (keylen > WUBU_BLAKE2B_OUT || !key) keylen = 0;
    c->outlen = outlen;
    for (int i = 0; i < 8; i++) c->h[i] = IV[i];
    /* parameter block: digest_length | key_length << 8 | fanout=1 << 16 | depth=1 << 24 */
    c->h[0] ^= 0x01010000ULL ^ ((uint64_t)keylen << 8) ^ (uint64_t)outlen;
    if (keylen) {
        uint8_t kb[WUBU_BLAKE2B_BLOCK];
        memset(kb, 0, sizeof kb);
        memcpy(kb, key, keylen);
        wubu_blake2b_update(c, kb, WUBU_BLAKE2B_BLOCK);
        memset(kb, 0, sizeof kb);
    }
}

void wubu_blake2b_update(WubuBlake2b *c, const void *data, size_t len) {
    if (!c || (!data && len)) return;
    const uint8_t *p = (const uint8_t *)data;
    while (len > 0) {
        if (c->buflen == WUBU_BLAKE2B_BLOCK) {
            add_counter(c, WUBU_BLAKE2B_BLOCK);
            compress(c, c->buf, 0);
            c->buflen = 0;
        }
        size_t take = WUBU_BLAKE2B_BLOCK - c->buflen;
        if (take > len) take = len;
        memcpy(c->buf + c->buflen, p, take);
        c->buflen += take;
        p += take;
        len -= take;
    }
}

void wubu_blake2b_final(WubuBlake2b *c, unsigned char *out) {
    if (!c || !out) return;
    add_counter(c, (uint64_t)c->buflen);
    memset(c->buf + c->buflen, 0, WUBU_BLAKE2B_BLOCK - c->buflen);
    compress(c, c->buf, 1);
    uint8_t full[WUBU_BLAKE2B_OUT];
    for (int i = 0; i < 8; i++) store64(full + i * 8, c->h[i]);
    memcpy(out, full, c->outlen);
    memset(full, 0, sizeof full);
    memset(c, 0, sizeof *c);
}

int wubu_blake2b(unsigned char *out, size_t outlen, const void *data, size_t len,
                 const void *key, size_t keylen) {
    if (!out || outlen == 0 || outlen > WUBU_BLAKE2B_OUT || keylen > WUBU_BLAKE2B_OUT) return -1;
    if (keylen && !key) return -1;
    WubuBlake2b c;
    wubu_blake2b_init(&c, outlen, key, keylen);
    wubu_blake2b_update(&c, data, len);
    wubu_blake2b_final(&c, out);
    return 0;
}
