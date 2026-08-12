/*
 * wubu_intelgpu_selftest.c -- verifies Intel GPU routing.
 */
#include "wubu_intelgpu.h"
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
    printf("=== wubu_intelgpu_selftest ===\n");

    wubu_intelgpu_probe();

    int p = wubu_intelgpu_present();
    CHECK(p == 0 || p == 1, "intelgpu present is boolean");

    /* Driver routing by generation. */
    CHECK(wubu_intelgpu_driver(7) == 0, "Gen7 = legacy i915");
    CHECK(wubu_intelgpu_driver(8) == 1, "Gen8 = i915/iris");
    CHECK(wubu_intelgpu_driver(10) == 2, "Gen10 = i915/iris + xe");
    CHECK(wubu_intelgpu_driver(12) == 3, "Gen12 = xe/anv");
    CHECK(wubu_intelgpu_driver(20) == 3, "Gen20 = Arc");

    /* Firmware requirement. */
    CHECK(wubu_intelgpu_needs_firmware(11) == 0, "Gen11 = no firmware");
    CHECK(wubu_intelgpu_needs_firmware(12) == 1, "Gen12 = firmware needed");
    CHECK(wubu_intelgpu_needs_firmware(15) == 1, "Gen15 = firmware needed");

    /* Summary builds. */
    char out[160] = "";
    wubu_intelgpu_summary(out, sizeof(out));
    CHECK(strstr(out, "intelgpu[") != NULL, "summary has intelgpu fragment");

    printf("\n=== INTELGPU TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
