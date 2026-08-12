/*
 * wubu_bcache_selftest.c -- verifies storage bcache routing.
 */
#include "wubu_bcache.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } \
    else { passes++; } \
} while (0)

int main(void)
{
    int passes = 0, fails = 0;
    wubu_bcache_probe();

    CHECK(wubu_bcache_present() >= 0, "bcache_present returns non-negative");

    CHECK(strcmp(wubu_bcache_mode_for("writeback"), "writeback") == 0, "mode writeback");
    CHECK(strcmp(wubu_bcache_mode_for("write_through"), "writethrough") == 0, "mode writethrough");
    CHECK(strcmp(wubu_bcache_mode_for("writearound"), "writearound") == 0, "mode writearound");
    CHECK(strcmp(wubu_bcache_mode_for("none"), "none") == 0, "mode none");
    CHECK(strcmp(wubu_bcache_mode_for("default"), "unknown") == 0, "mode unknown");
    CHECK(wubu_bcache_mode_for(NULL) == NULL, "mode null passthrough");

    CHECK(wubu_bcache_hit_pct(80, 20) == 80, "hit pct 80/100");
    CHECK(wubu_bcache_hit_pct(0, 100) == 0, "hit pct 0/100");
    CHECK(wubu_bcache_hit_pct(100, 0) == 100, "hit pct 100/0");
    CHECK(wubu_bcache_hit_pct(0, 0) == 0, "hit pct 0/0");

    char buf[256];
    wubu_bcache_summary(buf, sizeof(buf));
    CHECK(strstr(buf, "bcache[") != NULL, "summary has bcache header");
    CHECK(strstr(buf, "mode=") != NULL, "summary has mode");

    printf("=== BCACHE TESTS: %d passed, %d failed ===\n", passes, fails);
    return fails > 0 ? 1 : 0;
}
