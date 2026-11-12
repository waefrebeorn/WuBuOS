/* wubu_pe_modsym_test.c — P2 Piece A: the loader's import resolution goes
 * through the RUNTIME-EXTENSIBLE symbol registry (wubu_modsym), not a
 * hardcoded test callback. A PE whose .text entry is `call [rip+disp]; ret`
 * into its own IAT is loaded; the IAT is snapped by wubu_modsym_resolve, the
 * registered symbol the entry depended on resolves to a real host function,
 * and the entry returns that function's result — proving a dependency
 * actually connected at load time through the registry. This is the loader
 * half of "holyd modules resolve against the VSL/NT export table": the NT
 * personality publishes its handlers into the SAME registry (vsl_nt_export_modsym),
 * so the mechanism here is exactly what a loaded holyd PE uses.
 *
 * Kernel-clean: links wubu_pe.c + wubu_modsym.c only. */
#include "../../egojar.h"
#include "../../egojar_net.h"
#include "wubu_pe.h"
#include "wubu_modsym.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

static int host_answer(void) { return 7; }
static int host_answer_v2(void) { return 99; }

/* Build a PE32 with two sections (x64), mirroring test_pe_full:
 *   .text  @ RVA 0x1000 raw 0x170 : entry at RVA 0x1030
 *          FF 15 <disp32> C3 = call qword [rip+disp]; ret
 *          disp = target(IAT slot 0x00402050) - (entry 0x00401030 + 6)
 *   .idata @ RVA 0x2000 raw 0x1B0 : desc{NTOSKRNL.EXE -> ANSWER}
 * image_base = 0x00400000 (preferred base -> relocation is a no-op). */
static size_t build_pe(uint8_t *f, size_t cap) {
    memset(f, 0, cap);
    f[0] = 'M'; f[1] = 'Z';
    uint32_t pe_off = 0x80; memcpy(f + 0x3C, &pe_off, 4);
    uint8_t *pe = f + pe_off;
    uint32_t sig = 0x00004550; memcpy(pe, &sig, 4);
    uint16_t nsects = 2, optsize = 160;
    memcpy(pe + 7 - 1, &nsects, 2); memcpy(pe + 20, &optsize, 2);
    uint8_t *opt = pe + 24;
    uint16_t magic = 0x10B; memcpy(opt, &magic, 2);
    uint32_t base = 0x00400000; memcpy(opt + 28, &base, 4);
    uint32_t entry = 0x1030; memcpy(opt + 16, &entry, 4);
    uint16_t subsystem = 3; memcpy(opt + 68, &subsystem, 2);
    uint32_t irva = 0x2000, isz = 0x80; memcpy(opt + 104, &irva, 4); memcpy(opt + 108, &isz, 4);

    uint8_t *sect = opt + optsize;                 /* section table @ 0x138 */
    uint32_t v;
    memcpy(sect, ".text", 8);
    v = 0x40;  memcpy(sect + 8, &v, 4);
    v = 0x1000; memcpy(sect + 12, &v, 4);
    v = 0x40;  memcpy(sect + 16, &v, 4);
    v = 0x190; memcpy(sect + 20, &v, 4);
    memcpy(sect + 40, ".idata", 8);
    v = 0x80;  memcpy(sect + 48, &v, 4);
    v = 0x2000; memcpy(sect + 52, &v, 4);
    v = 0x80;  memcpy(sect + 56, &v, 4);
    v = 0x1D0; memcpy(sect + 60, &v, 4);

    uint8_t *text = f + 0x190;
    memset(text, 0xCC, 0x40);
    uint32_t iat_va = 0x00400000u + 0x2050u;
    uint32_t call_rip = 0x00400000u + 0x1030u + 6u;
    int32_t disp = (int32_t)(iat_va - call_rip);
    uint8_t *ep = text + 0x30;                     /* entry at RVA 0x1030 */
    ep[0] = 0xFF; ep[1] = 0x15;
    memcpy(ep + 2, &disp, 4);
    ep[6] = 0xC3;

    uint8_t *d = f + 0x1D0;
    *((uint32_t*)(d + 0))  = 0x2040; *((uint32_t*)(d + 12)) = 0x2030; *((uint32_t*)(d + 16)) = 0x2050;
    strcpy((char*)(d + 0x30), "NTOSKRNL.EXE");
    uint64_t *ilt = (void*)(d + 0x40); ilt[0] = 0x2060; ilt[1] = 0;
    uint8_t *ns = d + 0x60; *((uint16_t*)ns) = 0; strcpy((char*)(ns + 2), "ANSWER");
    return 0x1D0 + 0x80;
}

int main(void) {
    /* 1. the runtime publishes a symbol (the NT personality does this via
     *    vsl_nt_export_modsym; here we register the equivalent directly). */
    CHECK(wubu_modsym_register("NTOSKRNL.EXE", "ANSWER", (uintptr_t)&host_answer) == 0,
          "runtime registers NTOSKRNL.EXE!ANSWER into the symbol registry");

    uint8_t file[2048];
    size_t fsz = build_pe(file, sizeof(file));
    wubu_pe_info_t info;
    CHECK(wubu_pe_parse(file, fsz, &info) == 0, "parse succeeds");
    wubu_pe_section_t sects[4];
    int ns = wubu_pe_sections(file, fsz, &info, sects, 4);
    CHECK(ns == 2, "two sections (.text + .idata)");

    uint8_t *image = mmap((void *)0x00400000, 0x3000,
                          PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    CHECK(image != MAP_FAILED && image == (void *)0x00400000,
          "exec-memory mapped at preferred base 0x00400000");
    memset(image, 0, 0x3000);
    for (int i = 0; i < ns; i++)
        memcpy(image + sects[i].virtual_addr, file + sects[i].raw_ptr, sects[i].raw_size);

    CHECK(wubu_pe_relocate(&info, sects, ns, file, fsz, image, 0x00400000u) == 0,
          "relocate at preferred base returns 0");

    /* 2. snap the IAT through the REGISTRY resolver (not a callback). */
    int snapped = wubu_pe_snap_iat(file, fsz, &info, sects, ns, image, wubu_modsym_resolve);
    fprintf(stderr, "> snaps=%d count=%d\n", snapped, wubu_modsym_count());
    CHECK(snapped == 1, "one IAT slot snapped via the registry resolver");
    uintptr_t slot = 0;
    memcpy(&slot, image + 0x2050, sizeof(uintptr_t));
    CHECK(slot == (uintptr_t)&host_answer, "IAT slot points at the registered host function");

    /* 3. run the entry: it CALLS through the patched IAT and returns 7. */
    int rc = wubu_pe_call_entry(&info, sects, ns, image);
    fprintf(stderr, "> entry returned %d\n", rc);
    CHECK(rc == 7, "entry call-through-registry-import returned 7");

    /* 4. runtime extensibility: re-bind the same symbol to a different address. */
    wubu_modsym_clear();
    CHECK(wubu_modsym_register("NTOSKRNL.EXE", "ANSWER", (uintptr_t)&host_answer_v2) == 0,
          "registry re-registers ANSWER after clear (runtime extensible)");
    memset(image + 0x2050, 0, sizeof(uintptr_t));   /* reset the stale slot */
    snapped = wubu_pe_snap_iat(file, fsz, &info, sects, ns, image, wubu_modsym_resolve);
    CHECK(snapped == 1 && *(uintptr_t*)(image + 0x2050) == (uintptr_t)&host_answer_v2,
          "re-snap binds the NEW address (registry is the runtime source of truth)");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
