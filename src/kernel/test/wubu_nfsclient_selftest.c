/*
 * wubu_nfsclient_selftest.c -- verifies NFS client routing.
 */
#include "wubu_nfsclient.h"
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
    printf("=== wubu_nfsclient_selftest ===\n\n");
    wubu_hw_detect();
    wubu_nfsclient_probe();
    printf("  nfs=%d idmapd=%d statd=%d mount=%d sec=%d\n",
           wubu_nfsclient_present(), wubu_nfsclient_idmapd(),
           wubu_nfsclient_statd(), wubu_nfsclient_mount(),
           wubu_nfsclient_sec());

    CHECK(strcmp(wubu_nfsclient_vers_for("4.2"), "4.2") == 0,
          "4.2 -> 4.2");
    CHECK(strcmp(wubu_nfsclient_vers_for("4.1"), "4.1") == 0,
          "4.1 -> 4.1");
    CHECK(strcmp(wubu_nfsclient_vers_for("4.0"), "4.0") == 0,
          "4.0 -> 4.0");
    CHECK(strcmp(wubu_nfsclient_vers_for("3"), "3") == 0,
          "3 -> 3");
    CHECK(strcmp(wubu_nfsclient_vers_for("2"), "2") == 0,
          "2 -> 2");
    CHECK(strcmp(wubu_nfsclient_vers_for("zzz"), "4.0") == 0,
          "zzz -> 4.0 fallback");

    CHECK(strcmp(wubu_nfsclient_proto_for("tcp"), "tcp") == 0,
          "tcp -> tcp");
    CHECK(strcmp(wubu_nfsclient_proto_for("udp"), "udp") == 0,
          "udp -> udp");
    CHECK(strcmp(wubu_nfsclient_proto_for("rdma"), "rdma") == 0,
          "rdma -> rdma");
    CHECK(strcmp(wubu_nfsclient_proto_for("zzz"), "tcp") == 0,
          "zzz -> tcp fallback");

    char s[256];
    wubu_nfsclient_summary(s, sizeof(s));
    printf("  summary: %s\n", s);
    CHECK(s[0] != '\0', "nfsclient summary generated");

    printf("\n=== NFSCLIENT TESTS: %d passed, %d failed ===\n", passed, failures);
    return failures ? 1 : 0;
}
