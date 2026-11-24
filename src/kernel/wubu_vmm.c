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
#ifdef MYSEED_METAL
#define VMM_INVLPG(va) __asm__ volatile("invlpg (%0)" : : "r"(va) : "memory")
#else
#define VMM_INVLPG(va) ((void)(va))
#endif

/* The allocator is shared between the #PF handler (ISR context, demand
 * fill) and the kernel main path (future heap/segments): protect the
 * bitmap with wubu_sync's spinlock (gap D6 -- the first real metal user
 * of the sync module). cli-based: atomic on this single CPU. */
static wubu_spinlock_t g_vmm_lock;
/* 2 MB slab arena slot bitmap (one bit per 2 MB slot of the VA window). */
static uint8_t g_harena_used[(WUBU_VMM_HUGE_SLOTS + 7u) / 8u];
static wubu_spinlock_t g_harena_lock;

/* ---- the allocator bitmap ------------------------------------------ */

static uint8_t g_bitmap[WUBU_VMM_BITMAP_BITS / 8];
static uint64_t g_free_pages;
/* Upper bound of usable RAM. Firmware memmap narrows it; with no memmap
 * (WEMU guest gate) 256 MB is the conservative truth, not 1 GB. */
static uint64_t g_alloc_end = WUBU_VMM_PHYS_END;
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
/* Where the next first-fit scan starts (allocation hint, not an invariant). */
static uint32_t g_alloc_hint;
static uint32_t bm_test(uint32_t idx) {
    return (g_bitmap[idx >> 3] >> (idx & 7)) & 1u;
}
/* Mark [from,to) used in whole bitmap bytes. The boot layout marks several
 * overlapping ranges (heap inside the early-structures window); going through
 * bm_set per page both cost ~2M emulated steps and double-counted g_free_pages
 * on the overlap, so an already-set page is now skipped. */
static void bm_set_range(uint32_t from, uint32_t to) {
    if (from >= to) return;
    uint32_t i = from;
    while (i < to && (i & 7u)) { if (!bm_test(i)) bm_set(i); i++; }
    /* 64 pages per word. The byte-at-a-time SWAR measured 603k guest steps
     * (17% of the boot) on a 256 MB memmap; the word form does the same work
     * with 8x fewer loop iterations. __builtin_memcpy keeps the 8-byte load
     * unaligned-safe without a libgcc helper (kernel links -nostdlib). */
    while (i + 64 <= to) {
        uint64_t cur;
        __builtin_memcpy(&cur, g_bitmap + (i >> 3), sizeof cur);
        cur = ~cur;
        cur = cur - ((cur >> 1) & 0x5555555555555555ull);
        cur = (cur & 0x3333333333333333ull) + ((cur >> 2) & 0x3333333333333333ull);
        cur = (cur + (cur >> 4)) & 0x0F0F0F0F0F0F0F0Full;
        g_free_pages -= (uint32_t)((cur * 0x0101010101010101ull) >> 56);
        cur = ~0ull;
        __builtin_memcpy(g_bitmap + (i >> 3), &cur, sizeof cur);
        i += 64;
    }
    while (i + 8 <= to) {
        uint8_t cur = (uint8_t)~g_bitmap[i >> 3];
        /* popcount without libgcc: the kernel links -nostdlib */
        cur = (uint8_t)(cur - ((cur >> 1) & 0x55u));
        cur = (uint8_t)((cur & 0x33u) + ((cur >> 2) & 0x33u));
        g_free_pages -= (uint32_t)((cur + (cur >> 4)) & 0x0Fu);
        g_bitmap[i >> 3] = 0xFFu;
        i += 8;
    }
    while (i < to) { if (!bm_test(i)) bm_set(i); i++; }
}


