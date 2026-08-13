/*
 * wubu_opencl_selftest.c -- verifies OpenCL runtime routing.
 */
#include "wubu_opencl.h"
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
    printf("=== wubu_opencl_selftest ===\n");

    wubu_opencl_probe();

    int p = wubu_opencl_present();
    CHECK(p == 0 || p == 1, "opencl present is boolean");

    /* ROCm. */
    CHECK(wubu_opencl_has_rocm(1) == 1, "ROCm available (AMD)");
    CHECK(wubu_opencl_has_rocm(0) == 0, "no ROCm");

    /* CUDA. */
    CHECK(wubu_opencl_has_cuda(1) == 1, "CUDA available (NVIDIA)");
    CHECK(wubu_opencl_has_cuda(0) == 0, "no CUDA");

    /* Summary builds. */
    char out[160] = "";
    wubu_opencl_summary(out, sizeof(out));
    CHECK(strstr(out, "opencl[") != NULL, "summary has opencl fragment");

    printf("\n=== OPENCL TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
