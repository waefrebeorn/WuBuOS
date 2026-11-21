/* Fresh-process gate + diagnosis probe for the REAL glibc hello path.
 *
 * DIAGNOSIS (backing OFF, reproduced 3/3 runs):
 *   3 trapped syscalls: fstat(5), getrandom(318), mmap(9).
 *   glibc's malloc calls mmap(NULL, 0x100000, PROT_READ|WRITE,
 *   MAP_PRIVATE|MAP_ANONYMOUS, -1, 0); VSL returns 0x3000000 but registers
 *   only -- no host pages.  glibc faults at 0x3000008, RIP 0x21acb09 on
 *   `mov %rax,0x8(%r9)`: the top-chunk size write of the main arena.  The
 *   owning missing state is the ALLOCATOR / guest-mmap backing, NOT FILE
 *   /stdout or interpreter bootstrap: relocation is complete and all 3098
 *   real-libc exports bind before the fault.
 *
 * This probe MUST run as a fresh process.  Run late in the big single-process
 * VSL test binary the real hello guest spins in accumulated engine state.
 * test_vsl_exec therefore builds and execs this file as its own process.
 *
 * Build from /home/wubu/wubuos-leg-glibc-fault300 (see mk/tests.mk):
 *   gcc -O0 -g -D_POSIX_C_SOURCE=200809L -include wubu_gnu_compat.h \
 *       -DHAVE_VULKAN -DHAVE_CUDA -Isrc/runtime -Isrc/runtime/vsl -Iinclude \
 *       src/runtime/wubu_crc32.c $(VSL_EXEC_OBJS) src/runtime/wubu_kernel_xchg.o \
 *       src/runtime/vsl/probe_ifunc_glibc_hello.c -o /tmp/probe -ldl -lvulkan -lcuda
 *   timeout 40 /tmp/probe src/runtime/vsl/exec_guests
 */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vsl_exec_engine.h"

static int passed, failed;
#define CHECK(cond, msg) do { \
    if (cond) { passed++; printf("  ok: %s\n", (msg)); } \
    else { failed++; printf("  FAIL: %s\n", (msg)); } \
} while (0)

