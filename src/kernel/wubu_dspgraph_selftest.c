/*
 * wubu_dspgraph_selftest.c -- verifies kernel-owned DSP-graph routing.
 */
#include "wubu_dspgraph.h"
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
    printf("=== wubu_dspgraph_selftest ===\n\n");
    wubu_hw_detect();
    wubu_dspgraph_probe();
    printf("  graph=%d dapm=%d widget=%d path=%d route=%d\n",
           wubu_dspgraph_present(), wubu_dspgraph_dapm(), wubu_dspgraph_widget(),
           wubu_dspgraph_path(), wubu_dspgraph_route());

    CHECK(strcmp(wubu_dspgraph_widget_for("mixer"), "mixer") == 0, "mixer -> mixer");
    CHECK(strcmp(wubu_dspgraph_widget_for("dac"), "dac") == 0, "dac -> dac");
    CHECK(strcmp(wubu_dspgraph_widget_for("adc"), "adc") == 0, "adc -> adc");
    CHECK(strcmp(wubu_dspgraph_widget_for("mux"), "mux") == 0, "mux -> mux");
    CHECK(strcmp(wubu_dspgraph_widget_for("pin"), "pin") == 0, "pin -> pin");
    CHECK(strcmp(wubu_dspgraph_widget_for("switch"), "switch") == 0, "switch -> switch");
    CHECK(strcmp(wubu_dspgraph_widget_for("zzz"), "widget") == 0, "zzz -> widget fallback");

    CHECK(strcmp(wubu_dspgraph_path_for("up"), "up") == 0, "up -> up");
    CHECK(strcmp(wubu_dspgraph_path_for("down"), "down") == 0, "down -> down");
    CHECK(strcmp(wubu_dspgraph_path_for("direct"), "direct") == 0, "direct -> direct");
    CHECK(strcmp(wubu_dspgraph_path_for("muted"), "muted") == 0, "muted -> muted");
    CHECK(strcmp(wubu_dspgraph_path_for("zzz"), "direct") == 0, "zzz -> direct fallback");

    char s[256];
    wubu_dspgraph_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "dspgraph summary generated");

    printf("\n=== DSPGRAPH TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
