/*
 * wubu_vmm.c  --  WuBuOS Virtual Memory (freestanding, metal)
 *
 * Bitmap page allocator + CR3 page-table walk + demand-zero regions.
 * The #PF handler fills demand regions (allocate + map + retry) -- the
 * first real virtual-memory behavior on metal.
 *
 * Freestanding: no malloc, no hosted APIs.
 */

#include "wubu_vmm.h"
#include "wubu_sync.h"
#include "wubu_memmap.h"

#define PAGE 4096ull

/* The allocator is shared between the #PF handler (ISR context, demand
 * fill) and the kernel main path (future heap/segments): protect the
 * bitmap with wubu_sync's spinlock (gap D6 -- the first real metal user
 * of the sync module). cli-based: atomic on this single CPU. */
static wubu_spinlock_t g_vmm_lock;

/* ---- the allocator bitmap ------------------------------------------ */

static uint8_t g_bitmap[WUBU_VMM_BITMAP_BITS / 8];
static uint64_t g_free_pages;
/* Gap B8: per-page reference counts (uint16 per page; 0 = free). */
static uint16_t g_refs[WUBU_VMM_BITMAP_BITS];

static void bm_set(uint32_t idx) {
    g_bitmap[idx >> 3] |= (uint8_t)(1u << (idx & 7));
    g_free_pages--;
}
static void bm_clear(uint32_t idx) {
    g_bitmap[idx >> 3] &= (uint8_t)~(1u << (idx & 7));
    g_free_pages++;
}
static uint32_t bm_test(uint32_t idx) {
    return (g_bitmap[idx >> 3] >> (idx & 7)) & 1u;
}

void wubu_vmm_init(void)
{
    wubu_spin_init(&g_vmm_lock);
    for (uint32_t i = 0; i < WUBU_VMM_BITMAP_BITS / 8; i++)
        g_bitmap[i] = 0;
    for (uint32_t i = 0; i < WUBU_VMM_BITMAP_BITS; i++)
        g_refs[i] = 0;
    g_free_pages = WUBU_VMM_BITMAP_BITS;

    /* mark USED: kernel image + heap + vbe buffers + early stack */
    /* kernel: 0x100000 .. 0x1400000 (image + bss + page tables + pool) */
    /* heap (mem_alloc): 0x400000 .. 0x4400000 (64 MB) */
    /* vbe: 0x402018 .. 0x1402018 overlaps heap? no -- the heap starts at
     * 0x400000; the vbe alloc comes FROM the heap. So heap covers both. */
    struct { uint64_t base, end; } used[] = {
        { 0x100000ull, 0x200000ull },   /* kernel image + crt0 tables */
        { 0x200000ull, 0x500000ull },   /* page tables + early structures */
        { 0x400000ull, 0x4400000ull },  /* the mem_alloc heap (64 MB) */
        { 0x90000ull,  0x99000ull  },   /* attestation stash + memmap */
        { 0x70000ull,  0x78000ull  },   /* early stack */
    };
    for (uint32_t i = 0; i < sizeof(used) / sizeof(used[0]); i++) {
        uint64_t b = used[i].base, e = used[i].end;
        if (b < WUBU_VMM_PHYS_BASE) b = WUBU_VMM_PHYS_BASE;
        if (e > WUBU_VMM_PHYS_END)  e = WUBU_VMM_PHYS_END;
        for (uint64_t p = b; p < e; p += PAGE)
            bm_set((uint32_t)((p - WUBU_VMM_PHYS_BASE) / PAGE));
    }

    /* E820 (gap I1): the allocator owns ONLY real RAM. If the BIOS map
     * says the largest usable region ends below 1 GB (e.g. 512 MB in
     * QEMU), everything above that is marked used. */
    wubu_memmap_info_t mm;
    if (wubu_memmap_init(&mm) > 0 && mm.found) {
        uint64_t real_end = mm.base + mm.len;
        if (real_end < WUBU_VMM_PHYS_END) {
            for (uint64_t p = real_end; p < WUBU_VMM_PHYS_END; p += PAGE)
                bm_set((uint32_t)((p - WUBU_VMM_PHYS_BASE) / PAGE));
        }
    }
}

uint64_t wubu_vmm_alloc_pages(uint32_t n)
{
    if (n == 0 || n > 64) return 0;
    uint64_t out = 0;
    wubu_spin_lock(&g_vmm_lock);
    /* first-fit scan for n consecutive free pages */
    uint32_t run = 0, start = 0;
    for (uint32_t i = 0; i < WUBU_VMM_BITMAP_BITS; i++) {
        if (bm_test(i) == 0) {
            if (run == 0) start = i;
            if (++run == n) {
                for (uint32_t j = start; j < start + n; j++) {
                    bm_set(j);
                    g_refs[j] = 1;      /* gap B8: fresh alloc, ref=1 */
                }
                out = WUBU_VMM_PHYS_BASE + (uint64_t)start * PAGE;
                break;
            }
        } else {
            run = 0;
        }
    }
    wubu_spin_unlock(&g_vmm_lock);
    return out;
}

/* Gap B8: take a reference on already-allocated pages (the COW/shared
 * foundation). The pages must currently be allocated (ref >= 1). */
void wubu_vmm_ref(uint64_t phys, uint32_t n)
{
    if (phys < WUBU_VMM_PHYS_BASE) return;
    uint64_t off = phys - WUBU_VMM_PHYS_BASE;
    if (off % PAGE != 0) return;
    wubu_spin_lock(&g_vmm_lock);
    for (uint32_t i = 0; i < n; i++) {
        uint32_t idx = (uint32_t)(off / PAGE) + i;
        if (idx < WUBU_VMM_BITMAP_BITS && bm_test(idx) && g_refs[idx] < 0xFFFF)
            g_refs[idx]++;
    }
    wubu_spin_unlock(&g_vmm_lock);
}

