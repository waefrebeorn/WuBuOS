/*
 * wubu_gpucsched_selftest.c -- verifies GPU compute scheduler routing.
 */
#include "wubu_gpucsched.h"
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
    printf("=== wubu_gpucsched_selftest ===\n");

    wubu_gpucsched_probe();

    /* Present always returns valid (host owns GPU on WSL2). */
    int p = wubu_gpucsched_present();
    CHECK(p == 0 || p == 1, "gpucsched present is boolean");

    /* Priority is clamped non-negative. */
    CHECK(wubu_gpucsched_priority(5) == 5, "priority passes through");
    CHECK(wubu_gpucsched_priority(-1) == 0, "priority clamps negative");

    /* Timeslice grows with queue count. */
    CHECK(wubu_gpucsched_timeslice_ms(0) == 8, "timeslice base = 8ms");
    CHECK(wubu_gpucsched_timeslice_ms(2) == 16, "timeslice q2 = 16ms");
    CHECK(wubu_gpucsched_timeslice_ms(-1) == 0, "timeslice clamps negative");

    /* Summary builds. */
    char out[160] = "";
    wubu_gpucsched_summary(out, sizeof(out));
    CHECK(strstr(out, "gpucsched[") != NULL, "summary has gpucsched fragment");
    CHECK(strstr(out, "queue=") != NULL, "summary has queue field");

    printf("\n=== GPUCSCHED TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
