/* wubu_pe_holyd_test.c -- the SELF-HOSTING forcing function.
 * Proves holyd-compiled machine code runs through the in-kernel PE loader
 * end to end: hd_compile("...") emits a position-relative, directly-callable
 * blob -> packaging it as a PE .text -> parse -> map -> relocate -> call
 * entry -> the exact holyd arithmetic result comes out. The blob's encoding
 * (verified by disassembly) has NO absolute or RIP-relative references, so it
 * is a pure position-independent function body that survives being copied
 * into a mapped image and even a base-relocation pass.
 *
 * This is P1's forcing function: if holyd output can be carried inside a PE
 * and executed by wubu_pe_*, then a real holyd module can self-host against
 * the VSL/NT export table once the symbol registry resolvers are wired up. */
#include "../../egojar.h"
#include "wubu_pe.h"
#include "holyd.h"
#include "holyd_codegen.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

/* Build a minimal PE32 (x64) whose single .text section carries `blob` at
 * RVA 0x1000, entry RVA 0x1010. No imports, no reloc dir (the blob is PIC).
 * image_base = 0x00400000. Returns the file size. */
static size_t build_pe_wrap(uint8_t *f, size_t cap,
                            const uint8_t *blob, size_t blob_size) {
    memset(f, 0, cap);
    f[0] = 'M'; f[1] = 'Z';
    uint32_t pe_off = 0x80; memcpy(f + 0x3C, &pe_off, 4);
    uint8_t *pe = f + pe_off;
    uint32_t sig = 0x00004550; memcpy(pe, &sig, 4);
    uint16_t machine = 0x8664, nsects = 1, optsize = 160;
    memcpy(pe + 4, &machine, 2); memcpy(pe + 6, &nsects, 2);
    memcpy(pe + 20, &optsize, 2);
    uint8_t *opt = pe + 24;
    uint16_t magic = 0x10B; memcpy(opt, &magic, 2);
    uint32_t base = 0x00400000; memcpy(opt + 28, &base, 4);
    uint32_t entry = 0x1010; memcpy(opt + 16, &entry, 4);
    uint16_t subsystem = 3; memcpy(opt + 68, &subsystem, 2);

    uint8_t *sect = opt + optsize;                  /* section table @ 0x138 */
    uint32_t v;
    memcpy(sect, ".text", 8);
    v = 0x40;  memcpy(sect + 8, &v, 4);              /* virtual size */
    v = 0x1000; memcpy(sect + 12, &v, 4);            /* virtual addr */
    v = 0x40;  memcpy(sect + 16, &v, 4);             /* size raw */
    v = 0x1A0; memcpy(sect + 20, &v, 4);             /* raw ptr (past headers) */
    uint32_t chars = 0x60000020; memcpy(sect + 36, &chars, 4); /* X+R+code */

    /* .text raw @ 0x1A0: entry code at raw+0x10 (RVA 0x1010) */
    uint8_t *text = f + 0x1A0;
    memset(text, 0xCC, 0x40);
    memcpy(text + 0x10, blob, blob_size);
    return 0x1A0 + 0x40;
}

int main(void) {
    /* 1. holyd compiles an arithmetic expression to a runnable blob. */
    size_t bsz = 0;
    void *blob = hd_compile("40+2;", &bsz);
    CHECK(blob != NULL && bsz > 0, "hd_compile produced a blob");
    fprintf(stderr, "> holyd blob: %zu bytes\n", bsz);

    /* sanity: the raw blob IS callable and yields 42 before packaging. */
    int64_t direct = -1;
    if (blob && bsz) direct = ((int64_t (*)(void))blob)();
    CHECK(direct == 42, "raw holyd blob returns 42 (40+2)");
    fprintf(stderr, "> raw blob() -> %lld\n", (long long)direct);

    /* 2. package the blob as the .text of a PE. */
    uint8_t file[2048];
    size_t fsz = build_pe_wrap(file, sizeof(file), (const uint8_t*)blob, bsz);

    wubu_pe_info_t info;
    CHECK(wubu_pe_parse(file, fsz, &info) == 0, "PE parse succeeds");
    wubu_pe_section_t sects[4];
    int ns = wubu_pe_sections(file, fsz, &info, sects, 4);
    CHECK(ns == 1, "one section (.text)");
    fprintf(stderr, "> parsed, ns=%d entry_rva=%x\n", ns, info.entry_rva);

    /* 3. map at the preferred base (0x00400000) and load the section raw. */
    uint8_t *image = mmap((void *)0x00400000, 0x2000,
                          PROT_READ | PROT_WRITE | PROT_EXEC,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    CHECK(image != MAP_FAILED && image == (void *)0x00400000,
          "exec-memory mapped at preferred base");
    memset(image, 0, 0x2000);
    for (int i = 0; i < ns; i++)
        memcpy(image + sects[i].virtual_addr, file + sects[i].raw_ptr, sects[i].raw_size);

    /* 4. relocation (no-op: no .reloc dir; the blob is PIC anyway). */
    CHECK(wubu_pe_relocate(&info, sects, ns, file, fsz, image, 0x00400000u) == 0,
          "relocate returns 0 (no relocation directory)");

    /* 5. run the entry: the holyd machine code executes inside the image. */
    int rc = wubu_pe_call_entry(&info, sects, ns, image);
    fprintf(stderr, "> entry (holyd code) -> %d\n", rc);
    CHECK(rc == 42, "holyd blob executed through the PE loader returned 42");

    /* 6. PROVE POSITION INDEPENDENCE: map a SECOND copy at a DIFFERENT base
     * (0x00420000, delta = +0x20000) so any absolute/RIP-relative reference
     * would break. The blob has none, so it must still return 42 — this is
     * what lets busily-relocatable holyd modules survive an arbitrary base. */
    uint8_t *image2 = mmap((void *)0x00420000, 0x2000,
                           PROT_READ | PROT_WRITE | PROT_EXEC,
                           MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    CHECK(image2 != MAP_FAILED && image2 == (void *)0x00420000,
          "second image mapped at shifted base 0x00420000");
    memset(image2, 0, 0x2000);
    for (int i = 0; i < ns; i++)
        memcpy(image2 + sects[i].virtual_addr, file + sects[i].raw_ptr, sects[i].raw_size);
    (void)wubu_pe_relocate(&info, sects, ns, file, fsz, image2, 0x00420000u);
    int rc2 = wubu_pe_call_entry(&info, sects, ns, image2);
    fprintf(stderr, "> entry at shifted base -> %d\n", rc2);
    CHECK(rc2 == 42, "holyd blob at a shifted base still returns 42 (position-independent)");

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
