/*
 * wubu_vmm.h  --  WuBuOS Virtual Memory (freestanding, metal)
 *
 * The memory foundation (DA P0): a physical page allocator + generic
 * page-table mapping + DEMAND-ZERO regions where the #PF handler does
 * REAL work (allocate + map + retry) instead of halting.
 *
 *   wubu_vmm_init()        -- bitmap over 16MB..1GB, marks used regions
 *   wubu_vmm_alloc_pages   -- physical 4K pages from the bitmap
 *   wubu_vmm_map_page      -- 4K map at an arbitrary VA (walks CR3 tables)
 *   wubu_vmm_register_demand -- a VA range the #PF handler fills on touch
 *
 * handle_page_fault() consults the demand regions before halting: a fault
 * inside a demand range allocates a zero page, maps it, and RETURNS (the
 * iretq retries the faulting instruction -- real demand paging).
 */
#ifndef WUBU_VMM_H
#define WUBU_VMM_H

#include <stdint.h>

/* Physical range the allocator owns: 16MB .. 1GB (above kernel/heap/vbe). */
#define WUBU_VMM_PHYS_BASE   0x01000000ull
#define WUBU_VMM_PHYS_END    0x40000000ull
#define WUBU_VMM_BITMAP_BITS ((WUBU_VMM_PHYS_END - WUBU_VMM_PHYS_BASE) / 4096)

void wubu_vmm_init(void);

/* Physical page allocator. Returns 0 on OOM. n <= 64. */
uint64_t wubu_vmm_alloc_pages(uint32_t n);
void     wubu_vmm_free_pages(uint64_t phys, uint32_t n);

/* Gap B8: reference counts -- take a reference on allocated pages
 * (COW/shared foundation) + read a page's current count. */
void     wubu_vmm_ref(uint64_t phys, uint32_t n);
uint16_t wubu_vmm_refcount(uint64_t phys);

/* Map one 4K page at `virt` -> `phys` (walks/creates the CR3 tables;
 * intermediate tables come from the page allocator). flags: 3 = RW+present.
 * Returns 0 on success. */
int wubu_vmm_map_page(uint64_t virt, uint64_t phys, uint32_t flags);

/* Demand-zero regions: faults inside these VAs allocate a fresh zero page,
 * map it, and retry. Returns the registered base. */
uint64_t wubu_vmm_register_demand(uint64_t base, uint32_t pages);

/* Is `va` inside a registered demand region? */
int wubu_vmm_is_demand(uint64_t va);

/* Fill a demand page (called by the #PF handler): alloc + map.
 * Returns 0 on success (the faulting instruction retries). */
int wubu_vmm_demand_fill(uint64_t va);

/* Diagnostics. */
uint64_t wubu_vmm_free_count(void);
uint32_t wubu_vmm_demand_count(void);
uint32_t wubu_vmm_demand_faults(void);

#endif /* WUBU_VMM_H */
