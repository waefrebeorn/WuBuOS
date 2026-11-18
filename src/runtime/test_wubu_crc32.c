/*
 * test_wubu_crc32.c -- prove the single CRC-32 owner against known vectors.
 *
 * Standard reflected CRC-32 (IEEE 802.3, check value 0xCBF43926 for the ASCII
 * string "123456789"). Also proves that the incremental/chained forms agree
 * with the one-shot form, which is what the chunked writers depend on.
 */
#include "wubu_crc32.h"

#include <stdio.h>
#include <string.h>

static int g_fails;
#define CHECK(c) do { if (!(c)) { printf("  FAIL line %d: %s\n", __LINE__, #c); g_fails++; } } while (0)

int main(void)
{
    /* known-answer vectors */
    CHECK(wubu_crc32("", 0) == 0x00000000u);
    CHECK(wubu_crc32("a", 1) == 0xE8B7BE43u);
    CHECK(wubu_crc32("abc", 3) == 0x352441C2u);
    CHECK(wubu_crc32("123456789", 9) == 0xCBF43926u);
    CHECK(wubu_crc32("The quick brown fox jumps over the lazy dog", 43) == 0x414FA339u);

    /* NULL/empty safety: zero length over a null pointer must be the init value */
    CHECK(wubu_crc32(NULL, 0) == 0x00000000u);

    /* run() returns a RAW state: finish it and compare with one-shot */
    uint32_t st = wubu_crc32_run(WUBU_CRC32_INIT_STATE, "abc", 3);
    CHECK(st != wubu_crc32("abc", 3));          /* raw, not inverted */
    CHECK(~st == wubu_crc32("abc", 3));

    /* chaining over two halves equals one-shot over the whole buffer */
    const char *all = "Hello, WuBuOS";
    size_t n = strlen(all);
    for (size_t cut = 0; cut <= n; cut++) {
        uint32_t a = wubu_crc32(all, cut);
        uint32_t b = wubu_crc32_chain(a, all + cut, n - cut);
        CHECK(b == wubu_crc32(all, n));
    }

    /* running-state form agrees at every split too */
    for (size_t cut = 0; cut <= n; cut++) {
        uint32_t s = wubu_crc32_run(WUBU_CRC32_INIT_STATE, all, cut);
        s = wubu_crc32_run(s, all + cut, n - cut);
        CHECK(~s == wubu_crc32(all, n));
    }

    /* determinism */
    CHECK(wubu_crc32(all, n) == wubu_crc32(all, n));

    if (g_fails) {
        printf("wubu_crc32: %d FAILED\n", g_fails);
        return 1;
    }
    printf("ALL CRC32 TESTS PASSED\n");
    return 0;
}
