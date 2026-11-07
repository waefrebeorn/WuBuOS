/*
 * mkmod.c  --  emit the A3 demo module: a SELF-CONTAINED PE32 that a
 * wubu_zx_run image loader accepts and that returns a FIXED value.
 *
 * Layout mirrors the hand-built module in src/kernel/test/w9pc_test.c
 * exactly (so the loader's parser is known to accept it), minus the .idata
 * import section -- this module has NO imports, so it needs no symbol
 * registry and runs at any base. Code: `mov eax, imm32; ret`.
 *
 *   usage: mkmod <out.pe> <return-value:int>
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char **argv)
{
    if (argc != 3) { fprintf(stderr, "usage: %s <out.pe> <value>\n", argv[0]); return 2; }
    int retval = atoi(argv[2]);

    const uint32_t DOF = 0x80;   /* PE/COFF header offset */
    const uint32_t OPTS = 160;   /* optional header size (PE32) */
    const uint32_t NSECTS = 1;
    const uint32_t SECT = DOF + 24 + OPTS;   /* 0x138 */
    const uint32_t VA = 0x1000;
    const uint32_t ENTRY_RVA = 0x1030;
    const uint32_t RAW = 0x190;             /* .text raw file offset */
    const uint32_t VSZ = 0x40, RSZ = 0x40;

    uint32_t total = RAW + RSZ + 16;
    uint8_t *f = calloc(1, total);
    if (!f) { perror("calloc"); return 1; }

    /* DOS header */
    f[0] = 'M'; f[1] = 'Z';
    memcpy(f + 0x3C, &DOF, 4);

    /* PE header */
    uint32_t sig = 0x00004550; memcpy(f + DOF, &sig, 4);
    uint16_t machine = 0x8664; memcpy(f + DOF + 4, &machine, 2);
    uint16_t ns = 1;            memcpy(f + DOF + 6, &ns, 2);
    uint16_t osz = (uint16_t)OPTS; memcpy(f + DOF + 20, &osz, 2);
    uint16_t chars = 0x2022;    memcpy(f + DOF + 22, &chars, 2);

    /* optional header (PE32) */
    uint8_t *opt = f + DOF + 24;
    uint16_t magic = 0x10B;  memcpy(opt + 0, &magic, 2);
    uint32_t entry = ENTRY_RVA; memcpy(opt + 16, &entry, 4);
    uint32_t ibase = 0x00400000; memcpy(opt + 28, &ibase, 4);
    uint16_t subsys = 3;     memcpy(opt + 68, &subsys, 2);
    /* data dirs: import(1), reloc(5), tls(9) all stay zero -> no imports,
     * no base fixups, no TLS callbacks. */

    /* one .text section */
    uint8_t *sect = f + SECT;
    memcpy(sect, ".text", 8);
    memcpy(sect + 8, &VSZ, 4);          /* VirtualSize */
    memcpy(sect + 12, &VA, 4);          /* VirtualAddress */
    memcpy(sect + 16, &RSZ, 4);         /* SizeOfRawData */
    memcpy(sect + 20, &RAW, 4);         /* PointerToRawData */

    /* entry: mov eax, imm32 ; ret     (imm32 = return value) */
    uint8_t *ep = f + RAW + (ENTRY_RVA - VA);
    ep[0] = 0xB8;
    uint32_t v = (uint32_t)(int32_t)retval;
    memcpy(ep + 1, &v, 4);
    ep[5] = 0xC3;

    FILE *w = fopen(argv[1], "wb");
    if (!w) { perror(argv[1]); return 1; }
    if (fwrite(f, 1, total, w) != total) { perror("fwrite"); return 1; }
    fclose(w);
    fprintf(stderr, "mkmod: wrote %s (%u bytes, entry returns %d)\n", argv[1], total, retval);
    free(f);
    return 0;
}
