/*
 * wubu_porttiming_selftest.c -- verifies kernel-owned port-timing routing.
 */
#include "wubu_porttiming.h"
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
    printf("=== wubu_porttiming_selftest ===\n\n");

    wubu_hw_detect();
    wubu_porttiming_probe();

    printf("  mode=%d cvt=%d rb=%d link=%d preferred=%d\n",
           wubu_porttiming_mode(), wubu_porttiming_cvt(),
           wubu_porttiming_rb(), wubu_porttiming_link(),
           wubu_porttiming_preferred());

    /* Std routing. */
    CHECK(strcmp(wubu_porttiming_std_for("cvt"), "cvt") == 0,
          "cvt -> cvt");
    CHECK(strcmp(wubu_porttiming_std_for("rb"), "cvt-rb") == 0,
          "rb -> cvt-rb");
    CHECK(strcmp(wubu_porttiming_std_for("cvt-rb"), "cvt-rb") == 0,
          "cvt-rb -> cvt-rb");
    CHECK(strcmp(wubu_porttiming_std_for("vesa"), "vesa") == 0,
          "vesa -> vesa");
    CHECK(strcmp(wubu_porttiming_std_for("dmt"), "dmt") == 0,
          "dmt -> dmt");
    CHECK(strcmp(wubu_porttiming_std_for("unknown"), "gfx") == 0,
          "unknown -> gfx fallback");

    /* Link routing. */
    CHECK(strcmp(wubu_porttiming_link_for("rbr"), "rbr") == 0,
          "rbr -> rbr");
    CHECK(strcmp(wubu_porttiming_link_for("hbr"), "hbr") == 0,
          "hbr -> hbr");
    CHECK(strcmp(wubu_porttiming_link_for("hbr2"), "hbr2") == 0,
          "hbr2 -> hbr2");
    CHECK(strcmp(wubu_porttiming_link_for("hbr3"), "hbr3") == 0,
          "hbr3 -> hbr3");
    CHECK(strcmp(wubu_porttiming_link_for("uhbr"), "uhbr") == 0,
          "uhbr -> uhbr");
    CHECK(strcmp(wubu_porttiming_link_for("eDP"), "edp") == 0,
          "eDP -> edp");
    CHECK(strcmp(wubu_porttiming_link_for("unknown"), "link") == 0,
          "unknown -> link fallback");

    char s[256];
    wubu_porttiming_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "porttiming summary generated");

    printf("\n=== PORTTIMING TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
