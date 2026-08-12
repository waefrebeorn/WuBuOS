/*
 * wubu_fencesync_selftest.c -- verifies GPU fence sync routing.
 */
#include "wubu_fencesync.h"
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
    printf("=== wubu_fencesync_selftest ===\n\n");
    wubu_hw_detect();
    wubu_fencesync_probe();
    printf("  fence=%d timeline=%d wait=%d timeout=%d signal=%d\n",
           wubu_fencesync_present(), wubu_fencesync_timeline(), wubu_fencesync_wait(),
           wubu_fencesync_timeout(), wubu_fencesync_signal());

    CHECK(strcmp(wubu_fencesync_op_for("wait"), "wait") == 0,
          "wait -> wait");
    CHECK(strcmp(wubu_fencesync_op_for("signal"), "signal") == 0,
          "signal -> signal");
    CHECK(strcmp(wubu_fencesync_op_for("timeline"), "timeline") == 0,
          "timeline -> timeline");
    CHECK(strcmp(wubu_fencesync_op_for("timeout"), "timeout") == 0,
          "timeout -> timeout");
    CHECK(strcmp(wubu_fencesync_op_for("zzz"), "wait") == 0,
          "zzz -> wait fallback");

    CHECK(strcmp(wubu_fencesync_type_for("dma"), "dma-fence") == 0,
          "dma -> dma-fence");
    CHECK(strcmp(wubu_fencesync_type_for("sync-fd"), "sync-fd") == 0,
          "sync-fd -> sync-fd");
    CHECK(strcmp(wubu_fencesync_type_for("fd"), "sync-fd") == 0,
          "fd -> sync-fd");
    CHECK(strcmp(wubu_fencesync_type_for("seqno"), "seqno") == 0,
          "seqno -> seqno");
    CHECK(strcmp(wubu_fencesync_type_for("sdma"), "sdma-fence") == 0,
          "sdma -> sdma-fence");
    CHECK(strcmp(wubu_fencesync_type_for("zzz"), "dma-fence") == 0,
          "zzz -> dma-fence fallback");

    char s[256];
    wubu_fencesync_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "fencesync summary generated");

    printf("\n=== FENCESYNC TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
