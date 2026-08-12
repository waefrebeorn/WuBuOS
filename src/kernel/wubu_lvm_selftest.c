/*
 * wubu_lvm_selftest.c -- verifies storage LVM routing.
 */
#include "wubu_lvm.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } \
    else { passes++; } \
} while (0)

int main(void)
{
    int passes = 0, fails = 0;
    wubu_lvm_probe();

    CHECK(wubu_lvm_present() >= 0, "lvm_present returns non-negative");

    CHECK(strcmp(wubu_lvm_uuid_for("lvm"), "LVM") == 0, "uuid lvm by name");
    CHECK(strcmp(wubu_lvm_uuid_for("dm-0"), "LVM") == 0, "uuid dm-0");
    CHECK(strcmp(wubu_lvm_uuid_for("sda1"), "unknown") == 0, "uuid unknown");
    CHECK(wubu_lvm_uuid_for(NULL) == NULL, "uuid null passthrough");

    CHECK(wubu_lvm_health(1000, 500) == 0, "health 50% healthy");
    CHECK(wubu_lvm_health(1000, 900) == 1, "health 90% warning");
    CHECK(wubu_lvm_health(1000, 970) == 2, "health 97% critical");
    CHECK(wubu_lvm_health(0, 0) == 0, "health zero size");

    char buf[256];
    wubu_lvm_summary(buf, sizeof(buf));
    CHECK(strstr(buf, "lvm[") != NULL, "summary has header");
    CHECK(strstr(buf, "vg=") != NULL, "summary has vg");
    CHECK(strstr(buf, "lv=") != NULL, "summary has lv");

    printf("=== LVM TESTS: %d passed, %d failed ===\n", passes, fails);
    return fails > 0 ? 1 : 0;
}
