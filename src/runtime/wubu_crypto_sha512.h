/*
 * wubu_crypto_sha512.h -- WuBuOS runtime SHA-512 / SHA-384 (FIPS 180-4).
 *
 * GOAL (GOALS-1000 #778): SHA-512 and SHA-384 core with FIPS 180-4
 * known-answer vectors. Freestanding C11: no malloc, no hosted APIs. The
 * context is an opaque struct owned by the caller (allocate
 * WUBU_SHA512_CTX_SZ bytes, e.g. a byte array). Digests are emitted
 * big-endian, per the FIPS definition.
 */
#ifndef WUBU_CRYPTO_SHA512_H
#define WUBU_CRYPTO_SHA512_H

#include <stddef.h>
#include <stdint.h>

#define WUBU_SHA512_SZ  64   /* SHA-512 digest bytes   */
#define WUBU_SHA384_SZ  48   /* SHA-384 digest bytes (SHA-512 truncated) */

/*
 * Opaque, caller-owned context. The size is part of the contract (static-
 * assert checked against the definition in the .c). A plain byte array of
 * this size works fine.
 */
#define WUBU_SHA512_CTX_SZ 208
typedef struct wubu_sha512_ctx wubu_sha512_ctx;

/* One-shot: digest `len` bytes at `data`, big-endian into `out`. Returns 0. */
int wubu_sha512(const void *data, size_t len, uint8_t out[WUBU_SHA512_SZ]);
int wubu_sha384(const void *data, size_t len, uint8_t out[WUBU_SHA384_SZ]);

/* Streaming API (init / update / final). */
void wubu_sha512_init(wubu_sha512_ctx *c);
void wubu_sha384_init(wubu_sha512_ctx *c);
void wubu_sha512_update(wubu_sha512_ctx *c, const void *data, size_t len);
void wubu_sha512_final(wubu_sha512_ctx *c, uint8_t out[WUBU_SHA512_SZ]);
void wubu_sha384_final(wubu_sha512_ctx *c, uint8_t out[WUBU_SHA384_SZ]);

#endif /* WUBU_CRYPTO_SHA512_H */
