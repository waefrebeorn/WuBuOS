/*
 * mkmod.c  --  emit A3 demo modules for wubu_zx_run (docs/GAP_ROADMAP A3).
 *
 * Two modes:
 *
 *  1. Self-contained PE32 (single .text) returning a fixed constant:
 *       mkmod <out.pe> <value>
 *     entry = `mov eax, imm32; ret`. No imports -> runs at any base.
 *
 *  2. Kernel-import PE32 (2 sections .text + .idata) that imports ONE
 *     function from a DLL and COMBINES it with a local constant:
 *       mkmod <out.pe> <const> <dll> <name>
 *     entry = `mov ebx, <const>; call [IAT slot for <dll>!<name>];
 *              add eax, ebx; ret`. The imported function's return lands in
 *     eax, add ebx -> the result is (kernel_export() + <const>). If the IAT
 *     snap or the cross-module call fails, the result is wrong -> a broken
 *     on-kernel import path can't fake the observed value.
 *
 * Both layouts mirror the hand-built PE in src/kernel/test/w9pc_test.c
 * (proven to load through wubu_zx_run at two relocation bases on host).
 * PE32 magic 0x10B, image_base 0x00400000, entry_rva 0x1030, .text at
 * VA 0x1000 (raw 0x190), .idata at VA 0x2000 (raw 0x1D0). No .reloc: every
 * address is rip-relative, so the image runs at any load base.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define DOF      0x80
#define OPTS     160
#define TEXT_VA      0x1000
#define ENTRY_RVA    0x1030
#define TEXT_RAW     0x190
#define TEXT_SZ      0x40
#define IDATA_VA     0x2000
#define IDATA_RAW    0x1D0
#define IDATA_SZ     0x80
#define IAT_RVA      0x2050      /* FirstThunk slot -> gets resolved addr */
#define IMGBASE      0x00400000u

static void put32(uint8_t *p, uint32_t v){ memcpy(p, &v, 4); }
static void put16(uint8_t *p, uint16_t v){ memcpy(p, &v, 2); }

int main(int argc, char **argv)
{
    int import = (argc == 5);
    if (argc != 3 && !import) {
        fprintf(stderr, "usage: %s <out> <value>             (self-contained)\n"
                        "       %s <out> <const> <dll> <name>  (imports one fn)\n",
                argv[0], argv[0]);
        return 2;
    }
    int const_add = atoi(argv[2]);
    const char *dll  = import ? argv[3] : NULL;
    const char *name = import ? argv[4] : NULL;

    uint32_t total = import ? (IDATA_RAW + IDATA_SZ) : (TEXT_RAW + TEXT_SZ + 16);
    uint8_t *f = calloc(1, total);
    if (!f) { perror("calloc"); return 1; }

    /* DOS header */
    f[0] = 'M'; f[1] = 'Z';
    put32(f + 0x3C, DOF);

    /* PE header */
    put32(f + DOF, 0x00004550);
    put16(f + DOF + 4,  0x8664);            /* x64 machine */
    put16(f + DOF + 6,  import ? 2 : 1);    /* sections */
    put16(f + DOF + 20, OPTS);
    put16(f + DOF + 22, 0x2022);

    /* optional header (PE32) */
    uint8_t *opt = f + DOF + 24;
    put16(opt + 0,  0x10B);                 /* PE32 magic */
    put32(opt + 16, ENTRY_RVA);
    put32(opt + 28, IMGBASE);
    put16(opt + 68, 3);                     /* subsystem CUI */
    if (import) {
        /* data dir 1 = import table at RVA 0x2000, size 0x80 */
        put32(opt + 104, IDATA_VA);
        put32(opt + 108, IDATA_SZ);
    }

    /* section table @ 0x138 */
    uint8_t *sect = f + DOF + 24 + OPTS;
    memcpy(sect, ".text", 8);
    put32(sect + 8,  TEXT_SZ);
    put32(sect + 12, TEXT_VA);
    put32(sect + 16, TEXT_SZ);
    put32(sect + 20, TEXT_RAW);

    uint8_t *ep = f + TEXT_RAW + (ENTRY_RVA - TEXT_VA);   /* @ 0x1C0 */
    if (!import) {
        ep[0] = 0xB8; put32(ep + 1, (uint32_t)(int32_t)const_add); ep[5] = 0xC3;
    } else {
        /* mov ebx, const ; call [rip+disp -> IAT] ; add eax,ebx ; ret.
         * disp is relative to rip AFTER the 6-byte call, and the call is at
         * ENTRY_RVA+5 (after the 5-byte mov), so rip_after = ENTRY_RVA+11. */
        ep[0] = 0xBB; put32(ep + 1, (uint32_t)(int32_t)const_add);
        ep[5] = 0xFF; ep[6] = 0x15;
        int32_t disp = (int32_t)IAT_RVA - ((int32_t)ENTRY_RVA + 5 + 6);
        put32(ep + 7, (uint32_t)disp);
        ep[11] = 0x01; ep[12] = 0xD8;        /* add eax, ebx */
        ep[13] = 0xC3;
    }

    if (import) {
        uint8_t *idat = sect + 40;           /* second section header */
        memcpy(idat, ".idata", 8);
        put32(idat + 8,  IDATA_SZ);
        put32(idat + 12, IDATA_VA);
        put32(idat + 16, IDATA_SZ);
        put32(idat + 20, IDATA_RAW);

        /* import descriptor at 0x1D0 (VA 0x2000) */
        uint8_t *d = f + IDATA_RAW;
        put32(d + 0,  0x2040);              /* OriginalFirstThunk (ILT RVA) */
        put32(d + 12, 0x2030);              /* Name RVA (DLL string) */
        put32(d + 16, IAT_RVA);             /* FirstThunk -> patched slot */
        uint64_t ilt = 0x2060;              /* ILT @ raw 0x210 -> name entry */
        memcpy(d + 0x40, &ilt, 8);
        strcpy((char *)(d + 0x30), dll);    /* DLL name @ raw 0x200 -> 0x2030 */
        uint16_t hint = 0;                  /* name entry @ raw 0x230 -> 0x2060 */
        memcpy(d + 0x60, &hint, 2);
        strcpy((char *)(d + 0x62), name);
    }

    FILE *w = fopen(argv[1], "wb");
    if (!w) { perror(argv[1]); return 1; }
    if (fwrite(f, 1, total, w) != total) { perror("fwrite"); return 1; }
    fclose(w);
    fprintf(stderr, "mkmod: %s (%u bytes) const=%d %s\n",
            argv[1], total, const_add,
            import ? "IMPORT mode" : "self-contained");
    free(f);
    return 0;
}
