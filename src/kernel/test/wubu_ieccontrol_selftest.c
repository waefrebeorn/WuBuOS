/*
 * wubu_ieccontrol_selftest.c -- verifies IEC control routing.
 */
#include "wubu_ieccontrol.h"
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
    printf("=== wubu_ieccontrol_selftest ===\n\n");
    wubu_hw_detect();
    wubu_ieccontrol_probe();
    printf("  iec=%d aes=%d enc=%d clock=%d rate=%d\n",
           wubu_ieccontrol_present(), wubu_ieccontrol_aes(),
           wubu_ieccontrol_enc(), wubu_ieccontrol_clock(), wubu_ieccontrol_rate());

    CHECK(strcmp(wubu_ieccontrol_encoding_for("consumer"), "consumer") == 0,
          "consumer -> consumer");
    CHECK(strcmp(wubu_ieccontrol_encoding_for("pcm"), "consumer") == 0,
          "pcm -> consumer");
    CHECK(strcmp(wubu_ieccontrol_encoding_for("pro"), "professional") == 0,
          "pro -> professional");
    CHECK(strcmp(wubu_ieccontrol_encoding_for("broadcast"), "broadcast") == 0,
          "broadcast -> broadcast");
    CHECK(strcmp(wubu_ieccontrol_encoding_for("ac3"), "ac3") == 0,
          "ac3 -> ac3");
    CHECK(strcmp(wubu_ieccontrol_encoding_for("dts"), "dts") == 0,
          "dts -> dts");
    CHECK(strcmp(wubu_ieccontrol_encoding_for("zzz"), "consumer") == 0,
          "zzz -> consumer fallback");

    CHECK(strcmp(wubu_ieccontrol_clock_for("ext"), "external") == 0,
          "ext -> external");
    CHECK(strcmp(wubu_ieccontrol_clock_for("int"), "internal") == 0,
          "int -> internal");
    CHECK(strcmp(wubu_ieccontrol_clock_for("master"), "master") == 0,
          "master -> master");
    CHECK(strcmp(wubu_ieccontrol_clock_for("slave"), "slave") == 0,
          "slave -> slave");
    CHECK(strcmp(wubu_ieccontrol_clock_for("zzz"), "internal") == 0,
          "zzz -> internal fallback");

    char s[256];
    wubu_ieccontrol_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "ieccontrol summary generated");

    printf("\n=== IECCONTROL TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
