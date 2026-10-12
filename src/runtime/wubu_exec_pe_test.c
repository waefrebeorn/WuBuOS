/*
 * wubu_exec_pe_test.c -- RUNTIME #834: PE execution leg verified.
 *
 * Builds a tiny real PE (Windows x64 console EXE that exits with a known
 * code) at runtime via mingw-w64, dispatches it through the universal exec
 * path (wubu_exec_win_pe -> wubu_ct_steamos -> Wine), and asserts the exit
 * code round-trips. This proves a real PE binary runs on the kernel with
 * correct output, with no external game binary dependency.
 *
 * Requires: x86_64-w64-mingw32-gcc (to build the PE) and /usr/bin/wine (to
 * run it). Isolation is relaxed via WUBU_SECCOMP_PROFILE=none so the test
 * needs no privileged seccomp/namespace setup.
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

/* ---- stubs for symbols not in the linked .c set (PE path doesn't need
 * the HolyD JIT or DOS proc internals). The full container/isolation stack
 * is linked, so those symbols are NOT stubbed here. ---- */
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

/* Build a real 32-bit PE (exit `code`) via mingw-w64 into out_path. Returns 0
 * on ok. The kernel's Wine prefix is WINEARCH=win32 (see wubu_wine_configure_env),
 * so the PE must be 32-bit (PE32) to load in it. */
static int build_pe(const char *out_path, int code) {
    char src_path[512];
    snprintf(src_path, sizeof(src_path), "%s.c", out_path);
    FILE *sf = fopen(src_path, "w");
    if (!sf) return -1;
    /* ExitProcess(code) so the exit code is exact (not mingw's CRT mapping). */
    fprintf(sf,
        "#include <windows.h>\n"
        "int WINAPI WinMain(HINSTANCE h, HINSTANCE p, LPSTR c, int s){\n"
        "  (void)h;(void)p;(void)c;(void)s;\n"
        "  ExitProcess(%d);\n"
        "  return %d;\n"
        "}\n", code, code);
    fclose(sf);

    char cmd[1200];
    snprintf(cmd, sizeof(cmd),
             "i686-w64-mingw32-gcc -O0 %s.c -o %s -mwindows 2>/dev/null", out_path, out_path);
    int rc = system(cmd);
    unlink(src_path);
    return rc == 0 ? 0 : -1;
}

int main(void) {
    /* Relax container isolation + force host-namespace exec (no privileges). */
    setenv("WUBU_SECCOMP_PROFILE", "none", 1);
    setenv("WUBU_SECMON_PROFILE", "none", 1);
    setenv("WUBU_NS_FLAGS", "0", 1);

    printf("=== WuBuOS PE Execution Leg Test (RUNTIME #834) ===\n\n");

    /* Preconditions: the PE toolchain + Wine must be present. */
    int have_mingw = system("command -v i686-w64-mingw32-gcc >/dev/null 2>&1") == 0;
    int have_wine  = system("command -v wine >/dev/null 2>&1") == 0;
    T(have_mingw, "mingw-w64 available (builds the PE)");
    T(have_wine, "wine available (runs the PE)");

    if (!have_mingw || !have_wine) {
        printf("\n  (skipping exec: toolchain/wine not present)\n");
        printf("=== Results: %d/%d passed ===\n", g_pass, g_run);
        return g_pass == g_run ? 0 : 2;  /* 2 = env skip, not a code failure */
    }

    /* Build a real PE that exits 42 and run it through the exec dispatcher. */
    const char *pe_path = "/tmp/wubu_exec_pe_test_bin.exe";
    T(build_pe(pe_path, 42) == 0, "built real PE (exit 42) at runtime");

    size_t sz = 0;
    FILE *f = fopen(pe_path, "rb");
    T(f != NULL, "PE file opened for load");
    uint8_t *pe = NULL;
    if (f) {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (len > 0) {
            sz = (size_t)len;
            pe = malloc(sz);
            if (pe && fread(pe, 1, sz, f) != sz) { free(pe); pe = NULL; }
        }
        fclose(f);
    }
    T(pe != NULL && sz >= 2, "PE bytes loaded into memory");
    T(pe && pe[0] == 'M' && pe[1] == 'Z', "PE magic validated ('MZ')");

    /* The real leg: dispatch through wubu_exec_win_pe and verify the exit
     * code round-trips through the SteamOS container -> Wine path. */
    if (pe && sz >= 2) {
        int64_t rc = wubu_exec_win_pe(pe, sz);
        printf("    wubu_exec_win_pe -> exit code %lld\n", (long long)rc);
        T(rc == 42, "real PE ran on kernel, exit code 42 round-tripped");
    } else {
        T(0, "skipped exec (PE not loaded)");
    }

    /* Error path: garbage must be rejected, never silently succeed. */
    uint8_t garbage[8] = { 0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08 };
    T(wubu_exec_win_pe(garbage, sizeof(garbage)) == -1, "non-PE bytes rejected (-1)");
    T(wubu_exec_win_pe(NULL, 0) == -1, "NULL PE rejected (-1)");

    free(pe);
    unlink(pe_path);

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return g_pass == g_run ? 0 : 1;
}
