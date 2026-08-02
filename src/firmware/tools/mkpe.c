/*
 * mkpe.c  --  WuBuOS PE32+ EFI image builder (C11, no toolchain deps).
 *
 * GCC on Linux cannot emit PE32+, and pulling in a mingw cross toolchain to
 * produce a 3KB test payload is the wrong trade. This wraps a flat binary
 * (objcopy -O binary of a fixed-base freestanding link) into a valid PE32+
 * EFI application: DOS stub, COFF header, optional header, one .text
 * section covering the whole image.
 *
 * Because the payload is linked at its final ImageBase, no .reloc is needed;
 * the DLL characteristics bit for relocation-stripped images is set so a
 * conformant loader will not attempt to move it.
 *
 * usage: mkpe [-cert <cert.der>] <flat.bin> <out.efi> <image_base_hex> <entry_offset_hex>
 *       -cert attaches an Authenticode / WIN_CERTIFICATE_EFI_GUID certificate
 *             blob so Secure Boot can be exercised end-to-end.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static void put64(uint8_t *p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8 * i)); }

#define FILE_ALIGN    0x200
#define SECT_ALIGN    0x1000
#define HDR_SIZE      0x200

int main(int argc, char **argv) {
    /* Optional certificate for Authenticode / Secure Boot: -cert <file.der> */
    uint8_t *cert_blob = NULL; uint32_t cert_len = 0;
    int carg = 0;
    if (argc > 1 && strcmp(argv[1], "-cert") == 0) {
        FILE *cf = fopen(argv[2], "rb");
        if (!cf) { perror("open cert"); return 1; }
        fseek(cf, 0, SEEK_END); long cl = ftell(cf); fseek(cf, 0, SEEK_SET);
        if (cl <= 0) { fprintf(stderr, "empty cert\n"); fclose(cf); return 1; }
        cert_blob = calloc(1, (size_t)cl); cert_len = (uint32_t)cl;
        if (fread(cert_blob, 1, cert_len, cf) != cert_len) { perror("read cert"); free(cert_blob); fclose(cf); return 1; }
        fclose(cf);
        carg = 2;   /* consumed argv[1..2] */
    }
    if (argc != 5 + carg) {
        fprintf(stderr, "usage: %s [-cert <cert.der>] <flat.bin> <out.efi> <image_base_hex> <entry_off_hex>\n", argv[0]);
        return 2;
    }
    uint64_t image_base = strtoull(argv[3 + carg], NULL, 16);
    uint32_t entry_off  = (uint32_t)strtoul(argv[4 + carg], NULL, 16);

    FILE *f = fopen(argv[1 + carg], "rb");
    if (!f) { perror("open input"); return 1; }
    fseek(f, 0, SEEK_END);
    long raw_len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (raw_len <= 0) { fprintf(stderr, "empty input\n"); fclose(f); return 1; }

    uint8_t *raw = calloc(1, (size_t)raw_len);
    if (!raw || fread(raw, 1, (size_t)raw_len, f) != (size_t)raw_len) {
        fprintf(stderr, "read failed\n"); fclose(f); free(raw); return 1;
    }
    fclose(f);

    uint32_t raw_aligned = ((uint32_t)raw_len + FILE_ALIGN - 1) & ~(uint32_t)(FILE_ALIGN - 1);
    uint32_t virt_size   = (uint32_t)raw_len;
    uint32_t size_image  = (HDR_SIZE + ((virt_size + SECT_ALIGN - 1) & ~(uint32_t)(SECT_ALIGN - 1)));
    /* section RVA starts one section-alignment in */
    uint32_t sect_rva    = SECT_ALIGN;
    size_image           = sect_rva + ((virt_size + SECT_ALIGN - 1) & ~(uint32_t)(SECT_ALIGN - 1));

    uint32_t total = HDR_SIZE + raw_aligned;
    uint8_t *out = calloc(1, total);
    if (!out) { free(raw); return 1; }

    /* ---- DOS header ---- */
    out[0] = 'M'; out[1] = 'Z';
    put32(out + 0x3C, 0x80);                 /* e_lfanew */
    memcpy(out + 0x40, "WuBuFW PE32+ EFI image\r\n$", 25);

    uint8_t *pe = out + 0x80;
    put32(pe, 0x00004550);                   /* "PE\0\0" */

    /* ---- COFF header ---- */
    uint8_t *coff = pe + 4;
    put16(coff + 0,  0x8664);                /* Machine: amd64      */
    put16(coff + 2,  1);                     /* NumberOfSections    */
    put32(coff + 4,  0);                     /* TimeDateStamp       */
    put32(coff + 8,  0);
    put32(coff + 12, 0);
    put16(coff + 16, 240);                   /* SizeOfOptionalHeader */
    put16(coff + 18, 0x0022 | 0x0001);       /* EXECUTABLE | LARGE_ADDRESS_AWARE | RELOCS_STRIPPED */

    /* ---- Optional header (PE32+) ---- */
    uint8_t *opt = coff + 20;
    put16(opt + 0,  0x020B);                 /* PE32+                */
    opt[2] = 1; opt[3] = 0;                  /* linker version       */
    put32(opt + 4,  raw_aligned);            /* SizeOfCode           */
    put32(opt + 8,  0);
    put32(opt + 12, 0);
    put32(opt + 16, sect_rva + entry_off);   /* AddressOfEntryPoint  */
    put32(opt + 20, sect_rva);               /* BaseOfCode           */
    put64(opt + 24, image_base);             /* ImageBase            */
    put32(opt + 32, SECT_ALIGN);
    put32(opt + 36, FILE_ALIGN);
    put16(opt + 40, 0); put16(opt + 42, 0);  /* OS version           */
    put16(opt + 44, 0); put16(opt + 46, 0);  /* image version        */
    put16(opt + 48, 0); put16(opt + 50, 0);  /* subsystem version    */
    put32(opt + 52, 0);
    put32(opt + 56, size_image);             /* SizeOfImage          */
    put32(opt + 60, HDR_SIZE);               /* SizeOfHeaders        */
    put32(opt + 64, 0);                      /* CheckSum             */
    put16(opt + 68, 10);                     /* Subsystem: EFI app   */
    put16(opt + 70, 0);                      /* DllCharacteristics   */
    put64(opt + 72, 0x10000);                /* SizeOfStackReserve   */
    put64(opt + 80, 0x1000);
    put64(opt + 88, 0x10000);
    put64(opt + 96, 0x1000);
    put32(opt + 104, 0);
    put32(opt + 108, 16);                    /* NumberOfRvaAndSizes  */
    /* all 16 data directories left zero (offset 112 .. 240) */

    /* ---- Section header ---- */
    uint8_t *sec = opt + 240;
    memcpy(sec, ".text\0\0\0", 8);
    put32(sec + 8,  virt_size);              /* VirtualSize          */
    put32(sec + 12, sect_rva);               /* VirtualAddress       */
    put32(sec + 16, raw_aligned);            /* SizeOfRawData        */
    put32(sec + 20, HDR_SIZE);               /* PointerToRawData     */
    put32(sec + 24, 0);
    put32(sec + 28, 0);
    put16(sec + 32, 0);
    put16(sec + 34, 0);
    put32(sec + 36, 0x60000020 | 0x80000000);/* CODE|EXEC|READ|WRITE */

    memcpy(out + HDR_SIZE, raw, (size_t)raw_len);

    uint32_t cert_ptr = 0, cert_total = 0;
    uint32_t total_out = total;
    uint8_t *out2 = out;
    if (cert_blob) {
        /* Append a WIN_CERTIFICATE_EFI_GUID table aligned to 8.
         * Layout: dwLength(4) wRevision(2) wCertificateType(2)
         *         EFI_CERT_GUID: cert_type_guid(16) cert_blob(n)
         * EFI_CERT_TYPE_RSA2048_SHA256 guid =
         *   {0xa776ba23,0x90d8,0x4adf,{0xbd,0x30,0x93,0x31,0x1f,0xc3,0xa2,0x80}} */
        uint8_t cert_type_guid[16] = {
            0x23,0xba,0x76,0xa7, 0xd8,0x90, 0xad,0x4a,
            0xbd,0x30,0x93,0x31, 0x1f,0xc3,0xa2,0x80
        };
        uint32_t cert_hdr = 12 + 16;              /* WIN_CERT hdr + GUID header */
        cert_total = cert_hdr + cert_len;
        uint32_t pad = (8 - (cert_total % 8)) % 8;
        cert_total += pad;
        cert_ptr = (uint32_t)(HDR_SIZE + raw_aligned);
        cert_ptr = (cert_ptr + 7) & ~7u;
        uint32_t need = cert_ptr + cert_total;
        out2 = realloc(out, need);
        if (!out2) { free(raw); free(out); return 1; }
        out = out2; total_out = need;
        uint8_t *c = out + cert_ptr;
        put32(c + 0, cert_total);            /* dwLength           */
        put16(c + 4, 0x0200);                /* wRevision 2.0      */
        put16(c + 6, 0x0200);                /* WIN_CERT_TYPE_EFI_GUID */
        memcpy(c + 8, cert_type_guid, 16);   /* EFI_CERT_GUID type */
        memcpy(c + 24, cert_blob, cert_len); /* cert payload       */
        /* Zero-pad to 8-byte boundary. */
        memset(c + 24 + cert_len, 0, pad);
    }
    /* Point DataDirectory[4] (Security / cert table) at the cert table.
     * In a PE32+ OptionalHeader the NumberOfRvaAndSizes field is at opt+108;
     * DataDirectory begins right after it, so DD[4] sits at opt+108+4+4*8 = opt+0x90. */
    put32(opt + 0x90, cert_ptr);                /* VirtualAddress (file offset) */
    put32(opt + 0x94, cert_total);              /* Size                         */
    put16(coff + 18, 0x0022 | 0x0001);          /* mark as having a cert table */

    FILE *o = fopen(argv[2 + carg], "wb");
    if (!o) { perror("open output"); free(raw); free(out); return 1; }
    fwrite(out, 1, total_out, o);
    fclose(o);

    printf("mkpe: %s -> %s  base=0x%llx entry_rva=0x%x image=%u bytes (file %u)%s\n",
           argv[1 + carg], argv[2 + carg], (unsigned long long)image_base,
           sect_rva + entry_off, size_image, total,
           cert_blob ? " [signed]" : "");

    free(raw);
    free(out);
    return 0;
}
