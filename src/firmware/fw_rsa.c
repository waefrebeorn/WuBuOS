/*
 * fw_rsa.c -- WuBuFW RSA-2048 signature verification (PKCS#1 v1.5).
 *
 * The Secure Boot db is hash-of-cert in the base firmware (see
 * fw_secureboot.c), which anyone can satisfy by embedding an enrolled cert:
 * it is not authentication.  This module adds the real asymmetric check --
 * only the holder of the private key can produce an image whose RSA
 * signature verifies against the enrolled public key.
 *
 * Self-contained: 2048-bit bignum (64 x 32-bit limbs), Montgomery
 * multiplication, and EMSA-PKCS1-v1_5 with SHA-256.  No libc, no EDK2.
 *
 * Wire form:
 *   - public key:  n[64] limbs (little-endian), e[64] limbs
 *   - signature:   256 bytes (big-endian), appended to the image
 *   - hash:        SHA-256 of the image's Authenticode regions (fw_sha256)
 */
#include "fw_rsa.h"
#include <string.h>

#define RSA_L 64                 /* 64 x 32-bit = 2048 bits */
#define RSA_BYTES 256

uint32_t g_fw_rsa_last_m[64];


/* ---- 2048-bit bignum helpers (little-endian limbs) ---- */

static int bn_cmp(const uint32_t *a, const uint32_t *b, int L) {
    for (int i = L - 1; i >= 0; i--) {
        if (a[i] < b[i]) return -1;
        if (a[i] > b[i]) return 1;
    }
    return 0;
}

static void bn_sub(uint32_t *a, const uint32_t *b, int L) {
    uint64_t borrow = 0;
    for (int i = 0; i < L; i++) {
        uint64_t t = (uint64_t)a[i] - b[i] - borrow;
        a[i] = (uint32_t)t;
        borrow = (t >> 32) & 1u;
    }
}

/* schoolbook product: out[2L] = a[L] * b[L] */
static void bn_mul_wide(uint32_t *out, const uint32_t *a, const uint32_t *b, int L) {
    memset(out, 0, (size_t)2 * L * sizeof(uint32_t));
    for (int i = 0; i < L; i++) {
        uint64_t carry = 0;
        for (int j = 0; j < L; j++) {
            uint64_t t = (uint64_t)a[i] * b[j] + out[i + j] + carry;
            out[i + j] = (uint32_t)t;
            carry = t >> 32;
        }
        out[i + L] = (uint32_t)carry;
    }
}

/* -n[0]^-1 mod 2^32 (Newton iteration; n is odd for an RSA modulus). */
static uint32_t mod_inv_n0(uint32_t a) {
    uint32_t x = 1;
    for (int i = 0; i < 5; i++)
        x = x * (2u - a * x);
    return (uint32_t)(0u - x);
}

/* Montgomery reduction: T[2L] -> T[2L] * R^-1 mod n, result in the low L
 * limbs (then conditionally reduced).  n0inv = -n[0]^-1 mod 2^32. */
static void bn_redc(uint32_t *T, const uint32_t *n, uint32_t n0inv, int L) {
    for (int i = 0; i < L; i++) {
        uint32_t m = (uint32_t)((uint64_t)T[i] * n0inv & 0xFFFFFFFFu);
        uint64_t carry = 0;
        for (int j = 0; j < L; j++) {
            uint64_t t = (uint64_t)m * n[j] + T[i + j] + carry;
            T[i + j] = (uint32_t)t;
            carry = t >> 32;
        }
        int k = i + L;
        while (carry && k < 2 * L + 2) {
            uint64_t t = (uint64_t)T[k] + carry;
            T[k] = (uint32_t)t;
            carry = t >> 32;
            k++;
        }
    }
    /* result is the high L limbs T[L..2L-1] */
    if (bn_cmp(T + L, n, L) >= 0)
        bn_sub(T + L, n, L);
}

