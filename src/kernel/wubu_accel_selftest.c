/*
 * wubu_accel_selftest.c -- verifies kernel-owned NPU/accelerator routing.
 */
#include "wubu_accel.h"
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
    printf("=== wubu_accel_selftest ===\n\n");

    wubu_hw_detect();
    wubu_accel_probe();

    printf("  present=%d npu=%d dsp=%d drv=%s\n",
           wubu_accel_present(), wubu_accel_has_npu(), wubu_accel_has_dsp(),
           wubu_accel_driver() ? wubu_accel_driver() : "none");

    /* NPU driver routing is always consistent. */
    CHECK(strcmp(wubu_accel_npu_driver(0x8086), "ivpu") == 0,
          "Intel -> ivpu");
    CHECK(strcmp(wubu_accel_npu_driver(0x1022), "amdxdna") == 0,
          "AMD -> amdxdna");
    CHECK(strcmp(wubu_accel_npu_driver(0x17CB), "qaic") == 0,
          "Qualcomm -> qaic");
    CHECK(strcmp(wubu_accel_npu_driver(0x1AE0), "edgetpu") == 0,
          "Google -> edgetpu");
    CHECK(strcmp(wubu_accel_npu_driver(0xFFFF), "accel") == 0,
          "unknown -> accel fallback");

    /* NPU present implies a driver is named. */
    CHECK(!wubu_accel_has_npu() || wubu_accel_driver() != NULL,
          "NPU present -> driver named");

    char s[256];
    wubu_accel_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "accel summary generated");

    printf("\n=== ACCEL TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
