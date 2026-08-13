/*
 * wubu_compute_selftest.c -- verifies kernel-owned graphics compute routing.
 */
#include "wubu_compute.h"
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
    printf("=== wubu_compute_selftest ===\n\n");

    wubu_hw_detect();
    wubu_compute_probe();

    printf("  opencl=%d vulkan=%d cuda=%d rusticl=%d\n",
           wubu_compute_opencl(), wubu_compute_vulkan(),
           wubu_compute_cuda(), wubu_compute_rusticl());

    /* Compute driver routing is always consistent. */
    CHECK(strcmp(wubu_compute_driver_for("amd"), "rusticl") == 0,
          "amd -> rusticl");
    CHECK(strcmp(wubu_compute_driver_for("intel"), "rusticl") == 0,
          "intel -> rusticl");
    CHECK(strcmp(wubu_compute_driver_for("nvidia"), "cuda") == 0,
          "nvidia -> cuda");
    CHECK(strcmp(wubu_compute_driver_for("zlu"), "zlu") == 0,
          "zlu -> zlu");
    CHECK(strcmp(wubu_compute_driver_for("unknown"), "pocl") == 0,
          "unknown -> pocl fallback");

    /* Present iff any compute engine. */
    CHECK(wubu_compute_present() == (wubu_compute_opencl() || wubu_compute_vulkan() || wubu_compute_cuda()),
          "present == (any compute)");

    char s[256];
    wubu_compute_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "compute summary generated");

    printf("\n=== COMPUTE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
