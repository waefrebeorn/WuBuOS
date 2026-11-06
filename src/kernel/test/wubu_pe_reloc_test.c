/* wubu_pe_reloc_test.c — base relocation (.reloc) round-trip for the in-kernel PE loader.
 * Builds a minimal PE in memory, parses it, walks its section table, maps it, then
 * applies wubu_pe_relocate at a shifted load base and asserts every fixup raised by
 * delta = load_base - image_base (ReactOS loader relocation semantics). */
#include "wubu_pe.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>

static int g_pass = 0, g_fail = 0;
#define CHECK(cond, msg) do { if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL: %s\n", msg); } } while (0)

/* Build a minimal PE32 with one ".reloc" section whose block holds one
 * HIGHLOW(3) fixup at page offset 0x123 and one DIR64(10) fixup at 0x128.
 * Returns the file size; the on-image data lives at RVA 0x1123 (u32) and 0x1128 (u64). */
static size_t build_pe(uint8_t *file, size_t cap) {
    memset(file, 0, cap);
    file[0] = 'M'; file[1] = 'Z';
    uint32_t pe_off = 0x80;
    memcpy(file + 0x3C, &pe_off, 4);

    uint8_t *pe = file + pe_off;
    uint32_t sig = 0x00004550;
    memcpy(pe, &sig, 4);
    /* COFF header @ pe+4 (20 bytes) */
    uint16_t machine = 0x8664, nsects = 1, optsize = 160;
    memcpy(pe + 4, &machine, 2); memcpy(pe + 6, &nsects, 2);
    memcpy(pe + 20, &optsize, 2);

    uint8_t *opt = pe + 24;                 /* optional header @ 0x98 */
    uint16_t magic = 0x10B;
    memcpy(opt, &magic, 2);                 /* PE32 */
    uint32_t base = 0x00400000;
    memcpy(opt + 28, &base, 4);             /* image_base */
    uint32_t entry = 0x1000;
    memcpy(opt + 16, &entry, 4);            /* AddressOfEntryPoint */
    uint16_t subsystem = 3;
    memcpy(opt + 68, &subsystem, 2);
    uint32_t irva = 0, isz = 0;
    memcpy(opt + 104, &irva, 4); memcpy(opt + 108, &isz, 4);   /* import dir empty */
    uint32_t rrva = 0x1000, rsz = 12;
    memcpy(opt + 136, &rrva, 4); memcpy(opt + 140, &rsz, 4);    /* .reloc dir idx 5 */

    /* section table @ opt + optsize = 0x98 + 160 = 0x138 */
    uint8_t *sect = opt + optsize;
    memcpy(sect, ".reloc", 8);
    uint32_t vsz = 0x1000, vaddr = 0x1000, rsz2 = 12, rptr = 0x160;
    memcpy(sect + 8, &vsz, 4);
    memcpy(sect + 12, &vaddr, 4);
    memcpy(sect + 16, &rsz2, 4);
    memcpy(sect + 20, &rptr, 4);

    /* the .reloc block @ 0x160: page_rva=0x1000, size=12, two fixups */
    uint8_t *blk = file + rptr;
    uint32_t prva = 0x1000, psz = 12;
    memcpy(blk, &prva, 4); memcpy(blk + 4, &psz, 4);
    uint16_t e1 = (3u << 12) | 0x123;       /* HIGHLOW @ +0x123 */
    uint16_t e2 = (10u << 12) | 0x128;      /* DIR64   @ +0x128 */
    memcpy(blk + 8, &e1, 2); memcpy(blk + 10, &e2, 2);

    return rptr + psz;
}

int main(void) {
    uint8_t file[1024];
    size_t fsz = build_pe(file, sizeof(file));

    /* 1. parse — reloc dir must be found */
    wubu_pe_info_t info;
    CHECK(wubu_pe_parse(file, fsz, &info) == 0, "parse succeeds");
    CHECK(info.image_base == 0x00400000u, "image_base parsed");
    CHECK(info.reloc_rva == 0x1000u, "reloc_rva parsed (dir 5)");
    CHECK(info.reloc_size == 12u, "reloc_size parsed");

    /* 2. sections */
    wubu_pe_section_t sects[4];
    int ns = wubu_pe_sections(file, fsz, &info, sects, 4);
    CHECK(ns == 1, "one section");
    CHECK(ns > 0 && sects[0].virtual_addr == 0x1000u && sects[0].raw_ptr == 0x160u,
          "section covers the reloc block");

    /* 3. a "loaded" image (the mapped sections at some base). RVA 0x1123 = u32,
     * RVA 0x1128 = u64, both holding preferred-base-style absolute pointers. */
    uint8_t image[0x2000];
    memset(image, 0, sizeof(image));
    uint32_t p32 = 0x00401123;
    uint64_t p64 = 0x0000000000401128ULL;
    memcpy(image + 0x1123, &p32, 4);
    memcpy(image + 0x1128, &p64, 8);

    /* 4. relocation at the PREFERRED base is a no-op but still succeeds. */
    wubu_pe_section_t *sp = ns > 0 ? sects : NULL;
    int rc = wubu_pe_relocate(&info, sp, ns, file, fsz, image, 0x00400000u);
    CHECK(rc == 0, "relocate at preferred base returns 0");
    uint32_t v32; uint64_t v64;
    memcpy(&v32, image + 0x1123, 4);
    CHECK(v32 == 0x00401123u, "preferred base leaves u32 unchanged");

    /* 5. relocation at a SHIFTED base raises both fixups by the delta. */
    rc = wubu_pe_relocate(&info, sp, ns, file, fsz, image, 0x00500000u);
    CHECK(rc == 0, "relocate at shifted base returns 0");
    memcpy(&v32, image + 0x1123, 4);
    memcpy(&v64, image + 0x1128, 8);
    CHECK(v32 == 0x00401123u + 0x100000u, "HIGHLOW fixup raised by delta");
    CHECK(v64 == 0x0000000000401128ULL + 0x100000ULL, "DIR64 fixup raised by delta");

    /* 6. an image with NO reloc directory is a no-op success. */
    wubu_pe_info_t noinfo = info;
    noinfo.reloc_rva = 0;
    memset(image, 0, sizeof(image));
    memcpy(image + 0x1123, &p32, 4);
    CHECK(wubu_pe_relocate(&noinfo, sp, ns, file, fsz, image, 0x00500000u) == 0,
          "no-reloc image returns 0");
    memcpy(&v32, image + 0x1123, 4);
    CHECK(v32 == 0x00401123u, "no-reloc image leaves data unchanged");

    /* 7. malformed block (size < 8) is rejected. */
    uint8_t bad[1024];
    size_t bsz = build_pe(bad, sizeof(bad));
    /* corrupt block size to something < 8 (3) */
    memcpy(bad + 0x164, &(uint32_t){3}, 4);
    wubu_pe_info_t bi;
    if (wubu_pe_parse(bad, bsz, &bi) == 0) {
        wubu_pe_section_t bs[4];
        int bn = wubu_pe_sections(bad, bsz, &bi, bs, 4);
        CHECK(wubu_pe_relocate(&bi, bn > 0 ? bs : NULL, bn, bad, bsz, image, 0x00500000u) != 0,
              "malformed reloc block rejected");
    } else {
        CHECK(0, "malformed test: parse should still succeed");
    }

    printf("\n=== Results: %d passed, %d failed ===\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}