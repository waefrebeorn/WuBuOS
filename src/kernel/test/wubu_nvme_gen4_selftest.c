/*
 * wubu_nvme_gen4_selftest.c -- verifies NVMe Gen4 routing.
 */
#include "wubu_nvme_gen4.h"
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
    printf("=== wubu_nvme_gen4_selftest ===\n");

    wubu_nvme_gen4_probe();

    int p = wubu_nvme_gen4_present();
    CHECK(p == 0 || p == 1, "nvme_gen4 present is boolean");

    /* Speed calc. */
    CHECK(wubu_nvme_gen4_speed_gbps(4) == 64, "Gen4 x4 = 64 GT/s");
    CHECK(wubu_nvme_gen4_speed_gbps(1) == 16, "Gen4 x1 = 16 GT/s");

    /* Fast check. */
    CHECK(wubu_nvme_gen4_is_fast(64) == 1, "64 = fast");
    CHECK(wubu_nvme_gen4_is_fast(32) == 0, "32 = not fast");

    /* Summary builds. */
    char out[160] = "";
    wubu_nvme_gen4_summary(out, sizeof(out));
    CHECK(strstr(out, "nvme_gen4[") != NULL, "summary has nvme_gen4 fragment");

    printf("\n=== NVME_GEN4 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
