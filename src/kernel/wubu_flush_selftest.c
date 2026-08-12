/*
 * wubu_flush_selftest.c -- verifies kernel-owned flush routing.
 */
#include "wubu_flush.h"
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
    printf("=== wubu_flush_selftest ===\n\n");

    wubu_hw_detect();
    wubu_flush_probe();

    printf("  flush=%d barrier=%d wbcache=%d fsync=%d nvme=%d\n",
           wubu_flush_supported(), wubu_flush_barrier(),
           wubu_flush_wbcache(), wubu_flush_fsync(),
           wubu_flush_nvme());

    /* Mode routing. */
    CHECK(strcmp(wubu_flush_mode_for("wb"), "write-back") == 0,
          "wb -> write-back");
    CHECK(strcmp(wubu_flush_mode_for("writeback"), "write-back") == 0,
          "writeback -> write-back");
    CHECK(strcmp(wubu_flush_mode_for("wt"), "write-through") == 0,
          "wt -> write-through");
    CHECK(strcmp(wubu_flush_mode_for("writethrough"), "write-through") == 0,
          "writethrough -> write-through");
    CHECK(strcmp(wubu_flush_mode_for("unknown"), "write-through") == 0,
          "unknown -> write-through fallback");

    /* Op routing. */
    CHECK(strcmp(wubu_flush_op_for("fsync"), "fsync") == 0,
          "fsync -> fsync");
    CHECK(strcmp(wubu_flush_op_for("fdatasync"), "fdatasync") == 0,
          "fdatasync -> fdatasync");
    CHECK(strcmp(wubu_flush_op_for("nvme"), "nvme-flush") == 0,
          "nvme -> nvme-flush");
    CHECK(strcmp(wubu_flush_op_for("barrier"), "write-barrier") == 0,
          "barrier -> write-barrier");
    CHECK(strcmp(wubu_flush_op_for("unknown"), "flush") == 0,
          "unknown -> flush fallback");

    char s[256];
    wubu_flush_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "flush summary generated");

    printf("\n=== FLUSH TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
