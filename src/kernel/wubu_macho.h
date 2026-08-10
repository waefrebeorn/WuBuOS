/*
 * wubu_macho.h -- the IN-KERNEL Mach-O loader.
 */
#ifndef WUBU_MACHO_H
#define WUBU_MACHO_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t magic;
    uint32_t cpu_type;
    uint32_t cpu_sub;
    uint32_t filetype;
    uint32_t ncommands;
    uint32_t sizeofcmds;
    uint64_t entry;   /* the LC_UNIXTHREAD eip/r15 (lazy) */
} wubu_macho_info_t;

typedef struct {
    uint32_t cpu_type;
    uint32_t offset;  /* into the FAT file */
    uint32_t size;
} wubu_fat_slice_t;

/* MO1: parse a thin (non-FAT) Mach-O_64 header. */
int wubu_macho_parse(const void *data, size_t size, wubu_macho_info_t *out);

/* MO2: parse a FAT universal-binary header -> the slices. */
int wubu_fat_slices(const void *data, size_t size,
                    wubu_fat_slice_t *out, int max);

/* MO3: the cpu-type name. */
const char *wubu_macho_cpu_name(uint32_t cpu);

uint32_t bswap32(uint32_t v);

#endif
