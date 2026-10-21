/*
 * test_wubu_crypto_sha512.c -- FIPS 180-4 known-answer tests for GOALS-1000 #778.
 *
 * Vectors are the NIST FIPS 180-4 Appendix B.1 example messages (the canonical
 * 448-bit alphabet-walk "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq",
 * "abc", the empty string, and one million 'a's) plus their multi-block
 * concatenation. Every expected digest was reproduced with stdlib hashlib and
 * cross-checked against the NIST CAVP/diagram test oracle before being
 * hardcoded here. A streaming-vs-one-shot consistency check (odd chunk sizes)
 * exercises the init/update/final path; the SHA-384 truncation is asserted
 * distinct from SHA-512 (different IV -> distinct digest, not mere truncation).
 */
#include <stdio.h>
#include <string.h>

#include "wubu_crypto_sha512.h"

static int checks = 0, fails = 0;
#define CHECK(cond, msg) do { checks++; \
    if (!(cond)) { fails++; printf("  FAIL %s\n", msg); } \
    else printf("  ok   %s\n", msg); } while (0)

/* hex-encode `n` bytes of `in` into a caller buffer (2*n+1). */
static void
hexenc(char *out, const uint8_t *in, size_t n)
{
    for (size_t i = 0; i < n; i++)
        sprintf(out + i * 2, "%02x", in[i]);
    out[n * 2] = '\0';
}

#define HEXCMP(out, sz, expect) \
    (memcmp((out), (expect), (sz)) == 0)

/* Decode a null-terminated lowercase hex string into `buf` (len = strlen/2). */
static int
hexbin(const char *hex, uint8_t *buf, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        unsigned hi, lo;
        char c = hex[i * 2], d = hex[i * 2 + 1];
        if (c >= '0' && c <= '9') hi = c - '0';
        else if (c >= 'a' && c <= 'f') hi = c - 'a' + 10;
        else return -1;
        if (d >= '0' && d <= '9') lo = d - '0';
        else if (d >= 'a' && d <= 'f') lo = d - 'a' + 10;
        else return -1;
        buf[i] = (uint8_t)((hi << 4) | lo);
    }
    return 0;
}

/* NIST FIPS 180-4 Appendix B.1 vectors, reproduced by stdlib hashlib. */
#define SHA512_EMPTY \
    "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e"
#define SHA512_ABC \
    "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f"
#define SHA512_CHAIN \
    "204a8fc6dda82f0a0ced7beb8e08a41657c16ef468b228a8279be331a703c33596fd15c13b1b07f9aa1d3bea57789ca031ad85c7a71dd70354ec631238ca3445"
#define SHA512_1M_A \
    "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973ebde0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b"
#define SHA512_CHAIN3 \
    "33dd4ac487bcba1462df408fd383ebd5bfb2bfa4e7dd3572d752887d1ae788525f53b12fd1fd9a6a61e6607823e9a30028be4845276bd0e93411b42d3d084cf5"
#define SHA384_EMPTY \
    "38b060a751ac96384cd9327eb1b1e36a21fdb71114be07434c0cc7bf63f6e1da274edebfe76f65fbd51ad2f14898b95b"
#define SHA384_ABC \
    "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7"
#define SHA384_CHAIN \
    "3391fdddfc8dc7393707a65b1b4709397cf8b1d162af05abfe8f450de5f36bc6b0455a8520bc4e6f5fe95b1fe3c8452b"
#define SHA384_1M_A \
    "9d0e1809716474cb086e834e310a4a1ced149e9c00f248527972cec5704c2a5b07b8b3dc38ecc4ebae97ddd87f3d8985"
#define SHA384_CHAIN3 \
    "d1ec278f9b7f2f8c2ca965b3950117d978e5a60d36253cc1addd76644199a081df40d41b893bdca795979108ab57ad7b"

/* Compare a `dsz`-byte digest against a `dsz`-byte hex expected value. */
#define CHECKDIGEST(name, fn, data, len, expect, dsz) do {            \
    uint8_t got[WUBU_SHA512_SZ];                                      \
    char    hex[(dsz) * 2 + 1];                                       \
    fn((data), (len), got);                                           \
    hexenc(hex, got, (dsz));                                          \
    CHECK(strcmp(hex, expect) == 0, name);                            \
} while (0)

