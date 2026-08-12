/*
 * wubu_mst_selftest.c -- verifies kernel-owned MST/SRC routing.
 */
#include "wubu_mst.h"
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
    printf("=== wubu_mst_selftest ===\n\n");

    wubu_hw_detect();
    wubu_mst_probe();

    printf("  dp=%d top=%d dsc=%d src=%d resample=%d\n",
           wubu_mst_dp(), wubu_mst_topology(), wubu_mst_dsc(),
           wubu_mst_src(), wubu_mst_resample());

    /* MST payload routing. */
    CHECK(strcmp(wubu_mst_payload_for("single"), "single-stream") == 0,
          "single -> single-stream");
    CHECK(strcmp(wubu_mst_payload_for("multi"), "multi-stream") == 0,
          "multi -> multi-stream");
    CHECK(strcmp(wubu_mst_payload_for("dsc"), "dsc-compressed") == 0,
          "dsc -> dsc-compressed");
    CHECK(strcmp(wubu_mst_payload_for("unknown"), "dp") == 0,
          "unknown -> dp fallback");

    /* SRC routing. */
    CHECK(strcmp(wubu_mst_src_for("44"), "44100-src") == 0,
          "44 -> 44100-src");
    CHECK(strcmp(wubu_mst_src_for("48"), "48000-src") == 0,
          "48 -> 48000-src");
    CHECK(strcmp(wubu_mst_src_for("96"), "96000-src") == 0,
          "96 -> 96000-src");
    CHECK(strcmp(wubu_mst_src_for("192"), "192000-src") == 0,
          "192 -> 192000-src");
    CHECK(strcmp(wubu_mst_src_for("best"), "src-best-quality") == 0,
          "best -> src-best-quality");
    CHECK(strcmp(wubu_mst_src_for("unknown"), "src") == 0,
          "unknown -> src fallback");

    char s[256];
    wubu_mst_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "mst summary generated");

    printf("\n=== MST TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
