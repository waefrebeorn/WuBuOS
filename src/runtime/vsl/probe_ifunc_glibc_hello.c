/* Reproducible probe for the exact real-glibc hello boundary after the
 * bounded IFUNC-by-name leg.  Deliberately NOT part of the default gate
 * sequence: run late in one process the real hello guest spins in accumulated
 * engine state; as a fresh standalone process it terminates deterministically.
 *
 * Build from /home/wubu/wubuos (or this worktree):
 *   OBJS="$(ls src/runtime/vsl/*.o | grep -v vsl_exec_test.o) \
 *          src/runtime/wubu_kernel_xchg.o"   # canonical tree needs the
 *                                            # explicit VSL_OBJS list below
 *   gcc -O0 -g -D_POSIX_C_SOURCE=200809L -include wubu_gnu_compat.h \
 *       -DHAVE_VULKAN -DHAVE_CUDA -Isrc/runtime -Isrc/runtime/vsl -Iinclude \
 *       src/runtime/wubu_crc32.c $OBJS \
 *       src/runtime/vsl/probe_ifunc_glibc_hello.c \
 *       -o /tmp/probe_ifunc_glibc_hello -ldl -lvulkan -lcuda
 *   timeout 40 /tmp/probe_ifunc_glibc_hello src/runtime/vsl/exec_guests
 * Exact output (3/3 runs):
 *   rc=-9 status=-9 exit=0 interp=1 deps=2 relocs=9 syms=6 fault=0x3000008 \
 *   syscalls=3 ... out_len=0 out="" reloc_failure=""
 * The pre-change engine produces the identical boundary; no hello output is
 * fabricated.  The next work is DSO DT_INIT/DT_FINI, the allocator, a real
 * puts/stdout FILE state and full interpreter bootstrap. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vsl_exec_engine.h"

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "src/runtime/vsl/exec_guests";
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
    VslExecResult r;
    vsl_dyn_set_interp(1);
    vsl_dyn_set_interp_bind_main(1);
    vsl_dyn_set_init_fini(0);
    vsl_dyn_set_fs_tls(0);
    vsl_dyn_set_libc_rootfs(1);
    vsl_dyn_set_glibc_startup(1);
    int rc = vsl_exec_image(buf, (size_t)n, &cfg, &r);
    printf("rc=%d status=%d exit=%d interp=%u deps=%u relocs=%u syms=%u "
           "fault=0x%llx syscalls=%llu fs=%llu out_len=%zu out=\"%.*s\" "
           "reloc_failure=\"%s\"\n",
           rc, r.status, r.exit_code, r.interp_loaded, r.deps_loaded,
           r.relocations_applied, r.symbols_resolved,
           (unsigned long long)r.fault_addr, (unsigned long long)r.syscalls_trapped,
           (unsigned long long)r.fs_base, r.output_len,
           (int)(r.output_len < 200 ? r.output_len : 200), r.output,
           r.reloc_failure);
    free(buf);
    return 0;
}
