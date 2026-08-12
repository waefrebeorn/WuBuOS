/*
 * wubu_dsptrace_selftest.c -- verifies audio DSP trace routing.
 */
#include "wubu_dsptrace.h"
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
    printf("=== wubu_dsptrace_selftest ===\n");

    wubu_dsptrace_probe();

    int p = wubu_dsptrace_present();
    CHECK(p == 0 || p == 1, "dsptrace present is boolean");

    /* Level clamped 0-4. */
    CHECK(wubu_dsptrace_level(2) == 2, "level passes through");
    CHECK(wubu_dsptrace_level(-1) == 0, "level clamps negative");
    CHECK(wubu_dsptrace_level(9) == 4, "level clamps high to 4");

    /* Event codes. */
    CHECK(strcmp(wubu_dsptrace_evt(0), "ok") == 0, "evt 0 = ok");
    CHECK(strcmp(wubu_dsptrace_evt(1), "xrun") == 0, "evt 1 = xrun");
    CHECK(strcmp(wubu_dsptrace_evt(3), "firmware_error") == 0, "evt 3 = firmware_error");
    CHECK(strcmp(wubu_dsptrace_evt(99), "unknown") == 0, "evt 99 = unknown");

    /* Summary builds. */
    char out[160] = "";
    wubu_dsptrace_summary(out, sizeof(out));
    CHECK(strstr(out, "dsptrace[") != NULL, "summary has dsptrace fragment");

    printf("\n=== DSPTRACE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
