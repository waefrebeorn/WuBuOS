/*
 * wubu_nvmepower_selftest.c -- verifies NVMe power state routing.
 */
#include "wubu_nvmepower.h"
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
    printf("=== wubu_nvmepower_selftest ===\n");

    wubu_nvmepower_probe();

    int p = wubu_nvmepower_present();
    CHECK(p == 0 || p == 1, "nvmepower present is boolean");

    /* Power state clamped 0-4. */
    CHECK(wubu_nvmepower_state(0) == 0, "PS0 passes through");
    CHECK(wubu_nvmepower_state(2) == 2, "PS2 passes through");
    CHECK(wubu_nvmepower_state(-1) == 0, "state clamps negative");
    CHECK(wubu_nvmepower_state(9) == 4, "state clamps high to PS4");

    /* APST latency clamped non-negative. */
    CHECK(wubu_nvmepower_apst_latency(50) == 50, "apst latency 50us");
    CHECK(wubu_nvmepower_apst_latency(-1) == 0, "apst latency clamps negative");

    /* Summary builds. */
    char out[160] = "";
    wubu_nvmepower_summary(out, sizeof(out));
    CHECK(strstr(out, "nvmepower[") != NULL, "summary has nvmepower fragment");

    printf("\n=== NVMEPOWER TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
