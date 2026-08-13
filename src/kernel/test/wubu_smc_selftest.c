/*
 * wubu_smc_selftest.c -- verifies SMC firmware routing.
 */
#include "wubu_smc.h"
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
    printf("=== wubu_smc_selftest ===\n\n");
    wubu_hw_detect();
    wubu_smc_probe();
    printf("  smc=%d smu=%d vcn=%d uvd=%d fw=%d\n",
           wubu_smc_present(), wubu_smc_smu(), wubu_smc_vcn(),
           wubu_smc_uvd(), wubu_smc_fw());

    CHECK(strcmp(wubu_smc_block_for("smu"), "SMU") == 0,
          "smu -> SMU");
    CHECK(strcmp(wubu_smc_block_for("smu1"), "SMU") == 0,
          "smu1 -> SMU");
    CHECK(strcmp(wubu_smc_block_for("vcn"), "VCN") == 0,
          "vcn -> VCN");
    CHECK(strcmp(wubu_smc_block_for("uvd"), "UVD") == 0,
          "uvd -> UVD");
    CHECK(strcmp(wubu_smc_block_for("gfx"), "GFX") == 0,
          "gfx -> GFX");
    CHECK(strcmp(wubu_smc_block_for("cpu"), "CPU") == 0,
          "cpu -> CPU");
    CHECK(strcmp(wubu_smc_block_for("zzz"), "SMU") == 0,
          "zzz -> SMU fallback");

    CHECK(strcmp(wubu_smc_state_for("load"), "loading") == 0,
          "load -> loading");
    CHECK(strcmp(wubu_smc_state_for("boot"), "loading") == 0,
          "boot -> loading");
    CHECK(strcmp(wubu_smc_state_for("ready"), "ready") == 0,
          "ready -> ready");
    CHECK(strcmp(wubu_smc_state_for("fail"), "failed") == 0,
          "fail -> failed");
    CHECK(strcmp(wubu_smc_state_for("error"), "failed") == 0,
          "error -> failed");
    CHECK(strcmp(wubu_smc_state_for("verif"), "verifying") == 0,
          "verif -> verifying");
    CHECK(strcmp(wubu_smc_state_for("zzz"), "loading") == 0,
          "zzz -> loading fallback");

    char s[256];
    wubu_smc_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "smc summary generated");

    printf("\n=== SMC TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
