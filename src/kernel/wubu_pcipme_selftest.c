/*
 * wubu_pcipme_selftest.c -- verifies kernel-owned PCI PME routing.
 */
#include "wubu_pcipme.h"
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
    printf("=== wubu_pcipme_selftest ===\n\n");
    wubu_hw_detect();
    wubu_pcipme_probe();
    printf("  pme=%d pmcsr=%d acpi=%d wake=%d pmeint=%d\n",
           wubu_pcipme_present(), wubu_pcipme_pmcsr(), wubu_pcipme_acpi(),
           wubu_pcipme_wake(), wubu_pcipme_pmeint());

    CHECK(strcmp(wubu_pcipme_state_for("0"), "d0") == 0, "0 -> d0");
    CHECK(strcmp(wubu_pcipme_state_for("d0"), "d0") == 0, "d0 -> d0");
    CHECK(strcmp(wubu_pcipme_state_for("d1"), "d1") == 0, "d1 -> d1");
    CHECK(strcmp(wubu_pcipme_state_for("d3"), "d3") == 0, "d3 -> d3");
    CHECK(strcmp(wubu_pcipme_state_for("zzz"), "d0") == 0, "zzz -> d0 fallback");

    CHECK(strcmp(wubu_pcipme_event_for("pme"), "pme") == 0, "pme -> pme");
    CHECK(strcmp(wubu_pcipme_event_for("wake"), "wake") == 0, "wake -> wake");
    CHECK(strcmp(wubu_pcipme_event_for("suspend"), "suspend") == 0, "suspend -> suspend");
    CHECK(strcmp(wubu_pcipme_event_for("resume"), "resume") == 0, "resume -> resume");
    CHECK(strcmp(wubu_pcipme_event_for("zzz"), "pme") == 0, "zzz -> pme fallback");

    char s[256];
    wubu_pcipme_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "pcipme summary generated");

    printf("\n=== PCIPME TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
