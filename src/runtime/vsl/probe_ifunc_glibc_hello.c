/* Fresh-process gate for the REAL glibc hello path and its bounded stdout
 * model.  Must run as its own process: late in the big single-process VSL
 * test binary a real glibc guest spins in accumulated engine state.
 *
 * The pre-change path (stdout model OFF, mmap backing ON) runs real glibc to
 * exit(0) but emits 12284 bytes: the 23-byte hello plus ~12 KB of trailing
 * garbage from unmodelled stdout/FILE state (bogus write count 0x2502000,
 * then -EFAULT).  With the bounded stdout/FILE stream model enabled, the
 * guest's puts import binds to a bounded host implementation that emits the
 * exact string plus newline -- byte-exact against a real host build of the
 * same guest source -- with no bogus write, no EFAULT and no trailing bytes.
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
           "failure=\"%s\" stdout_w=%u stdout_b=%llu out_len=%zu out=\"%.*s\"\n",
           tag, rc, r->status, r->exit_code, r->interp_loaded, r->deps_loaded,
           r->relocations_applied, r->symbols_resolved,
           (unsigned long long)r->fault_addr, (unsigned long long)r->fault_rip,
           (unsigned long long)r->syscalls_trapped, r->mmap_backed,
           (unsigned long long)r->mmap_backed_bytes, r->mmap_failure,
           r->stdout_writes, (unsigned long long)r->stdout_bytes,
           r->output_len, (int)(r->output_len < 32 ? r->output_len : 32),
           r->output);
}

/* Compile and run the SAME guest source natively: the host puts oracle. */
static size_t host_oracle(const char *dir, char *buf, size_t cap) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "gcc -O0 %s/guest_hello.c -o /tmp/vsl_hello_oracle 2>/dev/null && "
             "/tmp/vsl_hello_oracle > /tmp/vsl_hello_oracle.out 2>/dev/null",
             dir);
    if (system(cmd) != 0) return 0;
    FILE *f = fopen("/tmp/vsl_hello_oracle.out", "rb");
    if (!f) return 0;
    size_t got = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[got] = 0;
    return got;
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "src/runtime/vsl/exec_guests";
    static const char hello[] = "VSL-HELLO-DYNAMIC-WORLD\n";
    static const char hello_body[] = "VSL-HELLO-DYNAMIC-WORLD";
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

    /* REAL host oracle for the exact guest source. */
    char oracle[256];
    size_t oracle_len = host_oracle(dir, oracle, sizeof(oracle));
    CHECK(oracle_len == sizeof(hello) - 1 &&
          memcmp(oracle, hello, oracle_len) == 0,
          "host build of guest_hello.c emits the 24-byte puts oracle");

    vsl_dyn_set_interp(1);
    vsl_dyn_set_interp_bind_main(1);
    vsl_dyn_set_init_fini(0);
    vsl_dyn_set_fs_tls(0);
    vsl_dyn_set_libc_rootfs(1);
    vsl_dyn_set_glibc_startup(1);
    VslExecResult r;

    /* NEGATIVE CONTROL: pre-change byte-for-byte path (no mmap backing).
     * The unbacked glibc allocator still faults at 0x3000008. */
    vsl_dyn_set_stdout_model(0);
    vsl_dyn_set_mmap_backing(0);
    int rc_off = vsl_exec_image(buf, (size_t)n, &cfg, &r);
    report("model OFF / backing OFF", rc_off, &r);
    CHECK(rc_off == VSL_EXEC_ERR_FAULT && r.fault_addr == 0x3000008 &&
          r.fault_rip != 0 && r.syscalls_trapped == 3 &&
          r.mmap_backed == 0 && r.mmap_failure[0] == 0,
          "OFF reproduces the exact unbacked-allocator fault at 0x3000008");

    /* BUG CONTROL: backing ON but stdout model OFF.  Real glibc reaches
     * exit(0) but the unmodelled FILE state emits 12284 bytes. */
    vsl_dyn_set_mmap_backing(1);
    int rc_bug = vsl_exec_image(buf, (size_t)n, &cfg, &r);
    report("model OFF / backing ON", rc_bug, &r);
    CHECK(rc_bug == VSL_EXEC_OK && r.exit_code == 0 && r.fault_addr == 0 &&
          r.mmap_backed == 1 && r.mmap_backed_bytes == 0x100000 &&
          r.output_len > oracle_len &&
          memcmp(r.output, hello_body, sizeof(hello_body) - 1) == 0,
          "without the model, backing ON leaks the 12 KB trailing-garbage tail");

    /* POSITIVE: bounded stdout/FILE model ON.  Byte-exact vs the real host
     * oracle, no trailing bytes, no bogus write, no EFAULT. */
    vsl_dyn_set_stdout_model(1);
    int rc_on = vsl_exec_image(buf, (size_t)n, &cfg, &r);
    report("model ON / backing ON", rc_on, &r);
    CHECK(rc_on == VSL_EXEC_OK && r.exit_code == 0 && r.fault_addr == 0,
          "bounded stdout model reaches exit(0) with no fault");
    CHECK(r.output_len == oracle_len &&
          memcmp(r.output, oracle, oracle_len) == 0,
          "real glibc stdout is byte-exact vs the host puts oracle");
    CHECK(r.output_len == sizeof(hello) - 1 &&
          memcmp(r.output, hello, sizeof(hello) - 1) == 0,
          "exactly VSL-HELLO-DYNAMIC-WORLD\\n and no trailing garbage");
    CHECK(r.stdout_writes == 2 && r.stdout_bytes == oracle_len &&
          r.stdout_failure[0] == 0,
          "the bounded model emitted the exact 24 bytes in two writes");
    CHECK(r.mmap_failure[0] == 0 && r.syscalls_trapped >= 1,
          "bounded stdout path ran with no backing refusal and a real guest exit");

    /* FAIL-CLOSED: unsupported FILE/stream shapes are refused, not guessed. */
    char diag[160];
    CHECK(vsl_stdout_shape_ok(1, oracle_len, diag, sizeof(diag)) == 0 &&
          diag[0] == 0, "fd=1 with a bounded count is a supported stream shape");
    CHECK(vsl_stdout_shape_ok(1, 0x2502000ULL, diag, sizeof(diag)) == -1 &&
          strstr(diag, "stdout: unsupported FILE/stream shape") != NULL &&
          strstr(diag, "count=0x2502000") != NULL,
          "bogus 0x2502000 FILE write count fails closed with exact diagnostic");
    CHECK(vsl_stdout_shape_ok(2, 1, diag, sizeof(diag)) == -1 &&
          strstr(diag, "fd=0x2") != NULL,
          "a non-stdout fd fails closed with an exact diagnostic");

    /* Mmap-backing budget fail-closed stays intact (model OFF for the
     * pre-change allocator boundary). */
    vsl_dyn_set_stdout_model(0);
    CHECK(vsl_dyn_set_mmap_backing_max(VSL_MMAP_BACK_CEILING + 1ULL) == -1,
          "budget setter refuses to grow past the fixed ceiling");
    CHECK(vsl_dyn_set_mmap_backing_max(0) == 0 && vsl_dyn_mmap_backing_max() == 0,
          "bounded setter lowers the effective budget to zero");
    int rc_zero = vsl_exec_image(buf, (size_t)n, &cfg, &r);
    report("backing budget 0", rc_zero, &r);
    CHECK(r.mmap_backed == 0 &&
          strstr(r.mmap_failure, "guest mmap backing:") != NULL &&
          strstr(r.mmap_failure, "addr=0x") != NULL &&
          strstr(r.mmap_failure, "size=0x") != NULL,
          "zero budget fails closed with an exact addr/size diagnostic");
    CHECK(rc_zero != rc_off, "differential: backing budget changes the real hello path");

    /* RESTORE + RE-RUN with the model on: exact boundary returns, no leak. */
    vsl_dyn_set_mmap_backing_max(VSL_MMAP_BACK_CEILING);
    vsl_dyn_set_stdout_model(1);
    int rc_again = vsl_exec_image(buf, (size_t)n, &cfg, &r);
    report("model ON again", rc_again, &r);
    CHECK(rc_again == rc_on && r.output_len == oracle_len &&
          memcmp(r.output, oracle, oracle_len) == 0 &&
          r.mmap_failure[0] == 0 && r.fault_addr == 0,
          "restored backing reproduces the exact byte-exact boundary (no leak)");

    vsl_dyn_set_stdout_model(0);
    vsl_dyn_set_mmap_backing(0);
    vsl_dyn_set_glibc_startup(0);
    vsl_dyn_set_libc_rootfs(0);
    vsl_dyn_set_interp_bind_main(0);
    vsl_dyn_set_interp(0);
    free(buf);
    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
