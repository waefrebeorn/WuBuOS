/*
 * wubu_nvidia_pascal_selftest.c -- verifies NVIDIA Pascal routing.
 */
#include "wubu_nvidia_pascal.h"
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
    printf("=== wubu_nvidia_pascal_selftest ===\n");

    wubu_nvidia_pascal_probe();

    int p = wubu_nvidia_pascal_present();
    CHECK(p == 0 || p == 1, "nvidia_pascal present is boolean");

    /* CUDA version support. */
    CHECK(wubu_nvidia_pascal_needs_535(1200) == 1, "CUDA 12.0 needs 535");
    CHECK(wubu_nvidia_pascal_needs_535(1300) == 1, "CUDA 13.x needs 590+");

    /* Max CUDA by driver. */
    CHECK(wubu_nvidia_pascal_max_cuda(470) == 0, "470 legacy = no CUDA 12");
    CHECK(wubu_nvidia_pascal_max_cuda(535) == 12, "535 = CUDA 12");

    /* Summary builds. */
    char out[160] = "";
    wubu_nvidia_pascal_summary(out, sizeof(out));
    CHECK(strstr(out, "nvidia_pascal[") != NULL, "summary has nvidia_pascal fragment");

    printf("\n=== NVIDIA_PASCAL TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
