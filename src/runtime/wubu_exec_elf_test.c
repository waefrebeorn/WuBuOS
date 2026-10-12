/*
 * wubu_exec_elf_test.c -- RUNTIME #833: ELF execution leg verified.
 *
 * Builds a tiny real ELF at runtime (a C program that exits with a known
 * code), dispatches it through the universal exec path (wubu_exec_linux_elf
 * -> wubu_ct_native -> fork+exec), and asserts the exit code round-trips.
 * This proves a real ELF binary runs on the kernel with correct output,
 * with no external game/binary dependency.
 *
 * Isolation is relaxed via WUBU_SECCOMP_PROFILE=none so the test does not
 * need privileged seccomp/namespace setup in CI.
 */

#define _GNU_SOURCE 1
#include "wubu_exec.h"
#include "wubu_exec_internal.h"
#include "wubu_host_exec.h"
#include "wubu_dos_proc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* ---- stub for symbols not in the linked .c set. The ELF path doesn't
 * need the HolyD JIT or DOS proc internals. The full container/isolation
 * stack (wubu_ct_isolate.c, ct_iso_seccomp.c, wubu_ct_isolate_cgroup.c)
 * IS linked, so those symbols are NOT stubbed here. ---- */
int hd_eval(const char *src) { (void)src; return 0; }
WubuDosProc *wubu_dos_proc_launch(const char *p, int fmt) { (void)p;(void)fmt; return NULL; }
void wubu_dos_proc_destroy(WubuDosProc *p) { (void)p; }
WubuDosProcState wubu_dos_proc_state(const WubuDosProc *p) { (void)p; return 0; }

static int g_run = 0, g_pass = 0;
#define T(cond, msg) do { \
    g_run++; \
    if (cond) { g_pass++; printf("  ✅ %s\n", msg); } \
    else { printf("  ❌ %s (line %d)\n", msg, __LINE__); } \
} while (0)

/* Compile a tiny ELF that exits with `code` into `out_path` using the host
 * cc. Returns 0 on success. */
static int build_elf(const char *out_path, int code) {
    char src_path[512];
    snprintf(src_path, sizeof(src_path), "%s.c", out_path);
    FILE *sf = fopen(src_path, "w");
    if (!sf) return -1;
    fprintf(sf, "int main(void){ return %d; }\n", code);
    fclose(sf);

    char cmd[1200];
    snprintf(cmd, sizeof(cmd),
             "cc -O0 %s.c -o %s 2>/dev/null", out_path, out_path);
    int rc = system(cmd);
    unlink(src_path);
    return rc == 0 ? 0 : -1;
}

int main(void) {
    /* Relax container isolation so the test needs no privileges. */
    setenv("WUBU_SECCOMP_PROFILE", "none", 1);
    setenv("WUBU_SECMON_PROFILE", "none", 1);

    printf("=== WuBuOS ELF Execution Leg Test (RUNTIME #833) ===\n\n");

    /* Build a real ELF that exits 42 and run it through the exec dispatcher. */
    const char *elf_path = "/tmp/wubu_exec_elf_test_bin";
    T(build_elf(elf_path, 42) == 0, "built real ELF (exit 42) at runtime");

    /* Load the ELF bytes. */
    size_t sz = 0;
    FILE *f = fopen(elf_path, "rb");
    T(f != NULL, "ELF file opened for load");
    uint8_t *elf = NULL;
    if (f) {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (len > 0) {
            sz = (size_t)len;
            elf = malloc(sz);
            if (elf && fread(elf, 1, sz, f) != sz) { free(elf); elf = NULL; }
        }
        fclose(f);
    }
    T(elf != NULL && sz >= 4, "ELF bytes loaded into memory");
    T(elf && elf[0] == 0x7F && elf[1] == 'E' && elf[2] == 'L' && elf[3] == 'F',
      "ELF magic validated (0x7F 'E' 'L' 'F')");

    /* The real leg: dispatch through wubu_exec_linux_elf and verify the
     * exit code round-trips through the container exec path. */
    if (elf && sz >= 4) {
        int64_t rc = wubu_exec_linux_elf(elf, sz);
        printf("    wubu_exec_linux_elf -> exit code %lld\n", (long long)rc);
        T(rc == 42, "real ELF ran on kernel, exit code 42 round-tripped");
    } else {
        T(0, "skipped exec (ELF not loaded)");
    }

    /* Error path: garbage must be rejected, never silently succeed. */
    uint8_t garbage[8] = { 0xDE,0xAD,0xBE,0xEF,0x00,0x01,0x02,0x03 };
    T(wubu_exec_linux_elf(garbage, sizeof(garbage)) == -1,
      "non-ELF bytes rejected (-1)");
    T(wubu_exec_linux_elf(NULL, 0) == -1, "NULL ELF rejected (-1)");

    free(elf);
    unlink(elf_path);

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return g_pass == g_run ? 0 : 1;
}
