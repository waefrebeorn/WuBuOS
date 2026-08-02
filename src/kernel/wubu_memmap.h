/*
 * wubu_memmap.h  --  WuBuOS Memory Map (E820, gap I1)
 *
 * boot.S collects the BIOS E820 table into 0x98000 during the 16-bit
 * stub: u16 count @ 0x98000, then up to 8 entries of
 * {u64 base, u64 length, u32 type} (20 bytes each).
 *
 * wubu_memmap_init() parses it and exposes the usable RAM range, so the
 * vmm allocator owns the REAL memory size instead of a hardcoded 1 GB.
 */
#ifndef WUBU_MEMMAP_H
#define WUBU_MEMMAP_H

#include <stdint.h>

#define WUBU_MEMMAP_ADDR 0x98000ull

/* Largest usable (type 1) region. */
typedef struct {
    uint64_t base;
    uint64_t len;
    int      found;
} wubu_memmap_info_t;

/* Parse the table; fills info with the LARGEST type-1 region.
 * Returns the entry count parsed (0 = no usable map). */
int wubu_memmap_init(wubu_memmap_info_t *info);

#endif /* WUBU_MEMMAP_H */
