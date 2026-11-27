/* wubu_x25519.h -- X25519 key agreement (RFC 7748).
 *
 * Own implementation (docs/GOAL-SLERM.json, SLERM-2023). Fifth and last piece of the
 * crypto channel that does not need OpenSSL: BLAKE2b (hashing), ChaCha20-Poly1305
 * (sealing), X25519 (agreeing on the key they seal with).
 *
 * The scalar is clamped internally, so a caller cannot forget to. A result that is all
 * zero means the peer sent a low-order point and the shared secret is worthless --
 * that is a refusal, not a key.
 */
#ifndef WUBU_X25519_H
#define WUBU_X25519_H

#include <stdint.h>

enum {
    WUBU_X25519_OK        =  0,
    WUBU_X25519_ARGS      = -1,
    WUBU_X25519_LOW_ORDER = -2   /* all-zero shared secret: do not use it */
};

/* out = scalar * point. Returns WUBU_X25519_OK, or an error; on error `out` is
 * zeroed so a caller cannot ship a key that was refused. */
int wubu_x25519(uint8_t out[32], const uint8_t scalar[32], const uint8_t point[32]);

const char *wubu_x25519_reason(int err);

#endif
