/*
 * wubu_gpufw_selftest.c -- verifies GPU firmware routing.
 */
#include "wubu_gpufw.h"
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
    printf("=== wubu_gpufw_selftest ===\n");

    wubu_gpufw_probe();

    int p = wubu_gpufw_present();
    CHECK(p == 0 || p == 1, "gpufw present is boolean");

    /* Vendor IDs. */
    CHECK(wubu_gpufw_match(0x10de) == 1, "nvda vendor matches");
    CHECK(wubu_gpufw_match(0x1002) == 1, "amd vendor matches");
    CHECK(wubu_gpufw_match(0x8086) == 1, "intel vendor matches");
    CHECK(wubu_gpufw_match(0x1af4) == 0, "qemu vendor does not match");

    /* Status strings. */
    CHECK(strcmp(wubu_gpufw_status(0), "nomatch") == 0, "status 0 = nomatch");
    CHECK(strcmp(wubu_gpufw_status(1), "ok") == 0, "status 1 = ok");
    CHECK(strcmp(wubu_gpufw_status(2), "mismatch") == 0, "status 2 = mismatch");

    /* Summary builds. */
    char out[160] = "";
    wubu_gpufw_summary(out, sizeof(out));
    CHECK(strstr(out, "gpufw[") != NULL, "summary has gpufw fragment");

    printf("\n=== GPUFW TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
