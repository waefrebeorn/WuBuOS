/*
 * wubu_gpudc_selftest.c -- verifies GPU display controller routing.
 */
#include "wubu_gpudc.h"
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
    printf("=== wubu_gpudc_selftest ===\n");

    wubu_gpudc_probe();

    int p = wubu_gpudc_present();
    CHECK(p == 0 || p == 1, "gpudc present is boolean");

    /* Display type routing. */
    CHECK(wubu_gpudc_type(0) == 0, "0 outputs = none");
    CHECK(wubu_gpudc_type(1) == 1, "1 output = single");
    CHECK(wubu_gpudc_type(3) == 2, "3 outputs = multi");
    CHECK(wubu_gpudc_type(5) == 3, "5 outputs = wide multi");

    /* Status strings. */
    CHECK(strcmp(wubu_gpudc_status_str(1), "connected") == 0, "1 = connected");
    CHECK(strcmp(wubu_gpudc_status_str(0), "disconnected") == 0, "0 = disconnected");
    CHECK(strcmp(wubu_gpudc_status_str(2), "multi") == 0, "2 = multi");

    /* Summary builds. */
    char out[160] = "";
    wubu_gpudc_summary(out, sizeof(out));
    CHECK(strstr(out, "gpudc[") != NULL, "summary has gpudc fragment");

    printf("\n=== GPUDC TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