/* Gap B8: the reference count of the page holding `phys`. */
uint16_t wubu_vmm_refcount(uint64_t phys)
{
    if (phys < WUBU_VMM_PHYS_BASE) return 0;
    uint64_t off = phys - WUBU_VMM_PHYS_BASE;
    if (off % PAGE != 0) return 0;
    uint32_t idx = (uint32_t)(off / PAGE);
    if (idx >= WUBU_VMM_BITMAP_BITS) return 0;
    return g_refs[idx];
}

void wubu_vmm_free_pages(uint64_t phys, uint32_t n)
{
    if (phys < WUBU_VMM_PHYS_BASE) return;
    uint64_t off = phys - WUBU_VMM_PHYS_BASE;
    if (off % PAGE != 0) return;
    wubu_spin_lock(&g_vmm_lock);
    for (uint32_t i = 0; i < n; i++) {
        uint32_t idx = (uint32_t)(off / PAGE) + i;
        if (idx >= WUBU_VMM_BITMAP_BITS || !bm_test(idx)) continue;
        /* Gap B8: a reference-counted release -- only the last unref
         * actually returns the page to the allocator. */
        if (g_refs[idx] > 0) g_refs[idx]--;
        if (g_refs[idx] == 0)
            bm_clear(idx);   /* the last unref returns the page */
    }
    wubu_spin_unlock(&g_vmm_lock);
}

/* ---- page-table mapping (walks the CR3 tables) ---------------------- */

/* the crt0 top-level tables (identity slot at PML4[0]) */
#define PML4_BASE 0x300000ull

static uint64_t vmm_phys_of(uint64_t va)
{
    /* kernel higher-half -> phys; identity stays */
    if (va >= 0xffffffff80000000ull)
        return va - 0xffffffff80000000ull;
    return va;
}

int wubu_vmm_map_page(uint64_t virt, uint64_t phys, uint32_t flags)
{
    uint64_t *pml4 = (uint64_t *)PML4_BASE;
    uint32_t i4 = (uint32_t)(virt >> 39) & 0x1FF;
    uint32_t i3 = (uint32_t)(virt >> 30) & 0x1FF;
    uint32_t i2 = (uint32_t)(virt >> 21) & 0x1FF;
    uint32_t i1 = (uint32_t)(virt >> 12) & 0x1FF;

    uint64_t *pdp = (uint64_t *)vmm_phys_of(pml4[i4] & ~0xFFFull);
    if (!(pml4[i4] & 1)) {
        uint64_t tab = wubu_vmm_alloc_pages(1);
        if (!tab) return -1;
        for (int i = 0; i < 512; i++) ((uint64_t *)tab)[i] = 0;
        pml4[i4] = tab | 3;
        pdp = (uint64_t *)tab;
    }
    uint64_t *pd = (uint64_t *)vmm_phys_of(pdp[i3] & ~0xFFFull);
    if (!(pdp[i3] & 1)) {
        uint64_t tab = wubu_vmm_alloc_pages(1);
        if (!tab) return -1;
        for (int i = 0; i < 512; i++) ((uint64_t *)tab)[i] = 0;
        pdp[i3] = tab | 3;
        pd = (uint64_t *)tab;
    }
    uint64_t *pt = (uint64_t *)vmm_phys_of(pd[i2] & ~0xFFFull);
    if (!(pd[i2] & 1)) {
        uint64_t tab = wubu_vmm_alloc_pages(1);
        if (!tab) return -1;
        for (int i = 0; i < 512; i++) ((uint64_t *)tab)[i] = 0;
        pd[i2] = tab | 3;
        pt = (uint64_t *)tab;
    }
    pt[i1] = phys | (flags & 0xFFFull);
    __asm__ volatile("invlpg (%0)" : : "r"(virt) : "memory");
    return 0;
}

/* ---- demand-zero regions ------------------------------------------- */

#define WUBU_VMM_MAX_DEMAND 4

static struct {
    uint64_t base;
    uint32_t pages;
} g_demand[WUBU_VMM_MAX_DEMAND];
static uint32_t g_demand_n;
static uint32_t g_demand_faults;

uint64_t wubu_vmm_register_demand(uint64_t base, uint32_t pages)
{
    if (g_demand_n >= WUBU_VMM_MAX_DEMAND) return 0;
    g_demand[g_demand_n].base = base;
    g_demand[g_demand_n].pages = pages;
    g_demand_n++;
    return base;
}

int wubu_vmm_is_demand(uint64_t va)
{
    for (uint32_t i = 0; i < g_demand_n; i++)
        if (va >= g_demand[i].base &&
            va < g_demand[i].base + (uint64_t)g_demand[i].pages * PAGE)
            return 1;
    return 0;
}

int wubu_vmm_demand_fill(uint64_t va)
{
    if (!wubu_vmm_is_demand(va)) return -1;
    uint64_t page_base = va & ~(PAGE - 1);
    uint64_t phys = wubu_vmm_alloc_pages(1);
    if (!phys) return -1;
    /* zero it (identity-mapped in the allocator's range) */
    uint64_t *p = (uint64_t *)phys;
    for (int i = 0; i < 512; i++) p[i] = 0;
    if (wubu_vmm_map_page(page_base, phys, 3) != 0) {
        wubu_vmm_free_pages(phys, 1);
        return -1;
    }
    g_demand_faults++;
    return 0;
}

uint64_t wubu_vmm_free_count(void) { return g_free_pages; }
uint32_t wubu_vmm_demand_count(void) { return g_demand_n; }
uint32_t wubu_vmm_demand_faults(void) { return g_demand_faults; }
