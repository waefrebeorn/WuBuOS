/*
 * wubu_btbeacon_selftest.c -- verifies Bluetooth beacon routing.
 */
#include "wubu_btbeacon.h"
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
    printf("=== wubu_btbeacon_selftest ===\n");

    wubu_btbeacon_probe();

    int p = wubu_btbeacon_present();
    CHECK(p == 0 || p == 1, "btbeacon present is boolean");

    /* Proximity type. */
    CHECK(wubu_btbeacon_type(-50) == 0, "rssi-50 = immediate");
    CHECK(wubu_btbeacon_type(-75) == 1, "rssi-75 = near");
    CHECK(wubu_btbeacon_type(-90) == 2, "rssi-90 = far");

    /* UUID validation. */
    CHECK(wubu_btbeacon_valid_uuid("fda5") == 1, "iBeacon UUID valid");
    CHECK(wubu_btbeacon_valid_uuid("febe") == 1, "Eddystone UUID valid");
    CHECK(wubu_btbeacon_valid_uuid("0123") == 1, "AltBeacon UUID valid");
    CHECK(wubu_btbeacon_valid_uuid("xxxx") == 0, "unknown UUID invalid");
    CHECK(wubu_btbeacon_valid_uuid(NULL) == 0, "NULL UUID invalid");

    /* Summary builds. */
    char out[160] = "";
    wubu_btbeacon_summary(out, sizeof(out));
    CHECK(strstr(out, "btbeacon[") != NULL, "summary has btbeacon fragment");

    printf("\n=== BTBEACON TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
