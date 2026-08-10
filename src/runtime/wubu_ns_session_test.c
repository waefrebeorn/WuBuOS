/*
 * wubu_ns_session_test.c -- the /n/session subtree test.
 *
 * Asserts the file<->API routing:
 *   1. publish creates /n/session/current + cmd
 *   2. echo game > /n/session/current switches + refreshes the file
 *   3. a bad session name is refused
 */
#include "wubu_ns_bridge_internal.h"
#include "wubu_ns_session.h"
#include "wubu_session.h"
#include <stdio.h>
#include <string.h>

#define NSROOT "/tmp/ns_session_test"
#define FAIL(...) do { printf("  FAIL: " __VA_ARGS__); printf("\n"); return 1; } while (0)

static int read_file(const char *p, char *out, size_t cap)
{
    FILE *f = fopen(p, "r");
    if (!f) return 0;
    size_t n = fread(out, 1, cap - 1, f);
    fclose(f);
    out[n] = '\0';
    while (n > 0 && (out[n-1] == '\n')) out[--n] = '\0';
    return 1;
}

int main(void)
{
    printf("=== wubu_ns_session_test (the /n/session subtree) ===\n");
    system("rm -rf " NSROOT);
    *(const char **)&g_ns_root = NSROOT;

    wubu_session_init(WUBU_SESSION_DESKTOP);
    if (wubu_ns_publish_session() != 0) FAIL("publish");

    /* 1. the files exist */
    char p[512], buf[256];
    snprintf(p, sizeof(p), "%s/session/current", NSROOT);
    if (!read_file(p, buf, sizeof(buf))) FAIL("no current");
    if (strcmp(buf, "desktop") != 0) FAIL("current = '%s', want desktop", buf);
    snprintf(p, sizeof(p), "%s/session/cmd", NSROOT);
    if (!read_file(p, buf, sizeof(buf))) FAIL("no cmd");
    if (!strstr(buf, "wubu-desktop")) FAIL("cmd = '%s'", buf);
    printf("  PASS: publish creates /n/session/current + cmd\n");

    /* 2. switch to game */
    if (wubu_ns_session_set("game") != 0) FAIL("set game");
    snprintf(p, sizeof(p), "%s/session/current", NSROOT);
    read_file(p, buf, sizeof(buf));
    if (strcmp(buf, "game") != 0) FAIL("current = '%s', want game", buf);
    snprintf(p, sizeof(p), "%s/session/cmd", NSROOT);
    read_file(p, buf, sizeof(buf));
    if (!strstr(buf, "gamescope")) FAIL("cmd lacks gamescope: '%s'", buf);
    printf("  PASS: echo game > /n/session/current switches the session\n");

    /* 3. back to desktop + a bad name */
    if (wubu_ns_session_set("desktop") != 0) FAIL("set desktop");
    if (wubu_ns_session_set("bogus") == 0) FAIL("bogus session accepted");
    printf("  PASS: the desktop switch + bad-name refusal\n");

    system("rm -rf " NSROOT);
    printf("=== ALL NS-SESSION TESTS PASSED ===\n");
    return 0;
}
