/* wubu_pe_iat_test.c — import address table snapping for the in-kernel PE loader.
 * Builds a PE whose .idata imports "Sleep" by name + ordinal 13 from KERNEL32.dll
 * and "Beep" by name from USER32.dll, maps it, snaps the IAT via a recording
 * resolver, and verifies each import resolved correctly and landed in the right
 * IAT slot (FirstThunk). */
#include "wubu_pe.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

#define MAXCALL 8
typedef struct { char dll[64]; char func[64]; uint32_t ord; int is_ord; uintptr_t addr; } call_t;
static call_t g_calls[MAXCALL];
static int g_ncalls = 0, g_failresolve = 0;

static int resolve(const char *dll, const char *func, uint32_t ord, int is_ord, uintptr_t *out) {
    if (g_ncalls >= MAXCALL) return 1;
    call_t *c = &g_calls[g_ncalls++];
    strncpy(c->dll, dll, 63); c->dll[63] = 0;
    strncpy(c->func, func, 63); c->func[63] = 0;
    c->ord = ord; c->is_ord = is_ord;
    c->addr = 0x1000u + (uintptr_t)g_ncalls * 0x100u;   /* fake resolved address */
    *out = c->addr;
    return g_failresolve ? 1 : 0;
}

/* Build a PE32 (x64 thunks) with a single .idata section @RVA 0x2000
 * (gap-free RVA layout; ILT arrays must NOT overlap):
 *   0x2000 desc0 KERNEL32.dll : ilt@0x2060 name@0x2040 iat@0x2090
 *   0x2014 desc1 USER32.dll   : ilt@0x2080 name@0x2050 iat@0x20A8
 *   0x2028 terminator (0)
 *   0x2040 "KERNEL32.dll"  0x2050 "USER32.dll"
 *   0x2060 ILT0: [0x20C0(Sleep), 0x800000000000000D(ord13), 0]   (24B -> ..0x2078)
 *   0x2080 ILT1: [0x20D0(Beep), 0]                               (16B -> ..0x2090)
 *   0x2090 IAT0: [0,0,0]   0x20A8 IAT1: [0,0]
 *   0x20C0 {hint0;"Sleep\0"}  0x20D0 {hint0;"Beep\0"}
 * Section table at 0x148; section raw (file) at 0x170. */
static size_t build_pe(uint8_t *f, size_t cap) {
    memset(f, 0, cap);
    f[0] = 'M'; f[1] = 'Z';
    uint32_t pe_off = 0x80; memcpy(f + 0x3C, &pe_off, 4);
    uint8_t *pe = f + pe_off;
    uint32_t sig = 0x00004550; memcpy(pe, &sig, 4);
    uint16_t nsects = 1, optsize = 176; memcpy(pe + 6, &nsects, 2); memcpy(pe + 20, &optsize, 2);
    uint8_t *opt = pe + 24;
    uint16_t magic = 0x10B; memcpy(opt, &magic, 2);
    uint32_t base = 0x00400000; memcpy(opt + 28, &base, 4);
    uint32_t entry = 0x1000; memcpy(opt + 16, &entry, 4);
    uint16_t subsystem = 3; memcpy(opt + 68, &subsystem, 2);
    uint32_t irva = 0x2000, isz = 0x100; memcpy(opt + 104, &irva, 4); memcpy(opt + 108, &isz, 4);

    uint8_t *sect = opt + optsize;             /* section table @ 0x148 */
    memcpy(sect, ".idata", 8);
    uint32_t v; v = 0x100; memcpy(sect + 8, &v, 4);
    v = 0x2000; memcpy(sect + 12, &v, 4);
    v = 0x100;  memcpy(sect + 16, &v, 4);
    v = 0x170;  memcpy(sect + 20, &v, 4);

    uint8_t *d = f + 0x170;                    /* .idata raw = RVA 0x2000 */
    /* descriptors (20B): ilt@0, ts@4, fwd@8, name@12, iat@16 */
    *((uint32_t*)(d + 0))  = 0x2060; *((uint32_t*)(d + 12)) = 0x2040; *((uint32_t*)(d + 16)) = 0x2090;
    *((uint32_t*)(d + 20)) = 0x2080; *((uint32_t*)(d + 32)) = 0x2050; *((uint32_t*)(d + 36)) = 0x20A8;
    strcpy((char*)(d + 0x40), "KERNEL32.dll");
    strcpy((char*)(d + 0x50), "USER32.dll");
    uint64_t *ilt0 = (void*)(d + 0x60); ilt0[0] = 0x20C0; ilt0[1] = 0x800000000000000DULL; ilt0[2] = 0;
    uint64_t *ilt1 = (void*)(d + 0x80); ilt1[0] = 0x20D0; ilt1[1] = 0;
    /* IAT slots (leave zero; the snap writes them) */
    uint8_t *nm0 = d + 0xC0; *((uint16_t*)nm0) = 0; strcpy((char*)(nm0 + 2), "Sleep");   /* 0x20C0 */
    uint8_t *nm1 = d + 0xD0; *((uint16_t*)nm1) = 0; strcpy((char*)(nm1 + 2), "Beep");    /* 0x20D0 */
    return 0x170 + 0x100;
}

