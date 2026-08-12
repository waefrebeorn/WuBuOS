/*
 * wubu_adreno700_selftest.c -- verifies Qualcomm Adreno 700 routing.
 */
#include "wubu_adreno700.h"
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
    printf("=== wubu_adreno700_selftest ===\n");

    wubu_adreno700_probe();

    int p = wubu_adreno700_present();
    CHECK(p == 0 || p == 1, "adreno700 present is boolean");

    /* freedreno routing. */
    CHECK(wubu_adreno700_uses_freedreno(1) == 1, "freedreno available = uses freedreno");
    CHECK(wubu_adreno700_uses_freedreno(0) == 0, "no freedreno = not used");

    /* Generation check. */
    CHECK(wubu_adreno700_gen(7) == 1, "gen 7 = Adreno 7xx");
    CHECK(wubu_adreno700_gen(6) == 0, "gen 6 = not 7xx");

    /* Summary builds. */
    char out[160] = "";
    wubu_adreno700_summary(out, sizeof(out));
    CHECK(strstr(out, "adreno700[") != NULL, "summary has adreno700 fragment");

    printf("\n=== ADRENO700 TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
