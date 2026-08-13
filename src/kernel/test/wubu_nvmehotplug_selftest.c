/*
 * wubu_nvmehotplug_selftest.c -- verifies NVMe hotplug routing.
 */
#include "wubu_nvmehotplug.h"
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
    printf("=== wubu_nvmehotplug_selftest ===\n");

    wubu_nvmehotplug_probe();

    int p = wubu_nvmehotplug_present();
    CHECK(p == 0 || p == 1, "nvmehotplug present is boolean");

    /* Hotplug stability. */
    CHECK(wubu_nvmehotplug_stable(0, 1000) == 1, "0 events in 1000ms = stable");
    CHECK(wubu_nvmehotplug_stable(2, 1000) == 1, "2 events in 1000ms = stable");
    CHECK(wubu_nvmehotplug_stable(15, 1000) == 0, "15 events in 1000ms = unstable");
    CHECK(wubu_nvmehotplug_stable(0, 0) == 0, "zero interval = not stable");

    /* Event detection. */
    CHECK(wubu_nvmehotplug_is_event(1, 2) == 1, "1->2 = event");
    CHECK(wubu_nvmehotplug_is_event(2, 2) == 0, "2->2 = no event");

    /* Summary builds. */
    char out[160] = "";
    wubu_nvmehotplug_summary(out, sizeof(out));
    CHECK(strstr(out, "nvmehotplug[") != NULL, "summary has nvmehotplug fragment");

    printf("\n=== NVMEHOTPLUG TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
