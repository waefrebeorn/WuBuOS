/*
 * wubu_cuda_selftest.c -- verifies CUDA runtime routing.
 */
#include "wubu_cuda.h"
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
    printf("=== wubu_cuda_selftest ===\n");

    wubu_cuda_probe();

    int p = wubu_cuda_present();
    CHECK(p == 0 || p == 1, "cuda present is boolean");

    /* Compute capability. */
    CHECK(wubu_cuda_has_cuda_cc(1) == 1, "CUDA cc available");
    CHECK(wubu_cuda_has_cuda_cc(0) == 0, "no CUDA cc");

    /* Min CUDA version. */
    CHECK(wubu_cuda_min_version(89) == 1, "cc 8.9 = CUDA 11.8+");
    CHECK(wubu_cuda_min_version(75) == 0, "cc 7.5 = no CUDA 11.8");

    /* Summary builds. */
    char out[160] = "";
    wubu_cuda_summary(out, sizeof(out));
    CHECK(strstr(out, "cuda[") != NULL, "summary has cuda fragment");

    printf("\n=== CUDA TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
