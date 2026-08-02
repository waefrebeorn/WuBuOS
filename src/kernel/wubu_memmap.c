/*
 * wubu_memmap.c  --  WuBuOS Memory Map (E820, gap I1)
 *
 * Parses the E820 table collected by boot.S. Type 1 = usable RAM; the
 * largest such region bounds the vmm's allocator.
 */

#include "wubu_memmap.h"

#define E820_TYPE_USABLE 1

int wubu_memmap_init(wubu_memmap_info_t *info)
{
    if (!info) return 0;
    info->base = 0;
    info->len = 0;
    info->found = 0;

    uint16_t count = *(volatile uint16_t *)WUBU_MEMMAP_ADDR;
    if (count == 0 || count > 8) return 0;    /* no map / garbage */

    uint8_t *p = (uint8_t *)(WUBU_MEMMAP_ADDR + 2);
    int n = 0;
    for (int i = 0; i < count; i++) {
        uint64_t base = *(volatile uint64_t *)(p + 0);
        uint64_t len  = *(volatile uint64_t *)(p + 8);
        uint32_t type = *(volatile uint32_t *)(p + 16);
        if (type == E820_TYPE_USABLE && len > info->len) {
            info->base = base;
            info->len = len;
            info->found = 1;
        }
        n++;
        p += 20;
    }
    return n;
}
