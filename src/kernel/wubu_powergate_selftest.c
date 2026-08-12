/*
 * wubu_powergate_selftest.c -- verifies GPU power-gate routing.
 */
#include "wubu_powergate.h"
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
    printf("=== wubu_powergate_selftest ===\n\n");
    wubu_hw_detect();
    wubu_powergate_probe();
    printf("  pg=%d runtime=%d shader=%d texture=%d l2=%d\n",
           wubu_powergate_present(), wubu_powergate_runtime(), wubu_powergate_shader(),
           wubu_powergate_texture(), wubu_powergate_l2());

    CHECK(strcmp(wubu_powergate_domain_for("shader"), "shader") == 0,
          "shader -> shader");
    CHECK(strcmp(wubu_powergate_domain_for("texture"), "texture") == 0,
          "texture -> texture");
    CHECK(strcmp(wubu_powergate_domain_for("l2"), "l2") == 0,
          "l2 -> l2");
    CHECK(strcmp(wubu_powergate_domain_for("mcv"), "mcv") == 0,
          "mcv -> mcv");
    CHECK(strcmp(wubu_powergate_domain_for("skep"), "skep") == 0,
          "skep -> skep");
    CHECK(strcmp(wubu_powergate_domain_for("zzz"), "shader") == 0,
          "zzz -> shader fallback");

    CHECK(strcmp(wubu_powergate_state_for("on"), "on") == 0,
          "on -> on");
    CHECK(strcmp(wubu_powergate_state_for("active"), "on") == 0,
          "active -> on");
    CHECK(strcmp(wubu_powergate_state_for("off"), "gated") == 0,
          "off -> gated");
    CHECK(strcmp(wubu_powergate_state_for("gated"), "gated") == 0,
          "gated -> gated");
    CHECK(strcmp(wubu_powergate_state_for("suspend"), "suspend") == 0,
          "suspend -> suspend");
    CHECK(strcmp(wubu_powergate_state_for("zzz"), "on") == 0,
          "zzz -> on fallback");

    char s[256];
    wubu_powergate_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "powergate summary generated");

    printf("\n=== POWERGATE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
