/*
 * wubu_zonecap_selftest.c -- verifies ZNS zone capacity routing.
 */
#include "wubu_zonecap.h"
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
    printf("=== wubu_zonecap_selftest ===\n");

    wubu_zonecap_probe();

    int p = wubu_zonecap_present();
    CHECK(p == 0 || p == 1, "zonecap present is boolean");

    /* Zone capacity safety: wp+len <= cap. */
    CHECK(wubu_zonecap_safe(0, 100, 50) == 1, "wp 0+50 <= 100");
    CHECK(wubu_zonecap_safe(80, 100, 30) == 0, "wp 80+30 > 100");
    CHECK(wubu_zonecap_safe(0, 100, 0) == 0, "zero length not safe");

    /* Zone full: wp >= cap. */
    CHECK(wubu_zonecap_full(100, 100) == 1, "wp 100 >= cap 100 = full");
    CHECK(wubu_zonecap_full(50, 100) == 0, "wp 50 < cap 100 = not full");

    /* Summary builds. */
    char out[160] = "";
    wubu_zonecap_summary(out, sizeof(out));
    CHECK(strstr(out, "zonecap[") != NULL, "summary has zonecap fragment");

    printf("\n=== ZONECAP TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
