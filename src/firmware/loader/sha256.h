/*
 * sha256.h -- self-contained SHA-256 for the WuBuOS EFI chainloader.
 * FIPS 180-4. No libcrypto, no firmware deps.
 */
#ifndef WUBUFW_LOADER_SHA256_H
#define WUBUFW_LOADER_SHA256_H

#include <stdint.h>

typedef struct {
    uint32_t h[8];
    uint64_t len;
    uint8_t  buf[64];
    uint32_t bufn;
} sha256_ctx;

void sha256_init(sha256_ctx *c);
void sha256_update(sha256_ctx *c, const void *data, uint64_t len);
void sha256_final(sha256_ctx *c, uint8_t out[32]);
void sha256(const void *data, uint64_t len, uint8_t out[32]);

#endif /* WUBUFW_LOADER_SHA256_H */
