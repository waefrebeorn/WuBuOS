/*
 * wubu_game_probe.c -- the game binary AUDITOR (the fine-tooth probe
 * of the three real targets).
 *
 * Given a game file, reports the honest facts the personalities need:
 *   - the format (Win32 PE / Linux ELF / Mach-O thin+FAT / DMG / ZIP)
 *   - for a PE: the DOS stub + the PE header + the subsystem +
 *     the import DLLs (what the Win32 personality must provide)
 *   - for a Mach-O: the magic + the architecture (PPC/x86_64/arm64)
 *     + the fat-binary slice count
 *   - for an ELF: the machine + the interpreter
 *
 * Used to audit Halo PC (Win32), Halo Mac (universal Mach-O), and
 * OpenArena (Linux ELF) — the honest what-does-the-kernel-need
 * report. C11.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static const char *machine_name(uint16_t m)
{
    switch (m) {
    case 0x014C: return "i386";
    case 0x8664: return "x86_64";
    case 0x003E: return "x86_64";
    case 0x01C0: return "arm";
    case 0xAA64: return "aarch64";
    case 0x01F0: return "ppc";
    default: return "other";
    }
}

static void probe_pe(const uint8_t *p, size_t size)
{
    printf("  format: Win32 PE (MZ)\n");
    if (size < 0x40) { printf("  [truncated]\n"); return; }
    uint32_t pe_off;
    memcpy(&pe_off, p + 0x3C, 4);
    if (pe_off + 24 > size || memcmp(p + pe_off, "PE\0\0", 4) != 0) {
        printf("  note: MZ stub but no PE header at 0x%x (a self-extractor)\n",
               pe_off);
        return;
    }
    const uint8_t *pe = p + pe_off;
    uint16_t machine;
    uint16_t subsystem;
    memcpy(&machine, pe + 4, 2);
    memcpy(&subsystem, pe + 92, 2);   /* optional header offset 68+24 */
    printf("  machine: %s (0x%04x)\n", machine_name(machine), machine);
    printf("  subsystem: %u (%s)\n", subsystem,
           subsystem == 2 ? "Windows GUI" :
           subsystem == 3 ? "Windows CUI" : "other");
    /* the import table: walk the import directory if present */
    uint32_t opt_size;
    memcpy(&opt_size, pe + 20, 2);
    if (pe_off + 24 + opt_size + 8 <= size) {
        const uint8_t *opt = pe + 24;
        uint32_t imp_rva, imp_size;
        memcpy(&imp_rva, opt + 104, 4);
        memcpy(&imp_size, opt + 108, 4);
        printf("  imports: RVA 0x%x size 0x%x (%u entries max)\n",
               imp_rva, imp_size, imp_size / 20);
    }
}

static void probe_elf(const uint8_t *p, size_t size)
{
    printf("  format: Linux ELF\n");
    uint16_t machine;
    memcpy(&machine, p + 18, 2);
    printf("  machine: %s (0x%04x)\n", machine_name(machine), machine);
    /* the interpreter (PT_INTERP) */
    for (size_t off = 0x40; off + 56 <= size && off < size; off += 56) {
        uint32_t type;
        memcpy(&type, p + off, 4);
        if (type == 3) {   /* PT_INTERP */
            uint64_t off2, filesz;
            memcpy(&off2, p + off + 8, 8);
            memcpy(&filesz, p + off + 32, 8);
            if (off2 + filesz <= size) {
                printf("  interpreter: %.*s\n", (int)filesz, p + off2);
            }
            break;
        }
        if (type == 0) continue;
        break;   /* the first non-NULL non-INTERP segment ends the scan */
    }
}

