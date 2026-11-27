/* wubu_aead.h -- ChaCha20-Poly1305 (RFC 8439) and its two primitives.
 *
 * Own implementation (docs/GOAL-SLERM.json, SLERM-2022). This is the AEAD a network
 * session or an encrypted file needs when there is no OpenSSL: one key, one nonce,
 * one tag, and a refusal path that never decrypts unauthenticated bytes.
 */
#ifndef WUBU_AEAD_H
#define WUBU_AEAD_H

#include <stddef.h>
#include <stdint.h>

#define WUBU_AEAD_KEY 32
#define WUBU_AEAD_NONCE 12
#define WUBU_AEAD_TAG 16

enum {
    WUBU_AEAD_OK      =  0,
    WUBU_AEAD_ARGS    = -1,   /* null pointers, or tag/nonce/key missing */
    WUBU_AEAD_TAG_BAD = -2,   /* authentication failed: plaintext is NOT produced */
    WUBU_AEAD_SMALL   = -3    /* output buffer too small */
};

/* ChaCha20 keystream XOR. `counter` is the initial block counter (normally 1 for
 * AEAD data, 0 only for the Poly1305 one-time key). out and in may be the same
 * buffer. */
void wubu_chacha20_xor(uint8_t *out, const uint8_t *in, size_t len,
                       const uint8_t key[WUBU_AEAD_KEY], const uint8_t nonce[WUBU_AEAD_NONCE],
                       uint32_t counter);

/* Poly1305 one-time authenticator (RFC 8439 section 2.5). */
void wubu_poly1305(uint8_t tag[WUBU_AEAD_TAG], const uint8_t *msg, size_t len,
                   const uint8_t key[32]);

/* Seal: writes ct_len bytes to `ct` and the tag to `tag`. `ct` may equal `pt`.
 * `aad` may be NULL with aad_len 0. Returns WUBU_AEAD_OK or an error. */
int wubu_aead_seal(uint8_t *ct, uint8_t tag[WUBU_AEAD_TAG],
                   const uint8_t *pt, size_t pt_len,
                   const uint8_t *aad, size_t aad_len,
                   const uint8_t key[WUBU_AEAD_KEY], const uint8_t nonce[WUBU_AEAD_NONCE]);

/* Open: verifies the tag BEFORE writing any plaintext. On a bad tag nothing is
 * written, so a caller cannot accidentally use unauthenticated data. */
int wubu_aead_open(uint8_t *pt, size_t pt_cap,
                   const uint8_t *ct, size_t ct_len, const uint8_t tag[WUBU_AEAD_TAG],
                   const uint8_t *aad, size_t aad_len,
                   const uint8_t key[WUBU_AEAD_KEY], const uint8_t nonce[WUBU_AEAD_NONCE]);

const char *wubu_aead_reason(int err);

#endif
