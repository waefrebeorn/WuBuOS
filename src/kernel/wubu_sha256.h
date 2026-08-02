/*
 * wubu_sha256.h -- WuBuOS kernel-side SHA-256 (FIPS 180-4), self-contained.
 *
 * Freestanding C11: no malloc, no hosted APIs. The context is an opaque
 * struct (128 bytes) owned by the caller. The digest is big-endian per
 * the FIPS definition (the caller memcpy's it out).
 */
#ifndef WUBU_SHA256_H
#define WUBU_SHA256_H

#include <stddef.h>
#include <stdint.h>

#define WUBU_SHA256_SZ 32   /* digest bytes */

/* Opaque context: callers allocate WUBU_SHA256_CTX_SZ bytes (a byte
 * array works fine) and pass the pointer. The size is part of the
 * contract (static-asserted in the .c). */
#define WUBU_SHA256_CTX_SZ 112
typedef struct wubu_sha256_ctx wubu_sha256_ctx;

/* One-shot: digest of `len` bytes at `data`, written big-endian to `out`
 * (must hold >= WUBU_SHA256_SZ bytes). Returns 0. */
int wubu_sha256(const void *data, size_t len, uint8_t out[WUBU_SHA256_SZ]);

/* Streaming API (init / update / final). */
void wubu_sha256_init(wubu_sha256_ctx *c);
void wubu_sha256_update(wubu_sha256_ctx *c, const void *data, size_t len);
void wubu_sha256_final(wubu_sha256_ctx *c, uint8_t out[WUBU_SHA256_SZ]);

#endif /* WUBU_SHA256_H */
