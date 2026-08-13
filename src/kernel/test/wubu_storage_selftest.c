/*
 * wubu_storage_selftest.c -- verifies kernel-owned storage driver routing.
 *
 * Tests the gaps closed:
 * 1. Storage topology detection (NVMe/SATA/IDE)
 * 2. Queue depth per drive type
 * 3. APST/queue kernel params generation
 * 4. Intel RST lockout detection + warning
 * 5. TRIM/discard config
 */
#include "wubu_storage.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); failures++; } \
    else { printf("  ok: %s\n", msg); passed++; } \
} while(0)

static int failures = 0;
static int passed = 0;

int main(void)
{
    printf("=== wubu_storage_selftest ===\n\n");

    /* Ensure hardware is detected first. */
    wubu_hw_detect();
    wubu_storage_probe();

    printf("  nvme      = %d\n", wubu_storage_has_nvme());
    printf("  sata      = %d\n", wubu_storage_has_sata());
    printf("  ide       = %d\n", wubu_storage_has_ide());
    printf("  rst       = %d\n", wubu_storage_has_raid_rst());
    printf("  queue_depth = %d\n", wubu_storage_queue_depth());
    printf("  path      = %s\n", wubu_storage_path() ? wubu_storage_path() : "(none)");

    /* 1. Storage present or WSL2 (host owns storage). */
    CHECK(wubu_storage_has_nvme() || wubu_storage_has_sata() ||
          wubu_storage_has_ide() || !wubu_hw_gpu_present() ||
          (wubu_hw_gpu_path() && strstr(wubu_hw_gpu_path(), "dxg")),
          "storage topology is valid (probed or WSL2-host-owned)");

    /* 2. Queue depth consistency: NVMe deep, SATA 32, IDE 1. */
    if (wubu_storage_has_nvme())
        CHECK(wubu_storage_queue_depth() >= 256,
              "NVMe queue depth is deep (>=256)");
    if (wubu_storage_has_sata())
        CHECK(wubu_storage_queue_depth() > 0 &&
              wubu_storage_queue_depth() <= 64,
              "SATA queue depth is moderate (NCQ ~32)");

    /* 3. Kernel params generation. */
    const char *kp = wubu_storage_kernel_params();
    printf("  kernel_params = %s\n", kp ? kp : "(empty)");
    if (wubu_storage_has_nvme()) {
        CHECK(strstr(kp, "nvme_core.default_ps_max_latency_us=0") != NULL,
              "APST disabled (no wake latency)");
        CHECK(strstr(kp, "io_queue_depth") != NULL,
              "NVMe queue depth set");
    }

    /* 4. Intel RST lockout warning. */
    const char *warn = wubu_storage_rst_warning();
    printf("  rst_warning = %s\n", warn ? "present" : "(none)");
    if (wubu_storage_has_raid_rst()) {
        CHECK(warn != NULL, "RST warning surfaced when RST mode detected");
    } else {
        CHECK(warn == NULL, "no RST warning when not in RST mode");
    }

    /* 5. TRIM config present for SSD (nvme/sata). */
    const char *trim = wubu_storage_trim_config();
    if (wubu_storage_has_nvme() || wubu_storage_has_sata()) {
        CHECK(trim != NULL && strstr(trim, "discard") != NULL,
              "TRIM/discard config generated for SSD");
    } else {
        CHECK(trim == NULL || trim[0] == '\0',
              "no TRIM config for non-SSD");
    }

    /* 6. Summary. */
    char sum[256] = "";
    wubu_storage_summary(sum, sizeof(sum));
    printf("  summary: %s\n", sum);
    CHECK(sum[0] != '\0', "storage summary string generated");

    printf("\n=== STORAGE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
