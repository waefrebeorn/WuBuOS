/*
 * wubu_btamesh_selftest.c -- verifies Bluetooth mesh routing.
 */
#include "wubu_btamesh.h"
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
    printf("=== wubu_btamesh_selftest ===\n");

    wubu_btamesh_probe();

    int p = wubu_btamesh_present();
    CHECK(p == 0 || p == 1, "btamesh present is boolean");

    /* Hop count clamping. */
    CHECK(wubu_btamesh_hops(3) == 3, "ttl 3 = 3 hops");
    CHECK(wubu_btamesh_hops(10) == 8, "ttl 10 = clamped to 8");
    CHECK(wubu_btamesh_hops(-1) == 0, "ttl -1 = 0 hops");

    /* Relay role detection. */
    CHECK(wubu_btamesh_is_relay(1) == 1, "role 1 = relay");
    CHECK(wubu_btamesh_is_relay(2) == 1, "role 2 = proxy");
    CHECK(wubu_btamesh_is_relay(3) == 0, "role 3 = friend, not relay");
    CHECK(wubu_btamesh_is_relay(0) == 0, "role 0 = none, not relay");

    /* Summary builds. */
    char out[160] = "";
    wubu_btamesh_summary(out, sizeof(out));
    CHECK(strstr(out, "btamesh[") != NULL, "summary has btamesh fragment");

    printf("\n=== BTAMESH TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
