/*
 * wubu_wifi_reg_selftest.c -- verifies kernel-owned WiFi-reg routing.
 */
#include "wubu_wifi_reg.h"
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
    printf("=== wubu_wifi_reg_selftest ===\n\n");

    wubu_hw_detect();
    wubu_wifi_reg_probe();

    printf("  db=%d crda=%d dfs=%d radar=%d country=%d\n",
           wubu_wifi_reg_db(), wubu_wifi_reg_crda(), wubu_wifi_reg_dfs(),
           wubu_wifi_reg_radar(), wubu_wifi_reg_country());

    /* Country routing. */
    CHECK(strcmp(wubu_wifi_reg_country_for("US"), "US") == 0,
          "US -> US");
    CHECK(strcmp(wubu_wifi_reg_country_for("DE"), "DE") == 0,
          "DE -> DE");
    CHECK(strcmp(wubu_wifi_reg_country_for("world"), "00") == 0,
          "world -> 00");
    CHECK(strcmp(wubu_wifi_reg_country_for("00"), "00") == 0,
          "00 -> 00");
    CHECK(strcmp(wubu_wifi_reg_country_for("us"), "US") == 0,
          "us -> US");

    /* DFS band routing. */
    CHECK(strcmp(wubu_wifi_reg_dfs_for("5g"), "dfs-5ghz") == 0,
          "5g -> dfs-5ghz");
    CHECK(strcmp(wubu_wifi_reg_dfs_for("6g"), "6ghz") == 0,
          "6g -> 6ghz");
    CHECK(strcmp(wubu_wifi_reg_dfs_for("2.4"), "2.4ghz") == 0,
          "2.4 -> 2.4ghz");
    CHECK(strcmp(wubu_wifi_reg_dfs_for("unknown"), "dfs") == 0,
          "unknown -> dfs fallback");

    char s[256];
    wubu_wifi_reg_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "wifireg summary generated");

    printf("\n=== WIFIREG TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
