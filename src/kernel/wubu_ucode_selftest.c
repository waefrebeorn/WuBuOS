/*
 * wubu_ucode_selftest.c -- verifies kernel-owned microcode routing.
 */
#include "wubu_ucode.h"
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
    printf("=== wubu_ucode_selftest ===\n\n");

    wubu_hw_detect();
    wubu_ucode_probe();

    printf("  intel=%d amd=%d early=%d late=%d loaded=%d\n",
           wubu_ucode_intel(), wubu_ucode_amd(), wubu_ucode_early(),
           wubu_ucode_late(), wubu_ucode_loaded());

    /* Microcode loader routing. */
    CHECK(strcmp(wubu_ucode_loader_for("intel"), "intel-ucode") == 0,
          "intel -> intel-ucode");
    CHECK(strcmp(wubu_ucode_loader_for("amd"), "amd-ucode") == 0,
          "amd -> amd-ucode");
    CHECK(strcmp(wubu_ucode_loader_for("hygon"), "amd-ucode") == 0,
          "hygon -> amd-ucode");
    CHECK(strcmp(wubu_ucode_loader_for("unknown"), "ucode") == 0,
          "unknown -> ucode fallback");

    /* Load path routing. */
    CHECK(strcmp(wubu_ucode_loadpath_for("early"), "initrd-early") == 0,
          "early -> initrd-early");
    CHECK(strcmp(wubu_ucode_loadpath_for("late"), "dev-cpu-microcode") == 0,
          "late -> dev-cpu-microcode");
    CHECK(strcmp(wubu_ucode_loadpath_for("unknown"), "ucode") == 0,
          "unknown -> ucode fallback");

    char s[256];
    wubu_ucode_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "ucode summary generated");

    printf("\n=== UCODE TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
