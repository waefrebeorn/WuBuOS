/*
 * wubu_crashcap.c -- when an app dies, leave evidence. See the header for why.
 *
 * Everything here runs INSIDE a fatal signal handler, so it uses open/write/read/close and its own
 * integer formatting: no stdio, no malloc, no locale. The backtrace is captured as raw addresses
 * (backtrace_symbols allocates, which is not safe here); the report names the module with
 * `addr2line` afterwards.
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <sys/stat.h>
#include <time.h>

#if defined(__has_include)
#  if __has_include(<execinfo.h>)
#    include <execinfo.h>
#    define HAVE_BACKTRACE 1
#  endif
#endif

#include "wubu_crashcap.h"

#define CRASH_DIR_MAX 512

static char g_dir[CRASH_DIR_MAX];
static int  g_installed;
static char g_alt_stack[64 * 1024];

const char *wubu_crashcap_dir(void)
{
    if (g_dir[0]) return g_dir;
    const char *d = getenv("WUBU_CRASH_DIR");
    if (d && *d) { snprintf(g_dir, sizeof g_dir, "%s", d); return g_dir; }
    const char *st = getenv("XDG_STATE_HOME");
    if (st && *st) { snprintf(g_dir, sizeof g_dir, "%s/wubuos/crashes", st); return g_dir; }
    const char *home = getenv("HOME");
    snprintf(g_dir, sizeof g_dir, "%s/.local/state/wubuos/crashes", (home && *home) ? home : "/tmp");
    return g_dir;
}

/* mkdir -p, leaf by leaf. A crash on a fresh HOME must not be lost because ~/.local did not
 * exist -- the same failure that silently dropped every screenshot save. */
static void mkdir_p(const char *path)
{
    char tmp[CRASH_DIR_MAX];
    size_t n = strlen(path);
    if (n == 0 || n >= sizeof tmp) return;
    memcpy(tmp, path, n + 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = '\0';
        mkdir(tmp, 0755);
        *p = '/';
    }
    mkdir(tmp, 0755);
}

/* ---- async-signal-safe formatting ---------------------------------------- */
static int s_put(char *b, int cap, int at, const char *s)
{
    while (*s && at < cap - 1) b[at++] = *s++;
    return at;
}

static int s_num(char *b, int cap, int at, unsigned long v)
{
    char t[24];
    int n = 0;
    if (v == 0) { if (at < cap - 1) b[at++] = '0'; return at; }
    while (v && n < (int)sizeof t) { t[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n-- > 0 && at < cap - 1) b[at++] = t[n];
    return at;
}

static int s_hex(char *b, int cap, int at, unsigned long v)
{
    static const char hx[] = "0123456789abcdef";
    char t[20];
    int n = 0;
    if (v == 0) { if (at < cap - 1) b[at++] = '0'; return at; }
    while (v && n < (int)sizeof t) { t[n++] = hx[v & 0xf]; v >>= 4; }
    while (n-- > 0 && at < cap - 1) b[at++] = t[n];
    return at;
}

static const char *sig_name(int sig)
{
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGBUS:  return "SIGBUS";
        case SIGILL:  return "SIGILL";
        case SIGFPE:  return "SIGFPE";
        case SIGABRT: return "SIGABRT";
        default:      return "SIG?";
    }
}

