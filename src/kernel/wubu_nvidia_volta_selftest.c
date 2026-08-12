/*
 * wubu_nvidia_volta_selftest.c -- verifies NVIDIA Volta routing.
 */
#include "wubu_nvidia_volta.h"
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
    printf("=== wubu_nvidia_volta_selftest ===\n");

    wubu_nvidia_volta_probe();

    int p = wubu_nvidia_volta_present();
    CHECK(p == 0 || p == 1, "nvidia_volta present is boolean");

    /* Datacenter check. */
    CHECK(wubu_nvidia_volta_is_datacenter(1) == 1, "dc V100 = datacenter");
    CHECK(wubu_nvidia_volta_is_datacenter(0) == 0, "non-dc = not datacenter");

    /* CUDA gencode. */
    CHECK(wubu_nvidia_volta_cuda_gencode(1200) == 1, "CUDA 12 sm_70 ok");
    CHECK(wubu_nvidia_volta_cuda_gencode(1400) == 0, "CUDA 14 = no sm_70");

    /* Summary builds. */
    char out[160] = "";
    wubu_nvidia_volta_summary(out, sizeof(out));
    CHECK(strstr(out, "nvidia_volta[") != NULL, "summary has nvidia_volta fragment");

    printf("\n=== NVIDIA_VOLTA TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
