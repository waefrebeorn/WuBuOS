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
/* Same, but reachable from ring 3 (US on the PTE and every table above it). */
int wubu_vmm_map_user_page(uint64_t virt, uint64_t phys, uint32_t flags);
/* Wave 3: 2 MB KV hugepage slab. The physical range is 512 contiguous
 * 4K pages, 2 MB aligned; the mapping is a single PS=1 PDE. */
uint64_t wubu_vmm_alloc_huge(void);
uint32_t wubu_vmm_huge_runs(void);
int wubu_vmm_map_huge(uint64_t virt, uint64_t phys, uint32_t flags);
uint64_t wubu_vmm_huge_pde(uint64_t virt);
int wubu_vmm_unmap_huge(uint64_t virt);
void wubu_vmm_free_huge(uint64_t phys);

/* ---- 2 MB slab arena: a VA window for long-lived huge slabs ---------
 * Each slot is one 2 MB slab = one PS=1 PDE. A caller asking for `slabs`
 * consecutive slots gets ONE VA span and ONE physically contiguous span,
 * which is what a GPU/DMA-visible tensor needs. The window lives in its
 * own PML4[511] PDP entry (i3 = 500), distinct from the kernel image
 * (i3 = 510), so kernel growth can never collide with an arena slab. */
#define WUBU_VMM_HUGE_SLAB_BYTES 0x200000ull
#define WUBU_VMM_HUGE_SLOTS      16u
#define WUBU_VMM_HUGE_VA_BASE    0xfffffffd00000000ull

/* Physically contiguous run of `slabs` huge pages (2 MB aligned). */
uint64_t wubu_vmm_alloc_huge_n(uint32_t slabs);
void     wubu_vmm_free_huge_n(uint64_t phys, uint32_t slabs);

/* Arena: reserve slots + back them with a contiguous phys run + map them.
 * Returns the VA span (0 on OOM / no free run). */
uint64_t wubu_vmm_huge_arena_alloc(uint32_t slabs);
void     wubu_vmm_huge_arena_free(uint64_t va, uint32_t slabs);
uint32_t wubu_vmm_huge_arena_free_slots(void);
/* Is `va` the base of a live arena span? (1 = yes) */
int      wubu_vmm_huge_arena_owned(uint64_t va);

/* Gap B3: unmap a VA (clear its PTE + invlpg). 0 on success. */
int wubu_vmm_unmap(uint64_t virt);
/* Gap B4: copy-on-write -- map shared (RO) + the fault handler. */
int wubu_vmm_map_shared(uint64_t virt, uint64_t phys);
int wubu_vmm_cow_fault(uint64_t va);

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
