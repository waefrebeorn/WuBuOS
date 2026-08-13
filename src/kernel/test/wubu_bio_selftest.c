/*
 * wubu_bio_selftest.c -- verifies storage bio routing.
 */
#include "wubu_bio.h"
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
    printf("=== wubu_bio_selftest ===\n\n");
    wubu_hw_detect();
    wubu_bio_probe();
    printf("  bio=%d vec=%d bdi=%d read=%d write=%d\n",
           wubu_bio_present(), wubu_bio_vec(), wubu_bio_bdi(),
           wubu_bio_read(), wubu_bio_write());

    CHECK(strcmp(wubu_bio_op_for("read"), "READ") == 0,
          "read -> READ");
    CHECK(strcmp(wubu_bio_op_for("write"), "WRITE") == 0,
          "write -> WRITE");
    CHECK(strcmp(wubu_bio_op_for("discard"), "DISCARD") == 0,
          "discard -> DISCARD");
    CHECK(strcmp(wubu_bio_op_for("flush"), "FLUSH") == 0,
          "flush -> FLUSH");
    CHECK(strcmp(wubu_bio_op_for("secure"), "WRITE_SECURE") == 0,
          "secure -> WRITE_SECURE");
    CHECK(strcmp(wubu_bio_op_for("zzz"), "READ") == 0,
          "zzz -> READ fallback");

    CHECK(strcmp(wubu_bio_layer_for("block"), "block") == 0,
          "block -> block");
    CHECK(strcmp(wubu_bio_layer_for("bio"), "bio") == 0,
          "bio -> bio");
    CHECK(strcmp(wubu_bio_layer_for("iov"), "iov") == 0,
          "iov -> iov");
    CHECK(strcmp(wubu_bio_layer_for("mm"), "mm") == 0,
          "mm -> mm");
    CHECK(strcmp(wubu_bio_layer_for("zzz"), "block") == 0,
          "zzz -> block fallback");

    char s[256];
    wubu_bio_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "bio summary generated");

    printf("\n=== BIO TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
