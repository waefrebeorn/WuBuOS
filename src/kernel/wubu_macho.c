/*
 * wubu_macho.c -- the IN-KERNEL Mach-O loader (the macOS format, on
 * OUR kernel — not host Darling).
 *
 * The user's correction: "all this needs to be running on temple os
 * kernel our kernel not just a arch connection." The Halo Mac demo
 * ships as a FAT Mach-O (ppc64 + i386 slices). This module:
 *   - validate the Mach-O magic (MH_MAGIC_64 / MH_CIGAM_64)
 *   - parse the mach_header_64 (cpu-type/subtype, the filetype,
 *     the command count)
 *   - walk the FAT header (FAT_MAGIC / FAT_CIGAM) -> the slices
 *     — with proper endianness (the OpenArena universal binary is
 *     CA FE BA BE = big-endian fields on a LE host)
 *   - the slice offsets (where each arch's Mach-O lives)
 *
 * C11, kernel-clean, opaque, self-contained.
 */
#include "wubu_macho.h"
#include <string.h>

/* Mach-O constants */
#define MH_MAGIC_64      0xFEEDFC02
#define MH_CIGAM_64      0xCFFAEDFE
#define FAT_MAGIC        0xCAFEBABE
#define FAT_CIGAM        0xBEBAFECA

/* the magic tells us endianness */
static int is_fat(uint32_t m) { return m == FAT_MAGIC || m == FAT_CIGAM; }
static int is_swapped(uint32_t m) { return m == FAT_CIGAM || m == MH_CIGAM_64; }

/* MO1: parse a thin Mach-O header (the arch's image). */
int wubu_macho_parse(const void *data, size_t size, wubu_macho_info_t *out)
{
    if (!data || !out || size < 32) return -1;
    const uint8_t *p = (const uint8_t *)data;
    uint32_t magic; memcpy(&magic, p, 4);
    int swapped = is_swapped(magic);
    if (magic != MH_MAGIC_64 && magic != MH_CIGAM_64) return -1;

    memcpy(&out->magic,        p, 4);
    memcpy(&out->filetype,     p + 12, 4);
    memcpy(&out->ncommands,    p + 16, 4);
    memcpy(&out->sizeofcmds,   p + 20, 4);
    if (swapped) {
        out->filetype  = bswap32(out->filetype);
        out->ncommands = bswap32(out->ncommands);
        out->sizeofcmds = bswap32(out->sizeofcmds);
    }
    memcpy(&out->cpu_type, p + 4, 4);
    memcpy(&out->cpu_sub,  p + 8, 4);
    if (swapped) {
        out->cpu_type = bswap32(out->cpu_type);
        out->cpu_sub  = bswap32(out->cpu_sub);
    }
    out->entry = 0;
    return 0;
}

/* MO2: parse a FAT header -> the slices (architecture images). */
int wubu_fat_slices(const void *data, size_t size,
                    wubu_fat_slice_t *out, int max)
{
    if (!data || !out || size < 8) return -1;
    const uint8_t *p = (const uint8_t *)data;
    uint32_t magic; memcpy(&magic, p, 4);
    if (!is_fat(magic)) return -1;   /* the magic says it IS a FAT */
    int swapped = is_swapped(magic);

    uint32_t n; memcpy(&n, p + 4, 4);
    if (swapped) n = bswap32(n);
    if (n > 8) n = 8;
    if (n > (uint32_t)max) n = max;

    int count = 0;
    for (uint32_t i = 0; i < n; i++) {
        const uint8_t *s = p + 8 + i * 20;
        uint32_t cputype; memcpy(&cputype, s, 4);
        memcpy(&out[i].offset, s + 4, 4);
        memcpy(&out[i].size,   s + 8, 4);
        if (swapped) {
            cputype     = bswap32(cputype);
            out[i].offset = bswap32(out[i].offset);
            out[i].size   = bswap32(out[i].size);
        }
        out[i].cpu_type = cputype;
        count++;
    }
    return count;
}

/* MO3: the cpu-type name (ppc64 / i386 / x86_64 / arm64). */
const char *wubu_macho_cpu_name(uint32_t cpu)
{
    switch (cpu) {
    case 0x01000012:       return "ppc64";     /* CPU_TYPE_POWERPC64 */
    case 18:               return "ppc";       /* CPU_TYPE_POWERPC   */
    case 7:                return "x86 (i386)";/* CPU_TYPE_X86       */
    case 0x01000007:       return "x86_64";    /* CPU_TYPE_X86_64    */
    case 0x0100000C:       return "arm64";     /* CPU_TYPE_ARM64     */
    case 12:               return "arm";       /* CPU_TYPE_ARM       */
    default:               return "unknown";
    }
}

uint32_t bswap32(uint32_t v)
{
    return ((v & 0x000000FF) << 24) |
           ((v & 0x0000FF00) << 8)  |
           ((v & 0x00FF0000) >> 8)  |
           ((v & 0xFF000000) >> 24);
}
