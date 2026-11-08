/*
 * wubu_daemon_client_test.c  --  behavior gate for the shared daemon wire client
 *
 * Exercises wubu_json_get_string against real daemon-shaped JSON lines and
 * exercises the line-accumulator path via a loopback (socketpair) so the
 * recv_line buffering is tested against an actual fd, not a stub.
 */

#define _POSIX_C_SOURCE 200809L

#include "wubu_daemon_client.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

static int g_fail = 0;
#define CHECK(cond, msg) do { \
    if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); g_fail++; } \
} while (0)

static void test_json_get_string(void) {
    char val[64];

    const char *s1 = "{\"cmd\":\"subscribe\",\"events\":[\"root-create\",\"root-state\"]}\n";
    /* no target string val for cmd here (array field); that's fine — must not crash */
    wubu_json_get_string(s1, "cmd", val, sizeof(val));

    const char *s2 = "{\"event\":\"root-create\",\"root_name\":\"alpine\",\"state\":\"active\"}\n";
    char e[16], n[32], st[16];
    CHECK(wubu_json_get_string(s2, "event", e, sizeof(e)) != NULL, "event key found");
    CHECK(strcmp(e, "root-create") == 0, "event value");
    CHECK(wubu_json_get_string(s2, "root_name", n, sizeof(n)) != NULL, "root_name key found");
    CHECK(strcmp(n, "alpine") == 0, "root_name value");
    CHECK(wubu_json_get_string(s2, "state", st, sizeof(st)) != NULL, "state key found");
    CHECK(strcmp(st, "active") == 0, "state value");

    /* absent key -> NULL + empty */
    char miss[16];
    CHECK(wubu_json_get_string(s2, "nope", miss, sizeof(miss)) == NULL, "absent key -> NULL");
    CHECK(miss[0] == '\0', "absent key leaves empty");

    /* holyd-shaped line */
    const char *s3 = "{\"event\":\"session-state\",\"session\":\"demo\",\"state\":\"running\"}\n";
    char sess[32], sst[16];
    CHECK(strcmp(wubu_json_get_string(s3, "session", sess, sizeof(sess)), "demo") == 0, "holyd session");
    CHECK(strcmp(wubu_json_get_string(s3, "state", sst, sizeof(sst)), "running") == 0, "holyd state");

    /* whitespace tolerance: "event" : "x" */
    const char *s4 = "{ \"event\" : \"x\" }\n";
    char ev[8];
    CHECK(strcmp(wubu_json_get_string(s4, "event", ev, sizeof(ev)), "x") == 0, "whitespace tolerant");
}

static void test_recv_line_buffering(void) {
    int sv[2];
    CHECK(socketpair(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0, sv) == 0, "socketpair created");

    /* two whole lines + a partial, all sent at once -> recv_line must parse
     * both complete lines and hold the partial until the rest arrives */
    const char *burst = "line-one here\npartial-tw";
    CHECK(write(sv[0], burst, strlen(burst)) == (ssize_t)strlen(burst), "wrote burst");

    DaemonConn c = {0};
    c.fd = sv[1];
    c.state = DAEMON_CONNECTED;

    char buf[256];
    int n1 = wubu_daemon_recv_line(&c, buf, sizeof(buf));
    CHECK(n1 > 0 && strcmp(buf, "line-one here") == 0, "first line parsed");

    /* second read: the partial has no newline yet -> 0, buffered */
    int n2 = wubu_daemon_recv_line(&c, buf, sizeof(buf));
    CHECK(n2 == 0, "partial held (0 returned)");

    /* finish the partial line */
    CHECK(write(sv[0], "o\n", 2) == 2, "finished partial");
    int n3 = wubu_daemon_recv_line(&c, buf, sizeof(buf));
    CHECK(n3 > 0 && strcmp(buf, "partial-two") == 0, "assembled partial line");

    close(sv[0]);
    close(sv[1]);
}

int main(void) {
    test_json_get_string();
    test_recv_line_buffering();

    if (g_fail) {
        fprintf(stderr, "wubu_daemon_client_test: %d FAILURES\n", g_fail);
        return 1;
    }
    printf("wubu_daemon_client_test: ALL PASS (json get-string, line buffering)\n");
    return 0;
}
