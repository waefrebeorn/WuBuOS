/*
 * test_sha256.c -- host test for wubu_sha256 (FIPS 180-4 vectors).
 */
#include "wubu_sha256.h"

#include <stdio.h>
#include <string.h>

static int hex_eq(const uint8_t *d, const char *hex)
{
    char buf[65];
    for (int i = 0; i < 32; i++)
        sprintf(buf + 2 * i, "%02x", d[i]);
    buf[64] = 0;
    return strcmp(buf, hex) == 0;
}

int main(void)
{
    int fails = 0;
    uint8_t out[WUBU_SHA256_SZ];

    /* FIPS 180-4 appendix B: the three standard vectors. */
    wubu_sha256("", 0, out);
    if (!hex_eq(out, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855")) {
        printf("FAIL: empty\n"); fails++;
    }
    wubu_sha256("abc", 3, out);
    if (!hex_eq(out, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad")) {
        printf("FAIL: abc\n"); fails++;
    }
    wubu_sha256("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", 56, out);
    if (!hex_eq(out, "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1")) {
        printf("FAIL: two-block\n"); fails++;
    }

    /* 1,000,000 x 'a' (multi-block streaming across the 64-byte
     * boundary + the 56..63-byte padding edge cases). */
    {
        uint8_t cbuf[WUBU_SHA256_CTX_SZ];
        wubu_sha256_ctx *c = (wubu_sha256_ctx *)cbuf;
        char chunk[1000];
        memset(chunk, 'a', sizeof(chunk));
        wubu_sha256_init(c);
        for (int i = 0; i < 1000; i++)
            wubu_sha256_update(c, chunk, sizeof(chunk));
        wubu_sha256_final(c, out);
        if (!hex_eq(out, "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0")) {
            printf("FAIL: 1M a\n"); fails++;
        }
    }

    /* Padding edge cases: lengths that leave the buffer at 55, 56, 57
     * after the 0x80 byte (the rem<8 branch). Cross-checked against the
     * empty+abc digests are not possible; instead verify determinism +
     * the streaming API equals the one-shot for a few odd lengths. */
    for (size_t n = 1; n <= 200; n++) {
        char buf[256];
        for (size_t i = 0; i < n; i++) buf[i] = (char)(i * 7 + 3);
        uint8_t a[32], b[32];
        wubu_sha256(buf, n, a);
        uint8_t cbuf[WUBU_SHA256_CTX_SZ];
        wubu_sha256_ctx *c = (wubu_sha256_ctx *)cbuf;
        wubu_sha256_init(c);
        wubu_sha256_update(c, buf, n / 2);
        wubu_sha256_update(c, buf + n / 2, n - n / 2);
        wubu_sha256_final(c, b);
        if (memcmp(a, b, 32) != 0) {
            printf("FAIL: stream mismatch n=%lu\n", (unsigned long)n);
            fails++;
            break;
        }
    }

    if (fails == 0) {
        printf("ALL SHA256 TESTS PASSED\n");
        return 0;
    }
    printf("SHA256 FAILURES: %d\n", fails);
    return 1;
}
