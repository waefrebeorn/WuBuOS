/*
 * wubu_crypto.h  --  WuBuOS Crypto Helpers (SHA256, CRC32)
 *
 * Self-contained, no external dependencies beyond <stdint.h> and <string.h>.
 * All functions are static -- include in any TU that needs them.
 * C11 compatible, no VLA, no platform-specific code.
 *
 * SHA256 implementation: public domain / FIPS 180-4
 * CRC32 implementation: POSIX cksum polynomial (0xEDB88320)
 */

#ifndef WUBU_CRYPTO_H
#define WUBU_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/*  CRC32 (POSIX cksum polynomial 0xEDB88320)                         */
/* ------------------------------------------------------------------ */

static uint32_t wubu_crc32(const void *data, size_t size) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < size; i++) {
        crc ^= p[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return crc ^ 0xFFFFFFFF;
}

/* ------------------------------------------------------------------ */
/*  SHA256                                                            */
/* ------------------------------------------------------------------ */

static const uint32_t wubu_sha256_k[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

typedef struct {
    uint32_t h[8];
    uint64_t total_len;
    uint8_t  buffer[64];
    size_t   buffer_len;
} wubu_SHA256_CTX;

static void wubu_sha256_init(wubu_SHA256_CTX *ctx) {
    ctx->h[0] = 0x6a09e667; ctx->h[1] = 0xbb67ae85;
    ctx->h[2] = 0x3c6ef372; ctx->h[3] = 0xa54ff53a;
    ctx->h[4] = 0x510e527f; ctx->h[5] = 0x9b05688c;
    ctx->h[6] = 0x1f83d9ab; ctx->h[7] = 0x5be0cd19;
    ctx->total_len = 0;
    ctx->buffer_len = 0;
}

static void wubu_sha256_transform(wubu_SHA256_CTX *ctx, const uint8_t *data) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = ((uint32_t)data[i*4] << 24) | ((uint32_t)data[i*4+1] << 16)
             | ((uint32_t)data[i*4+2] << 8)  | (uint32_t)data[i*4+3];
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ((w[i-15] >> 7) | (w[i-15] << 25))
                    ^ ((w[i-15] >> 18) | (w[i-15] << 14)) ^ (w[i-15] >> 3);
        uint32_t s1 = ((w[i-2] >> 17) | (w[i-2] << 15))
                    ^ ((w[i-2] >> 19) | (w[i-2] << 13)) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a = ctx->h[0], b = ctx->h[1], c = ctx->h[2], d = ctx->h[3];
    uint32_t e = ctx->h[4], f = ctx->h[5], g = ctx->h[6], h = ctx->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ((e >> 6) | (e << 26)) ^ ((e >> 11) | (e << 21)) ^ ((e >> 25) | (e << 7));
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t temp1 = h + S1 + ch + wubu_sha256_k[i] + w[i];
        uint32_t S0 = ((a >> 2) | (a << 30)) ^ ((a >> 13) | (a << 19)) ^ ((a >> 22) | (a << 10));
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t temp2 = S0 + maj;
        h = g; g = f; f = e; e = d + temp1;
        d = c; c = b; b = a; a = temp1 + temp2;
    }
    ctx->h[0] += a; ctx->h[1] += b; ctx->h[2] += c; ctx->h[3] += d;
    ctx->h[4] += e; ctx->h[5] += f; ctx->h[6] += g; ctx->h[7] += h;
}

static void wubu_sha256_update(wubu_SHA256_CTX *ctx, const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    ctx->total_len += len;
    while (len > 0) {
        size_t space = 64 - ctx->buffer_len;
        size_t take = len < space ? len : space;
        memcpy(ctx->buffer + ctx->buffer_len, p, take);
        ctx->buffer_len += (uint32_t)take;
        p += take;
        len -= take;
        if (ctx->buffer_len == 64) {
            wubu_sha256_transform(ctx, ctx->buffer);
            ctx->buffer_len = 0;
        }
    }
}

static void wubu_sha256_final(wubu_SHA256_CTX *ctx, uint8_t *hash) {
    ctx->buffer[ctx->buffer_len++] = 0x80;
    if (ctx->buffer_len > 56) {
        while (ctx->buffer_len < 64) ctx->buffer[ctx->buffer_len++] = 0;
        wubu_sha256_transform(ctx, ctx->buffer);
        ctx->buffer_len = 0;
    }
    while (ctx->buffer_len < 56) ctx->buffer[ctx->buffer_len++] = 0;
    uint64_t bits = ctx->total_len * 8;
    for (int i = 0; i < 8; i++)
        ctx->buffer[56 + i] = (uint8_t)((bits >> (56 - i * 8)) & 0xFF);
    wubu_sha256_transform(ctx, ctx->buffer);
    for (int i = 0; i < 8; i++) {
        hash[i]      = (uint8_t)(ctx->h[i] >> 24);
        hash[i + 8]  = (uint8_t)(ctx->h[i] >> 16);
        hash[i + 16] = (uint8_t)(ctx->h[i] >> 8);
        hash[i + 24] = (uint8_t)(ctx->h[i]);
    }
}

static void wubu_sha256_digest(const void *data, size_t size, char *out_hex, size_t out_size) {
    if (!out_hex || out_size < 65) { if (out_hex && out_size > 0) out_hex[0] = '\0'; return; }
    wubu_SHA256_CTX ctx;
    wubu_sha256_init(&ctx);
    wubu_sha256_update(&ctx, data, size);
    uint8_t hash[32];
    wubu_sha256_final(&ctx, hash);
    for (int i = 0; i < 32; i++)
        snprintf(out_hex + i * 2, out_size - i * 2, "%02x", hash[i]);
    out_hex[64] = '\0';
}

static void wubu_sha256_file(const char *path, char *out_hex, size_t out_size) {
    if (!out_hex || out_size < 65) { if (out_hex && out_size > 0) out_hex[0] = '\0'; return; }
    FILE *f = fopen(path, "rb");
    if (!f) { memset(out_hex, 0, out_size < 65 ? out_size : 65); return; }
    wubu_SHA256_CTX ctx;
    wubu_sha256_init(&ctx);
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
        wubu_sha256_update(&ctx, buf, n);
    fclose(f);
    uint8_t hash[32];
    wubu_sha256_final(&ctx, hash);
    for (int i = 0; i < 32; i++)
        snprintf(out_hex + i * 2, out_size - i * 2, "%02x", hash[i]);
    out_hex[64] = '\0';
}

#ifdef __cplusplus
}
#endif

#endif /* WUBU_CRYPTO_H */