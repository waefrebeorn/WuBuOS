/*
 * wubu_codecgraph_selftest.c -- verifies kernel-owned codec-graph routing.
 */
#include "wubu_codecgraph.h"
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
    printf("=== wubu_codecgraph_selftest ===\n\n");

    wubu_hw_detect();
    wubu_codecgraph_probe();

    printf("  codec=%d graph=%d amp=%d widgets=%d dapm=%d\n",
           wubu_codecgraph_present(), wubu_codecgraph_graph(),
           wubu_codecgraph_amp(), wubu_codecgraph_widgets(),
           wubu_codecgraph_dapm());

    /* Widget routing. */
    CHECK(strcmp(wubu_codecgraph_widget_for("pin"), "pin-complex") == 0,
          "pin -> pin-complex");
    CHECK(strcmp(wubu_codecgraph_widget_for("adc"), "adc") == 0,
          "adc -> adc");
    CHECK(strcmp(wubu_codecgraph_widget_for("dac"), "dac") == 0,
          "dac -> dac");
    CHECK(strcmp(wubu_codecgraph_widget_for("mixer"), "mixer") == 0,
          "mixer -> mixer");
    CHECK(strcmp(wubu_codecgraph_widget_for("selector"), "selector") == 0,
          "selector -> selector");
    CHECK(strcmp(wubu_codecgraph_widget_for("unknown"), "widget") == 0,
          "unknown -> widget fallback");

    /* Verb routing. */
    CHECK(strcmp(wubu_codecgraph_verb_for("pin"), "SET_PIN_WIDGET") == 0,
          "pin -> SET_PIN_WIDGET");
    CHECK(strcmp(wubu_codecgraph_verb_for("amp"), "SET_AMP_GAIN") == 0,
          "amp -> SET_AMP_GAIN");
    CHECK(strcmp(wubu_codecgraph_verb_for("gpio"), "SET_GPIO") == 0,
          "gpio -> SET_GPIO");
    CHECK(strcmp(wubu_codecgraph_verb_for("unknown"), "verb") == 0,
          "unknown -> verb fallback");

    char s[256];
    wubu_codecgraph_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "codecgraph summary generated");

    printf("\n=== CODECGRAPH TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