void wubu_vmm_init(void)
{
    wubu_spin_init(&g_vmm_lock);
    wubu_spin_init(&g_harena_lock);
    for (uint32_t i = 0; i < (WUBU_VMM_HUGE_SLOTS + 7u) / 8u; i++)
        g_harena_used[i] = 0;
    for (uint32_t i = 0; i < WUBU_VMM_BITMAP_BITS / 8; i++)
        g_bitmap[i] = 0;
    for (uint32_t i = 0; i < WUBU_VMM_BITMAP_BITS; i++)
        g_refs[i] = 0;
    g_free_pages = WUBU_VMM_BITMAP_BITS;
    g_alloc_hint = 0;

    /* mark USED: kernel image + heap + vbe buffers + early stack */
    /* heap (mem_alloc): 0x400000 .. 0x4400000 (64 MB) */
    /* vbe: 0x402018 .. 0x1402018 overlaps heap? no -- the heap starts at
     * 0x400000; the vbe alloc comes FROM the heap. So heap covers both. */
    /* Gap B11: the kernel image's bounds come from the LINKER symbols
     * (not a hardcoded range): whatever the image actually grew to is
     * protected from the allocator. The rest of the boot layout (page
     * tables, heap, attestation stash, early stack) is fixed by the
     * linker script + crt0, so those stay explicit. */
    extern char _kernel_start[], _stack_top[];
    uint64_t k_start = (uint64_t)(uintptr_t)_kernel_start;
    uint64_t k_end   = (uint64_t)(uintptr_t)_stack_top;
    if (k_start >= 0xffffffff80000000ull) k_start -= 0xffffffff80000000ull;
    if (k_end   >= 0xffffffff80000000ull) k_end   -= 0xffffffff80000000ull;
    struct { uint64_t base, end; } used[] = {
        { k_start,    k_end      },  /* kernel image + bss + stack (symbols) */
        { 0x200000ull, 0x500000ull },   /* page tables + early structures */
        { 0x400000ull, 0x4400000ull },  /* the mem_alloc heap (64 MB) */
        { 0x90000ull,  0x99000ull  },   /* attestation stash + memmap */
        { 0x70000ull,  0x78000ull  },   /* early stack */
    };
    for (uint32_t i = 0; i < sizeof(used) / sizeof(used[0]); i++) {
        uint64_t b = used[i].base, e = used[i].end;
        if (b < WUBU_VMM_PHYS_BASE) b = WUBU_VMM_PHYS_BASE;
        if (e > WUBU_VMM_PHYS_END)  e = WUBU_VMM_PHYS_END;
        if (e > b)
            bm_set_range((uint32_t)((b - WUBU_VMM_PHYS_BASE) / PAGE),
                         (uint32_t)((e - WUBU_VMM_PHYS_BASE) / PAGE));
    }

    /* E820 (gap I1): the allocator owns ONLY real RAM. If the BIOS map
     * says the largest usable region ends below 1 GB (e.g. 512 MB in
     * QEMU), everything above that is marked used. */
    wubu_memmap_info_t mm;
    uint64_t real_end = WUBU_VMM_PHYS_END;
    if (wubu_memmap_init(&mm) > 0 && mm.found) {
        uint64_t mapped_end = mm.base + mm.len;
        if (mapped_end > WUBU_VMM_PHYS_BASE && mapped_end < real_end)
            real_end = mapped_end;
    } else {
        real_end = 0x10000000ull;   /* WEMU / no-firmware default: 256 MB */
    }
    if (real_end < WUBU_VMM_PHYS_BASE) real_end = WUBU_VMM_PHYS_BASE;
    g_alloc_end = real_end;
    if (real_end < WUBU_VMM_PHYS_END)
        bm_set_range((uint32_t)((real_end - WUBU_VMM_PHYS_BASE) / PAGE),
                     (uint32_t)WUBU_VMM_BITMAP_BITS);
}

/* First-fit for n consecutive free pages inside [from,to). */
static uint64_t bm_scan_alloc(uint32_t from, uint32_t to, uint32_t n)
{
    uint32_t run = 0, start = 0;
    for (uint32_t i = from; i < to; i++) {
        /* A fully-used 64-page word can be skipped with one load: boot
         * allocations walk over megabytes the kernel already owns page by
         * page otherwise. Only taken when the word is entirely used, so a
         * free page inside it is never missed. */
        if ((i & 63u) == 0 && i + 64 <= to) {
            uint64_t w;
            __builtin_memcpy(&w, g_bitmap + (i >> 3), sizeof w);
            if (w == ~0ull) { i += 63; run = 0; continue; }
        }
        if (bm_test(i) == 0) {
            if (run == 0) start = i;
            if (++run == n) {
                for (uint32_t j = start; j < start + n; j++) {
                    bm_set(j);
                    g_refs[j] = 1;      /* gap B8: fresh alloc, ref=1 */
                }
                return WUBU_VMM_PHYS_BASE + (uint64_t)start * PAGE;
            }
        } else {
            run = 0;
        }
    }
    return 0;
}

