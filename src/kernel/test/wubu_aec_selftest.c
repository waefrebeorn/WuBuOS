/*
 * wubu_aec_selftest.c -- verifies kernel-owned AEC routing.
 */
#include "wubu_aec.h"
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
    printf("=== wubu_aec_selftest ===\n\n");

    wubu_hw_detect();
    wubu_aec_probe();

    printf("  aec=%d ns=%d webrtc=%d pw=%d pa=%d\n",
           wubu_aec_present(), wubu_aec_ns(),
           wubu_aec_webrtc(), wubu_aec_pw(), wubu_aec_pa());

    /* Method routing. */
    CHECK(strcmp(wubu_aec_method_for("webrtc"), "webrtc") == 0,
          "webrtc -> webrtc");
    CHECK(strcmp(wubu_aec_method_for("speex"), "speex") == 0,
          "speex -> speex");
    CHECK(strcmp(wubu_aec_method_for("rnnoise"), "rnnoise") == 0,
          "rnnoise -> rnnoise");
    CHECK(strcmp(wubu_aec_method_for("ooura"), "ooura") == 0,
          "ooura -> ooura");
    CHECK(strcmp(wubu_aec_method_for("unknown"), "webrtc") == 0,
          "unknown -> webrtc fallback");

    /* Level routing. */
    CHECK(strcmp(wubu_aec_level_for("aggressive"), "aggressive") == 0,
          "aggressive -> aggressive");
    CHECK(strcmp(wubu_aec_level_for("moderate"), "moderate") == 0,
          "moderate -> moderate");
    CHECK(strcmp(wubu_aec_level_for("light"), "light") == 0,
          "light -> light");
    CHECK(strcmp(wubu_aec_level_for("unknown"), "moderate") == 0,
          "unknown -> moderate fallback");

    char s[256];
    wubu_aec_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "aec summary generated");

    printf("\n=== AEC TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
