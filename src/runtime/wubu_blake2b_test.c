/* wubu_blake2b_test.c -- the slerm gate for SLERM-2008 (BLAKE2b).
 *
 * The expected digests come from Python's hashlib (an independent implementation of
 * RFC 7693), not from our own code. A hash that matches itself proves nothing; a hash
 * that differs by one bit from the reference is wrong.
 *
 * The second half is streaming: the same input hashed in chunks of 1, 7, 128, 129 and
 * 1000 bytes must give the one-shot digest, because a file reader never gets to
 * choose its chunk boundaries.
 */
#include "wubu_blake2b.h"
#include <stdio.h>
#include <string.h>

static int pass, fail;

typedef struct { const char *name; size_t outlen; size_t keylen; const char *hex; } Vec;

static const Vec VECS[] = {
    { "empty", 64, 0, "786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419d25e1031afee585313896444934eb04b903a685b1448b755d56f701afe9be2ce" },
    { "abc", 64, 0, "ba80a53f981c4d0d6a2797b69f12f6e94c212f14685ac4b74b12bb6fdbffa2d17d87c5392aab792dc252d5de4533cc9518d38aa8dbf1925ab92386edd4009923" },
    { "abc_keyed", 64, 32, "6957433255384f284a2164e3fe5fd002754c439f66799f4936c16638fc76b7c2d94a482b6a967a80d7f4c247478d4cbd4b732bb07cf56c1ddbf137dd979c464c" },
    { "pattern256", 64, 0, "1ecc896f34d3f9cac484c73f75f6a5fb58ee6784be41b35f46067b9c65c63a6794d3d744112c653f73dd7deb6666204c5a9bfa5b46081fc10fdbe7884fa5cbf8" },
    { "pattern1024_32", 32, 0, "8cac99b5587cc7ea85604c3e54c5b487d2adba46d3de4e1792a59a66d5711ae4" },
    { "empty_keyed", 64, 3, "5b3cfd8f422b490b764b55eceb330b500c79cbefa9a928ad00202b8b3c5dd778a81122570434a2e3b8bfd028d105dfefd0a9576e88ed66de742ca9fbb5f8d2b6" },
};

static const unsigned char *input_for(const char *name, size_t *len, unsigned char *buf, size_t cap) {
    if (!strcmp(name, "empty") || !strcmp(name, "empty_keyed")) { *len = 0; return (const unsigned char *)""; }
    if (!strcmp(name, "abc") || !strcmp(name, "abc_keyed")) { *len = 3; memcpy(buf, "abc", 3); return buf; }
    if (!strcmp(name, "pattern256")) { *len = 256; for (size_t i = 0; i < 256; i++) buf[i] = (unsigned char)i; return buf; }
    *len = 1024; for (size_t i = 0; i < 1024 && i < cap; i++) buf[i] = (unsigned char)((i * 7) & 0xff);
    return buf;
}

static void to_hex(const unsigned char *d, size_t n, char *out) {
    static const char *H = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) { out[i*2] = H[d[i] >> 4]; out[i*2+1] = H[d[i] & 15]; }
    out[n*2] = '\0';
}

int main(void) {
    printf("=== wubu_blake2b (slerm gate, SLERM-2008) ===\n");
    unsigned char in[1024], out[64];
    char hex[129];

    for (size_t i = 0; i < sizeof VECS / sizeof VECS[0]; i++) {
        const Vec *v = &VECS[i];
        size_t len = 0;
        const unsigned char *data = input_for(v->name, &len, in, sizeof in);
        unsigned char key[64];
        memset(key, 0, sizeof key);
        if (v->keylen == 3) memcpy(key, "key", 3);   /* 32-byte case stays all zeroes, matching the oracle */
        memset(out, 0, sizeof out);
        int rc = wubu_blake2b(out, v->outlen, data, len, v->keylen ? key : NULL, v->keylen);
        to_hex(out, v->outlen, hex);
        if (rc == 0 && strcmp(hex, v->hex) == 0) pass++;
        else { printf("  [FAIL] %-14s\n    got  %s\n    want %s\n", v->name, hex, v->hex); fail++; }
    }

    /* streaming: every chunking must equal the one-shot digest */
    {
        size_t len = 1024;
        for (size_t i = 0; i < len; i++) in[i] = (unsigned char)((i * 7) & 0xff);
        unsigned char want[64];
        wubu_blake2b(want, 64, in, len, NULL, 0);
        static const size_t chunks[] = { 1, 7, 128, 129, 1000, 1024 };
        for (size_t c = 0; c < sizeof chunks / sizeof chunks[0]; c++) {
            WubuBlake2b ctx;
            wubu_blake2b_init(&ctx, 64, NULL, 0);
            for (size_t off = 0; off < len; off += chunks[c]) {
                size_t take = chunks[c];
                if (off + take > len) take = len - off;
                wubu_blake2b_update(&ctx, in + off, take);
            }
            unsigned char got[64];
            wubu_blake2b_final(&ctx, got);
            if (memcmp(got, want, 64) == 0) pass++;
            else { printf("  [FAIL] streaming in %zu-byte chunks differs\n", chunks[c]); fail++; }
        }
    }

    /* different keys must not collide, and a bad output length is refused */
    {
        unsigned char a[64], b[64];
        wubu_blake2b(a, 64, "abc", 3, "k1", 2);
        wubu_blake2b(b, 64, "abc", 3, "k2", 2);
        if (memcmp(a, b, 64) != 0) pass++; else { printf("  [FAIL] keyed digests collide\n"); fail++; }
        if (wubu_blake2b(a, 0, "abc", 3, NULL, 0) == -1 &&
            wubu_blake2b(a, 65, "abc", 3, NULL, 0) == -1 &&
            wubu_blake2b(a, 64, "abc", 3, NULL, 65) == -1) pass++;
        else { printf("  [FAIL] bad lengths accepted\n"); fail++; }
    }

    printf("\n=== %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
