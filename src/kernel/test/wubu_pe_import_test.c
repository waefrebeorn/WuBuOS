/* wubu_pe_import_test.c -- rung 6: holyd EMITS an import-carrying PE.
 *
 * Rungs 2-5 proved holyd emits position-independent programs and that the
 * loader resolves imports from the registry when an image has an .idata table
 * (test_pe_modsym / test_wubu_zx used HAND-BUILT import PEs). This proof closes
 * the compiler side of that seam: hd_write_pe_imported(...) emits the .idata
 * directory (descriptor + ILT + INT + IAT + name rows + dll string) itself, and
 * rewrites a `call qword [rip+disp32]` in the code to point at its IAT slot, so
 * a compiler-produced PE carries its own imports. Run it through wubu_zx_run:
 * the entry calls through the IAT -> wubu_pe_snap_iat resolves the slot via the
 * registry -> returns the registered host function's result.
 *
 * code:   FF 15 <disp> C3       call qword [rip+disp]; ret
 *         disp patched by the emitter to reach the IAT slot.
 * import: NTOSKRNL.EXE!ANSWER (registered => host_answer() = 7)
 * expect: entry -> 7, at the preferred base (no .reloc needed: the IAT is patched
 *         in-place by snap_iat, and the code reference to it is via RIP). */
#include "../../egojar.h"
#include "wubu_zx.h"
#include "wubu_modsym.h"
#include "holyd.h"
#include "holyd_codegen.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

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

int main(void) {
    /* code: FF 15 <disp@offset4> C3 — call qword[rip+disp] through IAT, then ret.
     * The emitter patches the disp32 at call_disp[0]=4 to reach the IAT slot. */
    uint8_t code[12];
    code[0] = 0xFF; code[1] = 0x15;          /* call qword ptr [rip+disp32] */
    code[2] = 0; code[3] = 0; code[4] = 0; code[5] = 0;  /* disp placeholder */
    code[6] = 0xC3;                          /* ret */
    size_t call_disp[1] = { 2 };             /* disp32 starts after the opcode (FF 15) */
    const char *dll = "NTOSKRNL.EXE";
    const char *fn[1] = { "ANSWER" };
    const char *out = "/tmp/holyd_import_pe.bin";

    CHECK(hd_write_pe_imported(out, code, 7, call_disp, 1,
                               dll, fn, 1, NULL) == 0,
          "hd_write_pe_imported emitted a PE with an import table");

    /* registry: NT personality would publish here; register ANSWER directly */
    CHECK(wubu_modsym_register("NTOSKRNL.EXE", "ANSWER", (uintptr_t)&host_answer) == 0,
          "registry has NTOSKRNL.EXE!ANSWER");

    /* load + run the emitted PE by name through the module-runner primitive */
    wubu_zx_ops ops = { host_map, host_unmap };
    FILE *f = fopen(out, "rb");
    CHECK(f != NULL, "opened emitted import PE");
    long sz = -1; if (f) { fseek(f,0,SEEK_END); sz=ftell(f); fseek(f,0,SEEK_SET); }
    uint8_t *fb = sz>0 ? (uint8_t*)malloc((size_t)sz) : NULL;
    if (fb && f) { if (fread(fb,1,(size_t)sz,f)!=(size_t)sz){ free(fb); fb=NULL; } }
    if (f) fclose(f);
    CHECK(fb != NULL && sz > 0, "read emitted import PE");

    int err = -99;
    int rc = wubu_zx_run(&ops, fb, (size_t)sz, 0x00400000u, NULL, &err);
    fprintf(stderr, "> emitted import PE -> %d (err=%d)\n", rc, err);
    CHECK(err == 0 && rc == 7,
          "wubu_zx_run: emitted .idata PE entry calls through IAT -> registry -> 7");
    free(fb);

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
