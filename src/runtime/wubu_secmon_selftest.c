/*
 * wubu_secmon_selftest.c — proves the syscall camera sees game calls.
 *
 *  1. init KV-FS (the substrate the camera writes into)
 *  2. fork a traced child, make real syscalls under ptrace
 *  3. attach the monitor, run it until the child exits
 *  4. assert: captured > 0, KV writes > 0, read data back
 *
 * C11, user-space compatible (links libc for I/O, only needs the KV-FS
 * + secmon kernel modules + minimal libc_string/memory).
 */
#include "wubu_secmon.h"
#include "wubu_kvfs.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

static int failures = 0;
#define CHECK(cond, msg) \
    do { if (!(cond)) { printf("  FAIL: %s\n", msg); failures++; } \
       else { printf("  ok:  %s\n", msg); } } while(0)

int main(void)
{
    printf("=== wubu_secmon_selftest (kernel syscall camera) ===\n");

    /* 1. init the KV-FS substrate */
    if (g_wubu_kvfs) { wubu_kvfs_free(g_wubu_kvfs); g_wubu_kvfs = NULL; }
    if (g_wubu_kv_base) { free(g_wubu_kv_base); g_wubu_kv_base = NULL; }
    g_wubu_kv_capacity = 0;

    if (wubu_kvfs_kernel_init(256, 4096) != 0) { CHECK(0, "kvfs init"); return 1; }
    if (wubu_kvfs_mount(g_wubu_kvfs, "/kv/agent", 2048, 2048) != 0) { CHECK(0, "mount /kv/agent"); return 1; }
    CHECK(1, "KV-FS live, /kv/agent mounted");

    /* 2. fork a child that makes real syscalls, trace it from birth */
    pid_t pid = fork();
    if (pid < 0) { CHECK(0, "fork"); return 1; }

    if (pid == 0) {
        /* child: make a deterministic set of syscalls */
        char buf[64];
        int fd = open("/etc/hostname", O_RDONLY);
        if (fd >= 0) { read(fd, buf, sizeof(buf)); close(fd); }
        getpid();
        write(1, buf, 0);
        _exit(0);
    }

    /* 3. attach the camera */
    wubu_secmon_t *m = wubu_secmon_create(0);
    if (!m) { CHECK(0, "secmon_create"); return 1; }
    CHECK(1, "secmon_create");

    if (wubu_secmon_attach(m, pid) != 0) {
        /* may lack CAP_SYS_PTRACE on some hosts */
        CHECK(0, "ptrace attach (needs CAP_SYS_PTRACE)");
        wubu_secmon_destroy(m);
        waitpid(pid, NULL, 0);
        printf("=== SKIPPED (ptrace not available) ===\n");
        return 0;
    }
    CHECK(1, "ptrace attach");

    /* 4. run the camera until the child exits */
    int spans = wubu_secmon_wait(m);
    uint64_t kv = wubu_secmon_kv_writes(m);

    CHECK(spans > 0, "captured syscalls (>0)");
    CHECK(kv > 0, "KV-FS writes (>0)");

    /* 5. read one span back through the KV tensor */
    float vec[6];
    char path[128];
    snprintf(path, sizeof(path), "/kv/agent/sys_%d/000000", (int)pid);

    if (wubu_kvfs_read(g_wubu_kvfs, path, g_wubu_kv_base, vec, 6) == 0) {
        CHECK(1, "read span from KV-FS");
        printf("   first span: kind=%d nr=%d pid=%d\n",
               (int)vec[0], (int)vec[1], (int)vec[5]);
    } else {
        CHECK(0, "read span from KV-FS");
    }

    wubu_secmon_destroy(m);
    waitpid(pid, NULL, 0);

    /* cleanup */
    if (g_wubu_kvfs) wubu_kvfs_free(g_wubu_kvfs);
    if (g_wubu_kv_base) free(g_wubu_kv_base);

    printf("\n=== SECMON TESTS: %s (%d failures) ===\n",
           failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
