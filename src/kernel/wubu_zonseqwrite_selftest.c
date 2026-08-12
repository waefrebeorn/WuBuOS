/*
 * wubu_zonseqwrite_selftest.c -- verifies ZNS sequential write routing.
 */
#include "wubu_zonseqwrite.h"
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
    printf("=== wubu_zonseqwrite_selftest ===\n");

    wubu_zonseqwrite_probe();

    int p = wubu_zonseqwrite_present();
    CHECK(p == 0 || p == 1, "zonseqwrite present is boolean");

    /* Sequential write validity: open zones only, within capacity. */
    CHECK(wubu_zonseqwrite_ok(1, 50, 30, 200) == 1, "active zone wp50+30<=200 ok");
    CHECK(wubu_zonseqwrite_ok(2, 180, 30, 200) == 0, "implicit wp180+30>200 not ok");
    CHECK(wubu_zonseqwrite_ok(0, 0, 10, 200) == 0, "offline zone not ok");
    CHECK(wubu_zonseqwrite_ok(6, 100, 10, 100) == 0, "full zone not ok");
    CHECK(wubu_zonseqwrite_ok(1, 0, 0, 200) == 0, "zero length not ok");

    /* State strings. */
    CHECK(strcmp(wubu_zonseqwrite_state_str(1), "active") == 0, "state 1 = active");
    CHECK(strcmp(wubu_zonseqwrite_state_str(6), "full") == 0, "state 6 = full");

    /* Summary builds. */
    char out[160] = "";
    wubu_zonseqwrite_summary(out, sizeof(out));
    CHECK(strstr(out, "zonseqwrite[") != NULL, "summary has zonseqwrite fragment");

    printf("\n=== ZONSEQWRITE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
