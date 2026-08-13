/*
 * wubu_zonefmt_selftest.c -- verifies ZNS zone reset routing.
 */
#include "wubu_zonefmt.h"
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
    printf("=== wubu_zonefmt_selftest ===\n");

    wubu_zonefmt_probe();

    int p = wubu_zonefmt_present();
    CHECK(p == 0 || p == 1, "zonefmt present is boolean");

    /* Zone reset validity: only offline (0) and full (6). */
    CHECK(wubu_zonefmt_reset_ok(0) == 1, "offline zone reset ok");
    CHECK(wubu_zonefmt_reset_ok(6) == 1, "full zone reset ok");
    CHECK(wubu_zonefmt_reset_ok(1) == 0, "active zone reset not ok");
    CHECK(wubu_zonefmt_reset_ok(4) == 0, "closed zone reset not ok");
    CHECK(wubu_zonefmt_reset_ok(-1) == 0, "invalid state reset not ok");

    /* State strings. */
    CHECK(strcmp(wubu_zonefmt_state_str(0), "offline") == 0, "state 0 = offline");
    CHECK(strcmp(wubu_zonefmt_state_str(6), "full") == 0, "state 6 = full");

    /* Summary builds. */
    char out[160] = "";
    wubu_zonefmt_summary(out, sizeof(out));
    CHECK(strstr(out, "zonefmt[") != NULL, "summary has zonefmt fragment");

    printf("\n=== ZONEFMT TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
