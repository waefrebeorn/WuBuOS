/*
 * wubu_storagesched_selftest.c -- verifies storage scheduler routing.
 */
#include "wubu_storagesched.h"
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
    printf("=== wubu_storagesched_selftest ===\n\n");
    wubu_hw_detect();
    wubu_storagesched_probe();
    printf("  ss=%d mq=%d bfq=%d deadline=%d none=%d\n",
           wubu_storagesched_present(), wubu_storagesched_mq(), wubu_storagesched_bfq(),
           wubu_storagesched_deadline(), wubu_storagesched_none());

    CHECK(strcmp(wubu_storagesched_type_for("mq-deadline"), "mq-deadline") == 0,
          "mq-deadline -> mq-deadline");
    CHECK(strcmp(wubu_storagesched_type_for("mq_deadline"), "mq-deadline") == 0,
          "mq_deadline -> mq-deadline");
    CHECK(strcmp(wubu_storagesched_type_for("bfq"), "bfq") == 0,
          "bfq -> bfq");
    CHECK(strcmp(wubu_storagesched_type_for("deadline"), "deadline") == 0,
          "deadline -> deadline");
    CHECK(strcmp(wubu_storagesched_type_for("cfq"), "cfq") == 0,
          "cfq -> cfq");
    CHECK(strcmp(wubu_storagesched_type_for("kyber"), "kyber") == 0,
          "kyber -> kyber");
    CHECK(strcmp(wubu_storagesched_type_for("none"), "none") == 0,
          "none -> none");
    CHECK(strcmp(wubu_storagesched_type_for("zzz"), "mq-deadline") == 0,
          "zzz -> mq-deadline fallback");

    CHECK(strcmp(wubu_storagesched_mode_for("mq"), "multi-queue") == 0,
          "mq -> multi-queue");
    CHECK(strcmp(wubu_storagesched_mode_for("multi"), "multi-queue") == 0,
          "multi -> multi-queue");
    CHECK(strcmp(wubu_storagesched_mode_for("single"), "single-queue") == 0,
          "single -> single-queue");
    CHECK(strcmp(wubu_storagesched_mode_for("zzz"), "multi-queue") == 0,
          "zzz -> multi-queue fallback");

    char s[256];
    wubu_storagesched_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "storagesched summary generated");

    printf("\n=== STORAGESCHED TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
