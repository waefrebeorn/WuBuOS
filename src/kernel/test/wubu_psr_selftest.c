/*
 * wubu_psr_selftest.c -- verifies kernel-owned PSR/SR-IOV routing.
 */
#include "wubu_psr.h"
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
    printf("=== wubu_psr_selftest ===\n\n");

    wubu_hw_detect();
    wubu_psr_probe();

    printf("  psr=%d sriov=%d vf=%d vfs=%d\n",
           wubu_psr_supported(), wubu_psr_sriov(), wubu_psr_vf(),
           wubu_psr_num_vfs());

    /* PSR routing. */
    CHECK(strcmp(wubu_psr_driver_for("i915"), "i915-psr") == 0,
          "i915 -> i915-psr");
    CHECK(strcmp(wubu_psr_driver_for("amdgpu"), "amdgpu-psr") == 0,
          "amdgpu -> amdgpu-psr");
    CHECK(strcmp(wubu_psr_driver_for("xe"), "xe-psr") == 0,
          "xe -> xe-psr");
    CHECK(strcmp(wubu_psr_driver_for("unknown"), "drm-psr") == 0,
          "unknown -> drm-psr fallback");

    /* SR-IOV routing. */
    CHECK(strcmp(wubu_psr_sriov_for("ixgbe"), "ixgbe") == 0,
          "ixgbe -> ixgbe");
    CHECK(strcmp(wubu_psr_sriov_for("i40e"), "i40e") == 0,
          "i40e -> i40e");
    CHECK(strcmp(wubu_psr_sriov_for("ice"), "ice") == 0,
          "ice -> ice");
    CHECK(strcmp(wubu_psr_sriov_for("mlx5"), "mlx5") == 0,
          "mlx5 -> mlx5");
    CHECK(strcmp(wubu_psr_sriov_for("unknown"), "sriov") == 0,
          "unknown -> sriov fallback");

    char s[256];
    wubu_psr_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "psr summary generated");

    printf("\n=== PSR TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
