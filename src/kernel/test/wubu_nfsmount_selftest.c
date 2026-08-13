/*
 * wubu_nfsmount_selftest.c -- verifies NFS mount routing.
 */
#include "wubu_nfsmount.h"
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
    printf("=== wubu_nfsmount_selftest ===\n\n");
    wubu_hw_detect();
    wubu_nfsmount_probe();
    printf("  nfs=%d mount=%d vers=%d rsize=%d wsize=%d\n",
           wubu_nfsmount_present(), wubu_nfsmount_mount(),
           wubu_nfsmount_vers(), wubu_nfsmount_rsize(),
           wubu_nfsmount_wsize());

    CHECK(strcmp(wubu_nfsmount_vers_for("4.1"), "4.1") == 0,
          "4.1 -> 4.1");
    CHECK(strcmp(wubu_nfsmount_vers_for("4.2"), "4.2") == 0,
          "4.2 -> 4.2");
    CHECK(strcmp(wubu_nfsmount_vers_for("4.0"), "4.0") == 0,
          "4.0 -> 4.0");
    CHECK(strcmp(wubu_nfsmount_vers_for("3"), "3") == 0,
          "3 -> 3");
    CHECK(strcmp(wubu_nfsmount_vers_for("2"), "2") == 0,
          "2 -> 2");
    CHECK(strcmp(wubu_nfsmount_vers_for("zzz"), "4.0") == 0,
          "zzz -> 4.0 fallback");

    CHECK(strcmp(wubu_nfsmount_opt_for("rsize"), "rsize") == 0,
          "rsize -> rsize");
    CHECK(strcmp(wubu_nfsmount_opt_for("wsize"), "wsize") == 0,
          "wsize -> wsize");
    CHECK(strcmp(wubu_nfsmount_opt_for("timeo"), "timeo") == 0,
          "timeo -> timeo");
    CHECK(strcmp(wubu_nfsmount_opt_for("intr"), "intr") == 0,
          "intr -> intr");
    CHECK(strcmp(wubu_nfsmount_opt_for("hard"), "hard") == 0,
          "hard -> hard");
    CHECK(strcmp(wubu_nfsmount_opt_for("soft"), "soft") == 0,
          "soft -> soft");
    CHECK(strcmp(wubu_nfsmount_opt_for("ac"), "attr-cache") == 0,
          "ac -> attr-cache");
    CHECK(strcmp(wubu_nfsmount_opt_for("zzz"), "rsize") == 0,
          "zzz -> rsize fallback");

    char s[256];
    wubu_nfsmount_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "nfsmount summary generated");

    printf("\n=== NFSMOUNT TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
