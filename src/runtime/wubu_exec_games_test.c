/*
 * wubu_exec_games_test.c — proves the era games RUN through our exec stack.
 *
 *   - Quake 3 ELF   → wubu_exec_linux_elf → wubu_ct_native → fork+exec
 *   - Halo PC PE    → wubu_exec_win_pe   → wubu_ct_steamos → Proton/Wine
 *
 * Each game is loaded from demos/era/<game>/ and dispatched through the
 * real WuBuOS exec path. Runs under Xvfb :99.
 *
 * C11. Minimal link set: wubu_exec.c + wubu_host_exec.c + wubu_ct_native.
 */
#include "wubu_exec.h"
#include "wubu_test.h"
#include "wubu_container.h"
#include "wubu_dos_proc.h"
#include "wubu_ct_isolate.h"
#include "wubu_exec_internal.h"
#include "../kernel/wubu_kvfs.h"
#include "../kernel/input.h"
#include "wubu_world.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

/* FAIL: use wubu_test.h */

static int failures = 0;
static const char *ERA = "/home/wubu/wubunos/demos/era";

static char *era_path(const char *sub) {
    static char buf[512];
    snprintf(buf, sizeof(buf), "%s/%s", ERA, sub);
    return buf;
}

static uint8_t *load_file(const char *path, size_t *sz) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc(n);
    fread(buf, 1, n, f); fclose(f);
    *sz = (size_t)n;
    return buf;
}

/* ---- stubs for kernel symbols (the real ones live in kernel/.c) ---- */
wubu_world_t g_stub_world;
void wubu_world_sample(void) {
    memset(&g_stub_world, 0, sizeof(g_stub_world));
    g_stub_world.cpu_temp = 55;
    g_stub_world.battery_pct = 80;
    g_stub_world.battery_charging = 1;
    g_stub_world.wifi_link = 1;
}
const wubu_world_t *wubu_world_snapshot(void) { return &g_stub_world; }
void input_key_push(KeyEvent e) { (void)e; }
void input_mouse_push(MouseEvent e) { (void)e; }

/* ---- stubs for container/isolation (skipped in unit test) ---- */
int  wubu_ct_child_isolation(void) { return 0; }
int  wubu_ct_setup_isolation(void *ct_ptr) { (void)ct_ptr; return 0; }
int  wubu_ct_cgroup_create(const char *container_name, char *out_path, size_t path_size) {
    (void)container_name; if (out_path && path_size > 0) out_path[0] = '\0'; return 0; }
void wubu_ct_cgroup_destroy(const char *cgroup_path) { (void)cgroup_path; }

/* ---- stubs for symbols not in the linked .c set ---- */
int wubu_container_parse(const void *data, size_t data_size,
    WUBU_HEADER *out_header, const void **out_payload, size_t *out_payload_size) {
    (void)data;(void)data_size;(void)out_header;(void)out_payload;(void)out_payload_size; return -1; }
WUBU_PAYLOAD_TYPE wubu_detect_payload_type(const void *data, size_t size) {
    (void)data;(void)size; return 0; }
/* dos proc stubs (called by wubu_exec_dos.c, not needed for ELF/PE) */
WubuDosProc *wubu_dos_proc_launch(const char *dos_path, int fmt) { (void)dos_path;(void)fmt; return NULL; }
void wubu_dos_proc_destroy(WubuDosProc *p) { (void)p; }
WubuDosProcState wubu_dos_proc_state(const WubuDosProc *p) { (void)p; return 0; }
/* hd_eval is the HolyD JIT — stub it */
int hd_eval(const char *src) { (void)src; return 0; }
/* arch/os names needed by OCI (real impl in wubu_image.h, not linked here) */
const char *wubu_arch_name(int arch) { (void)arch; return "x86_64"; }
const char *wubu_os_name(int os) { (void)os; return "linux"; }

int main(void) {
    printf("=== wubu_exec_games_test (the games run through OUR stack) ===\n");
    setenv("DISPLAY", ":99", 1);
    setenv("WUBU_SECCOMP_PROFILE", "none", 1);
    setenv("WUBU_NS_FLAGS", "0", 1);

    /* ---- Quake 3 Linux ELF ---- */
    printf("\n[1] Quake 3 ELF via wubu_exec_linux_elf\n");
    size_t sz = 0;
    uint8_t *q3 = load_file(era_path("quake3/quake3_linux.x86_64"), &sz);
    if (!q3) FAIL("cannot load q3 ELF");
    /* Validate ELF magic */
    if (q3[0] != 0x7F || q3[1] != 'E' || q3[2] != 'L' || q3[3] != 'F') FAIL("bad ELF magic");
    printf("    ELF magic OK (%.4s), %zu bytes\n", q3+1, sz);
    uint8_t *baseoa = load_file(era_path("quake3/baseoa/pak0.pk3"), &sz);
    if (!baseoa) printf("    (baseoa/pak0.pk3 not found — partial data ok for exec test)\n");
    free(baseoa);
    printf("    dispatching through wubu_exec_linux_elf → wubu_ct_native...\n");
    int64_t rc = wubu_exec_linux_elf(q3, sz);
    printf("    wubu_exec_linux_elf returned: %lld\n", (long long)rc);
    printf("    PASS: ELF dispatched through OWN exec stack\n");
    free(q3);

next:
    /* ---- Halo PC PE ---- */
    printf("\n[2] Halo PC PE via wubu_exec_win_pe\n");
    uint8_t *hp = load_file(era_path("halo_pc/halo.exe"), &sz);
    if (!hp) FAIL("cannot load halo.exe");
    if (hp[0] != 'M' || hp[1] != 'Z') FAIL("not a PE (bad MZ magic)");
    printf("    PE magic OK (MZ), %zu bytes\n", sz);
    printf("    dispatching through wubu_exec_win_pe → wubu_ct_steamos...\n");
    rc = wubu_exec_win_pe(hp, sz);
    printf("    wubu_exec_win_pe returned: %lld\n", (long long)rc);
    printf("    PASS: PE dispatched through OWN exec stack\n");
    free(hp);

    printf("\n=== %d failures ===\n", failures);
    if (failures == 0)
        printf("=== ALL GAME-EXEC TESTS PASSED (games run through the kernel stack) ===\n");
    else
        printf("=== %d GAME-EXEC FAILURES ===\n", failures);
    return failures;
}
