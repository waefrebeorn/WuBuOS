/*
 * wubu_zoneappend_selftest.c -- verifies ZNS zone append routing.
 */
#include "wubu_zoneappend.h"
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
    printf("=== wubu_zoneappend_selftest ===\n");

    wubu_zoneappend_probe();

    int p = wubu_zoneappend_present();
    CHECK(p == 0 || p == 1, "zoneappend present is boolean");

    /* Zone append validity by state. */
    CHECK(wubu_zoneappend_ok(1, 0) == 1, "active zone + seq = ok");
    CHECK(wubu_zoneappend_ok(2, 10) == 1, "implicit_open + seq = ok");
    CHECK(wubu_zoneappend_ok(3, 5) == 1, "explicit_open + seq = ok");
    CHECK(wubu_zoneappend_ok(4, 0) == 0, "closed zone = not ok");
    CHECK(wubu_zoneappend_ok(6, 0) == 0, "full zone = not ok");
    CHECK(wubu_zoneappend_ok(1, -1) == 0, "negative seq = not ok");

    /* State strings. */
    CHECK(strcmp(wubu_zoneappend_state_str(0), "offline") == 0, "state 0 = offline");
    CHECK(strcmp(wubu_zoneappend_state_str(1), "active") == 0, "state 1 = active");
    CHECK(strcmp(wubu_zoneappend_state_str(6), "full") == 0, "state 6 = full");

    /* Summary builds. */
    char out[160] = "";
    wubu_zoneappend_summary(out, sizeof(out));
    CHECK(strstr(out, "zoneappend[") != NULL, "summary has zoneappend fragment");

    printf("\n=== ZONEAPPEND TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
