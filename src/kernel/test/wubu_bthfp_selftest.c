/*
 * wubu_bthfp_selftest.c -- verifies Bluetooth HSP/HFP routing.
 */
#include "wubu_bthfp.h"
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
    printf("=== wubu_bthfp_selftest ===\n");

    wubu_bthfp_probe();

    int p = wubu_bthfp_present();
    CHECK(p == 0 || p == 1, "bthfp present is boolean");

    /* Call state: sco + at = active, at only = ringing, none = idle, sco only = idle. */
    CHECK(wubu_bthfp_call_state(1, 1) == 1, "sco+at = active");
    CHECK(wubu_bthfp_call_state(0, 1) == 2, "at only = ringing");
    CHECK(wubu_bthfp_call_state(0, 0) == 0, "none = idle");
    CHECK(wubu_bthfp_call_state(1, 0) == 0, "sco without at = idle");

    /* State strings. */
    CHECK(strcmp(wubu_bthfp_state_str(0), "idle") == 0, "state 0 = idle");
    CHECK(strcmp(wubu_bthfp_state_str(1), "active") == 0, "state 1 = active");
    CHECK(strcmp(wubu_bthfp_state_str(2), "ringing") == 0, "state 2 = ringing");

    /* Summary builds. */
    char out[160] = "";
    wubu_bthfp_summary(out, sizeof(out));
    CHECK(strstr(out, "bthfp[") != NULL, "summary has bthfp fragment");

    printf("\n=== BTHFP TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
