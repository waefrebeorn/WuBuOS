/*
 * wubu_nvme_gen5_selftest.c -- verifies NVMe Gen5 routing.
 */
#include "wubu_nvme_gen5.h"
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
    printf("=== wubu_nvme_gen5_selftest ===\n");

    wubu_nvme_gen5_probe();

    int p = wubu_nvme_gen5_present();
    CHECK(p == 0 || p == 1, "nvme_gen5 present is boolean");

    /* Speed calc. */
    CHECK(wubu_nvme_gen5_speed_gbps(5, 4) == 256, "Gen5 x4 = 256 GT/s");
    CHECK(wubu_nvme_gen5_speed_gbps(4, 4) == 128, "Gen4 x4 = 128 GT/s");
    CHECK(wubu_nvme_gen5_speed_gbps(3, 4) == 64, "Gen3 x4 = 64 GT/s");
    CHECK(wubu_nvme_gen5_speed_gbps(0, 4) == 0, "Gen0 = 0");

    /* Fast check. */
    CHECK(wubu_nvme_gen5_is_fast(256) == 1, "256 = fast");
    CHECK(wubu_nvme_gen5_is_fast(128) == 0, "128 = not fast");

    /* Summary builds. */
    char out[160] = "";
    wubu_nvme_gen5_summary(out, sizeof(out));
    CHECK(strstr(out, "nvme_gen5[") != NULL, "summary has nvme_gen5 fragment");

    printf("\n=== NVME_GEN5 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
