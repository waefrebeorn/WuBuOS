/*
 * wubu_xe3_selftest.c -- verifies Intel Xe3 (Celestial) routing.
 */
#include "wubu_xe3.h"
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
    printf("=== wubu_xe3_selftest ===\n");

    wubu_xe3_probe();

    int p = wubu_xe3_present();
    CHECK(p == 0 || p == 1, "xe3 present is boolean");

    /* xe driver routing. */
    CHECK(wubu_xe3_uses_xe_driver(1) == 1, "xe available = uses xe driver");
    CHECK(wubu_xe3_uses_xe_driver(0) == 0, "no xe = not used");

    /* Mesa Iris/ANV. */
    CHECK(wubu_xe3_uses_iris_anv(1) == 1, "mesa ready = uses iris/anv");
    CHECK(wubu_xe3_uses_iris_anv(0) == 0, "mesa not ready = no iris/anv");

    /* Summary builds. */
    char out[160] = "";
    wubu_xe3_summary(out, sizeof(out));
    CHECK(strstr(out, "xe3[") != NULL, "summary has xe3 fragment");

    printf("\n=== XE3 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
