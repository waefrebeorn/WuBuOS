/*
 * wubu_auracast_selftest.c -- verifies Bluetooth Auracast routing.
 */
#include "wubu_auracast.h"
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
    printf("=== wubu_auracast_selftest ===\n");

    wubu_auracast_probe();

    int p = wubu_auracast_present();
    CHECK(p == 0 || p == 1, "auracast present is boolean");

    /* Stream count validation. */
    CHECK(wubu_auracast_streams(1) == 1, "1 stream = valid");
    CHECK(wubu_auracast_streams(8) == 1, "8 streams = valid");
    CHECK(wubu_auracast_streams(16) == 1, "16 streams = valid (max)");
    CHECK(wubu_auracast_streams(17) == 0, "17 streams = invalid");
    CHECK(wubu_auracast_streams(0) == 0, "0 streams = invalid");

    /* Broadcasting check. */
    CHECK(wubu_auracast_is_broadcasting(1, 1) == 1, "broadcaster+PA = broadcasting");
    CHECK(wubu_auracast_is_broadcasting(1, 0) == 0, "broadcaster only = not");
    CHECK(wubu_auracast_is_broadcasting(0, 1) == 0, "PA only = not");

    /* Summary builds. */
    char out[160] = "";
    wubu_auracast_summary(out, sizeof(out));
    CHECK(strstr(out, "auracast[") != NULL, "summary has auracast fragment");

    printf("\n=== AURACAST TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
