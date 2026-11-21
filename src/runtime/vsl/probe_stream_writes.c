/* Fresh-process gate for the bounded printf-format-free fwrite stream shape.
 *
 * Runs as its own process for the same reason as probe_ifunc_glibc_hello.c:
 * late in the big single-process VSL test binary a real glibc guest spins in
 * accumulated engine state.
 *
 * POSITIVE: a REAL dynamically linked glibc guest calls
 * fwrite(msg, 1, len, stdout).  With the bounded stdout/FILE stream model ON,
 * VSL selects the bounded host fwrite writer by the exact
 * (fwrite, GLIBC_2.2.5, libc.so.6) triple.  It shares the SAME single writer
 * and accounting owner as the puts model (no duplicate FILE/stream model):
 * the exact buffer bytes land on fd 1, byte-exact vs a native build of the
 * same guest source, with one raw write and no trailing FILE garbage.
 *
 * NEGATIVE CONTROLS: (1) the model-OFF path is NOT byte-exact; (2) a wrong
 * version or vn_file never selects the bounded writer (exact triple only);
 * (3) unsupported stream shapes (NULL/empty/overflow/over-cap) fail closed
 * with exact diagnostics and emit no bytes.
 */
#define _GNU_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "vsl_exec_engine.h"

static int passed, failed;
#define CHECK(cond, msg) do { \
    if (cond) { passed++; printf("  ok: %s\n", (msg)); } \
    else { failed++; printf("  FAIL: %s\n", (msg)); } \
} while (0)

static void report(const char *tag, int rc, const VslExecResult *r) {
    printf("[%s] rc=%d status=%d exit=%d interp=%u deps=%u relocs=%u syms=%u "
           "fault=0x%llx syscalls=%llu backed=%u failure=\"%s\" "
           "stdout_w=%u stdout_b=%llu stdout_fail=\"%s\" out_len=%zu out=\"%.*s\"\n",
           tag, rc, r->status, r->exit_code, r->interp_loaded, r->deps_loaded,
           r->relocations_applied, r->symbols_resolved,
           (unsigned long long)r->fault_addr,
           (unsigned long long)r->syscalls_trapped, r->mmap_backed,
           r->mmap_failure, r->stdout_writes,
           (unsigned long long)r->stdout_bytes, r->stdout_failure,
           r->output_len, (int)(r->output_len < 40 ? r->output_len : 40),
           r->output);
}

/* Capture fd 1 into a memfd around a bounded-model call, so "emits no bytes"
 * and byte-exact direct output are both observable without touching stdio. */
static int g_cap = -1, g_saved = -1;
static void cap_begin(void) {
    fflush(stdout);
    g_cap = (int)syscall(SYS_memfd_create, "vsl_stream_cap", 0);
    g_saved = dup(1);
    dup2(g_cap, 1);
}
static size_t cap_end(char *buf, size_t cap) {
    dup2(g_saved, 1); close(g_saved);
    off_t n = lseek(g_cap, 0, SEEK_END);
    lseek(g_cap, 0, SEEK_SET);
    size_t want = (n <= 0 || (size_t)n > cap) ? 0 : (size_t)n;
    ssize_t got = read(g_cap, buf, want);
    close(g_cap);
    g_cap = -1; g_saved = -1;
    fflush(stdout);
    return got < 0 ? 0 : (size_t)got;
}