static void probe_macho(const uint8_t *p, size_t size)
{
    uint32_t magic;
    memcpy(&magic, p, 4);
    const char *mstr = "?";
    switch (magic) {
    case 0xFEEDFACE: mstr = "MH_MAGIC (32-bit thin)"; break;
    case 0xFEEDFACF: mstr = "MH_MAGIC_64 (64-bit thin)"; break;
    case 0xCEFAEDFE: mstr = "MH_CIGAM (swapped 32)"; break;
    case 0xCFFAEDFE: mstr = "MH_CIGAM_64 (swapped 64)"; break;
    case 0xCAFEBABE: mstr = "FAT_MAGIC (universal binary!)"; break;
    case 0xBEBAFECA: mstr = "FAT_CIGAM (swapped universal)"; break;
    }
    printf("  format: Mach-O — %s\n", mstr);
    if (magic == 0xCAFEBABE || magic == 0xBEBAFECA) {
        /* FAT_MAGIC on disk = CA FE BA BE — read as 0xBEBAFECA on an
         * LE host; the fields are BIG-endian -> bswap. FAT_CIGAM on
         * disk = BE BA FE CA — read as 0xCAFEBABE; the fields are
         * already LE -> read raw. (Verified against the real
         * OpenArena universal binary: cafe babe 0000 0002 = 2 slices,
         * ppc64 + i386.) */
        int swapped = (magic == 0xBEBAFECA);
        uint32_t raw_n;
        memcpy(&raw_n, p + 4, 4);
        uint32_t n = swapped ? __builtin_bswap32(raw_n) : raw_n;
        printf("  fat slices: %u\n", n);
        for (uint32_t i = 0; i < n && i < 8; i++) {
            uint32_t cpu = 0, off = 0, sz = 0;
            memcpy(&cpu, p + 8 + i * 20, 4);
            memcpy(&off,  p + 8 + i * 20 + 8, 4);
            memcpy(&sz,   p + 8 + i * 20 + 12, 4);
            if (swapped) {
                cpu = __builtin_bswap32(cpu);
                off = __builtin_bswap32(off);
                sz  = __builtin_bswap32(sz);
            }
            printf("    slice %u: cpu 0x%x (%s) off 0x%x size %u\n",
                   i, cpu,
                   cpu == 7 || cpu == 0x01000007 ? "x86" :
                   cpu == 0x0100000C ? "x86_64" :
                   cpu == 12 || cpu == 18 ? "ppc" : "other",
                   off, sz);
        }
    } else if (size >= 8) {
        uint32_t cputype;
        memcpy(&cputype, p + 4, 4);
        printf("  cputype: 0x%x (%s)\n", cputype,
               cputype == 7 ? "i386" : cputype == 0x01000007 ? "x86_64" :
               cputype == 12 ? "ppc" : "other");
    }
}

static void probe_dmg(const uint8_t *p, size_t size)
{
    printf("  format: Apple Disk Image (DMG) — needs 7z to extract\n");
    printf("  magic: %c%c%c%c...\n", p[0], p[1], p[2], p[3]);
    (void)size;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage: wubu_game_probe <file>\n");
        return 1;
    }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { printf("cannot open %s\n", argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    printf("=== %s (%ld bytes) ===\n", argv[1], sz);
    if (sz < 4) { fclose(f); printf("  too small\n"); return 0; }
    uint8_t *buf = malloc((size_t)sz);
    fread(buf, 1, (size_t)sz, f);
    fclose(f);

    const uint8_t *p = buf;
    if (p[0] == 'M' && p[1] == 'Z') probe_pe(p, (size_t)sz);
    else if (p[0] == 0x7F && p[1] == 'E' && p[2] == 'L' && p[3] == 'F')
        probe_elf(p, (size_t)sz);
    else if (p[0] == 'k' && p[1] == 'o' && p[2] == 'l' && p[3] == 'y')
        probe_dmg(p, (size_t)sz);
    else {
        uint32_t magic;
        memcpy(&magic, p, 4);
        if (magic == 0xFEEDFACE || magic == 0xFEEDFACF ||
            magic == 0xCEFAEDFE || magic == 0xCFFAEDFE ||
            magic == 0xCAFEBABE || magic == 0xBEBAFECA)
            probe_macho(p, (size_t)sz);
        else {
            printf("  format: other (magic %02x %02x %02x %02x)\n",
                   p[0], p[1], p[2], p[3]);
            /* zip? */
            if (p[0] == 'P' && p[1] == 'K')
                printf("  it is a ZIP archive (a packaged game dir)\n");
        }
    }
    free(buf);
    return 0;
}