int
main(void)
{
    uint8_t big[1000000];
    memset(big, 'a', sizeof(big));

    /* Canonical NIST FIPS 180-4 B.1 messages. */
    const char *chain   = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    const char *chain3  = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
                           "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"
                           "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

    printf("=== wubu_crypto_sha512_test (SHA-512/384 FIPS 180-4 KATs #778) ===\n");

    CHECKDIGEST("sha512 empty",    wubu_sha512, (const uint8_t *)"",      0,      SHA512_EMPTY,  WUBU_SHA512_SZ);
    CHECKDIGEST("sha512 abc",      wubu_sha512, (const uint8_t *)"abc",  3,      SHA512_ABC,    WUBU_SHA512_SZ);
    CHECKDIGEST("sha512 56-byte",  wubu_sha512, (const uint8_t *)chain,  56,     SHA512_CHAIN,  WUBU_SHA512_SZ);
    CHECKDIGEST("sha512 1MB a",    wubu_sha512, big,                     1000000, SHA512_1M_A,  WUBU_SHA512_SZ);
    CHECKDIGEST("sha512 chain*3",  wubu_sha512, (const uint8_t *)chain3, 168,    SHA512_CHAIN3,  WUBU_SHA512_SZ);

    CHECKDIGEST("sha384 empty",    wubu_sha384, (const uint8_t *)"",      0,      SHA384_EMPTY,  WUBU_SHA384_SZ);
    CHECKDIGEST("sha384 abc",      wubu_sha384, (const uint8_t *)"abc",  3,      SHA384_ABC,    WUBU_SHA384_SZ);
    CHECKDIGEST("sha384 56-byte",  wubu_sha384, (const uint8_t *)chain,  56,     SHA384_CHAIN,  WUBU_SHA384_SZ);
    CHECKDIGEST("sha384 1MB a",    wubu_sha384, big,                     1000000, SHA384_1M_A,  WUBU_SHA384_SZ);
    CHECKDIGEST("sha384 chain*3",  wubu_sha384, (const uint8_t *)chain3, 168,    SHA384_CHAIN3,  WUBU_SHA384_SZ);

    /* SHA-384 must NOT equal the first 48 bytes of SHA-512: different IV
     * means the digest diverges, not merely truncates. */
    {
        uint8_t h512[WUBU_SHA512_SZ], h384[WUBU_SHA384_SZ], head[WUBU_SHA384_SZ];
        wubu_sha512((const uint8_t *)"abc", 3, h512);
        wubu_sha384((const uint8_t *)"abc", 3, h384);
        memcpy(head, h512, WUBU_SHA384_SZ);
        CHECK(memcmp(h384, head, WUBU_SHA384_SZ) != 0,
              "sha384('abc') != first48(sha512('abc'))");
    }

    /* Streaming in oddly-sized chunks must reproduce one-shot. The context is
     * opaque (definition lives in the .c), so instantiate it through an
     * aligned byte buffer -- exactly how callers use a freestanding ctx. */
    {
        union { uint64_t align; uint8_t bytes[WUBU_SHA512_CTX_SZ]; } buf;
        struct wubu_sha512_ctx *c = (struct wubu_sha512_ctx *)buf.bytes;
        uint8_t chunked[WUBU_SHA512_SZ], once[WUBU_SHA512_SZ];
        const uint8_t *data = (const uint8_t *)chain3;
        size_t n = 168;
        wubu_sha512_init(c);
        wubu_sha512_update(c, data, 1);        /* 1  */
        wubu_sha512_update(c, data + 1, 7);    /* 7  */
        wubu_sha512_update(c, data + 8, n - 8);/* 160 */
        wubu_sha512_final(c, chunked);
        wubu_sha512(data, n, once);
        CHECK(memcmp(chunked, once, WUBU_SHA512_SZ) == 0,
              "streaming 1+7+160 == one-shot");
    }

    printf("=== wubu_crypto_sha512_test: %d checks, %d failures ===\n", checks, fails);
    if (fails == 0)
        printf("ALL TESTS PASSED\n");
    return fails ? 1 : 0;
}
