/*
 * wubu_fw_selftest.c -- verifies kernel-owned firmware routing.
 */
#include "wubu_fw.h"
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
    printf("=== wubu_fw_selftest ===\n\n");

    wubu_hw_detect();
    wubu_fw_probe();

    printf("  fw=%d lib=%d raid=%d hba=%d update=%d\n",
           wubu_fw_loader(), wubu_fw_lib(), wubu_fw_raid(),
           wubu_fw_hba(), wubu_fw_update());

    /* Controller routing. */
    CHECK(strcmp(wubu_fw_controller_for("megaraid"), "megaraid-sas") == 0,
          "megaraid -> megaraid-sas");
    CHECK(strcmp(wubu_fw_controller_for("hpsa"), "hpsa") == 0,
          "hpsa -> hpsa");
    CHECK(strcmp(wubu_fw_controller_for("mpt3"), "mpt3sas") == 0,
          "mpt3 -> mpt3sas");
    CHECK(strcmp(wubu_fw_controller_for("aac"), "aacraid") == 0,
          "aac -> aacraid");
    CHECK(strcmp(wubu_fw_controller_for("flash"), "fw-flash") == 0,
          "flash -> fw-flash");
    CHECK(strcmp(wubu_fw_controller_for("unknown"), "fw-loader") == 0,
          "unknown -> fw-loader fallback");

    /* Stage routing. */
    CHECK(strcmp(wubu_fw_stage_for("load"), "load") == 0,
          "load -> load");
    CHECK(strcmp(wubu_fw_stage_for("verify"), "verify") == 0,
          "verify -> verify");
    CHECK(strcmp(wubu_fw_stage_for("apply"), "apply") == 0,
          "apply -> apply");
    CHECK(strcmp(wubu_fw_stage_for("commit"), "commit") == 0,
          "commit -> commit");
    CHECK(strcmp(wubu_fw_stage_for("unknown"), "load") == 0,
          "unknown -> load fallback");

    char s[256];
    wubu_fw_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "fw summary generated");

    printf("\n=== FW TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
