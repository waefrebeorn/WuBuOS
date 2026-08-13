/*
 * wubu_memmgr_selftest.c -- verifies GPU memory-manager routing.
 */
#include "wubu_memmgr.h"
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
    printf("=== wubu_memmgr_selftest ===\n\n");
    wubu_hw_detect();
    wubu_memmgr_probe();
    printf("  mm=%d gem=%d ttm=%d vram=%d gtt=%d\n",
           wubu_memmgr_present(), wubu_memmgr_gem(), wubu_memmgr_ttm(),
           wubu_memmgr_vram(), wubu_memmgr_gtt());

    CHECK(strcmp(wubu_memmgr_heap_for("vram"), "vram") == 0,
          "vram -> vram");
    CHECK(strcmp(wubu_memmgr_heap_for("gtt"), "gtt") == 0,
          "gtt -> gtt");
    CHECK(strcmp(wubu_memmgr_heap_for("shared"), "shared") == 0,
          "shared -> shared");
    CHECK(strcmp(wubu_memmgr_heap_for("shm"), "shared") == 0,
          "shm -> shared");
    CHECK(strcmp(wubu_memmgr_heap_for("stolen"), "stolen") == 0,
          "stolen -> stolen");
    CHECK(strcmp(wubu_memmgr_heap_for("zzz"), "vram") == 0,
          "zzz -> vram fallback");

    CHECK(strcmp(wubu_memmgr_type_for("gem"), "gem") == 0,
          "gem -> gem");
    CHECK(strcmp(wubu_memmgr_type_for("ttm"), "ttm") == 0,
          "ttm -> ttm");
    CHECK(strcmp(wubu_memmgr_type_for("bo"), "bo") == 0,
          "bo -> bo");
    CHECK(strcmp(wubu_memmgr_type_for("zzz"), "gem") == 0,
          "zzz -> gem fallback");

    char s[256];
    wubu_memmgr_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "memmgr summary generated");

    printf("\n=== MEMMGR TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