int main(void) {
    uint8_t file[2048];
    size_t fsz = build_pe(file, sizeof(file));
    wubu_pe_info_t info;
    CHECK(wubu_pe_parse(file, fsz, &info) == 0, "parse succeeds");
    CHECK(info.import_rva == 0x2000u, "import_rva parsed (dir 1)");

    wubu_pe_section_t sects[4];
    int ns = wubu_pe_sections(file, fsz, &info, sects, 4);
    CHECK(ns == 1, "one .idata section");

    static uint8_t image[0x3000];              /* IAT is data only; no exec needed */
    memset(image, 0, sizeof(image));
    memcpy(image + sects[0].virtual_addr, file + sects[0].raw_ptr, sects[0].raw_size);

    /* sanity: the import WALKER (PE4) sees 2 DLLs, KERNEL32 with 2 funcs */
    wubu_pe_import_t imps[4];
    int ni = wubu_pe_imports(file, fsz, &info, sects, ns, imps, 4);
    CHECK(ni == 2, "two import descriptors walked");
    CHECK(ni >= 1 && strcmp(imps[0].dll, "KERNEL32.dll") == 0, "desc0 dll name");
    CHECK(ni >= 1 && imps[0].func_count > 0, "desc0 has resolved function thunks");

    /* snap the IAT */
    g_ncalls = 0; g_failresolve = 0;
    int patched = wubu_pe_snap_iat(file, fsz, &info, sects, ns, image, resolve);
    CHECK(patched == 3, "3 IAT slots patched (1 name + 1 ordinal + 1 name)");

    CHECK(g_ncalls == 3, "resolver called 3 times");
    if (g_ncalls >= 1) {
        CHECK(strcmp(g_calls[0].dll, "KERNEL32.dll") == 0 && strcmp(g_calls[0].func, "Sleep") == 0
              && g_calls[0].is_ord == 0, "call0 = KERNEL32!Sleep by name");
        CHECK(strcmp(g_calls[1].dll, "KERNEL32.dll") == 0 && g_calls[1].is_ord == 1
              && g_calls[1].ord == 13, "call1 = KERNEL32 ordinal 13");
        CHECK(strcmp(g_calls[2].dll, "USER32.dll") == 0 && strcmp(g_calls[2].func, "Beep") == 0
              && g_calls[2].is_ord == 0, "call2 = USER32!Beep by name");
    }
    /* each resolved address landed in the correct IAT slot */
    if (patched >= 3) {
        uint64_t a0, a1, a2;
        memcpy(&a0, image + 0x2090, 8);   /* IAT0[0] Sleep */
        memcpy(&a1, image + 0x2098, 8);   /* IAT0[1] ord 13 */
        memcpy(&a2, image + 0x20A8, 8);   /* IAT1[0] Beep */
        CHECK(a0 == g_calls[0].addr, "IAT[0] (Sleep) written");
        CHECK(a1 == g_calls[1].addr, "IAT[1] (ord 13) written");
        CHECK(a2 == g_calls[2].addr, "IAT[2] (Beep, next descriptor) written");
        uint64_t t0; memcpy(&t0, image + 0x2060, 8);
        CHECK(t0 == 0x20C0, "ILT (OriginalFirstThunk) left untouched");
    }

    /* a resolver miss aborts the load */
    g_ncalls = 0; g_failresolve = 1;
    wubu_pe_section_t *sp = ns > 0 ? sects : NULL;
    CHECK(wubu_pe_snap_iat(file, fsz, &info, sp, ns, image, resolve) == -1,
          "unsatisfiable import returns -1");

    /* an image with no import directory snaps 0 slots */
    wubu_pe_info_t ni2 = info; ni2.import_rva = 0;
    g_ncalls = 0; g_failresolve = 0;
    CHECK(wubu_pe_snap_iat(file, fsz, &ni2, sp, ns, image, resolve) == 0,
          "no-import image returns 0 (no slots)");
    CHECK(g_ncalls == 0, "no-import image calls resolver zero times");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}