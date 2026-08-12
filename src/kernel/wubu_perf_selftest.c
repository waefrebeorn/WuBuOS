/*
 * wubu_perf_selftest.c -- verifies GPU performance counter routing.
 */
#include "wubu_perf.h"
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
    wubu_perf_probe();

    CHECK(wubu_perf_present() >= 0, "perf_present returns non-negative");
    CHECK(wubu_perf_present() == 0 || wubu_perf_present() == 1, "perf_present is boolean");

    const char *eng = wubu_perf_engine_for("render");
    CHECK(eng != NULL && strcmp(eng, "render") == 0, "engine render");
    CHECK(strcmp(wubu_perf_engine_for("blitter"), "blitter") == 0, "engine blitter");
    CHECK(strcmp(wubu_perf_engine_for("video_decoder"), "decode") == 0, "engine decode");
    CHECK(strcmp(wubu_perf_engine_for("video_encoder"), "encode") == 0, "engine encode");
    CHECK(strcmp(wubu_perf_engine_for("copy_engine_0"), "copy") == 0, "engine copy");
    CHECK(strcmp(wubu_perf_engine_for("unknown_engine"), "unknown") == 0, "engine unknown");
    CHECK(wubu_perf_engine_for(NULL) == NULL, "engine null passthrough");

    const char *freq = wubu_perf_freq_str();
    CHECK(freq != NULL, "freq string non-null");

    char buf[256];
    wubu_perf_summary(buf, sizeof(buf));
    CHECK(strstr(buf, "perf[") != NULL, "summary has perf header");
    CHECK(strstr(buf, "engines=") != NULL, "summary has engines");

    printf("=== PERF TESTS: %d passed, %d failed ===\n", passes, fails);
    return fails > 0 ? 1 : 0;
}
