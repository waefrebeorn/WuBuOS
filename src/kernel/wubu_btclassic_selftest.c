/*
 * wubu_btclassic_selftest.c -- verifies Bluetooth classic routing.
 */
#include "wubu_btclassic.h"
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
    printf("=== wubu_btclassic_selftest ===\n");

    wubu_btclassic_probe();

    int p = wubu_btclassic_present();
    CHECK(p == 0 || p == 1, "btclassic present is boolean");

    /* SCO/eSCO rates. */
    CHECK(wubu_btclassic_rate(1, 1) == 128, "sco+esco = 128kHz");
    CHECK(wubu_btclassic_rate(1, 0) == 64, "sco only = 64kHz");
    CHECK(wubu_btclassic_rate(0, 0) == 0, "none = 0kHz");

    /* Profile strings. */
    CHECK(strcmp(wubu_btclassic_profile_str(0), "none") == 0, "profile 0 = none");
    CHECK(strcmp(wubu_btclassic_profile_str(1), "a2dp") == 0, "profile 1 = a2dp");
    CHECK(strcmp(wubu_btclassic_profile_str(3), "ftp") == 0, "profile 3 = ftp");

    /* Summary builds. */
    char out[160] = "";
    wubu_btclassic_summary(out, sizeof(out));
    CHECK(strstr(out, "btclassic[") != NULL, "summary has btclassic fragment");

    printf("\n=== BTLASSIC TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
