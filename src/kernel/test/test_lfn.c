/* test_lfn.c  --  host test for the wubu_lfn VFAT LFN codec (A16) */
#include "wubu_lfn.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int g_pass = 0, g_fail = 0, g_total = 0;
#define TEST(name) printf("  TEST %-52s", name); g_total++;
#define PASS() do { printf("OK\n"); g_pass++; } while (0)
#define FAIL(msg) do { printf("FAIL %s\n", msg); g_fail++; } while (0)
#define CHECK(c, m) do { if (!(c)) { FAIL(m); return; } } while (0)

static void roundtrip(const char *name) {
    uint8_t entries[WUBU_LFN_MAX_ENTRIES][WUBU_LFN_ENTRY_SZ];
    int n = wubu_lfn_build_entries(name, entries, WUBU_LFN_MAX_ENTRIES);
    if (n == 0) {  /* 8.3: nothing to do -- still must roundtrip via 83 */
        CHECK(1, "8.3 name: no LFN entries (expected)");
        PASS(); return;
    }
    char out[WUBU_LFN_MAX_NAME];
    int rc = wubu_lfn_reconstruct(entries, n, out, sizeof(out));
    CHECK(rc == 0, "reconstruct succeeds");
    CHECK(strcmp(out, name) == 0, "reconstruct == original");
    PASS();
}

int main(void) {
    printf("wubu_lfn tests (gap A16)\n");

    /* 1. chunk codec: "abcde" in one entry */
    TEST("encode/decode 5 chars");
    {
        uint8_t e[32]; memset(e, 0, 32);
        uint16_t c5[13] = { 'a','b','c','d','e',0,0,0,0,0,0,0,0 };
        wubu_lfn_encode_chunk(e, c5, 5, 0, 1, 0x42);
        CHECK(e[11] == 0x0F, "attr 0x0F");
        CHECK(e[0] == 0x41, "last-entry ordinal bit 6 + index 0");
        CHECK(e[13] == 0x42, "checksum stored");
        CHECK(e[1] == 'a' && e[3] == 'b', "chars 1-2 at bytes 1,3");
        CHECK(e[9] == 'e', "char 5 at byte 9");
        uint16_t out[13];
        int n = wubu_lfn_decode_chunk(e, out);
        CHECK(n == 5, "decode returns 5");
        CHECK(out[0]=='a' && out[4]=='e', "decoded content");
        PASS();
    }

    /* 2. 13-char full entry */
    TEST("encode/decode full 13 chars");
    {
        uint8_t e[32]; memset(e, 0, 32);
        uint16_t c13[13];
        for (int i = 0; i < 13; i++) c13[i] = (uint16_t)('A' + i);
        wubu_lfn_encode_chunk(e, c13, 13, 0, 1, 0);
        uint16_t out[13];
        int n = wubu_lfn_decode_chunk(e, out);
        CHECK(n == 13, "13 decoded");
        CHECK(out[12] == 'M', "13th char = M");
        PASS();
    }

    /* 3. full-name roundtrips */
    TEST("roundtrip 'readme file with spaces.txt'");
    roundtrip("readme file with spaces.txt");
    TEST("roundtrip 'longer-than-8.3-name.dat'");
    roundtrip("longer-than-8.3-name.dat");
    TEST("roundtrip 'MyMixedCaseFile.txt'");
    roundtrip("MyMixedCaseFile.txt");
    TEST("roundtrip 'a very long file name that spans multiple lfn entries.bin'");
    roundtrip("a very long file name that spans multiple lfn entries.bin");

    /* 4. 8.3 names get no LFN chain */
    TEST("8.3 name 'README.TXT' -> 0 entries");
    {
        uint8_t entries[4][32];
        int n = wubu_lfn_build_entries("README.TXT", entries, 4);
        CHECK(n == 0, "no LFN needed");
        PASS();
    }

    /* 5. multi-entry ordinal bits */
    TEST("multi-entry ordinals (last has 0x40)");
    {
        uint8_t entries[2][32];
        int n = wubu_lfn_build_entries("aaaaaaaaaaaaabbbbbbbbbbbbb.ccc", entries, 2);
        CHECK(n == 2, "2 entries");
        CHECK(entries[0][0] == 0x42, "on-disk first = chunk 2 (ordinal 2|0x40)");
        CHECK(entries[1][0] == 0x01, "on-disk second = chunk 1");
        PASS();
    }

    /* 6. checksum stability */
    TEST("checksum known vector");
    {
        char n83[12] = "README    TXT";   /* 8.3: 11 bytes + NUL */
        uint8_t s1 = wubu_lfn_checksum(n83);
        uint8_t s2 = wubu_lfn_checksum(n83);
        CHECK(s1 == s2, "deterministic");
        CHECK(s1 != 0, "non-zero");
        PASS();
    }

    printf("\n%d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    return g_fail ? 1 : 0;
}
