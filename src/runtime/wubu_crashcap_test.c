/*
 * wubu_crashcap_test.c -- a crash must leave evidence, and a clean run must not.
 *
 * The point of the module is that a report exists after a process dies, so the test has to let a
 * process actually die: it forks a child, installs the handlers there, dereferences NULL, and then
 * reads what landed on disk. A handler that formats a report but never writes it (or writes zero
 * bytes) passes every in-process assertion and fails here.
 *
 * C11.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>

#include "wubu_crashcap.h"

static int g_pass, g_fail;

#define CHECK(cond, msg)                                                      \
    do {                                                                      \
        if (cond) { g_pass++; }                                               \
        else { g_fail++; fprintf(stderr, "  FAIL: %s\n", (msg)); }            \
    } while (0)

static void rmrf(const char *p)
{
    char cmd[600];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", p);
    if (system(cmd) != 0) { /* best effort */ }
}

static int count_txt(const char *dir)
{
    DIR *d = opendir(dir);
    if (!d) return -1;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t l = strlen(e->d_name);
        if (l > 4 && strcmp(e->d_name + l - 4, ".txt") == 0) n++;
    }
    closedir(d);
    return n;
}

/* Read the only (or newest) report into buf. Returns its size, 0 when none. */
static long read_report(const char *dir, char *buf, size_t cap)
{
    DIR *d = opendir(dir);
    if (!d) return 0;
    char newest[512] = "";
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t l = strlen(e->d_name);
        if (l > 4 && strcmp(e->d_name + l - 4, ".txt") == 0 && strcmp(e->d_name, newest) > 0)
            snprintf(newest, sizeof newest, "%s", e->d_name);
    }
    closedir(d);
    if (!newest[0]) return 0;
    char path[600];
    snprintf(path, sizeof path, "%s/%s", dir, newest);
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[n] = '\0';
    return (long)n;
}

int main(void)
{
    char base[] = "/tmp/wubu-crash-XXXXXX";
    char *root = mkdtemp(base);
    if (!root) { fprintf(stderr, "mkdtemp failed\n"); return 1; }
    setenv("WUBU_CRASH_DIR", root, 1);

    printf("=== wubu_crashcap_test (a dying process leaves a readable report) ===\n");

    /* 1. Nothing installed yet: no report from a clean child. */
    pid_t clean = fork();
    if (clean == 0) { _exit(0); }
    int st = 0;
    waitpid(clean, &st, 0);
    CHECK(count_txt(root) == 0, "a clean exit writes no report");

    /* 2. A child that faults must leave one. */
    pid_t crash = fork();
    if (crash == 0) {
        wubu_crashcap_install();
        volatile int *p = (volatile int *)0;
        *p = 1;                       /* the point of the test */
        _exit(0);                     /* unreachable */
    }
    waitpid(crash, &st, 0);
    CHECK(WIFSIGNALED(st), "the crashing child died from a signal, not a normal exit");
    CHECK(count_txt(root) == 1, "exactly one report was written");

    static char rep[64 * 1024];
    long n = read_report(root, rep, sizeof rep);
    CHECK(n > 100, "the report is not empty (a zero-byte file is not evidence)");
    CHECK(strstr(rep, "wubuos crash report") != NULL, "the report identifies itself");
    CHECK(strstr(rep, "SIGSEGV") != NULL, "the report names the signal");
    CHECK(strstr(rep, "pid=") != NULL, "the report carries the pid");
    CHECK(strstr(rep, "maps:") != NULL, "the report carries the memory map at the fault");
    printf("  report: %ld bytes, first line: %.60s\n", n, rep);

    /* 3. The counting API agrees with the directory. */
    CHECK(wubu_crashcap_count() == 1, "wubu_crashcap_count agrees with the directory");

    /* 4. Pruning keeps the newest `keep` and removes the rest. */
    for (int i = 0; i < 3; i++) {
        char p[600];
        snprintf(p, sizeof p, "%s/crash-%d-1.txt", root, 1000 + i);
        FILE *f = fopen(p, "w");
        if (f) { fprintf(f, "wubuos crash report\n"); fclose(f); }
    }
    CHECK(wubu_crashcap_count() == 4, "the extra reports are visible to the counter");
    wubu_crashcap_prune(2);
    CHECK(wubu_crashcap_count() == 2, "prune(2) leaves two reports");

    /* 5. The dir honours the seam and never returns NULL. */
    CHECK(wubu_crashcap_dir() != NULL && strcmp(wubu_crashcap_dir(), root) == 0,
          "the report directory honours WUBU_CRASH_DIR");
    unsetenv("WUBU_CRASH_DIR");
    /* The path is resolved once and cached: the handler must not depend on getenv() while the
     * process is dying. What matters is that it is absolute and stable, never NULL. */
    CHECK(wubu_crashcap_dir() != NULL && wubu_crashcap_dir()[0] == '/',
          "the crash directory is absolute and stable across calls");

    rmrf(root);
    printf("\n Results: %d/%d passed, %d failed\n", g_pass, g_pass + g_fail, g_fail);
    return g_fail ? 1 : 0;
}