uint64_t wubu_vmm_alloc_pages(uint32_t n)
{
    if (n == 0 || n > 64) return 0;
    uint64_t out = 0;
    wubu_spin_lock(&g_vmm_lock);
    /* First fit, starting where the last allocation ended. Every alloc used to
     * rescan from page 0 over the kernel image + heap that boot had already
     * marked used; that was 293k guest steps (8.5%) before the first console
     * line. The hint is only a starting point: a second pass covers [0,hint)
     * so a hole below it is still found. */
    uint32_t limit = (uint32_t)((g_alloc_end - WUBU_VMM_PHYS_BASE) / PAGE);
    uint32_t hint = g_alloc_hint < limit ? g_alloc_hint : 0;
    out = bm_scan_alloc(hint, limit, n);
    if (!out && hint) out = bm_scan_alloc(0, hint, n);
    if (out) g_alloc_hint = (uint32_t)((out - WUBU_VMM_PHYS_BASE) / PAGE) + n;
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

/* The LIVE top-level table. On metal that is whatever CR3 holds right now:
 * a loader/harness may install its own PML4 (the WEMU kernel gate builds
 * tables at 0x5000000 and enters at .long_mode_start), so walking a
 * hardcoded crt0 symbol maps pages into a DEAD table and every access
 * faults. Hosted gates install their scratch at the documented PML4_BASE.
 * CR3 is read into RAX explicitly to keep the encoding REX.B-free. */
static uint64_t vmm_pml4(void)
{
#ifdef MYSEED_METAL
    uint64_t cr3;
    __asm__ volatile("mov %%cr3, %0" : "=a"(cr3));
    return cr3 & 0x000ffffffffff000ull;
#else
    return PML4_BASE;
#endif
}

static uint64_t vmm_phys_of(uint64_t va)
{
    /* kernel higher-half -> phys; identity stays */
    if (va >= 0xffffffff80000000ull)
        return va - 0xffffffff80000000ull;
    return va;
}

static int vmm_map_page_ex(uint64_t virt, uint64_t phys, uint32_t flags,
                            uint32_t inter_flags)
{
    uint64_t *pml4 = (uint64_t *)vmm_pml4();
    uint32_t i4 = (uint32_t)(virt >> 39) & 0x1FF;
    uint32_t i3 = (uint32_t)(virt >> 30) & 0x1FF;
    uint32_t i2 = (uint32_t)(virt >> 21) & 0x1FF;
    uint32_t i1 = (uint32_t)(virt >> 12) & 0x1FF;

    /* Callers that map a KERNEL object (the vdso, a driver struct) pass its
     * higher-half VA, not a physical address. Convert once, here: a raw VA
     * in a PTE sets NX + reserved bits and the mapping is dead on arrival. */
    phys = vmm_phys_of(phys);

    uint64_t *pdp = (uint64_t *)vmm_phys_of(pml4[i4] & ~0xFFFull);
    if (!(pml4[i4] & 1)) {
        uint64_t tab = wubu_vmm_alloc_pages(1);
        if (!tab) return -1;
        for (int i = 0; i < 512; i++) ((uint64_t *)tab)[i] = 0;
        pml4[i4] = tab | inter_flags;
        pdp = (uint64_t *)tab;
    }
    uint64_t *pd = (uint64_t *)vmm_phys_of(pdp[i3] & ~0xFFFull);
    if (!(pdp[i3] & 1)) {
        uint64_t tab = wubu_vmm_alloc_pages(1);
        if (!tab) return -1;
        for (int i = 0; i < 512; i++) ((uint64_t *)tab)[i] = 0;
        pdp[i3] = tab | inter_flags;
        pd = (uint64_t *)tab;
    }
    uint64_t *pt;
    uint64_t pd_ent = pd[i2];
    if (pd_ent & 0x80u) {
        /* A 2 MB leaf is already here (crt0's identity PD, or a loader's
         * tables). Split it into a 512-entry PT covering the SAME 2 MB with
         * the same permission bits, so a 4 KiB mapping can live inside it;
         * leaving the PS leaf in place would make us write a PTE into raw
         * physical memory. */
        uint64_t tab = wubu_vmm_alloc_pages(1);
        if (!tab) return -1;
        uint64_t base = pd_ent & 0x000fffffffe00000ull;
        uint64_t fl   = pd_ent & 0xFFFull;
        for (int i = 0; i < 512; i++)
            ((uint64_t *)tab)[i] = (base + (uint64_t)i * 4096u) | fl;
        pd[i2] = tab | inter_flags;
        pt = (uint64_t *)tab;
        VMM_INVLPG(virt & ~0x1FFFFFull);
    } else if (!(pd_ent & 1)) {
        uint64_t tab = wubu_vmm_alloc_pages(1);
        if (!tab) return -1;
        for (int i = 0; i < 512; i++) ((uint64_t *)tab)[i] = 0;
        pd[i2] = tab | inter_flags;
        pt = (uint64_t *)tab;
    } else {
        pt = (uint64_t *)vmm_phys_of(pd_ent & ~0xFFFull);
    }
    pt[i1] = phys | (flags & 0xFFFull);
    VMM_INVLPG(virt);
    return 0;
}

int wubu_vmm_map_page(uint64_t virt, uint64_t phys, uint32_t flags)
{
    return vmm_map_page_ex(virt, phys, flags, 3);
}

/* Ring-3 reachable: US on the PTE AND on every table above it. A user PTE under
 * kernel-only tables is still unreachable from ring 3. */
int wubu_vmm_map_user_page(uint64_t virt, uint64_t phys, uint32_t flags)
{
    return vmm_map_page_ex(virt, phys, (flags & ~0x4u) | 0x4u, 3 | 0x4u);
}

/* ---- 2 MB hugepage KV slab ----------------------------------------- */
#define HUGE_PAGES 512u

uint32_t wubu_vmm_huge_runs(void)
{
    uint32_t runs = 0;
    wubu_spin_lock(&g_vmm_lock);
    for (uint32_t i = 0; i + HUGE_PAGES <= WUBU_VMM_BITMAP_BITS; i += HUGE_PAGES) {
        int ok = 1;
        for (uint32_t j = 0; j < HUGE_PAGES; j++) {
            if (bm_test(i + j)) { ok = 0; break; }
        }
        if (ok) runs++;
    }
    wubu_spin_unlock(&g_vmm_lock);
    return runs;
}

uint64_t wubu_vmm_alloc_huge_n(uint32_t slabs)
{
    if (slabs == 0 || slabs > WUBU_VMM_HUGE_SLOTS) return 0;
    uint32_t want = slabs * HUGE_PAGES;
    uint64_t out = 0;
    wubu_spin_lock(&g_vmm_lock);
    uint32_t limit = (uint32_t)((g_alloc_end - WUBU_VMM_PHYS_BASE) / PAGE);
    /* first-fit scan for `want` consecutive free pages, 2 MB aligned */
    for (uint32_t first = 0; first + want <= limit; first += HUGE_PAGES) {
        uint32_t j = 0;
        for (; j < want; j++) {
            if (bm_test(first + j)) break;
        }
        if (j != want) continue;
        for (j = 0; j < want; j++) {
            bm_set(first + j);
            g_refs[first + j] = 1;      /* fresh alloc, ref=1 (gap B8) */
        }
        out = WUBU_VMM_PHYS_BASE + (uint64_t)first * PAGE;
        break;
    }
    wubu_spin_unlock(&g_vmm_lock);
    return out;
}

uint64_t wubu_vmm_alloc_huge(void)
{
    return wubu_vmm_alloc_huge_n(1);
}

static uint64_t *vmm_pd_for(uint64_t virt)
{
    uint64_t *pml4 = (uint64_t *)vmm_pml4();
    uint32_t i4 = (uint32_t)(virt >> 39) & 0x1FFu;
    uint32_t i3 = (uint32_t)(virt >> 30) & 0x1FFu;
    if (!(pml4[i4] & 1u)) return NULL;
    uint64_t *pdp = (uint64_t *)vmm_phys_of(pml4[i4] & ~0xFFFull);
    if (!(pdp[i3] & 1u)) return NULL;
    return (uint64_t *)vmm_phys_of(pdp[i3] & ~0xFFFull);
}

uint64_t wubu_vmm_huge_pde(uint64_t virt)
{
    uint64_t *pd = vmm_pd_for(virt);
    if (!pd) return 0;
    return pd[(virt >> 21) & 0x1FFu];
}

int wubu_vmm_map_huge(uint64_t virt, uint64_t phys, uint32_t flags)
{
    if (!phys || !virt || (phys & 0x1FFFFFull) || (virt & 0x1FFFFFull))
        return -1;
    uint64_t *pml4 = (uint64_t *)vmm_pml4();
    uint32_t i4 = (uint32_t)(virt >> 39) & 0x1FFu;
    uint32_t i3 = (uint32_t)(virt >> 30) & 0x1FFu;
    uint64_t *pdp;
    if (!(pml4[i4] & 1u)) {
        uint64_t tab = wubu_vmm_alloc_pages(1);
        if (!tab) return -1;
        for (int i = 0; i < 512; i++) ((uint64_t *)tab)[i] = 0;
        pml4[i4] = tab | 3;
    }
    pdp = (uint64_t *)vmm_phys_of(pml4[i4] & ~0xFFFull);
    if (!(pdp[i3] & 1u)) {
        uint64_t tab = wubu_vmm_alloc_pages(1);
        if (!tab) return -1;
        for (int i = 0; i < 512; i++) ((uint64_t *)tab)[i] = 0;
        pdp[i3] = tab | 3;
    }
    uint64_t *pd = (uint64_t *)vmm_phys_of(pdp[i3] & ~0xFFFull);
    uint64_t *entry = &pd[(virt >> 21) & 0x1FFu];
    if (*entry & 1u) return -1;      /* never clobber an existing mapping */
    *entry = phys | (flags & 0xFFFull) | 0x80u; /* PS=1 */
    VMM_INVLPG(virt);
    return 0;
}

int wubu_vmm_unmap_huge(uint64_t virt)
{
    uint64_t *pd = vmm_pd_for(virt);
    if (!pd) return -1;
    uint64_t *entry = &pd[(virt >> 21) & 0x1FFu];
    if (!(*entry & 0x80u)) return -1;
    *entry = 0;
    VMM_INVLPG(virt);
    return 0;
}

void wubu_vmm_free_huge_n(uint64_t phys, uint32_t slabs)
{
    if (!phys || (phys & 0x1FFFFFull)) return;
    if (slabs == 0 || slabs > WUBU_VMM_HUGE_SLOTS) return;
    wubu_vmm_free_pages(phys, slabs * HUGE_PAGES);
}

void wubu_vmm_free_huge(uint64_t phys)
{
    wubu_vmm_free_huge_n(phys, 1);
}

/* ---- 2 MB slab arena ----------------------------------------------- */

static int ha_test(uint32_t s) {
    return (g_harena_used[s >> 3] >> (s & 7u)) & 1u;
}
static void ha_set(uint32_t s) {
    g_harena_used[s >> 3] |= (uint8_t)(1u << (s & 7u));
}
static void ha_clear(uint32_t s) {
    g_harena_used[s >> 3] &= (uint8_t)~(1u << (s & 7u));
}

uint32_t wubu_vmm_huge_arena_free_slots(void)
{
    uint32_t n = 0;
    wubu_spin_lock(&g_harena_lock);
    for (uint32_t i = 0; i < WUBU_VMM_HUGE_SLOTS; i++)
        if (!ha_test(i)) n++;
    wubu_spin_unlock(&g_harena_lock);
    return n;
}

int wubu_vmm_huge_arena_owned(uint64_t va)
{
    if (va < WUBU_VMM_HUGE_VA_BASE) return 0;
    uint64_t off = va - WUBU_VMM_HUGE_VA_BASE;
    if (off % WUBU_VMM_HUGE_SLAB_BYTES) return 0;
    uint32_t slot = (uint32_t)(off / WUBU_VMM_HUGE_SLAB_BYTES);
    if (slot >= WUBU_VMM_HUGE_SLOTS) return 0;
    wubu_spin_lock(&g_harena_lock);
    int used = ha_test(slot);
    wubu_spin_unlock(&g_harena_lock);
    if (!used) return 0;
    /* a reserved slot must also carry a live PS=1 PDE */
    return (wubu_vmm_huge_pde(va) & 0x81u) == 0x81u;
}

uint64_t wubu_vmm_huge_arena_alloc(uint32_t slabs)
{
    if (slabs == 0 || slabs > WUBU_VMM_HUGE_SLOTS) return 0;

    /* 1. reserve a run of slots (bookkeeping only -- no lock held across
     *    the page allocator / mapping calls, which take g_vmm_lock) */
    uint32_t first = WUBU_VMM_HUGE_SLOTS;
    wubu_spin_lock(&g_harena_lock);
    for (uint32_t i = 0; i + slabs <= WUBU_VMM_HUGE_SLOTS && first == WUBU_VMM_HUGE_SLOTS; i++) {
        uint32_t j = 0;
        for (; j < slabs; j++)
            if (ha_test(i + j)) break;
        if (j == slabs) {
            first = i;
            for (j = 0; j < slabs; j++) ha_set(i + j);
        }
    }
    wubu_spin_unlock(&g_harena_lock);
    if (first == WUBU_VMM_HUGE_SLOTS) return 0;

    uint64_t va0 = WUBU_VMM_HUGE_VA_BASE +
                   (uint64_t)first * WUBU_VMM_HUGE_SLAB_BYTES;

    /* 2. back it with ONE physically contiguous run */
    uint64_t phys = wubu_vmm_alloc_huge_n(slabs);
    if (!phys) {
        wubu_spin_lock(&g_harena_lock);
        for (uint32_t j = 0; j < slabs; j++) ha_clear(first + j);
        wubu_spin_unlock(&g_harena_lock);
        return 0;
    }

    /* 3. one PS=1 PDE per slab; roll everything back on any failure */
    for (uint32_t i = 0; i < slabs; i++) {
        uint64_t va = va0 + (uint64_t)i * WUBU_VMM_HUGE_SLAB_BYTES;
        uint64_t pa = phys + (uint64_t)i * WUBU_VMM_HUGE_SLAB_BYTES;
        if (wubu_vmm_map_huge(va, pa, 3u) != 0) {
            while (i > 0) {
                i--;
                wubu_vmm_unmap_huge(va0 + (uint64_t)i * WUBU_VMM_HUGE_SLAB_BYTES);
            }
            wubu_vmm_free_huge_n(phys, slabs);
            wubu_spin_lock(&g_harena_lock);
            for (uint32_t j = 0; j < slabs; j++) ha_clear(first + j);
            wubu_spin_unlock(&g_harena_lock);
            return 0;
        }
    }
    return va0;
}

void wubu_vmm_huge_arena_free(uint64_t va, uint32_t slabs)
{
    if (!slabs || slabs > WUBU_VMM_HUGE_SLOTS) return;
    if (va < WUBU_VMM_HUGE_VA_BASE) return;
    uint64_t off = va - WUBU_VMM_HUGE_VA_BASE;
    if (off % WUBU_VMM_HUGE_SLAB_BYTES) return;
    uint32_t first = (uint32_t)(off / WUBU_VMM_HUGE_SLAB_BYTES);
    if (first + slabs > WUBU_VMM_HUGE_SLOTS) return;
    uint64_t pde = wubu_vmm_huge_pde(va);
    if ((pde & 0x81u) != 0x81u) return;          /* not a live huge mapping */
    uint64_t phys = pde & 0x000ffffffffff000ull;
    for (uint32_t i = 0; i < slabs; i++)
        wubu_vmm_unmap_huge(va + (uint64_t)i * WUBU_VMM_HUGE_SLAB_BYTES);
    wubu_vmm_free_huge_n(phys, slabs);
    wubu_spin_lock(&g_harena_lock);
    for (uint32_t i = 0; i < slabs; i++) ha_clear(first + i);
    wubu_spin_unlock(&g_harena_lock);
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

/* Gap B3: unmap a VA -- clear its PTE + invalidate the TLB entry. */
int wubu_vmm_unmap(uint64_t virt)
{
    uint64_t *pml4 = (uint64_t *)vmm_pml4();
    uint32_t i4 = (uint32_t)(virt >> 39) & 0x1FF;
    uint32_t i3 = (uint32_t)(virt >> 30) & 0x1FF;
    uint32_t i2 = (uint32_t)(virt >> 21) & 0x1FF;
    uint32_t i1 = (uint32_t)(virt >> 12) & 0x1FF;
    uint64_t *pdp = (uint64_t *)vmm_phys_of(pml4[i4] & ~0xFFFull);
    if (!(pml4[i4] & 1) || !(pdp[i3] & 1)) return -1;
    uint64_t *pd = (uint64_t *)vmm_phys_of(pdp[i3] & ~0xFFFull);
    if (!(pd[i2] & 1)) return -1;
    uint64_t *pt = (uint64_t *)vmm_phys_of(pd[i2] & ~0xFFFull);
    pt[i1] = 0;
    VMM_INVLPG(virt);
    return 0;
}

/* ---- gap B4: copy-on-write ------------------------------------------ */

/* Map `phys` at `virt` read-only and take a reference on it -- the
 * COW contract: the first write faults, the fault handler copies, and
 * the copy becomes writable while the shared original stays RO. */
int wubu_vmm_map_shared(uint64_t virt, uint64_t phys)
{
    wubu_vmm_ref(phys, 1);
    /* map without the writable bit (flags: P=1, U/S=0, W=0) */
    return wubu_vmm_map_page(virt, phys, 1);
}

/* The COW fault: called when a write faults on a shared (RO) page.
 * If the page is genuinely shared (refcount > 1) a private copy is
 * made and remapped writable; the shared original loses one ref.
 * Returns 1 when handled (the instruction retries), 0 when the fault
 * is not a COW fault. */
int wubu_vmm_cow_fault(uint64_t va)
{
    uint64_t page_base = va & ~(PAGE - 1);
    /* walk the live tables for the current PTE (identity rules: the
     * demand region lives in the higher-half window) */
    uint64_t *pml4 = (uint64_t *)vmm_pml4();
    uint32_t i4 = (uint32_t)(page_base >> 39) & 0x1FF;
    uint32_t i3 = (uint32_t)(page_base >> 30) & 0x1FF;
    uint32_t i2 = (uint32_t)(page_base >> 21) & 0x1FF;
    uint32_t i1 = (uint32_t)(page_base >> 12) & 0x1FF;
    uint64_t *pdp = (uint64_t *)vmm_phys_of(pml4[i4] & ~0xFFFull);
    if (!(pml4[i4] & 1) || !(pdp[i3] & 1)) return 0;
    uint64_t *pd = (uint64_t *)vmm_phys_of(pdp[i3] & ~0xFFFull);
    if (!(pd[i2] & 1)) return 0;
    uint64_t *pt = (uint64_t *)vmm_phys_of(pd[i2] & ~0xFFFull);
    uint64_t pte = pt[i1];
    if (!(pte & 1)) return 0;
    if (pte & 2) return 0;             /* already writable: not COW */

    uint64_t phys = pte & ~0xFFFull;
    if (wubu_vmm_refcount(phys) <= 1) {
        /* sole owner: just make it writable, no copy needed */
        pt[i1] = pte | 2;
        __asm__ __volatile__("invlpg (%0)" : : "r"(page_base) : "memory");
        return 1;
    }

    /* shared: copy into a fresh page + remap writable */
    uint64_t copy = wubu_vmm_alloc_pages(1);
    if (!copy) return 0;
    uint64_t *src = (uint64_t *)phys;
    uint64_t *dst = (uint64_t *)copy;
    for (int i = 0; i < 512; i++) dst[i] = src[i];
    wubu_vmm_free_pages(phys, 1);      /* drop the shared reference */
    pt[i1] = (copy & ~0xFFFull) | 3;   /* P|W|U */
    __asm__ __volatile__("invlpg (%0)" : : "r"(page_base) : "memory");
    return 1;
}
uint32_t wubu_vmm_demand_faults(void) { return g_demand_faults; }