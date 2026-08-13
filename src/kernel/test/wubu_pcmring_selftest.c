/*
 * wubu_pcmring_selftest.c -- verifies audio PCM ring buffer routing.
 */
#include "wubu_pcmring.h"
#include "wubu_hw_detect.h"
#include <stdio.h>
#include <string.h>

#define CHECK(cond, msg) do { \
    if (!(cond)) { printf("FAIL: %s\n", msg); fails++; } \
    else { passes++; } \
} while (0)

int main(void)
{
    int passes = 0, fails = 0;
    wubu_pcmring_probe();

    CHECK(wubu_pcmring_present() >= 0, "pcmring_present returns non-negative");

    CHECK(strcmp(wubu_pcmring_format_for("S32_LE"), "S32_LE") == 0, "format S32_LE");
    CHECK(strcmp(wubu_pcmring_format_for("S24_3LE"), "S24_3LE") == 0, "format S24_3LE");
    CHECK(strcmp(wubu_pcmring_format_for("S24_LE"), "S24_LE") == 0, "format S24_LE");
    CHECK(strcmp(wubu_pcmring_format_for("S16_LE"), "S16_LE") == 0, "format S16_LE");
    CHECK(strcmp(wubu_pcmring_format_for("FLOAT64"), "FLOAT") == 0, "format FLOAT");
    CHECK(strcmp(wubu_pcmring_format_for("S32_BE"), "S32") == 0, "format S32 fallback");
    CHECK(strcmp(wubu_pcmring_format_for("U8"), "U8") == 0, "format U8");
    CHECK(wubu_pcmring_format_for(NULL) == NULL, "format null passthrough");

    CHECK(wubu_pcmring_latency_us(48000, 1024, 4096) > 0, "latency non-zero");
    CHECK(wubu_pcmring_latency_us(0, 1024, 4096) == 0, "latency zero for bad rate");

    char buf[256];
    wubu_pcmring_summary(buf, sizeof(buf));
    CHECK(strstr(buf, "pcmring[") != NULL, "summary has pcmring header");
    CHECK(strstr(buf, "buf=") != NULL, "summary has buf");

    printf("=== PCMRING TESTS: %d passed, %d failed ===\n", passes, fails);
    return fails > 0 ? 1 : 0;
}
