/*
 * wubu_ioprio_selftest.c -- verifies I/O priority routing.
 */
#include "wubu_ioprio.h"
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
    printf("=== wubu_ioprio_selftest ===\n\n");
    wubu_hw_detect();
    wubu_ioprio_probe();
    printf("  iop=%d rt=%d be=%d idle=%d sched=%d\n",
           wubu_ioprio_present(), wubu_ioprio_rt(), wubu_ioprio_be(),
           wubu_ioprio_idle(), wubu_ioprio_sched());

    CHECK(strcmp(wubu_ioprio_class_for("rt"), "rt") == 0,
          "rt -> rt");
    CHECK(strcmp(wubu_ioprio_class_for("realtime"), "rt") == 0,
          "realtime -> rt");
    CHECK(strcmp(wubu_ioprio_class_for("be"), "be") == 0,
          "be -> be");
    CHECK(strcmp(wubu_ioprio_class_for("best-effort"), "be") == 0,
          "best-effort -> be");
    CHECK(strcmp(wubu_ioprio_class_for("idle"), "idle") == 0,
          "idle -> idle");
    CHECK(strcmp(wubu_ioprio_class_for("none"), "none") == 0,
          "none -> none");
    CHECK(strcmp(wubu_ioprio_class_for("zzz"), "be") == 0,
          "zzz -> be fallback");

    CHECK(strcmp(wubu_ioprio_sched_for("noop"), "noop") == 0,
          "noop -> noop");
    CHECK(strcmp(wubu_ioprio_sched_for("deadline"), "deadline") == 0,
          "deadline -> deadline");
    CHECK(strcmp(wubu_ioprio_sched_for("cfq"), "cfq") == 0,
          "cfq -> cfq");
    CHECK(strcmp(wubu_ioprio_sched_for("mq-deadline"), "mq-deadline") == 0,
          "mq-deadline -> mq-deadline");
    CHECK(strcmp(wubu_ioprio_sched_for("bfq"), "bfq") == 0,
          "bfq -> bfq");
    CHECK(strcmp(wubu_ioprio_sched_for("kyber"), "kyber") == 0,
          "kyber -> kyber");
    CHECK(strcmp(wubu_ioprio_sched_for("zzz"), "mq-deadline") == 0,
          "zzz -> mq-deadline fallback");

    char s[256];
    wubu_ioprio_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "ioprio summary generated");

    printf("\n=== IOPRIO TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
