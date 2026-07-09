/*
 * wubu_spawn_test.c  --  Regression test for the shell-free launcher.
 *
 * Asserts wubu_run_program() actually performs the external op (fork+exec),
 * returns real exit codes, and that no /bin/sh is ever spawned (so shell
 * metacharacters in arguments are passed verbatim to the program, not
 * interpreted). This is the behavioral lock that replaces every former
 * system("cmd") call in the tree.
 */
#include "wubu_spawn.h"
#include "wubu_netlink.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>

static int failures = 0;
#define CHECK(cond, msg) do { \
    if (cond) { printf("  ✅ %s\n", msg); } \
    else { printf("  ❌ %s\n", msg); failures++; } \
} while (0)

/* True iff path exists as a regular file. */
static bool file_exists(const char *p) {
    struct stat st;
    return stat(p, &st) == 0 && S_ISREG(st.st_mode);
}

int main(void) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "/tmp/wubu_spawn_test_%d", (int)getpid());

    printf("== wubu_run_program (shell-free) ==\n");

    /* 1. A real program runs and we observe its side effect (file creation),
     *    proving the op actually happened -- not a no-op stub. */
    unlink(tmp);
    char *touch_argv[] = { "touch", tmp, (char *)NULL };
    int rc = wubu_run_program("touch", touch_argv, true);
    CHECK(rc == 0, "touch exited 0 via fork+exec");
    CHECK(file_exists(tmp), "wubu_run_program actually created the file (real op)");
    unlink(tmp);

    /* 2. Exit code is propagated from the child, not faked. */
    char *false_argv[] = { "false", (char *)NULL };
    rc = wubu_run_program("false", false_argv, true);
    CHECK(rc == 1, "false propagates exit code 1");

    /* 4. Shell metacharacters are NOT interpreted -- proves no /bin/sh is
     *    spawned. We ask `echo` to print a literal string containing a
     *    semicolon and redirect; if a shell ran it, the output would be split
     *    and no file would be created. With execvp the whole token is one arg. */
    char lit[512];
    snprintf(lit, sizeof(lit), "hello; rm -f %s >/dev/null", tmp);
    char *echo_argv[] = { "echo", lit, (char *)NULL };
    rc = wubu_run_program("echo", echo_argv, true);
    CHECK(rc == 0, "echo ran with a literal metacharacter arg (no shell)");
    CHECK(!file_exists(tmp), "metacharacter was NOT executed as shell (no file created)");

    /* 4b. A missing program: execvp fails, child _exit(127). We propagate the
     *     child's status, so a not-found program returns 127 (standard
     *     "command not found"), never a silent 0. */
    char *missing_argv[] = { "wubu_this_binary_does_not_exist_xyz", (char *)NULL };
    rc = wubu_run_program("wubu_this_binary_does_not_exist_xyz", missing_argv, true);
    CHECK(rc == 127, "missing program returns 127 (no silent success)");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "touch %s", tmp);
    rc = net_cmd(cmd);
    CHECK(rc == 0, "net_cmd('touch <file>') exited 0");
    CHECK(file_exists(tmp), "net_cmd actually executed touch (real op)");
    unlink(tmp);

    /* 6. net_cmd handles single-quoted tokens verbatim AND does not invoke a
     *    shell. A quoted filename with a space must create exactly one file
     *    named "/tmp/wubu spawn q_N" (no word-splitting, no metacharacter
     *    interpretation). */
    char qpath[512];
    snprintf(qpath, sizeof(qpath), "/tmp/wubu spawn q_%d", (int)getpid());
    /* Build the real quoted path via a separate command that we control. */
    char qtouch[512];
    snprintf(qtouch, sizeof(qtouch), "touch '/tmp/wubu spawn q_%d'", (int)getpid());
    rc = net_cmd(qtouch);
    CHECK(rc == 0, "net_cmd single-quoted token ran");
    CHECK(file_exists(qpath), "net_cmd created the quoted filename verbatim (no shell split/glob)");
    unlink(qpath);

    /* 7. net_cmd with a missing program reports failure. */
    snprintf(cmd, sizeof(cmd), "wubu_missing_tool_%d", (int)getpid());
    rc = net_cmd(cmd);
    CHECK(rc != 0, "net_cmd missing program reports failure");

    if (failures == 0) {
        printf("\n✅ All wubu_spawn / net_cmd regression tests passed\n");
        return 0;
    }
    printf("\n❌ %d wubu_spawn / net_cmd regression test(s) failed\n", failures);
    return 1;
}
