/*
 * wubu_dapmwidget_selftest.c -- verifies DAPM widget routing.
 */
#include "wubu_dapmwidget.h"
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
    printf("=== wubu_dapmwidget_selftest ===\n\n");
    wubu_hw_detect();
    wubu_dapmwidget_probe();
    printf("  dapm=%d widget=%d power=%d path=%d stream=%d\n",
           wubu_dapmwidget_present(), wubu_dapmwidget_widget(), wubu_dapmwidget_power(),
           wubu_dapmwidget_path(), wubu_dapmwidget_stream());

    CHECK(strcmp(wubu_dapmwidget_type_for("adc"), "adc") == 0, "adc -> adc");
    CHECK(strcmp(wubu_dapmwidget_type_for("dac"), "dac") == 0, "dac -> dac");
    CHECK(strcmp(wubu_dapmwidget_type_for("mix"), "mixer") == 0, "mix -> mixer");
    CHECK(strcmp(wubu_dapmwidget_type_for("mux"), "mux") == 0, "mux -> mux");
    CHECK(strcmp(wubu_dapmwidget_type_for("pga"), "pga") == 0, "pga -> pga");
    CHECK(strcmp(wubu_dapmwidget_type_for("switch"), "switch") == 0, "switch -> switch");
    CHECK(strcmp(wubu_dapmwidget_type_for("zzz"), "adc") == 0, "zzz -> adc fallback");

    CHECK(strcmp(wubu_dapmwidget_power_for("on"), "on") == 0, "on -> on");
    CHECK(strcmp(wubu_dapmwidget_power_for("power"), "on") == 0, "power -> on");
    CHECK(strcmp(wubu_dapmwidget_power_for("off"), "off") == 0, "off -> off");
    CHECK(strcmp(wubu_dapmwidget_power_for("suspend"), "suspend") == 0, "suspend -> suspend");
    CHECK(strcmp(wubu_dapmwidget_power_for("zzz"), "off") == 0, "zzz -> off fallback");

    char s[256];
    wubu_dapmwidget_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "dapmwidget summary generated");

    printf("\n=== DAPMWIDGET TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
