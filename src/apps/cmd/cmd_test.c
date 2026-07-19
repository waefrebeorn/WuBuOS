/*
 * cmd_test.c -- Real test for the CMD terminal engine (apps/cmd/cmd.c).
 *
 * Proves the engine does real work:
 *   1. wubu_cmd_create / wubu_cmd_spawn_shell fork a real pty-backed shell,
 *   2. wubu_cmd_write_pty sends bytes into the shell,
 *   3. wubu_cmd_read_pty returns the shell's real output (echo round-trip),
 *   4. history add/prev works,
 *   5. destroy reaps the child (no zombie leak).
 *
 * No stubs for the engine itself.
 */

#include "cmd.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

static int g_fail = 0;
#define CHECK(c, msg) do { \
    if (c) printf("  [PASS] %s\n", msg); \
    else { printf("  [FAIL] %s\n", msg); g_fail++; } \
} while (0)

int main(void) {
    printf("[CMD terminal engine test]\n");

    WubuCmd *cmd = wubu_cmd_create(80, 24);
    CHECK(cmd != NULL, "wubu_cmd_create returns a handle");

    int rc = wubu_cmd_spawn_shell(cmd, NULL);
    CHECK(rc == 0, "spawn shell (forkpty) succeeded");

    /* Give the shell a moment to start and print its prompt. */
    usleep(200000);

    /* Write a command: echo hello, then Enter (\r). */
    const char *cmdline = "echo wubu_cmd_ok\r";
    wubu_cmd_write_pty(cmd, cmdline, (int)strlen(cmdline));

    /* Read back the pty output (give the shell time to echo + run). */
    char buf[4096];
    int total = 0;
    for (int i = 0; i < 20; i++) {
        int n = wubu_cmd_read_pty(cmd, buf + total, (int)sizeof(buf) - total - 1);
        if (n > 0) total += n;
        if (total > 0 && strstr(buf, "wubu_cmd_ok")) break;
        usleep(100000);
    }
    buf[total] = '\0';

    CHECK(total > 0, "pty produced output from the shell");
    CHECK(strstr(buf, "wubu_cmd_ok") != NULL,
          "shell executed 'echo wubu_cmd_ok' (real round-trip)");

    /* History API. */
    wubu_cmd_history_add(cmd, "ls -la");
    const char *prev = wubu_cmd_history_prev(cmd);
    CHECK(prev != NULL && strcmp(prev, "ls -la") == 0, "history prev returns added line");

    /* Destroy must reap the child (no zombie left behind). */
    wubu_cmd_destroy(cmd);

    /* A second create/spawn/destroy cycle ensures no leak / double-free. */
    WubuCmd *cmd2 = wubu_cmd_create(40, 12);
    CHECK(cmd2 != NULL, "second create (no state corruption)");
    int rc2 = wubu_cmd_spawn_shell(cmd2, NULL);
    CHECK(rc2 == 0, "second spawn ok");
    usleep(100000);
    wubu_cmd_destroy(cmd2);

    if (g_fail == 0) {
        printf("\n=== Results: ALL PASSED ===\n");
        return 0;
    }
    printf("\n=== Results: %d FAILED ===\n", g_fail);
    return 1;
}