/* Montgomery multiply: out[L] = a*b*R^-1 mod n.  T has 2L+2 limbs: REDC's
 * carry propagation can walk one past the 2L window, and dropping it (or
 * writing out of bounds into an adjacent static) corrupts the result. */
static void bn_mont_mul(uint32_t *out, const uint32_t *a, const uint32_t *b,
                        const uint32_t *n, uint32_t n0inv, int L) {
    static uint32_t T[2 * RSA_L + 2];
    bn_mul_wide(T, a, b, L);
    T[2 * L] = T[2 * L + 1] = 0;
    bn_redc(T, n, n0inv, L);
    memcpy(out, T + L, (size_t)L * sizeof(uint32_t));
}

/* 2^(bits) mod n, by repeated doubling + conditional subtract.  Uses L+1
 * limbs so the carry out of the top limb is retained before reduction.
 * bits = 32*L gives R = 2^2048 mod n; bits = 64*L gives R^2 = 2^4096 mod n. */
static void bn_pow2_mod_n(uint32_t *out, const uint32_t *n, int L, int bits) {
    static uint32_t a[RSA_L + 1];
    static uint32_t n_ext[RSA_L + 1];
    memset(a, 0, sizeof a);
    memset(n_ext, 0, sizeof n_ext);
    memcpy(n_ext, n, (size_t)L * sizeof(uint32_t));
    a[0] = 1;                            /* 1 */
    for (int b = 0; b < bits; b++) {     /* double `bits` times */
        uint64_t carry = 0;
        for (int i = 0; i < L + 1; i++) {
            uint64_t t = ((uint64_t)a[i] << 1) | carry;
            a[i] = (uint32_t)t;
            carry = t >> 32;
        }
        /* a < 2n, so one conditional subtract reduces it below n */
        if (bn_cmp(a, n_ext, L + 1) >= 0)
            bn_sub(a, n_ext, L + 1);
    }
    memcpy(out, a, (size_t)L * sizeof(uint32_t));
}

/* 256 big-endian bytes -> 64 limbs.  Each 4-byte group is big-endian: the
 * group's first byte is the most significant of that limb. */
static void bn_from_bytes_be(uint32_t *out, const uint8_t *b, int L) {
    for (int i = 0; i < L; i++) {
        const uint8_t *p = b + (L - 1 - i) * 4;
        out[i] = ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
                 ((uint32_t)p[2] << 8)  | (uint32_t)p[3];
    }
}

static void bn_to_bytes_be(uint8_t *b, const uint32_t *a, int L) {
    for (int i = 0; i < L; i++) {
        uint32_t v = a[i];
        uint8_t *p = b + (L - 1 - i) * 4;
        p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16);
        p[2] = (uint8_t)(v >> 8);  p[3] = (uint8_t)v;
    }
}

/* out = base^exp mod n, all bignums 64 limbs. */
static void bn_modexp(uint32_t *out, const uint32_t *base, const uint32_t *exp,
                      const uint32_t *n, uint32_t n0inv, int L) {
    static uint32_t R2[RSA_L], acc[RSA_L], b[RSA_L];
    static uint32_t one[RSA_L]; memset(one, 0, sizeof one); one[0] = 1;
    bn_pow2_mod_n(R2, n, L, 64 * L);         /* R^2 = 2^4096 mod n */
    bn_mont_mul(acc, one, R2, n, n0inv, L);  /* acc = to_mont(1) = R */
    bn_mont_mul(b, base, R2, n, n0inv, L);   /* b   = to_mont(base)  */
    int top = 32 * L - 1;
    while (top > 0 && (exp[top / 32] & (1u << (top % 32))) == 0) top--;
    for (int i = top; i >= 0; i--) {
        bn_mont_mul(acc, acc, acc, n, n0inv, L);
        if (exp[i / 32] & (1u << (i % 32)))
            bn_mont_mul(acc, acc, b, n, n0inv, L);
    }
    bn_mont_mul(acc, acc, one, n, n0inv, L); /* from_mont */
    memcpy(out, acc, (size_t)L * sizeof(uint32_t));
}

