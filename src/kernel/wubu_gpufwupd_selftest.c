/*
 * wubu_gpufwupd_selftest.c -- verifies GPU firmware update routing.
 */
#include "wubu_gpufwupd.h"
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
    printf("=== wubu_gpufwupd_selftest ===\n");

    wubu_gpufwupd_probe();

    int p = wubu_gpufwupd_present();
    CHECK(p == 0 || p == 1, "gpufwupd present is boolean");

    /* Firmware version match. */
    CHECK(wubu_gpufwupd_match(1, 1) == 1, "matching firmware = 1");
    CHECK(wubu_gpufwupd_match(1, 2) == 0, "mismatched firmware = 0");
    CHECK(wubu_gpufwupd_match(-1, 1) == 0, "invalid current = 0");
    CHECK(wubu_gpufwupd_match(0, -1) == 0, "invalid expected = 0");

    /* Status strings. */
    CHECK(strcmp(wubu_gpufwupd_status(0), "ok") == 0, "status 0 = ok");
    CHECK(strcmp(wubu_gpufwupd_status(1), "stale") == 0, "status 1 = stale");
    CHECK(strcmp(wubu_gpufwupd_status(2), "mismatch") == 0, "status 2 = mismatch");
    CHECK(strcmp(wubu_gpufwupd_status(3), "flashing") == 0, "status 3 = flashing");

    /* Summary builds. */
    char out[160] = "";
    wubu_gpufwupd_summary(out, sizeof(out));
    CHECK(strstr(out, "gpufwupd[") != NULL, "summary has gpufwupd fragment");

    printf("\n=== GPUFWUPD TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
