/*
 * wubu_gpushader_selftest.c -- verifies GPU shader model routing.
 */
#include "wubu_gpushader.h"
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
    printf("=== wubu_gpushader_selftest ===\n");

    wubu_gpushader_probe();

    int p = wubu_gpushader_present();
    CHECK(p == 0 || p == 1, "gpushader present is boolean");

    /* Shader model levels. */
    CHECK(wubu_gpushader_model_int(5, 0) == 1, "SM 5_0 = baseline");
    CHECK(wubu_gpushader_model_int(6, 0) == 2, "SM 6_0 = advanced");
    CHECK(wubu_gpushader_model_int(6, 7) == 2, "SM 6_7 = advanced");
    CHECK(wubu_gpushader_model_int(4, 0) == 0, "SM 4_0 = legacy");
    CHECK(wubu_gpushader_model_int(0, 0) == 0, "SM 0_0 = legacy");

    /* Model strings. */
    CHECK(strcmp(wubu_gpushader_model_str(0), "legacy") == 0, "level 0 = legacy");
    CHECK(strcmp(wubu_gpushader_model_str(1), "baseline") == 0, "level 1 = baseline");
    CHECK(strcmp(wubu_gpushader_model_str(2), "advanced") == 0, "level 2 = advanced");

    /* Summary builds. */
    char out[160] = "";
    wubu_gpushader_summary(out, sizeof(out));
    CHECK(strstr(out, "gpushader[") != NULL, "summary has gpushader fragment");

    printf("\n=== GPUSHADER TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
