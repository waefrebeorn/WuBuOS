/*
 * wubu_as.c  --  per-address-space isolation (gap B7)
 *
 * A new AS = a private PML4 page whose kernel entries are CLONED from
 * the boot tables (the kernel stays visible; the address spaces differ
 * in their user windows). Switching reloads CR3; the TLB follows.
 * The kernel singleton wraps the boot's own tables (PML4 at 0x300000).
 */
#include "wubu_as.h"

#define PML4_BASE 0x300000ull
#define PAGE 4096ull

typedef struct wubu_as {
    uint64_t *pml4;          /* the AS's top-level table (physical) */
    uint64_t *pml4_src;      /* the boot table it cloned (kernel AS: self) */
    uint32_t  refs;          /* bound tasks */
} wubu_as_t;

static wubu_as_t g_kernel_as = { (uint64_t *)PML4_BASE, (uint64_t *)PML4_BASE, 1 };
static wubu_as_t *g_current = &g_kernel_as;
static uint32_t g_as_count = 1;

wubu_as_t *wubu_as_kernel(void) { return &g_kernel_as; }

wubu_as_t *wubu_as_create(void)
{
    extern uint64_t wubu_vmm_alloc_pages(uint32_t);
    uint64_t tab = wubu_vmm_alloc_pages(1);
    if (!tab) return NULL;
    /* clone the kernel's PML4 entries (identity + higher-half) */
    uint64_t *src = (uint64_t *)PML4_BASE;
    uint64_t *dst = (uint64_t *)tab;
    for (int i = 0; i < 512; i++) dst[i] = src[i];

    static wubu_as_t store[16];     /* bounded AS pool (no heap) */
    static uint32_t used = 0;
    for (uint32_t i = 0; i < used; i++) {
        if (store[i].pml4 == NULL) {           /* recycled slot */
            store[i].pml4 = (uint64_t *)tab;
            store[i].pml4_src = src;
            store[i].refs = 0;
            g_as_count++;
            return &store[i];
        }
    }
    if (used < 16) {
        wubu_as_t *as = &store[used++];
        as->pml4 = (uint64_t *)tab;
        as->pml4_src = src;
        as->refs = 0;
        g_as_count++;
        return as;
    }
    /* pool full: free the page + fail */
    extern void wubu_vmm_free_pages(uint64_t, uint32_t);
    wubu_vmm_free_pages(tab, 1);
    return NULL;
}

int wubu_as_bind(wubu_as_t *as)
{
    if (!as) return -1;
    if (g_current && g_current != as && g_current->refs > 0)
        g_current->refs--;
    as->refs++;
    g_current = as;
    return 0;
}

void wubu_as_switch(wubu_as_t *as)
{
    if (!as) return;
    g_current = as;
    __asm__ __volatile__("mov %0, %%cr3" : : "r"(as->pml4) : "memory");
}

void wubu_as_destroy(wubu_as_t *as)
{
    if (!as || as == &g_kernel_as) return;
    if (as->refs > 0) return;              /* still bound */
    if (as->pml4) {
        extern void wubu_vmm_free_pages(uint64_t, uint32_t);
        wubu_vmm_free_pages((uint64_t)(uintptr_t)as->pml4, 1);
    }
    as->pml4 = NULL;
    g_as_count--;
}

wubu_as_t *wubu_as_current(void) { return g_current; }

uint32_t wubu_as_count(void) { return g_as_count; }