/* Native-build host oracle for the exact guest source. */
static size_t host_oracle(const char *dir, char *buf, size_t cap) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
             "gcc -O0 %s/guest_fwrite.c -o /tmp/vsl_fwrite_oracle 2>/dev/null && "
             "/tmp/vsl_fwrite_oracle > /tmp/vsl_fwrite_oracle.out 2>/dev/null",
             dir);
    if (system(cmd) != 0) return 0;
    FILE *f = fopen("/tmp/vsl_fwrite_oracle.out", "rb");
    if (!f) return 0;
    size_t got = fread(buf, 1, cap - 1, f);
    fclose(f);
    buf[got] = 0;
    return got;
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "src/runtime/vsl/exec_guests";
    static const char fw[] = "VSL-FWRITE-STREAM-WORLD";   /* 23 bytes, no \n */
    char root[512], realroot[512], path[512];
    snprintf(root, sizeof(root), "%s/glibc_rootfs", dir);
    snprintf(realroot, sizeof(realroot), "%s/glibc_rootfs_reallibc", dir);
    const char *none[]       = { NULL };
    const char *real_first[] = { realroot, root, NULL };
    VslImageCfg cfg = { none, 0, 0, NULL, 0, NULL, real_first };
    snprintf(path, sizeof(path), "%s/guest_fwrite.elf", dir);
    FILE *fp = fopen(path, "rb");
    if (!fp) { printf("no fixture %s\n", path); return 2; }
    fseek(fp, 0, SEEK_END); long n = ftell(fp); fseek(fp, 0, SEEK_SET);
    unsigned char *buf = malloc((size_t)n);
    if (fread(buf, 1, (size_t)n, fp) != (size_t)n) { printf("short read\n"); return 2; }
    fclose(fp);

    char oracle[256];
    size_t olen = host_oracle(dir, oracle, sizeof(oracle));
    CHECK(olen == sizeof(fw) - 1 && memcmp(oracle, fw, olen) == 0,
          "native guest_fwrite.c emits the exact 23-byte fwrite oracle (no newline)");

    vsl_dyn_set_interp(1);
    vsl_dyn_set_interp_bind_main(1);
    vsl_dyn_set_init_fini(0);
    vsl_dyn_set_fs_tls(0);
    vsl_dyn_set_libc_rootfs(1);
    vsl_dyn_set_glibc_startup(1);
    vsl_dyn_set_mmap_backing(1);
    VslExecResult r;

    /* NEGATIVE CONTROL: no bounded stream model -> the unmodelled glibc FILE*
     * path is not byte-exact (trailing garbage / bogus write), proving the
     * model below is doing real work rather than passthrough. */
    vsl_dyn_set_stdout_model(0);
    int rc_off = vsl_exec_image(buf, (size_t)n, &cfg, &r);
    report("model OFF / backing ON", rc_off, &r);
    CHECK(rc_off != VSL_EXEC_OK || r.output_len != olen ||
          memcmp(r.output, oracle, olen) != 0,
          "model OFF: unmodelled fwrite FILE path is not byte-exact (negative control)");

    /* POSITIVE: exact-triple fwrite model ON -> one bounded raw write of the
     * exact buffer bytes, byte-exact vs the native oracle. */
    vsl_dyn_set_stdout_model(1);
    int rc_on = vsl_exec_image(buf, (size_t)n, &cfg, &r);
    report("model ON / backing ON", rc_on, &r);
    CHECK(rc_on == VSL_EXEC_OK && r.exit_code == 0 && r.fault_addr == 0,
          "bounded fwrite model reaches exit(0) with no fault");
    CHECK(r.output_len == olen && memcmp(r.output, oracle, olen) == 0,
          "real glibc fwrite stdout is byte-exact vs the native host oracle");
    CHECK(r.output_len == sizeof(fw) - 1 && memcmp(r.output, fw, sizeof(fw) - 1) == 0,
          "exactly VSL-FWRITE-STREAM-WORLD and no trailing FILE garbage");
    CHECK(r.stdout_writes == 1 && r.stdout_bytes == olen &&
          r.stdout_failure[0] == 0,
          "the shared bounded writer emitted the exact 23 bytes in one write");
    CHECK(r.mmap_failure[0] == 0 && r.syscalls_trapped >= 1,
          "bounded fwrite path ran with no backing refusal and a real guest exit");

    /* FAIL-CLOSED selection: only the exact (name, version, vn_file) triple
     * selects the bounded writer; near misses return 0.  puts and fwrite use
     * the SAME owner table. */
    uint64_t fw_impl  = vsl_dyn_stream_impl("fwrite", "GLIBC_2.2.5", "libc.so.6");
    uint64_t puts_impl = vsl_dyn_stream_impl("puts", "GLIBC_2.2.5", "libc.so.6");
    CHECK(fw_impl != 0 && puts_impl != 0 && fw_impl != puts_impl,
          "fwrite and puts both select exact-triple impls from the shared table");
    CHECK(vsl_dyn_stream_impl("fwrite", "GLIBC_9.9", "libc.so.6") == 0,
          "a wrong GLIBC version fails closed at selection");
    CHECK(vsl_dyn_stream_impl("fwrite", "GLIBC_2.2.5", "libevil.so") == 0,
          "a wrong vn_file fails closed at selection");
    CHECK(vsl_dyn_stream_impl("fprintf", "GLIBC_2.2.5", "libc.so.6") == 0,
          "an unmodelled stream symbol (fprintf formatting) fails closed");
    CHECK(vsl_dyn_stream_impl("fwrite", "", NULL) != 0 &&
          vsl_dyn_stream_impl("fwrite", "", "libc.so.6") != 0,
          "unversioned fwrite selects by name only (any vn_file)");

    /* BYTE-EXACT DIRECT ORACLE: an arbitrary buffer with an embedded newline,
     * through the SAME bounded fwrite impl the relocation binder installs,
     * compared to a native fwrite of the same bytes. */
    static const char direct[] = "DIRECT-ORACLE: line1\nline2\n";
    const size_t dlen = sizeof(direct) - 1;
    FILE *tf = tmpfile();
    if (!tf) { printf("no tmpfile\n"); return 2; }
    size_t host_n = fwrite(direct, 1, dlen, tf);
    rewind(tf);
    char host_bytes[128];
    size_t host_got = fread(host_bytes, 1, sizeof(host_bytes), tf);
    fclose(tf);
    char model_bytes[128];
    cap_begin();
    size_t items = vsl_dyn_stream_fwrite(direct, 1, dlen, NULL);
    size_t model_got = cap_end(model_bytes, sizeof(model_bytes));
    CHECK(host_n == dlen && host_got == dlen && items == dlen &&
          model_got == dlen && memcmp(host_bytes, model_bytes, dlen) == 0,
          "direct bounded fwrite is byte-exact vs a native fwrite of the same bytes");

    /* FAIL-CLOSED shapes: each refusal emits NO bytes and an exact diagnostic. */
    char got[256];
    size_t g;
    cap_begin();
    size_t r0 = vsl_dyn_stream_fwrite(NULL, 1, 4, NULL);
    g = cap_end(got, sizeof(got));
    CHECK(r0 == 0 && g == 0 && vsl_dyn_stream_failure()[0] &&
          strstr(vsl_dyn_stream_failure(), "NULL buffer") != NULL,
          "NULL buffer fails closed with an exact diagnostic and no bytes");
    cap_begin();
    size_t r1 = vsl_dyn_stream_fwrite(direct, 0, 4, NULL);
    g = cap_end(got, sizeof(got));
    CHECK(r1 == 0 && g == 0 && strstr(vsl_dyn_stream_failure(), "zero count") != NULL,
          "zero item size fails closed with no bytes");
    cap_begin();
    size_t r2 = vsl_dyn_stream_fwrite(direct, 4, 0, NULL);
    g = cap_end(got, sizeof(got));
    CHECK(r2 == 0 && g == 0 && strstr(vsl_dyn_stream_failure(), "zero count") != NULL,
          "zero item count fails closed with no bytes");
    cap_begin();
    size_t r3 = vsl_dyn_stream_fwrite(direct, VSL_STDOUT_WRITE_MAX, 2, NULL);
    g = cap_end(got, sizeof(got));
    CHECK(r3 == 0 && g == 0 && strstr(vsl_dyn_stream_failure(), "overflow") != NULL,
          "size*nmemb overflow fails closed with no bytes");
    cap_begin();
    size_t r4 = vsl_dyn_stream_fwrite(direct, VSL_STDOUT_WRITE_MAX + 1, 1, NULL);
    g = cap_end(got, sizeof(got));
    CHECK(r4 == 0 && g == 0 && strstr(vsl_dyn_stream_failure(), "overflow") != NULL,
          "over-cap single item fails closed with no bytes");
    cap_begin();
    size_t r5 = vsl_dyn_stream_fwrite(direct, 1, VSL_STDOUT_WRITE_MAX + 1, NULL);
    g = cap_end(got, sizeof(got));
    CHECK(r5 == 0 && g == 0 && strstr(vsl_dyn_stream_failure(), "overflow") != NULL,
          "over-cap item count fails closed with no bytes");

    /* Model OFF disables the selection probe entirely. */
    vsl_dyn_set_stdout_model(0);
    CHECK(vsl_dyn_stream_impl("fwrite", "GLIBC_2.2.5", "libc.so.6") == 0,
          "model OFF: exact triple does not select a writer (opt-in default OFF)");

    vsl_dyn_set_mmap_backing(0);
    vsl_dyn_set_glibc_startup(0);
    vsl_dyn_set_libc_rootfs(0);
    vsl_dyn_set_interp_bind_main(0);
    vsl_dyn_set_interp(0);
    free(buf);
    printf("Results: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
