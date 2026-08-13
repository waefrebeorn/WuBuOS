/*
 * wubu_leaudioldr_selftest.c -- verifies Bluetooth LE Audio routing.
 */
#include "wubu_leaudioldr.h"
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
    printf("=== wubu_leaudioldr_selftest ===\n");

    wubu_leaudioldr_probe();

    int p = wubu_leaudioldr_present();
    CHECK(p == 0 || p == 1, "leaudioldr present is boolean");

    /* LC3 sample count. */
    CHECK(wubu_leaudioldr_samples(10000) == 80, "10ms frame = 80 samples");
    CHECK(wubu_leaudioldr_samples(7500) == 60, "7.5ms frame = 60 samples");

    /* Valid frame sizes. */
    CHECK(wubu_leaudioldr_is_valid_frame(48) == 1, "48 = valid");
    CHECK(wubu_leaudioldr_is_valid_frame(96) == 1, "96 = valid");
    CHECK(wubu_leaudioldr_is_valid_frame(50) == 0, "50 = invalid");
    CHECK(wubu_leaudioldr_is_valid_frame(0) == 0, "0 = invalid");

    /* Summary builds. */
    char out[160] = "";
    wubu_leaudioldr_summary(out, sizeof(out));
    CHECK(strstr(out, "leaudioldr[") != NULL, "summary has leaudioldr fragment");

    printf("\n=== LEAUDIOLDR TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
