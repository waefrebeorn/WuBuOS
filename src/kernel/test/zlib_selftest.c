/*
 * zlib_selftest.c -- verify the kernel DEFLATE inflater (wubu_inflate)
 * byte-for-byte against the host zlib as an oracle.
 *
 * Build (from repo root):
 *   gcc -std=c11 -Wall -Wextra -O2 -Isrc/kernel \
 *       src/kernel/wubu_inflate.c src/kernel/libc_string.c src/kernel/zlib_selftest.c \
 *       -o /tmp/zlib_selftest -lz
 *
 * Run: /tmp/zlib_selftest
 *
 * (libz is used ONLY as an independent check on the output bytes; the
 *  kernel code links against nothing but its own sources.)
 */
#include "wubu_zlib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <zlib.h>   /* host oracle only */

static int fail = 0;

static void check(const char *label, const uint8_t *src, uint32_t slen,
                  const uint8_t *expect, uint32_t elen) {
    uint8_t *out = malloc(elen ? elen : 1);
    uint32_t got = 0, consumed = 0;
    int rc = wubu_inflate(src, slen, out, elen, &got, &consumed);
    if (rc != 0) { printf("  FAIL %s: wubu_inflate rc=%d\n", label, rc); fail = 1; free(out); return; }
    if (got != elen || memcmp(out, expect, elen) != 0) {
        printf("  FAIL %s: len %u!=%u or bytes differ\n", label, got, elen);
        fail = 1;
    } else {
        printf("  ok   %s: %u bytes match zlib\n", label, got);
    }
    free(out);
}

int main(void) {
    printf("=== wubu_inflate selftest (oracle: host zlib) ===\n");

    const char *payloads[] = {
        "Hello, WuBuOS kernel!",
        "LZMA is NOT what Halo PC uses; LZX is. But DEFLATE is universal.",
        "The quick brown fox jumps over the lazy dog. The quick brown fox jumps over the lazy dog.",
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA",
        "0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF0123456789ABCDEF",
    };
    for (int i = 0; i < 5; i++) {
        const char *t = payloads[i];
        uint32_t len = (uint32_t)strlen(t);
        uint8_t comp[512]; uLongf clen = sizeof(comp);
        if (compress2(comp, &clen, (const Bytef*)t, len, 9) != Z_OK) {
            printf("  FAIL payload %d: host compress failed\n", i); fail = 1; continue;
        }
        char lbl[32]; snprintf(lbl, sizeof lbl, "payload[%d] len=%u", i, len);
        check(lbl, comp, (uint32_t)clen, (const uint8_t*)t, len);
    }

    if (!fail) printf("=== ALL ZLIB SELFTESTS PASSED ===\n");
    return fail ? 1 : 0;
}
