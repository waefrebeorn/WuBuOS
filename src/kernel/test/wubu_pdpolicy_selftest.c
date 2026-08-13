/*
 * wubu_pdpolicy_selftest.c -- verifies kernel-owned PD-policy routing.
 */
#include "wubu_pdpolicy.h"
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
    printf("=== wubu_pdpolicy_selftest ===\n\n");
    wubu_hw_detect();
    wubu_pdpolicy_probe();
    printf("  pd=%d contract=%d pdo=%d pps=%d dual=%d\n",
           wubu_pdpolicy_present(), wubu_pdpolicy_contract(),
           wubu_pdpolicy_pdo(), wubu_pdpolicy_pps(),
           wubu_pdpolicy_dual());

    /* Role routing. */
    CHECK(strcmp(wubu_pdpolicy_role_for("source"), "source") == 0,
          "source -> source");
    CHECK(strcmp(wubu_pdpolicy_role_for("src"), "source") == 0,
          "src -> source");
    CHECK(strcmp(wubu_pdpolicy_role_for("sink"), "sink") == 0,
          "sink -> sink");
    CHECK(strcmp(wubu_pdpolicy_role_for("snk"), "sink") == 0,
          "snk -> sink");
    CHECK(strcmp(wubu_pdpolicy_role_for("zzz"), "dual") == 0,
          "zzz -> dual fallback");

    /* PDO routing. */
    CHECK(strcmp(wubu_pdpolicy_pdo_for("fixed"), "fixed") == 0,
          "fixed -> fixed");
    CHECK(strcmp(wubu_pdpolicy_pdo_for("battery"), "battery") == 0,
          "battery -> battery");
    CHECK(strcmp(wubu_pdpolicy_pdo_for("variable"), "variable") == 0,
          "variable -> variable");
    CHECK(strcmp(wubu_pdpolicy_pdo_for("pps"), "pps") == 0,
          "pps -> pps");
    CHECK(strcmp(wubu_pdpolicy_pdo_for("programmable"), "pps") == 0,
          "programmable -> pps");
    CHECK(strcmp(wubu_pdpolicy_pdo_for("zzz"), "fixed") == 0,
          "zzz -> fixed fallback");

    char s[256];
    wubu_pdpolicy_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "pdpolicy summary generated");

    printf("\n=== PDPOLICY TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
