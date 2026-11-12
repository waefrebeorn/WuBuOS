/* wubu_pe_full_test.c — the full load→entry pipeline end to end.
 * Builds a PE whose .text entry is `call [rip+disp]` into its own IAT (which the
 * loader snaps to a real host C function), proving: parse -> map sections ->
 * base-relocate (no-op at preferred base) -> snap IAT -> TLS (none) -> call
 * entry. The entry returns the imported function's result, so a non-7 result
 * means the dependency did NOT actually connect. */
#include "../../egojar.h"
#include "../../egojar_net.h"
#include "wubu_pe.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

static int host_answer(void) { return 7; }

static int g_resolve_hits = 0;
static int resolve(const char *dll, const char *func, uint32_t ord, int is_ord, uintptr_t *out) {
    if (!is_ord && strcmp(dll, "HOST.dll") == 0 && strcmp(func, "ANSWER") == 0) {
        *out = (uintptr_t)&host_answer;
        g_resolve_hits++;
        return 0;
    }
    return 1;
}

/* Build a PE32 with two sections (x64):
 *   .text  @ RVA 0x1000 raw 0x170 : entry code at RVA 0x1030
 *          FF 15 <disp32> C3  = call qword [rip+disp] ; ret
 *          disp = target(IAT slot VA 0x00402050) - (entry VA 0x00401030 + 6)
 *   .idata @ RVA 0x2000 raw 0x1B0 : desc{HOST.dll -> ANSWER}
 * Section table @ 0x138 (optsize=160, 2 sections).
 * image_base = 0x00400000 (load at preferred base -> relocation is a no-op). */
static size_t build_pe(uint8_t *f, size_t cap) {
    memset(f, 0, cap);
    f[0] = 'M'; f[1] = 'Z';
    uint32_t pe_off = 0x80; memcpy(f + 0x3C, &pe_off, 4);
    uint8_t *pe = f + pe_off;
    uint32_t sig = 0x00004550; memcpy(pe, &sig, 4);
    uint16_t nsects = 2, optsize = 160; memcpy(pe + 7 - 1, &nsects, 2); memcpy(pe + 20, &optsize, 2);
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

    /* .text raw: entry stub `call [rip+disp]; ret` at raw+0x30 (RVA 0x1030) */
    uint8_t *text = f + 0x190;
    memset(text, 0xCC, 0x40);
    uint32_t iat_va = 0x00400000u + 0x2050u;       /* IAT slot "ANSWER" */
    uint32_t call_rip = 0x00400000u + 0x1030u + 6u;
    int32_t disp = (int32_t)(iat_va - call_rip);
    uint8_t *ep = text + 0x30;                     /* entry at RVA 0x1030 */
    ep[0] = 0xFF; ep[1] = 0x15;                    /* call qword [rip+disp32] */
    memcpy(ep + 2, &disp, 4);
    ep[6] = 0xC3;                                  /* ret */

    /* .idata raw @ 0x1B0 (RVA 0x2000) */
    uint8_t *d = f + 0x1D0;
    *((uint32_t*)(d + 0))  = 0x2040; *((uint32_t*)(d + 12)) = 0x2030; *((uint32_t*)(d + 16)) = 0x2050;
    strcpy((char*)(d + 0x30), "HOST.dll");
    uint64_t *ilt = (void*)(d + 0x40); ilt[0] = 0x2060; ilt[1] = 0;
    uint8_t *ns = d + 0x60; *((uint16_t*)ns) = 0; strcpy((char*)(ns + 2), "ANSWER");
    return 0x1D0 + 0x80;
}

int main(void) {
    uint8_t file[2048];
    size_t fsz = build_pe(file, sizeof(file));
    wubu_pe_info_t info;
    CHECK(wubu_pe_parse(file, fsz, &info) == 0, "parse succeeds");

    wubu_pe_section_t sects[4];
    int ns = wubu_pe_sections(file, fsz, &info, sects, 4);
    fprintf(stderr, "> parsed, ns=%d\n", ns);
    CHECK(ns == 2, "two sections (.text + .idata)");

    /* map into EXECUTABLE memory AT THE PREFERRED BASE (0x00400000). The image's
     * cross-section call targets are absolute VAs (entry computes RIP-relative
     * displacement against 0x400000), so the loader must place it there — this
     * is exactly why a loader chooses a base and relocates when it cannot. */
    uint8_t *image = mmap((void *)0x00400000, 0x3000,
                          PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    CHECK(image != MAP_FAILED && image == (void *)0x00400000,
          "exec-memory mapped at the preferred base (0x00400000)");
    fprintf(stderr, "> mmap=%p rc=%d\n", (void*)image, (int)((uintptr_t)image-0x400000));
    memset(image, 0, 0x3000);
    for (int i = 0; i < ns; i++)
        memcpy(image + sects[i].virtual_addr, file + sects[i].raw_ptr, sects[i].raw_size);

    /* 1. relocation (no-op at preferred base) */
    CHECK(wubu_pe_relocate(&info, sects, ns, file, fsz, image, 0x00400000u) == 0,
          "relocate at preferred base returns 0");

    /* 2. snap the IAT: the entry depends on HOST.dll!ANSWER */
    g_resolve_hits = 0;
    CHECK(wubu_pe_snap_iat(file, fsz, &info, sects, ns, image, resolve) == 1,
          "one IAT slot snapped");
    uintptr_t slot = 0;
    memcpy(&slot, image + 0x2050, sizeof(uintptr_t));
    CHECK(slot == (uintptr_t)&host_answer, "IAT slot points at the host function");

    /* 3. run the entry: it must CALL through the patched IAT and return 7.
     *    If the dependency did NOT connect, the call target is 0/0xCC -> crash
     *    or garbage, proving the whole load pipeline worked. */
    int rc = wubu_pe_call_entry(&info, sects, ns, image);
    fprintf(stderr, "> entry returned %d\n", rc);
    CHECK(rc == 7, "entry call-through-import returned the host function's result (7)");
    CHECK(g_resolve_hits == 1, "resolver consulted exactly once for the import");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}