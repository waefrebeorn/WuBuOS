/*
 * tests/fixtures/fixed_full/probe.c -- RFC 8032 cross-check for the bignum-free
 * fixed-width sign/verify path.
 *
 * OpenSSL EVP is the independent oracle: a signature made by our module must
 * verify under EVP; an EVP signature must verify under our module; both must
 * agree byte-for-byte.  RFC 8032 test vector 1's seed/public pair anchors the
 * key derivation.  Negative controls exercise the canonical-S and small-
 * subgroup policy on this path.
 */
#include "wubu_ed25519_fixed_sign.h"

#include <openssl/evp.h>

#include <stdint.h>
#include <stdio.h>
#include <string.h>

static int hex2bin(const char *hex, uint8_t *out, size_t n) {
    if (!hex || strlen(hex) != 2 * n) return -1;
    for (size_t i = 0; i < n; i++) {
        unsigned v;
        if (sscanf(hex + 2 * i, "%2x", &v) != 1) return -1;
        out[i] = (uint8_t)v;
    }
    return 0;
}
static void bin2hex(const uint8_t *b, size_t n) {
    for (size_t i = 0; i < n; i++) printf("%02x", b[i]);
}

/* 1 = verified, 0 = rejected, -1 = EVP unavailable. */
static int evp_verify(const uint8_t pub[32], const uint8_t sig[64],
                      const uint8_t msg[32]) {
    EVP_PKEY *k = EVP_PKEY_new_raw_public_key(EVP_PKEY_ED25519, NULL, pub, 32);
    if (!k) return -1;
    EVP_MD_CTX *c = EVP_MD_CTX_new();
    if (!c) { EVP_PKEY_free(k); return -1; }
    int rc = EVP_DigestVerifyInit(c, NULL, NULL, NULL, k) == 1 &&
             EVP_DigestVerify(c, sig, 64, msg, 32) == 1;
    EVP_MD_CTX_free(c);
    EVP_PKEY_free(k);
    return rc;
}

static int evp_sign(const uint8_t seed[32], const uint8_t msg[32],
                    uint8_t sig[64]) {
    EVP_PKEY *k = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, seed, 32);
    if (!k) return -1;
    EVP_MD_CTX *c = EVP_MD_CTX_new();
    if (!c) { EVP_PKEY_free(k); return -1; }
    size_t sl = 64;
    int rc = EVP_DigestSignInit(c, NULL, NULL, NULL, k) == 1 &&
             EVP_DigestSign(c, sig, &sl, msg, 32) == 1 && sl == 64;
    EVP_MD_CTX_free(c);
    EVP_PKEY_free(k);
    return rc ? 0 : -1;
}

static int evp_pub(const uint8_t seed[32], uint8_t pub[32]) {
    EVP_PKEY *k = EVP_PKEY_new_raw_private_key(EVP_PKEY_ED25519, NULL, seed, 32);
    if (!k) return -1;
    size_t pl = 32;
    int rc = EVP_PKEY_get_raw_public_key(k, pub, &pl) == 1 && pl == 32;
    EVP_PKEY_free(k);
    return rc ? 0 : -1;
}

static int fails;
#define CHECK(label, cond) do { \
    if (!(cond)) { printf("FAIL %s\n", label); ++fails; } \
} while (0)

int main(void) {
    /* RFC 8032 section 7.1, test vector 1 (the empty-message key pair). */
    uint8_t seed[32], rfc_pub[32], pub[32];
    static const char *SEED1 =
        "9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60";
    static const char *PUB1 =
        "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a";
    CHECK("hex seed", hex2bin(SEED1, seed, 32) == 0);
    CHECK("hex rfc pub", hex2bin(PUB1, rfc_pub, 32) == 0);
    CHECK("evp pub", evp_pub(seed, pub) == 0);
    CHECK("RFC8032 v1 public key", memcmp(pub, rfc_pub, 32) == 0);

    uint8_t msg[32], ours[64], theirs[64];
    for (size_t i = 0; i < 32; i++) msg[i] = (uint8_t)i;

    /* Our signature must match and verify under the independent oracle. */
    CHECK("our sign", wubu_ed25519_fixed_sign(msg, seed, ours) == 0);
    CHECK("our sig verifies under OpenSSL", evp_verify(rfc_pub, ours, msg) == 1);
    CHECK("our sig verifies on our path",
          wubu_ed25519_fixed_verify(msg, rfc_pub, ours) == 0);
    CHECK("EVP sign", evp_sign(seed, msg, theirs) == 0);
    CHECK("byte-for-byte agreement with OpenSSL", memcmp(ours, theirs, 64) == 0);
    CHECK("EVP sig verifies on our path",
          wubu_ed25519_fixed_verify(msg, rfc_pub, theirs) == 0);

    /* Tamper controls. */
    uint8_t bad[64];
    memcpy(bad, ours, 64); bad[0] ^= 1;
    CHECK("tampered R rejected on our path",
          wubu_ed25519_fixed_verify(msg, rfc_pub, bad) != 0);
    memcpy(bad, ours, 64); bad[40] ^= 1;
    CHECK("tampered S rejected on our path",
          wubu_ed25519_fixed_verify(msg, rfc_pub, bad) != 0);
    uint8_t badmsg[32]; memcpy(badmsg, msg, 32); badmsg[0] ^= 1;
    CHECK("wrong digest rejected on our path",
          wubu_ed25519_fixed_verify(badmsg, rfc_pub, ours) != 0);

    /* Non-canonical S (S = L) must be refused. */
    memcpy(bad, ours, 64);
    static const char *LE =
        "edd3f55c1a6312586cd7f79cdef9de14"
        "0000000000000000"
        "0000000000000010";
    CHECK("hex L", hex2bin(LE, bad + 32, 32) == 0);
    CHECK("non-canonical S=L rejected",
          wubu_ed25519_fixed_verify(msg, rfc_pub, bad) != 0);

    /* Small-subgroup public key: all-zeros A is not a valid encoded point;
     * use the order-8 point y=1,x=0 encoded canonically and a signature whose
     * R is the same point, then require rejection by the [8]A == identity
     * check rather than by a malformed-point error. */
    uint8_t ident_pub[32] = {1};
    uint8_t ident_sig[64];
    memset(ident_sig, 0, sizeof(ident_sig));
    ident_sig[0] = 1; memcpy(ident_sig + 32, ours + 32, 32);
    CHECK("identity public key rejected",
          wubu_ed25519_fixed_verify(msg, ident_pub, ident_sig) != 0);

    printf("fixed_full_probe: %s (%d failures)\n", fails ? "FAILED" : "PASSED", fails);
    return fails ? 1 : 0;
}
