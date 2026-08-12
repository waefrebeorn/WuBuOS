/*
 * wubu_uas_selftest.c -- verifies storage UAS routing.
 */
#include "wubu_uas.h"
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
    printf("=== wubu_uas_selftest ===\n\n");
    wubu_hw_detect();
    wubu_uas_probe();
    printf("  uas=%d bot=%d uasp=%d q=%d part=%d\n",
           wubu_uas_present(), wubu_uas_bot(),
           wubu_uas_uasp(), wubu_uas_queue(),
           wubu_uas_part());

    CHECK(strcmp(wubu_uas_proto_for("uas"), "UAS") == 0,
          "uas -> UAS");
    CHECK(strcmp(wubu_uas_proto_for("uasp"), "UASP") == 0,
          "uasp -> UASP");
    CHECK(strcmp(wubu_uas_proto_for("bot"), "BOT") == 0,
          "bot -> BOT");
    CHECK(strcmp(wubu_uas_proto_for("bulk"), "BOT") == 0,
          "bulk -> BOT");
    CHECK(strcmp(wubu_uas_proto_for("cbi"), "CBI") == 0,
          "cbi -> CBI");
    CHECK(strcmp(wubu_uas_proto_for("ccb"), "CCB") == 0,
          "ccb -> CCB");
    CHECK(strcmp(wubu_uas_proto_for("zzz"), "BOT") == 0,
          "zzz -> BOT fallback");

    CHECK(strcmp(wubu_uas_dir_for("in"), "IN") == 0,
          "in -> IN");
    CHECK(strcmp(wubu_uas_dir_for("out"), "OUT") == 0,
          "out -> OUT");
    CHECK(strcmp(wubu_uas_dir_for("bi"), "BIDIRECTIONAL") == 0,
          "bi -> BIDIRECTIONAL");
    CHECK(strcmp(wubu_uas_dir_for("zzz"), "IN") == 0,
          "zzz -> IN fallback");

    char s[256];
    wubu_uas_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "uas summary generated");

    printf("\n=== UAS TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
