/*
 * wubu_znszone_selftest.c -- verifies ZNS zone routing.
 */
#include "wubu_znszone.h"
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
    printf("=== wubu_znszone_selftest ===\n");

    wubu_znszone_probe();

    int p = wubu_znszone_present();
    CHECK(p == 0 || p == 1, "znszone present is boolean");

    /* Active zones = total - used. */
    CHECK(wubu_znszone_active(10, 3) == 7, "active zones = 7");
    CHECK(wubu_znszone_active(5, 5) == 0, "active zones = 0 (full)");
    CHECK(wubu_znszone_active(0, 0) == 0, "active zones = 0 (no total)");
    CHECK(wubu_znszone_active(5, -1) == 0, "active zones = 0 (invalid used)");

    /* State strings. */
    CHECK(strcmp(wubu_znszone_state_str(0), "offline") == 0, "state 0 = offline");
    CHECK(strcmp(wubu_znszone_state_str(1), "implicit_open") == 0, "state 1 = implicit_open");
    CHECK(strcmp(wubu_znszone_state_str(5), "full") == 0, "state 5 = full");

    /* Summary builds. */
    char out[160] = "";
    wubu_znszone_summary(out, sizeof(out));
    CHECK(strstr(out, "znszone[") != NULL, "summary has znszone fragment");

    printf("\n=== ZNSZONE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
