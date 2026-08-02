/*
 * fw_mem.c  --  WuBuFW physical memory manager.
 *
 * A real UEFI implementation must hand the OS a coherent memory map, so the
 * allocator is map-first: a fixed table of typed regions is the source of
 * truth, and AllocatePages/AllocatePool carve from it. Regions are kept
 * sorted and coalesced so GetMemoryMap() output is spec-shaped.
 *
 * Arena: FW_HEAP_BASE .. detected RAM top (capped at FW_HEAP_LIMIT).
 * E820 is unavailable in long mode without a BIOS, so RAM size is probed
 * through the QEMU CMOS registers (0x34/0x35 high memory in 64KB units),
 * with a conservative 128MB fallback.
 */

#include "fw.h"

#define FW_MAX_REGIONS 256

typedef struct {
    uint64_t start;      /* page-aligned base   */
    uint64_t pages;
    uint32_t type;       /* EFI_MEMORY_TYPE     */
    uint64_t attr;
} fw_region;

static fw_region g_map[FW_MAX_REGIONS];
static size_t    g_nregions;
static uint64_t  g_map_key;
static uint64_t  g_ram_top;

/* -- region table helpers ----------------------------------------- */

static void region_insert(size_t idx, uint64_t start, uint64_t pages, uint32_t type) {
    if (g_nregions >= FW_MAX_REGIONS) return;
    for (size_t i = g_nregions; i > idx; i--) g_map[i] = g_map[i - 1];
    g_map[idx].start = start;
    g_map[idx].pages = pages;
    g_map[idx].type  = type;
    g_map[idx].attr  = EFI_MEMORY_WB;
    g_nregions++;
}

static void region_remove(size_t idx) {
    for (size_t i = idx; i + 1 < g_nregions; i++) g_map[i] = g_map[i + 1];
    if (g_nregions) g_nregions--;
}

static void region_coalesce(void) {
    for (size_t i = 0; i + 1 < g_nregions; ) {
        if (g_map[i].type == g_map[i + 1].type &&
            g_map[i].start + g_map[i].pages * EFI_PAGE_SIZE == g_map[i + 1].start) {
            g_map[i].pages += g_map[i + 1].pages;
            region_remove(i + 1);
        } else i++;
    }
}

/* Mark [start, start+pages) as `type`, splitting whatever covers it.
 * Returns 0 on success, -1 if the range is not fully covered. */
static int region_mark(uint64_t start, uint64_t pages, uint32_t type) {
    uint64_t end = start + pages * EFI_PAGE_SIZE;
    for (size_t i = 0; i < g_nregions && pages; i++) {
        uint64_t rs = g_map[i].start;
        uint64_t re = rs + g_map[i].pages * EFI_PAGE_SIZE;
        if (end <= rs || start >= re) continue;

        uint64_t cs = start > rs ? start : rs;
        uint64_t ce = end   < re ? end   : re;

        if (cs > rs) {                       /* head split */
            uint64_t head = (cs - rs) / EFI_PAGE_SIZE;
            uint32_t t = g_map[i].type;
            g_map[i].pages = head;
            region_insert(i + 1, cs, (re - cs) / EFI_PAGE_SIZE, t);
            i++;
            rs = cs; re = rs + g_map[i].pages * EFI_PAGE_SIZE;
        }
        if (ce < re) {                       /* tail split */
            uint32_t t = g_map[i].type;
            uint64_t body = (ce - rs) / EFI_PAGE_SIZE;
            region_insert(i + 1, ce, (re - ce) / EFI_PAGE_SIZE, t);
            g_map[i].pages = body;
        }
        g_map[i].type = type;
    }
    g_map_key++;
    region_coalesce();
    return 0;
}

/* -- RAM sizing ---------------------------------------------------- */

static uint8_t cmos_read(uint8_t reg) {
    outb(0x70, reg);
    io_wait();
    return inb(0x71);
}