/* ---- the handler -------------------------------------------------------- */
static void crash_handler(int sig, siginfo_t *info, void *uctx)
{
    (void)uctx;
    char path[1024];
    char rep[2048];
    int at = 0;

    mkdir_p(wubu_crashcap_dir());
    at = s_put(rep, sizeof rep, at, "wubuos crash report\n");
    at = s_put(rep, sizeof rep, at, "pid=");
    at = s_num(rep, sizeof rep, at, (unsigned long)getpid());
    at = s_put(rep, sizeof rep, at, " signal=");
    at = s_num(rep, sizeof rep, at, (unsigned long)sig);
    at = s_put(rep, sizeof rep, at, " ");
    at = s_put(rep, sizeof rep, at, sig_name(sig));
    at = s_put(rep, sizeof rep, at, "\nfault_addr=");
    at = s_hex(rep, sizeof rep, at, (unsigned long)(info ? (unsigned long)info->si_addr : 0UL));
    at = s_put(rep, sizeof rep, at, "\nbacktrace:\n");

#if defined(HAVE_BACKTRACE)
    {
        void *frames[32];
        int n = backtrace(frames, 32);
        for (int i = 0; i < n && at < (int)sizeof rep - 24; i++) {
            at = s_put(rep, sizeof rep, at, "  0x");
            at = s_hex(rep, sizeof rep, at, (unsigned long)frames[i]);
            at = s_put(rep, sizeof rep, at, "\n");
        }
    }
#endif

    /* The memory map at the fault names the module even without symbols. */
    {
        int m = open("/proc/self/maps", O_RDONLY);
        if (m >= 0) {
            at = s_put(rep, sizeof rep, at, "maps:\n");
            char buf[512];
            ssize_t r;
            while ((r = read(m, buf, sizeof buf)) > 0) {
                if (at + r >= (int)sizeof rep - 1) break;
                memcpy(rep + at, buf, (size_t)r);
                at += (int)r;
            }
            close(m);
        }
    }

    /* Timestamped name: crash-<epoch>-<pid>.txt. `at` is the REPORT LENGTH here -- reusing it as
     * a path index (as a first draft did) truncates the report to zero bytes, which is exactly the
     * "evidence that exists but says nothing" failure this module is meant to end. */
    snprintf(path, sizeof path, "%s/crash-%lu-%lu.txt", wubu_crashcap_dir(),
             (unsigned long)time(NULL), (unsigned long)getpid());
    int fd = open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd >= 0) {
        size_t len = (size_t)at;
        size_t off = 0;
        while (off < len) {
            ssize_t w = write(fd, rep + off, len - off);
            if (w <= 0) break;
            off += (size_t)w;
        }
        close(fd);
    }

    /* Let the shell and the host's core settings still see the signal. */
    signal(sig, SIG_DFL);
    raise(sig);
}

int wubu_crashcap_install(void)
{
    if (g_installed) return 0;

    /* A stack overflow faults with no usable stack: report from an alternate one. */
    stack_t ss;
    memset(&ss, 0, sizeof ss);
    ss.ss_sp = g_alt_stack;
    ss.ss_size = sizeof g_alt_stack;
    sigaltstack(&ss, NULL);

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = crash_handler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
    sigemptyset(&sa.sa_mask);

    int rc = 0;
    if (sigaction(SIGSEGV, &sa, NULL) != 0) rc = -1;
    if (sigaction(SIGBUS,  &sa, NULL) != 0) rc = -1;
    if (sigaction(SIGILL,  &sa, NULL) != 0) rc = -1;
    if (sigaction(SIGFPE,  &sa, NULL) != 0) rc = -1;
    if (sigaction(SIGABRT, &sa, NULL) != 0) rc = -1;
    g_installed = 1;
    return rc;
}

int wubu_crashcap_count(void)
{
    DIR *d = opendir(wubu_crashcap_dir());
    if (!d) return 0;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        size_t l = strlen(e->d_name);
        if (l > 4 && strcmp(e->d_name + l - 4, ".txt") == 0) n++;
    }
    closedir(d);
    return n;
}

/* Oldest-first removal. Names are crash-<epoch>-<pid>.txt, so a lexical sort is chronological. */
void wubu_crashcap_prune(int keep)
{
    if (keep < 0) keep = 0;
    DIR *d = opendir(wubu_crashcap_dir());
    if (!d) return;
    char names[64][64];
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && n < 64) {
        size_t l = strlen(e->d_name);
        if (l > 4 && strcmp(e->d_name + l - 4, ".txt") == 0)
            snprintf(names[n++], sizeof names[0], "%.63s", e->d_name);
    }
    closedir(d);
    /* insertion sort: at most 64 entries, and the handler must never see a crash in a sort */
    for (int i = 1; i < n; i++) {
        char tmp[64];
        snprintf(tmp, sizeof tmp, "%s", names[i]);
        int j = i - 1;
        while (j >= 0 && strcmp(names[j], tmp) > 0) {
            snprintf(names[j + 1], sizeof names[0], "%s", names[j]);
            j--;
        }
        snprintf(names[j + 1], sizeof names[0], "%s", tmp);
    }
    int over = n - keep;
    for (int i = 0; i < over; i++) {
        char path[1024];
        snprintf(path, sizeof path, "%s/%s", wubu_crashcap_dir(), names[i]);
        unlink(path);
    }
}
