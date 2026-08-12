/*
 * wubu_compress_selftest.c -- verifies kernel-owned compression routing.
 */
#include "wubu_compress.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("  ok: %s\n", msg); passed++; } \
} while(0)

static int failures = 0;
static int passed = 0;

int main(void)
{
    printf("=== wubu_compress_selftest ===\n\n");

    wubu_hw_detect();
    wubu_compress_probe();

    printf("  compress=%d btrfs=%d zfs=%d zstd=%d lz4=%d\n",
           wubu_compress_present(), wubu_compress_btrfs(),
           wubu_compress_zfs(), wubu_compress_zstd(), wubu_compress_lz4());

    /* Algo routing. */
    CHECK(strcmp(wubu_compress_algo_for("zstd"), "zstd") == 0,
          "zstd -> zstd");
    CHECK(strcmp(wubu_compress_algo_for("lz4"), "lz4") == 0,
          "lz4 -> lz4");
    CHECK(strcmp(wubu_compress_algo_for("lzo"), "lzo") == 0,
          "lzo -> lzo");
    CHECK(strcmp(wubu_compress_algo_for("zlib"), "zlib") == 0,
          "zlib -> zlib");
    CHECK(strcmp(wubu_compress_algo_for("gzip"), "gzip") == 0,
          "gzip -> gzip");
    CHECK(strcmp(wubu_compress_algo_for("unknown"), "none") == 0,
          "unknown -> none fallback");

    /* Mode routing. */
    CHECK(strcmp(wubu_compress_mode_for("transparent"), "transparent") == 0,
          "transparent -> transparent");
    CHECK(strcmp(wubu_compress_mode_for("force"), "force") == 0,
          "force -> force");
    CHECK(strcmp(wubu_compress_mode_for("zlib"), "zlib") == 0,
          "zlib -> zlib");
    CHECK(strcmp(wubu_compress_mode_for("unknown"), "auto") == 0,
          "unknown -> auto fallback");

    char s[256];
    wubu_compress_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "compress summary generated");

    printf("\n=== COMPRESS TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