static void report(const char *tag, int rc, const VslExecResult *r) {
    printf("[%s] rc=%d status=%d exit=%d interp=%u deps=%u relocs=%u syms=%u "
           "fault=0x%llx rip=0x%llx syscalls=%llu backed=%u bytes=%llu "
           "failure=\"%s\" out_len=%zu out=\"%.*s\"\n",
           tag, rc, r->status, r->exit_code, r->interp_loaded, r->deps_loaded,
           r->relocations_applied, r->symbols_resolved,
           (unsigned long long)r->fault_addr, (unsigned long long)r->fault_rip,
           (unsigned long long)r->syscalls_trapped, r->mmap_backed,
           (unsigned long long)r->mmap_backed_bytes, r->mmap_failure,
           r->output_len, (int)(r->output_len < 32 ? r->output_len : 32),
           r->output);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "src/runtime/vsl/exec_guests";
    static const char hello[] = "VSL-HELLO-DYNAMIC-WORLD";
    char root[512], realroot[512], path[512];
    snprintf(root, sizeof(root), "%s/glibc_rootfs", dir);
    snprintf(realroot, sizeof(realroot), "%s/glibc_rootfs_reallibc", dir);
    const char *none[]       = { NULL };
    const char *real_first[] = { realroot, root, NULL };
    VslImageCfg cfg = { none, 0, 0, NULL, 0, NULL, real_first };
    snprintf(path, sizeof(path), "%s/guest_hello.elf", dir);
    FILE *fp = fopen(path, "rb");
    if (!fp) { printf("no fixture %s\n", path); return 2; }
    fseek(fp, 0, SEEK_END);
    long n = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    unsigned char *buf = malloc((size_t)n);
    if (fread(buf, 1, (size_t)n, fp) != (size_t)n) { printf("short read\n"); return 2; }
    fclose(fp);

    vsl_dyn_set_interp(1);
    vsl_dyn_set_interp_bind_main(1);
    vsl_dyn_set_init_fini(0);
    vsl_dyn_set_fs_tls(0);
    vsl_dyn_set_libc_rootfs(1);
    vsl_dyn_set_glibc_startup(1);
    VslExecResult r;

    /* NEGATIVE CONTROL: the pre-change byte-for-byte path. */
    vsl_dyn_set_mmap_backing(0);
    int rc_off = vsl_exec_image(buf, (size_t)n, &cfg, &r);
    report("backing OFF", rc_off, &r);
    CHECK(rc_off == VSL_EXEC_ERR_FAULT && r.fault_addr == 0x3000008 &&
          r.fault_rip != 0 && r.syscalls_trapped == 3 &&
          r.mmap_backed == 0 && r.mmap_failure[0] == 0,
          "OFF reproduces the exact unbacked-allocator fault at 0x3000008");

    /* POSITIVE: one bounded zeroed host range materialized at the guest
     * address, real glibc runs to exit(0) and emits the exact hello bytes. */
    vsl_dyn_set_mmap_backing(1);
    int rc_on = vsl_exec_image(buf, (size_t)n, &cfg, &r);
    report("backing ON", rc_on, &r);
    CHECK(rc_on == VSL_EXEC_OK && r.exit_code == 0 && r.fault_addr == 0,
          "real glibc hello reaches exit(0) once its allocator has host pages");
    CHECK(r.mmap_backed == 1 && r.mmap_backed_bytes == 0x100000 &&
          r.mmap_backed_bytes <= vsl_dyn_mmap_backing_max(),
          "exactly one bounded 1 MiB guest map was host-backed");
    CHECK(r.output_len >= sizeof(hello) - 1 &&
          memcmp(r.output, hello, sizeof(hello) - 1) == 0,
          "real glibc wrote the exact hello bytes at output offset 0");
    /* out_len also carries trailing bytes from the real libc's unmodelled
     * stdio flush at exit (a bogus write count, then -EFAULT).  Only the
     * hello prefix is asserted; the FILE/stdout state is a remaining gap. */
    CHECK(r.mmap_failure[0] == 0, "no backing refusal on the default budget");
    CHECK(rc_on != rc_off, "differential: host backing advances the real hello");

    /* FAIL-CLOSED BOUNDS: the budget setter cannot grow past the ceiling,
     * and a zero budget refuses backing with an exact addr/size diagnostic. */
    CHECK(vsl_dyn_set_mmap_backing_max(VSL_MMAP_BACK_CEILING + 1ULL) == -1,
          "budget setter refuses to grow past the fixed ceiling");
    CHECK(vsl_dyn_set_mmap_backing_max(0) == 0 && vsl_dyn_mmap_backing_max() == 0,
          "bounded setter lowers the effective budget to zero");
    int rc_zero = vsl_exec_image(buf, (size_t)n, &cfg, &r);
    report("budget 0", rc_zero, &r);
    CHECK(r.mmap_backed == 0 &&
          strstr(r.mmap_failure, "guest mmap backing:") != NULL &&
          strstr(r.mmap_failure, "addr=0x") != NULL &&
          strstr(r.mmap_failure, "size=0x") != NULL,
          "zero budget fails closed with an exact addr/size diagnostic");

    /* RESTORE + RE-RUN: the ceiling is restored and no host page leaks into
     * the next run -- the exact positive boundary returns. */
    vsl_dyn_set_mmap_backing_max(VSL_MMAP_BACK_CEILING);
    CHECK(vsl_dyn_mmap_backing_max() == VSL_MMAP_BACK_CEILING,
          "budget restored to the fixed ceiling after negative control");
    int rc_again = vsl_exec_image(buf, (size_t)n, &cfg, &r);
    report("backing ON again", rc_again, &r);
    CHECK(rc_again == rc_on && r.mmap_backed == 1 &&
          r.mmap_backed_bytes == 0x100000 && r.mmap_failure[0] == 0,
          "restored backing reproduces the exact positive boundary (no leak)");

    vsl_dyn_set_mmap_backing(0);
    vsl_dyn_set_glibc_startup(0);
    vsl_dyn_set_libc_rootfs(0);
    vsl_dyn_set_interp_bind_main(0);
    vsl_dyn_set_interp(0);
    free(buf);
    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