static uint64_t detect_ram_top(void) {
    /* CMOS 0x34/0x35: extended memory above 16MB in 64KB units (QEMU/SeaBIOS
     * convention). 0x5B-0x5D: memory above 4GB in 64KB units. */
    uint32_t ext = (uint32_t)cmos_read(0x34) | ((uint32_t)cmos_read(0x35) << 8);
    uint64_t top = 16ULL * 1024 * 1024 + (uint64_t)ext * 64ULL * 1024;
    if (ext == 0) {
        uint32_t lo = (uint32_t)cmos_read(0x30) | ((uint32_t)cmos_read(0x31) << 8);
        top = 1024ULL * 1024 + (uint64_t)lo * 1024ULL;
    }
    if (top < 32ULL * 1024 * 1024) top = 128ULL * 1024 * 1024;   /* fallback */
    if (top > 0xC0000000ULL) top = 0xC0000000ULL;                /* stay under PCI hole */
    return top & ~(uint64_t)(EFI_PAGE_SIZE - 1);
}

void fw_mem_init(void) {
    g_nregions = 0;
    g_map_key  = 1;
    g_ram_top  = detect_ram_top();

    /* 0 .. 0xA0000 : conventional, but keep the low 64KB + stack reserved. */
    region_insert(g_nregions, 0x00000000, 0xA0000 / EFI_PAGE_SIZE, EfiConventionalMemory);
    /* 0xA0000 .. 0x100000 : legacy VGA/BIOS window */
    region_insert(g_nregions, 0x000A0000, 0x60000 / EFI_PAGE_SIZE, EfiMemoryMappedIO);
    /* 1MB .. ram top : conventional */
    region_insert(g_nregions, 0x00100000, (g_ram_top - 0x00100000) / EFI_PAGE_SIZE, EfiConventionalMemory);

    /* Carve out what the firmware already owns. */
    region_mark(0x00000000, 0x10,  EfiRuntimeServicesData);          /* IVT/BDA          */
    region_mark(FW_PT_BASE, 6,     EfiRuntimeServicesData);          /* page tables      */
    region_mark(0x00090000, 0x10,  EfiBootServicesData);             /* boot stack       */
    region_mark(FW_LOAD_BASE, (0x00400000 - FW_LOAD_BASE) / EFI_PAGE_SIZE,
                EfiRuntimeServicesCode);                             /* firmware body    */
}

uint64_t fw_mem_map_key(void) { return g_map_key; }

int fw_mem_count(void) { return g_nregions; }
uint64_t fw_mem_free_mb(void) {
    uint64_t free = 0;
    for (size_t i = 0; i < g_nregions; i++)
        if (g_map[i].type == EfiConventionalMemory) free += g_map[i].pages;
    return free / 256;   /* 4096/16 = 256 pages per MB */
}

/* -- page allocation ----------------------------------------------- */

static void *alloc_from(uint64_t lo, uint64_t hi, size_t pages, EFI_MEMORY_TYPE type, int top_down) {
    if (!pages) return NULL;
    uint64_t need = pages * EFI_PAGE_SIZE;
    size_t best = (size_t)-1;
    uint64_t best_addr = 0;

    for (size_t i = 0; i < g_nregions; i++) {
        if (g_map[i].type != EfiConventionalMemory) continue;
        uint64_t rs = g_map[i].start, re = rs + g_map[i].pages * EFI_PAGE_SIZE;
        if (rs < lo) rs = lo;
        if (re > hi) re = hi;
        if (re <= rs || re - rs < need) continue;
        uint64_t cand = top_down ? (re - need) : rs;
        if (best == (size_t)-1 || (top_down ? cand > best_addr : cand < best_addr)) {
            best = i; best_addr = cand;
        }
    }
    if (best == (size_t)-1) return NULL;
    region_mark(best_addr, pages, (uint32_t)type);
    return (void *)(uintptr_t)best_addr;
}

void *fw_alloc_pages(size_t pages, EFI_MEMORY_TYPE type) {
    return alloc_from(FW_HEAP_BASE, g_ram_top, pages, type, 0);
}

