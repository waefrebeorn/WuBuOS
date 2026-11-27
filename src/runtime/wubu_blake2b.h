/* wubu_blake2b.h -- BLAKE2b as specified in RFC 7693.
 *
 * Own implementation (docs/GOAL-SLERM.json, SLERM-2008). BLAKE2b is the keyed and
 * unkeyed hash this tree needs for content-addressed manifests and for MACs that do
 * not require a full TLS stack. Nothing here is inventive: the spec is exact, the
 * test vectors are public, and a hash that differs from them by one bit is wrong.
 *
 * Streaming, no allocation, no globals.
 */
#ifndef WUBU_BLAKE2B_H
#define WUBU_BLAKE2B_H

#include <stddef.h>
#include <stdint.h>

#define WUBU_BLAKE2B_OUT 64
#define WUBU_BLAKE2B_BLOCK 128

typedef struct {
    uint64_t h[8];
    uint64_t t[2];
    uint8_t  buf[WUBU_BLAKE2B_BLOCK];
    size_t   buflen;
    size_t   outlen;
} WubuBlake2b;

/* Start a hash. `outlen` is 1..64 (48 and 64 are the common ones). `key` may be NULL
 * with keylen 0; keyed mode is what a MAC uses, and RFC 7693 requires keylen <= 64. */
void wubu_blake2b_init(WubuBlake2b *c, size_t outlen, const void *key, size_t keylen);

void wubu_blake2b_update(WubuBlake2b *c, const void *data, size_t len);

/* Finish and write `c->outlen` bytes. The context is wiped, so a second final call
 * cannot leak a second digest. */
void wubu_blake2b_final(WubuBlake2b *c, unsigned char *out);

/* One-shot convenience. returns 0, or -1 on bad lengths. */
int wubu_blake2b(unsigned char *out, size_t outlen, const void *data, size_t len,
                 const void *key, size_t keylen);

#endif
