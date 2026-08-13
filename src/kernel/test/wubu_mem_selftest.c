/*
 * wubu_mem_selftest.c -- verifies kernel-owned memory/ECC routing.
 */
#include "wubu_mem.h"
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
    printf("=== wubu_mem_selftest ===\n\n");

    wubu_hw_detect();
    wubu_mem_probe();

    printf("  edac=%d ecc=%d spd=%d ce=%ld ue=%ld\n",
           wubu_mem_has_edac(), wubu_mem_has_ecc(), wubu_mem_has_spd(),
           wubu_mem_ce_count(), wubu_mem_ue_count());

    /* ECC implies EDAC. */
    CHECK(!wubu_mem_has_ecc() || wubu_mem_has_edac(),
          "ECC present implies EDAC present");
    CHECK(wubu_mem_ce_count() >= 0, "CE count >= 0");
    CHECK(wubu_mem_ue_count() >= 0, "UE count >= 0");

    /* EDAC route is always consistent. */
    CHECK(strcmp(wubu_mem_edac_route("i7"), "i7core_edac") == 0,
          "i7 -> i7core_edac");
    CHECK(strcmp(wubu_mem_edac_route("skylake"), "skx_edac") == 0,
          "skylake -> skx_edac");
    CHECK(strcmp(wubu_mem_edac_route("zen"), "amd64_edac") == 0,
          "zen -> amd64_edac");
    CHECK(wubu_mem_edac_route("unknown") != NULL,
          "unknown controller -> edac_mc fallback");

    char s[256];
    wubu_mem_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "memory summary generated");

    printf("\n=== MEM TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
