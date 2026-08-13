/*
 * wubu_iosched_selftest.c -- verifies kernel-owned I/O-scheduler routing.
 */
#include "wubu_iosched.h"
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
    printf("=== wubu_iosched_selftest ===\n\n");
    wubu_hw_detect();
    wubu_iosched_probe();
    printf("  sched=%d mq=%d deadline=%d kyber=%d bfq=%d\n",
           wubu_iosched_present(), wubu_iosched_mq(), wubu_iosched_deadline(),
           wubu_iosched_kyber(), wubu_iosched_bfq());

    /* Algo routing. */
    CHECK(strcmp(wubu_iosched_algo_for("deadline"), "mq-deadline") == 0,
          "deadline -> mq-deadline");
    CHECK(strcmp(wubu_iosched_algo_for("kyber"), "kyber") == 0,
          "kyber -> kyber");
    CHECK(strcmp(wubu_iosched_algo_for("bfq"), "bfq") == 0,
          "bfq -> bfq");
    CHECK(strcmp(wubu_iosched_algo_for("noop"), "none") == 0,
          "noop -> none");
    CHECK(strcmp(wubu_iosched_algo_for("none"), "none") == 0,
          "none -> none");
    CHECK(strcmp(wubu_iosched_algo_for("cfq"), "cfq") == 0,
          "cfq -> cfq");
    CHECK(strcmp(wubu_iosched_algo_for("zzz"), "mq-deadline") == 0,
          "zzz -> mq-deadline fallback");

    /* Mode routing. */
    CHECK(strcmp(wubu_iosched_mode_for("wrr"), "wrr") == 0,
          "wrr -> wrr");
    CHECK(strcmp(wubu_iosched_mode_for("fair"), "wrr") == 0,
          "fair -> wrr");
    CHECK(strcmp(wubu_iosched_mode_for("fifo"), "fifo") == 0,
          "fifo -> fifo");
    CHECK(strcmp(wubu_iosched_mode_for("prio"), "priority") == 0,
          "prio -> priority");
    CHECK(strcmp(wubu_iosched_mode_for("zzz"), "fifo") == 0,
          "zzz -> fifo fallback");

    char s[256];
    wubu_iosched_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "iosched summary generated");

    printf("\n=== IOSCHED TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
