/*
 * wubu_pcmplugin_selftest.c -- verifies PCM plugin routing.
 */
#include "wubu_pcmplugin.h"
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
    printf("=== wubu_pcmplugin_selftest ===\n\n");
    wubu_hw_detect();
    wubu_pcmplugin_probe();
    printf("  plug=%d rate=%d vol=%d copy=%d plugtype=%d\n",
           wubu_pcmplugin_present(), wubu_pcmplugin_rate(), wubu_pcmplugin_vol(),
           wubu_pcmplugin_copy(), wubu_pcmplugin_plugtype());

    CHECK(strcmp(wubu_pcmplugin_type_for("rate"), "rate") == 0,
          "rate -> rate");
    CHECK(strcmp(wubu_pcmplugin_type_for("resample"), "rate") == 0,
          "resample -> rate");
    CHECK(strcmp(wubu_pcmplugin_type_for("vol"), "vol") == 0,
          "vol -> vol");
    CHECK(strcmp(wubu_pcmplugin_type_for("softvol"), "vol") == 0,
          "softvol -> vol");
    CHECK(strcmp(wubu_pcmplugin_type_for("copy"), "copy") == 0,
          "copy -> copy");
    CHECK(strcmp(wubu_pcmplugin_type_for("plug"), "plug") == 0,
          "plug -> plug");
    CHECK(strcmp(wubu_pcmplugin_type_for("dmix"), "dmix") == 0,
          "dmix -> dmix");
    CHECK(strcmp(wubu_pcmplugin_type_for("route"), "plug") == 0,
          "route -> plug");
    CHECK(strcmp(wubu_pcmplugin_type_for("zzz"), "plug") == 0,
          "zzz -> plug fallback");

    CHECK(strcmp(wubu_pcmplugin_chain_for("rate"), "rate") == 0,
          "chain rate -> rate");
    CHECK(strcmp(wubu_pcmplugin_chain_for("vol"), "vol") == 0,
          "chain vol -> vol");
    CHECK(strcmp(wubu_pcmplugin_chain_for("copy"), "copy") == 0,
          "chain copy -> copy");
    CHECK(strcmp(wubu_pcmplugin_chain_for("plug"), "plug") == 0,
          "chain plug -> plug");
    CHECK(strcmp(wubu_pcmplugin_chain_for("dmix"), "dmix") == 0,
          "chain dmix -> dmix");

    char s[256];
    wubu_pcmplugin_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "pcmplugin summary generated");

    printf("\n=== PCMPLUGIN TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
