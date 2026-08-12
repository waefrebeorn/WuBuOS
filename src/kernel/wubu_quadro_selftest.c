/*
 * wubu_quadro_selftest.c -- verifies NVIDIA Quadro routing.
 */
#include "wubu_quadro.h"
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
    printf("=== wubu_quadro_selftest ===\n");

    wubu_quadro_probe();

    int p = wubu_quadro_present();
    CHECK(p == 0 || p == 1, "quadro present is boolean");

    /* Professional. */
    CHECK(wubu_quadro_is_professional(1) == 1, "Quadro = professional");
    CHECK(wubu_quadro_is_professional(0) == 0, "GeForce = not professional");

    /* ISV. */
    CHECK(wubu_quadro_has_isv(1) == 1, "ISV certified");
    CHECK(wubu_quadro_has_isv(0) == 0, "no ISV");

    /* Summary builds. */
    char out[160] = "";
    wubu_quadro_summary(out, sizeof(out));
    CHECK(strstr(out, "quadro[") != NULL, "summary has quadro fragment");

    printf("\n=== QUADRO TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