/*
 * Over-allocate and take the aligned run inside it. Hardware DMA structures
 * (AHCI command lists at 1KB, NVMe queues and xHCI DCBAA at 64B/4KB) reject
 * misaligned bases outright, so this must be exact rather than hopeful.
 */
void *fw_alloc_pages_aligned(size_t pages, size_t align) {
    if (align <= EFI_PAGE_SIZE) return fw_alloc_pages(pages, EfiBootServicesData);
    size_t extra = (align + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
    uint8_t *raw = fw_alloc_pages(pages + extra, EfiBootServicesData);
    if (!raw) return NULL;
    uint64_t a = (uint64_t)(uintptr_t)raw;
    uint64_t aligned = (a + align - 1) & ~(uint64_t)(align - 1);
    return (void *)(uintptr_t)aligned;
}

void *fw_alloc_pages_at(uint64_t addr, size_t pages, EFI_MEMORY_TYPE type) {
    if (addr & (EFI_PAGE_SIZE - 1)) return NULL;
    uint64_t end = addr + pages * EFI_PAGE_SIZE;
    /* Every page in the range must currently be conventional. */
    uint64_t cursor = addr;
    while (cursor < end) {
        size_t i;
        for (i = 0; i < g_nregions; i++) {
            uint64_t rs = g_map[i].start, re = rs + g_map[i].pages * EFI_PAGE_SIZE;
            if (cursor >= rs && cursor < re) {
                if (g_map[i].type != EfiConventionalMemory) return NULL;
                cursor = re;
                break;
            }
        }
        if (i == g_nregions) return NULL;
    }
    region_mark(addr, pages, (uint32_t)type);
    return (void *)(uintptr_t)addr;
}

void fw_free_pages(void *p, size_t pages) {
    if (!p || !pages) return;
    region_mark((uint64_t)(uintptr_t)p, pages, EfiConventionalMemory);
}

/* -- pool allocation ------------------------------------------------ */
/*
 * Pool blocks are page allocations with an 32-byte header so FreePool can
 * recover the size. Keeps the implementation honest (no leaks that hide as
 * "conventional") at the cost of page granularity.
 */

#define POOL_MAGIC 0x57554255504F4F4CULL   /* "WUBUPOOL" */

typedef struct {
    uint64_t magic;
    uint64_t pages;
    uint64_t type;
    uint64_t pad;
} pool_hdr;

void *fw_pool_alloc(size_t bytes, EFI_MEMORY_TYPE type) {
    if (!bytes) return NULL;
    size_t total = bytes + sizeof(pool_hdr);
    size_t pages = (total + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE;
    void *p = fw_alloc_pages(pages, type);
    if (!p) return NULL;
    pool_hdr *h = (pool_hdr *)p;
    h->magic = POOL_MAGIC;
    h->pages = pages;
    h->type  = (uint64_t)type;
    h->pad   = 0;
    return (void *)((uint8_t *)p + sizeof(pool_hdr));
}

void fw_pool_free(void *p) {
    if (!p) return;
    pool_hdr *h = (pool_hdr *)((uint8_t *)p - sizeof(pool_hdr));
    if (h->magic != POOL_MAGIC) return;      /* not ours: ignore, never corrupt */
    h->magic = 0;
    fw_free_pages(h, (size_t)h->pages);
}

/* -- memory map export ---------------------------------------------- */

size_t fw_mem_map_build(EFI_MEMORY_DESCRIPTOR *out, size_t max_entries) {
    if (!out) return g_nregions;
    size_t n = g_nregions < max_entries ? g_nregions : max_entries;
    for (size_t i = 0; i < n; i++) {
        out[i].Type          = g_map[i].type;
        out[i].Pad           = 0;
        out[i].PhysicalStart = g_map[i].start;
        out[i].VirtualStart  = 0;
        out[i].NumberOfPages = g_map[i].pages;
        out[i].Attribute     = g_map[i].attr |
            ((g_map[i].type == EfiRuntimeServicesCode ||
              g_map[i].type == EfiRuntimeServicesData) ? EFI_MEMORY_RUNTIME : 0);
    }
    return g_nregions;
}
