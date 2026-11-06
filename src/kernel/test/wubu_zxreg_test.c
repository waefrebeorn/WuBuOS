/* wubu_zxreg_test.c -- module registry (ZXE-style) semantics.
 *
 * Builds on rungs 4-6 (wubu_zx_run + wubu_zxload + import emitter) and proves
 * the registry invariants a ".ZXE loader on /n" needs:
 *   1. LOAD: run a module by name from the /n root -> entry result (7).
 *   2. DEDUP: loading the SAME module again does not re-map/run it — the slot
 *      is reused, refcount bumps, and the cached result returns (7 again).
 *   3. COUNT: registry row count is 1, not 2 (one mapping, two refs).
 *   4. UNLOAD once -> refcount drops to 1, slot still valid.
 *   5. UNLOAD again -> refcount 0, slot freed, count 0.
 *
 * Uses a real ns root (g_ns_root set directly; wubu_ns_fs.o, not the bridge).
 */
#include "wubu_zxreg.h"
#include "wubu_zx.h"
#include "wubu_modsym.h"
#include "wubu_ns_bridge_internal.h"
#include "holyd.h"
#include "holyd_codegen.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

static void *host_map(uintptr_t base, size_t span) {
    uint8_t *p = mmap((void *)base, span, PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (p == MAP_FAILED || p != (void *)base) return NULL;
    memset(p, 0, span);
    return p;
}
static void host_unmap(void *image, uintptr_t base, size_t span) { munmap(image, span); (void)base; }
static int host_answer(void) { return 7; }

/* write the one-import PE (NTOSKRNL.EXE!ANSWER) the emitter produces, into /n */
static int publish_import_module(const char *out) {
    uint8_t code[12];
    code[0]=0xFF; code[1]=0x15; code[2]=0; code[3]=0; code[4]=0; code[5]=0; code[6]=0xC3;
    size_t disp[1]={2}; const char*dll="NTOSKRNL.EXE"; const char*fn[1]={"ANSWER"};
    return hd_write_pe_imported(out, code, 7, disp, 1, dll, fn, 1, NULL);
}

int main(void) {
    g_ns_root = "/tmp/ns_zxreg_test";
    mkdir(g_ns_root, 0755);
    wubu_zx_ops ops = { host_map, host_unmap };
    wubu_zxreg_clear();

    /* publish the module into /n */
    CHECK(publish_import_module("/tmp/ns_zxreg_test/mod.bin") == 0,
          "published import module into /n");
    CHECK(wubu_modsym_register("NTOSKRNL.EXE","ANSWER",(uintptr_t)&host_answer)==0,
          "registry has NTOSKRNL.EXE!ANSWER");

    /* 1. load by name -> 7 */
    wubu_zxreg_entry_t *e = NULL;
    int r1 = wubu_zxreg_load(&ops, "/n/mod.bin", 0x00400000u, NULL, &e);
    fprintf(stderr, "> load1 -> %d (entry=%p refcount=%d)\n", r1, (void*)e, e?e->refcount:-1);
    CHECK(r1 == 7 && e && e->refcount == 1, "load #1 runs module -> 7, refcount 1");

    /* 2. dedup: load the SAME module again */
    wubu_zxreg_entry_t *e2 = NULL;
    int r2 = wubu_zxreg_load(&ops, "/n/mod.bin", 0x00400000u, NULL, &e2);
    fprintf(stderr, "> load2 (dedup) -> %d (same=%d refcount=%d count=%d)\n",
            r2, e2==e, e2?e2->refcount:-1, wubu_zxreg_count());
    CHECK(r2 == 7 && e2 == e && e->refcount == 2, "load #2 dedups -> cached 7, refcount 2");
    CHECK(wubu_zxreg_count() == 1, "one mapping for two refs (registry count == 1)");

    /* 3. unload once -> refcount 1, slot still valid */
    wubu_zxreg_unload(e);
    fprintf(stderr, "> unload1 refcount=%d count=%d\n", e->refcount, wubu_zxreg_count());
    CHECK(e->valid && e->refcount == 1 && wubu_zxreg_count() == 1,
          "unload #1 decrements to 1, slot stays");

    /* 4. unload again -> freed, count 0 */
    wubu_zxreg_unload(e);
    fprintf(stderr, "> unload2 refcount=%d count=%d\n", e->refcount, wubu_zxreg_count());
    CHECK(wubu_zxreg_count() == 0, "unload #2 releases slot, count 0");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
