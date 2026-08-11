/*
 * wubu_secmon_selftest.c — proves the syscall camera sees game calls.
 *
 *  1. init KV-FS (the substrate the camera writes into)
 *  2. fork a traced child, make real syscalls under ptrace
 *  3. attach the monitor, run it until the child exits
 *  4. assert: captured > 0, KV writes > 0, read data back
 *
 * C11, kernel-compatible (uses klog for output, minimal includes).
 */
#include "wubu_secmon.h"
#include "wubu_kvfs.h"
#include "klog.h"

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

/* Minimal string output */
static void out(const char *s) { while (*s) { klog_write_n(s, 1); s++; } }
static int g_pass = 0, g_fail = 0, g_total = 0;

#define TEST(name) out("  TEST "); out(name); out("\n"); g_total++
#define PASS() out("  PASS\n"), g_pass++
#define FAIL(msg) out("  FAIL: "), out(msg), out("\n"), g_fail++

int main(void)
{
    out("\n");
    out("=== wubu_secmon_selftest (kernel syscall camera) ===\n");

    /* 1. init the KV-FS substrate */
    if (g_wubu_kvfs) { wubu_kvfs_free(g_wubu_kvfs); g_wubu_kvfs = NULL; }
    if (g_wubu_kv_base) { free(g_wubu_kv_base); g_wubu_kv_base = NULL; }
    g_wubu_kv_capacity = 0;

    if (wubu_kvfs_kernel_init(6, 4096) != 0) { FAIL("kvfs init"); return 1; }
    if (wubu_kvfs_mount(g_wubu_kvfs, "/kv/agent", 2048, 2048) != 0) { FAIL("mount /kv/agent"); return 1; }
    TEST("KV-FS mounted");

    /* 2. fork a traced child */
    pid_t pid = fork();
    if (pid < 0) { FAIL("fork"); return 1; }

    if (pid == 0) {
        char buf[64];
        int fd = open("/etc/hostname", O_RDONLY);
        if (fd >= 0) { read(fd, buf, sizeof(buf)); close(fd); }
        getpid();
        _exit(0);
    }

    wubu_secmon_t *m = wubu_secmon_create(0);
    if (!m) { FAIL("secmon create"); return 1; }
    TEST("secmon_create");

    if (wubu_secmon_attach(m, pid) != 0) { FAIL("ptrace attach"); return 1; }
    TEST("ptrace attach");

    /* 3. run the camera until the child exits */
    int spans = wubu_secmon_wait(m);
    uint64_t kv = wubu_secmon_kv_writes(m);

    /* 4. verify data exists */
    if (spans <= 0) { FAIL("no syscalls captured"); } else TEST("captured syscalls (>0)");
    if (kv <= 0) { FAIL("no KV-FS writes"); } else TEST("KV-FS writes (>0)");

    /* read one span back */
    float vec[6];
    char path[128];
    snprintf(path, sizeof(path), "/kv/agent/sys_%d/000000", (int)pid);
    if (wubu_kvfs_read(g_wubu_kvfs, path, g_wubu_kv_base, vec, 6) == 0) {
        TEST("read span from KV-FS");
    } else {
        FAIL("read span from KV-FS");
    }

    wubu_secmon_destroy(m);
    waitpid(pid, NULL, 0);

    /* cleanup */
    if (g_wubu_kvfs) wubu_kvfs_free(g_wubu_kvfs);
    if (g_wubu_kv_base) free(g_wubu_kv_base);

    char buf[128];
    snprintf(buf, sizeof(buf), "%d passed, %d failed, %d total\n", g_pass, g_fail, g_total);
    out(buf);

    if (g_fail == 0) {
        out("PASS: kernel syscall camera works\n");
        return 0;
    } else {
        out("FAIL: see above\n");
        return 1;
    }
}