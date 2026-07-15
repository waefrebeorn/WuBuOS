/*
 * wubu_shell_test.c -- WuBuOS shell test suite
 *
 * Exercises the four previously-form-not-function shell gaps (BATTLESHIP
 * 228-231): command history, tab completion, pipelines, and I/O redirection.
 *
 * Pure functions (history, completion, parser) are asserted directly. The
 * fork/exec engine (pipelines, redirection, ';') is verified via real
 * side-effects: exit status codes and on-disk file contents produced by the
 * genuine engine (not stubs).
 *
 * Build/run: make test_shell
 */

#include "wubu_shell_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* ---- tiny test harness ---- */

static int g_pass;
static int g_fail;

#define TEST(name) do { printf("TEST %s ... ", name); } while(0)
#define ASSERT(cond, why) do { \
    if (cond) { printf("PASS\n"); g_pass++; } \
    else { printf("FAIL (%s)\n", why); g_fail++; } \
} while (0)

static int file_contains(const char *path, const char *needle) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char buf[4096];
    int found = 0;
    while (fgets(buf, sizeof buf, f)) {
        if (strstr(buf, needle)) { found = 1; break; }
    }
    fclose(f);
    return found;
}

static void rmfile(const char *p) { unlink(p); }

/* ---- tests: pure functions ---- */

static void test_history(void) {
    TEST("history records and recalls lines (gap 228)");
    ShellState *st = shell_state_new();
    shell_dispatch(st, "echo one");
    shell_dispatch(st, "echo two");
    int c = shell_history_count(st);
    /* history ring stores the dispatched lines */
    int ok = (c == 2);
    /* verify the ring content via history_prev navigation */
    const char *h = shell_history_prev(st);   /* newest: echo two */
    ok = ok && h && strcmp(h, "echo two") == 0;
    h = shell_history_prev(st);                /* older: echo one */
    ok = ok && h && strcmp(h, "echo one") == 0;
    ASSERT(ok, "history count/navigation wrong");
    shell_state_free(st);
}

static void test_completion(void) {
    TEST("tab completion resolves command prefix (gap 229)");
    ShellState *st = shell_state_new();
    char out[64];
    int n = shell_complete(st, "ech", out, sizeof out);
    ASSERT(n == 1 && strcmp(out, "echo") == 0, "ech -> echo (exact)");

    n = shell_complete(st, "zzz", out, sizeof out);
    ASSERT(n == 0 && out[0] == '\0', "zzz -> none");

    char cands[8][1024];
    int m = shell_complete_list("e", cands, 8);
    int has_echo = 0;
    for (int i = 0; i < m; i++) if (strcmp(cands[i], "echo") == 0) has_echo = 1;
    ASSERT(m > 0 && has_echo, "list 'e' includes echo");
    shell_state_free(st);
}

/* ---- tests: fork/exec engine via real side-effects ---- */

static void test_pipe(void) {
    TEST("pipeline through two stages (gap 230)");
    ShellState *st = shell_state_new();
    /* echo a marker, pipe through tr -> uppercase, redirect to file, then
     * verify the file holds the uppercased marker. */
    rmfile("/tmp/wubu_sh_pipe.txt");
    int rc = shell_exec_pipeline(st, "echo wubupipe | tr a-z A-Z > /tmp/wubu_sh_pipe.txt");
    int ok = (rc == 0) && file_contains("/tmp/wubu_sh_pipe.txt", "WUBUPIPE");
    ASSERT(ok, "pipe+redirect produced wrong/empty file");
    rmfile("/tmp/wubu_sh_pipe.txt");
    shell_state_free(st);
}

static void test_redirect(void) {
    TEST("stdout redirection to file (gap 231a)");
    ShellState *st = shell_state_new();
    rmfile("/tmp/wubu_sh_out.txt");
    int rc1 = shell_exec_pipeline(st, "echo hello > /tmp/wubu_sh_out.txt");
    int ok_out = (rc1 == 0) && file_contains("/tmp/wubu_sh_out.txt", "hello");
    ASSERT(ok_out, "stdout redirect content wrong");
    rmfile("/tmp/wubu_sh_out.txt");

    TEST("stderr redirection to file 2> (gap 231b)");
    rmfile("/tmp/wubu_sh_err.txt");
    int rc2 = shell_exec_pipeline(st, "ls /no_such_path_xyz 2>/tmp/wubu_sh_err.txt");
    int ok_err = (rc2 != 0) && file_contains("/tmp/wubu_sh_err.txt", "No such file");
    ASSERT(ok_err, "stderr redirect empty/incorrect");

    TEST("fd duplication 2>&1 into redirect (gap 231c)");
    rmfile("/tmp/wubu_sh_merge.txt");
    int rc3 = shell_exec_pipeline(st, "ls /no_such_path_xyz > /tmp/wubu_sh_merge.txt 2>&1");
    int ok_merge = (rc3 != 0) && file_contains("/tmp/wubu_sh_merge.txt", "No such file");
    ASSERT(ok_merge, "2>&1 did not merge stderr into redirect file");
    rmfile("/tmp/wubu_sh_merge.txt");
    rmfile("/tmp/wubu_sh_err.txt");
    rmfile("/tmp/wubu_sh_merge.txt");
    shell_state_free(st);
}

static void test_semicolon(void) {
    TEST("command lists separated by ';'");
    ShellState *st = shell_state_new();
    rmfile("/tmp/wubu_sh_a.txt");
    rmfile("/tmp/wubu_sh_b.txt");
    int rc = shell_exec_pipeline(st, "echo aa > /tmp/wubu_sh_a.txt ; echo bb > /tmp/wubu_sh_b.txt");
    int ok = (rc == 0) &&
             file_contains("/tmp/wubu_sh_a.txt", "aa") &&
             file_contains("/tmp/wubu_sh_b.txt", "bb");
    ASSERT(ok, "semicolon list did not run both sides");
    rmfile("/tmp/wubu_sh_a.txt");
    rmfile("/tmp/wubu_sh_b.txt");
    shell_state_free(st);
}

int main(void) {
    g_pass = g_fail = 0;
    test_history();
    test_completion();
    test_pipe();
    test_redirect();
    test_semicolon();
    printf("\n=== shell test: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
