/*
 * wubu_writeback_selftest.c -- verifies writeback routing.
 */
#include "wubu_writeback.h"
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
    printf("=== wubu_writeback_selftest ===\n\n");
    wubu_hw_detect();
    wubu_writeback_probe();
    printf("  wb=%d dirty=%d sync=%d interval=%d thread=%d\n",
           wubu_writeback_present(), wubu_writeback_dirty(), wubu_writeback_sync(),
           wubu_writeback_interval(), wubu_writeback_thread());

    CHECK(strcmp(wubu_writeback_mode_for("sync"), "sync") == 0,
          "sync -> sync");
    CHECK(strcmp(wubu_writeback_mode_for("async"), "async") == 0,
          "async -> async");
    CHECK(strcmp(wubu_writeback_mode_for("background"), "async") == 0,
          "background -> async");
    CHECK(strcmp(wubu_writeback_mode_for("periodic"), "periodic") == 0,
          "periodic -> periodic");
    CHECK(strcmp(wubu_writeback_mode_for("zzz"), "async") == 0,
          "zzz -> async fallback");

    CHECK(strcmp(wubu_writeback_thread_for("writeback"), "writeback/N") == 0,
          "writeback -> writeback/N");
    CHECK(strcmp(wubu_writeback_thread_for("flush"), "flush-") == 0,
          "flush -> flush-");
    CHECK(strcmp(wubu_writeback_thread_for("jbd"), "jbd") == 0,
          "jbd -> jbd");
    CHECK(strcmp(wubu_writeback_thread_for("ext4"), "ext4") == 0,
          "ext4 -> ext4");
    CHECK(strcmp(wubu_writeback_thread_for("zzz"), "writeback/N") == 0,
          "zzz -> writeback/N fallback");

    char s[256];
    wubu_writeback_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "writeback summary generated");

    printf("\n=== WRITEBACK TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
