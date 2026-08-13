/*
 * wubu_dapm_selftest.c -- verifies audio DAPM routing.
 */
#include "wubu_dapm.h"
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
    wubu_dapm_probe();

    CHECK(wubu_dapm_present() >= 0, "dapm_present returns non-negative");

    CHECK(strcmp(wubu_dapm_widget_type_str(0), "input") == 0, "widget input");
    CHECK(strcmp(wubu_dapm_widget_type_str(1), "output") == 0, "widget output");
    CHECK(strcmp(wubu_dapm_widget_type_str(2), "mux") == 0, "widget mux");
    CHECK(strcmp(wubu_dapm_widget_type_str(3), "mixer") == 0, "widget mixer");
    CHECK(strcmp(wubu_dapm_widget_type_str(4), "pga") == 0, "widget pga");
    CHECK(strcmp(wubu_dapm_widget_type_str(5), "speaker") == 0, "widget speaker");
    CHECK(strcmp(wubu_dapm_widget_type_str(99), "none") == 0, "widget none");

    CHECK(wubu_dapm_path_active("Speaker_on") == 1, "path active on");
    CHECK(wubu_dapm_path_active("enable") == 1, "path active enable");
    CHECK(wubu_dapm_path_active("off") == 0, "path inactive off");
    CHECK(wubu_dapm_path_active(NULL) == 0, "path null");

    char buf[256];
    wubu_dapm_summary(buf, sizeof(buf));
    CHECK(strstr(buf, "dapm[") != NULL, "summary has header");
    CHECK(strstr(buf, "widgets=") != NULL, "summary has widgets");
    CHECK(strstr(buf, "paths=") != NULL, "summary has paths");

    printf("=== DAPM TESTS: %d passed, %d failed ===\n", passes, fails);
    return fails > 0 ? 1 : 0;
}
