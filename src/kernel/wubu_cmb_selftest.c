/*
 * wubu_cmb_selftest.c -- verifies kernel-owned CMB routing.
 */
#include "wubu_cmb.h"
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
    printf("=== wubu_cmb_selftest ===\n\n");

    wubu_hw_detect();
    wubu_cmb_probe();

    printf("  cmb=%d nvme=%d qmem=%d pmicm=%d squeue=%d\n",
           wubu_cmb_present(), wubu_cmb_nvme(),
           wubu_cmb_qmem(), wubu_cmb_pmicm(), wubu_cmb_squeue());

    /* Register routing. */
    CHECK(strcmp(wubu_cmb_reg_for("cap1"), "cap1") == 0,
          "cap1 -> cap1");
    CHECK(strcmp(wubu_cmb_reg_for("cap2"), "cap2") == 0,
          "cap2 -> cap2");
    CHECK(strcmp(wubu_cmb_reg_for("qbr"), "qbr") == 0,
          "qbr -> qbr");
    CHECK(strcmp(wubu_cmb_reg_for("sqs"), "sqs") == 0,
          "sqs -> sqs");
    CHECK(strcmp(wubu_cmb_reg_for("cqs"), "cqs") == 0,
          "cqs -> cqs");
    CHECK(strcmp(wubu_cmb_reg_for("unknown"), "cmb-reg") == 0,
          "unknown -> cmb-reg fallback");

    /* Queue routing. */
    CHECK(strcmp(wubu_cmb_queue_for("sq"), "submission-queue") == 0,
          "sq -> submission-queue");
    CHECK(strcmp(wubu_cmb_queue_for("cq"), "completion-queue") == 0,
          "cq -> completion-queue");
    CHECK(strcmp(wubu_cmb_queue_for("unknown"), "queue") == 0,
          "unknown -> queue fallback");

    char s[256];
    wubu_cmb_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "cmb summary generated");

    printf("\n=== CMB TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