/* ---- EMSA-PKCS1-v1_5 (SHA-256) ---- */
static const uint8_t SHA256_DIGESTINFO[19] = {
    0x30,0x31,0x30,0x0d,0x06,0x09,0x60,0x86,0x48,0x01,0x65,0x03,0x04,0x02,0x01,
    0x05,0x00,0x04,0x20
};

int fw_rsa2048_verify_pkcs1(const uint32_t n[64], const uint32_t e[64],
                            const uint8_t *hash32, const uint8_t sig[256]) {
    if (!n || !e || !hash32 || !sig) return -1;
    static uint8_t em[RSA_BYTES];
    memset(em, 0xFF, sizeof em);
    em[0] = 0x00; em[1] = 0x01;
    em[2 + (RSA_BYTES - 3 - 51)] = 0x00;      /* separator before T */
    memcpy(em + (RSA_BYTES - 51), SHA256_DIGESTINFO, 19);
    memcpy(em + (RSA_BYTES - 32), hash32, 32);

    static uint32_t s[RSA_L], m[RSA_L];
    bn_from_bytes_be(s, sig, RSA_L);
    if (bn_cmp(s, n, RSA_L) >= 0) return -1;   /* s must be < n */
    uint32_t n0inv = mod_inv_n0(n[0]);
    bn_modexp(m, s, e, n, n0inv, RSA_L);

    static uint8_t got[RSA_BYTES];
    bn_to_bytes_be(got, m, RSA_L);
    for (int i = 0; i < RSA_L; i++) g_fw_rsa_last_m[i] = m[i];
    /* constant-time-ish compare */
    uint8_t diff = 0;
    for (int i = 0; i < RSA_BYTES; i++) diff |= (uint8_t)(em[i] ^ got[i]);
    return diff == 0 ? 0 : -1;
}

/* ---- boot-time self-test (fixed openssl-generated vector) ----
 * Generated with: openssl genrsa 2048; openssl dgst -sha256 -sign over
 * "WuBuOS Secure Boot RSA test message".  Verifies the Montgomery core and
 * the EMSA-PKCS1-v1_5 wrapper end to end at every boot. */
