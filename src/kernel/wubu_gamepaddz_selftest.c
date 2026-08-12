/*
 * wubu_gamepaddz_selftest.c -- verifies gamepad deadzone routing.
 */
#include "wubu_gamepaddz.h"
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
    printf("=== wubu_gamepaddz_selftest ===\n");

    wubu_gamepaddz_probe();

    int p = wubu_gamepaddz_present();
    CHECK(p == 0 || p == 1, "gamepaddz present is boolean");

    /* Deadzone filtering. */
    CHECK(wubu_gamepaddz_filter(0, 10) == 0, "0 within dz=10 -> 0");
    CHECK(wubu_gamepaddz_filter(5, 10) == 0, "5 within dz=10 -> 0");
    CHECK(wubu_gamepaddz_filter(-7, 10) == 0, "-7 within dz=10 -> 0");
    CHECK(wubu_gamepaddz_filter(15, 10) == 15, "15 outside dz=10 -> 15");
    CHECK(wubu_gamepaddz_filter(-20, 10) == -20, "-20 outside dz=10 -> -20");

    /* Drift detection. */
    CHECK(wubu_gamepaddz_is_drift(3, 10) == 1, "3 within dz=10 = drift");
    CHECK(wubu_gamepaddz_is_drift(12, 10) == 0, "12 outside dz=10 = no drift");

    /* Summary builds. */
    char out[160] = "";
    wubu_gamepaddz_summary(out, sizeof(out));
    CHECK(strstr(out, "gamepaddz[") != NULL, "summary has gamepaddz fragment");

    printf("\n=== GAMEPADDZ TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
