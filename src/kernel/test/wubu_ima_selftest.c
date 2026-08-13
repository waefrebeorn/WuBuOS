/*
 * wubu_ima_selftest.c -- verifies kernel-owned IMA/EVM routing.
 */
#include "wubu_ima.h"
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
    printf("=== wubu_ima_selftest ===\n\n");

    wubu_hw_detect();
    wubu_ima_probe();

    printf("  ima=%d evm=%d measure=%d appraise=%d pcr=%d\n",
           wubu_ima_present(), wubu_ima_evm(), wubu_ima_measure(),
           wubu_ima_appraise(), wubu_ima_pcr());

    /* Mode routing. */
    CHECK(strcmp(wubu_ima_mode_for("measure"), "measure") == 0,
          "measure -> measure");
    CHECK(strcmp(wubu_ima_mode_for("appraise"), "appraise") == 0,
          "appraise -> appraise");
    CHECK(strcmp(wubu_ima_mode_for("audit"), "audit") == 0,
          "audit -> audit");
    CHECK(strcmp(wubu_ima_mode_for("unknown"), "measure") == 0,
          "unknown -> measure fallback");

    /* Policy routing. */
    CHECK(strcmp(wubu_ima_policy_for("tcb"), "tcb") == 0,
          "tcb -> tcb");
    CHECK(strcmp(wubu_ima_policy_for("ape"), "ape") == 0,
          "ape -> ape");
    CHECK(strcmp(wubu_ima_policy_for("ltcb"), "ltcb") == 0,
          "ltcb -> ltcb");
    CHECK(strcmp(wubu_ima_policy_for("critical"), "critical-data") == 0,
          "critical -> critical-data");
    CHECK(strcmp(wubu_ima_policy_for("unknown"), "tcb") == 0,
          "unknown -> tcb fallback");

    char s[256];
    wubu_ima_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "ima summary generated");

    printf("\n=== IMA TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
