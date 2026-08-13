/*
 * wubu_fusefs_selftest.c -- verifies storage FUSE routing.
 */
#include "wubu_fusefs.h"
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
    printf("=== wubu_fusefs_selftest ===\n\n");
    wubu_hw_detect();
    wubu_fusefs_probe();
    printf("  fuse=%d dev=%d mount=%d ctl=%d conn=%d\n",
           wubu_fusefs_present(), wubu_fusefs_dev(),
           wubu_fusefs_mount(), wubu_fusefs_ctl(),
           wubu_fusefs_conn());

    CHECK(strcmp(wubu_fusefs_impl_for("ssh"), "sshfs") == 0,
          "ssh -> sshfs");
    CHECK(strcmp(wubu_fusefs_impl_for("ntfs"), "ntfs-3g") == 0,
          "ntfs -> ntfs-3g");
    CHECK(strcmp(wubu_fusefs_impl_for("mp3"), "mp3fs") == 0,
          "mp3 -> mp3fs");
    CHECK(strcmp(wubu_fusefs_impl_for("iso"), "fuseiso") == 0,
          "iso -> fuseiso");
    CHECK(strcmp(wubu_fusefs_impl_for("enc"), "encfs") == 0,
          "enc -> encfs");
    CHECK(strcmp(wubu_fusefs_impl_for("bind"), "bindfs") == 0,
          "bind -> bindfs");
    CHECK(strcmp(wubu_fusefs_impl_for("zzz"), "sshfs") == 0,
          "zzz -> sshfs fallback");

    CHECK(strcmp(wubu_fusefs_op_for("getattr"), "getattr") == 0,
          "getattr -> getattr");
    CHECK(strcmp(wubu_fusefs_op_for("readdir"), "readdir") == 0,
          "readdir -> readdir");
    CHECK(strcmp(wubu_fusefs_op_for("open"), "open") == 0,
          "open -> open");
    CHECK(strcmp(wubu_fusefs_op_for("read"), "read") == 0,
          "read -> read");
    CHECK(strcmp(wubu_fusefs_op_for("write"), "write") == 0,
          "write -> write");
    CHECK(strcmp(wubu_fusefs_op_for("unlink"), "unlink") == 0,
          "unlink -> unlink");
    CHECK(strcmp(wubu_fusefs_op_for("release"), "release") == 0,
          "release -> release");
    CHECK(strcmp(wubu_fusefs_op_for("zzz"), "getattr") == 0,
          "zzz -> getattr fallback");

    char s[256];
    wubu_fusefs_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "fusefs summary generated");

    printf("\n=== FUSEFS TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