int fw_rsa_selftest(void) {
    static const uint32_t n[64] = {
        0x1bbea259u,0x0c14e9c8u,0x91866737u,0xeb10a236u,0xb43d1c8bu,0x33f6478au,0xf5ca22cfu,0xc5241916u,
        0x971cc040u,0xd7a7c2dau,0x4519d60eu,0xa27659eeu,0x88b13f43u,0xc394d40cu,0x58ee3871u,0x5703fbe1u,
        0xcb3e0cf2u,0xd31ba74bu,0x85543733u,0xed6da789u,0x9ad4afffu,0x1267750fu,0x37c2ffa6u,0x048805bcu,
        0x0f861187u,0xacce04b4u,0x71a8961du,0xe232475cu,0x9b7bbeadu,0xe0080ac0u,0x3f960b88u,0x8994b237u,
        0xc06bc056u,0xdccd4772u,0xe3c73789u,0x5586f5e6u,0x8964e76fu,0xd3a36c85u,0xbb0fda7cu,0x82596a3bu,
        0x0a94007bu,0x127cd029u,0x84bf8e30u,0x0872d29fu,0x75fbf7f9u,0x16ce8b21u,0x6cd79203u,0xd5a0f53cu,
        0x9d28c1e8u,0xa8dbb9cau,0x3cdbea65u,0xc367b5f9u,0x639f5442u,0x8a2d46ffu,0x9e266da0u,0x3548f2dau,
        0xdabcbf1du,0x6b0e5842u,0x7bcd88b2u,0x64afe3eeu,0xbab28c8eu,0x87480c8cu,0x987734e8u,0x84cd413eu,
    };
    static const uint32_t e[64] = { 0x00010001u };
    static const uint8_t sig[256] = {
        0x39,0xaa,0xab,0x19,0x6b,0x7b,0x14,0x72,0x62,0xda,0x6a,0x10,0x44,0x8c,0x21,0x2c,
        0x7f,0x9b,0xdb,0x27,0x97,0x3b,0xab,0x25,0xe4,0xac,0xe9,0xee,0x1b,0x3a,0x79,0xfd,
        0x68,0x02,0x70,0xf5,0xb3,0x9f,0xca,0x3a,0x26,0xc7,0x39,0x95,0x63,0x97,0xdc,0x30,
        0xe0,0x4a,0xd3,0xac,0x60,0xc7,0x9f,0x3b,0x39,0xec,0x04,0xbc,0x24,0x6d,0x04,0x6f,
        0x23,0xb8,0xbc,0xab,0x6b,0x22,0xc4,0x7b,0xdf,0xd9,0xa4,0x1d,0xea,0x74,0x1e,0x02,
        0x5c,0x2a,0xd1,0x39,0xd3,0xec,0x23,0x1e,0xe2,0x37,0xb1,0xe8,0xf7,0x85,0xf3,0x41,
        0xaa,0xc4,0x65,0xb3,0xa5,0xd1,0x33,0xb1,0x22,0xfb,0xf3,0xcf,0x15,0xd8,0xb7,0x23,
        0x6b,0x8b,0x13,0xbd,0xd0,0xd3,0xa0,0xea,0x72,0x2f,0x27,0x22,0xfc,0x1e,0x54,0x22,
        0x85,0xc1,0x5a,0xfd,0xa3,0x49,0x6b,0xdd,0xc0,0x84,0xfb,0x8a,0x59,0xb9,0xb0,0x2e,
        0x40,0x37,0x60,0x82,0x7e,0x90,0x87,0x0c,0xec,0x0b,0x40,0xb1,0xc0,0xb5,0x1c,0x59,
        0x1e,0x99,0x86,0x5a,0x34,0x84,0xf9,0x9c,0xc3,0xfd,0xe0,0xf4,0xf3,0x6f,0xe6,0xcf,
        0x2a,0x56,0x23,0xfa,0xa4,0x30,0xb0,0x56,0xc6,0xe0,0x6c,0xbf,0xaf,0x87,0xe7,0x5f,
        0x9f,0x93,0x81,0x3a,0x14,0x3b,0xf6,0xeb,0x5a,0x3e,0xe8,0xbb,0x6a,0x54,0x75,0xb1,
        0x1d,0x0d,0x6c,0x06,0x8e,0x74,0xdf,0xc5,0x09,0x4e,0x5f,0xfb,0x43,0xd5,0xa0,0xa8,
        0xf7,0xc6,0x0e,0xc9,0xa6,0xaa,0xbb,0xa7,0xc7,0xdb,0x1d,0xde,0xe1,0x8a,0x2c,0x3a,
        0x55,0xdc,0xd3,0xdb,0x13,0x20,0xca,0x08,0x09,0xe6,0xb2,0x4e,0xae,0xa7,0xa8,0xbf,
    };
    static const uint8_t hash[32] = {
        0x93,0x41,0x15,0x04,0xc7,0x54,0xde,0x70,0x5b,0x6f,0x84,0xb3,0xe2,0x31,0xa0,0xec,
        0x84,0x10,0xcb,0x5e,0xd0,0x2f,0xbb,0x86,0x41,0xad,0x2b,0xe4,0x1c,0xf3,0x64,0x3a,
    };
    int pass = 1;
    if (fw_rsa2048_verify_pkcs1(n, e, hash, sig) != 0) pass = 0;
    uint8_t bad[256]; memcpy(bad, sig, 256); bad[255] ^= 0x01;
    if (fw_rsa2048_verify_pkcs1(n, e, hash, bad) == 0) pass = 0;
    uint8_t wh[32]; memcpy(wh, hash, 32); wh[0] ^= 0x01;
    if (fw_rsa2048_verify_pkcs1(n, e, wh, sig) == 0) pass = 0;
    return pass ? 0 : -1;
}
