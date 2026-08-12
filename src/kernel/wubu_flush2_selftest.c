/*
 * wubu_flush2_selftest.c -- verifies storage flush/barrier routing.
 */
#include "wubu_flush2.h"
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
    printf("=== wubu_flush2_selftest ===\n\n");
    wubu_hw_detect();
    wubu_flush2_probe();
    printf("  fl=%d barrier=%d fsync=%d cache=%d flushcmd=%d\n",
           wubu_flush2_present(), wubu_flush2_barrier(), wubu_flush2_fsync(),
           wubu_flush2_cache(), wubu_flush2_flushcmd());

    CHECK(strcmp(wubu_flush2_type_for("barrier"), "barrier") == 0,
          "barrier -> barrier");
    CHECK(strcmp(wubu_flush2_type_for("fsync"), "fsync") == 0,
          "fsync -> fsync");
    CHECK(strcmp(wubu_flush2_type_for("data"), "fdatasync") == 0,
          "data -> fdatasync");
    CHECK(strcmp(wubu_flush2_type_for("fdatasync"), "fdatasync") == 0,
          "fdatasync -> fdatasync");
    CHECK(strcmp(wubu_flush2_type_for("cache"), "cache-flush") == 0,
          "cache -> cache-flush");
    CHECK(strcmp(wubu_flush2_type_for("write"), "write-barrier") == 0,
          "write -> write-barrier");
    CHECK(strcmp(wubu_flush2_type_for("zzz"), "barrier") == 0,
          "zzz -> barrier fallback");

    CHECK(strcmp(wubu_flush2_cmd_for("ATA"), "FLUSH CACHE") == 0,
          "ATA -> FLUSH CACHE");
    CHECK(strcmp(wubu_flush2_cmd_for("SCSI"), "SYNCHRONIZE CACHE") == 0,
          "SCSI -> SYNCHRONIZE CACHE");
    CHECK(strcmp(wubu_flush2_cmd_for("NVMe"), "FLUSH") == 0,
          "NVMe -> FLUSH");
    CHECK(strcmp(wubu_flush2_cmd_for("write"), "write barrier") == 0,
          "write -> write barrier");
    CHECK(strcmp(wubu_flush2_cmd_for("zzz"), "FLUSH CACHE") == 0,
          "zzz -> FLUSH CACHE fallback");

    char s[256];
    wubu_flush2_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "flush2 summary generated");

    printf("\n=== FLUSH2 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
