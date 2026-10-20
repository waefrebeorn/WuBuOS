/*
 * wubu_dma.c -- coherent DMA buffer allocator (DRIVERS #360).
 * See wubu_dma.h for the contract and the coherence doctrine.
 */
#include "wubu_dma.h"

#include <string.h>

/* The pool: static, 4 KiB-aligned, inside the kernel's identity-mapped
 * low window (kernel .bss) so vaddr == paddr holds by construction. */
static uint8_t g_dma_pool[WUBU_DMA_POOL_SZ]
    __attribute__((aligned(WUBU_DMA_BLOCK_SZ)));

/* Per-block state: which allocation each block belongs to. A run of
 * N blocks for one allocation is marked with the same alloc id. */
static int g_dma_block_owner[WUBU_DMA_POOL_BLOCKS];
static int g_dma_next_alloc_id;
static int g_dma_blocks_used;

void wubu_dma_init(void)
{
    memset(g_dma_block_owner, 0, sizeof(g_dma_block_owner));
    g_dma_next_alloc_id = 1;   /* 0 = free */
    g_dma_blocks_used = 0;
}

static int addr_in_pool(const void *p)
{
    return (const uint8_t *)p >= g_dma_pool &&
           (const uint8_t *)p <  g_dma_pool + WUBU_DMA_POOL_SZ;
}

static size_t block_index(const void *p)
{
    return (size_t)((const uint8_t *)p - g_dma_pool) / WUBU_DMA_BLOCK_SZ;
}

/* Largest contiguous free run, in blocks. */
static size_t max_free_run_blocks(void)
{
    size_t best = 0, run = 0;
    for (size_t i = 0; i < WUBU_DMA_POOL_BLOCKS; i++) {
        if (g_dma_block_owner[i] == 0) {
            run++;
            if (run > best) best = run;
        } else {
            run = 0;
        }
    }
    return best;
}

void *wubu_dma_alloc(size_t size, size_t align)
{
    if (size == 0 || align == 0 || align > WUBU_DMA_BLOCK_SZ) return NULL;
    if (size > WUBU_DMA_POOL_SZ) return NULL;
    size_t want = (size + WUBU_DMA_BLOCK_SZ - 1) / WUBU_DMA_BLOCK_SZ;
    /* first-fit contiguous run */
    size_t run = 0, start = 0;
    for (size_t i = 0; i < WUBU_DMA_POOL_BLOCKS; i++) {
        if (g_dma_block_owner[i] == 0) {
            if (run == 0) start = i;
            run++;
            if (run == want) break;
        } else {
            run = 0;
        }
    }
    if (run < want) return NULL;                 /* no contiguous fit */
    int id = g_dma_next_alloc_id++;
    for (size_t i = start; i < start + want; i++) {
        g_dma_block_owner[i] = id;
        g_dma_blocks_used++;
    }
    return g_dma_pool + start * WUBU_DMA_BLOCK_SZ;  /* block-aligned */
}

int wubu_dma_free(void *vaddr)
{
    if (!vaddr || !addr_in_pool(vaddr)) return -1;
    size_t bi = block_index(vaddr);
    if (bi >= WUBU_DMA_POOL_BLOCKS) return -1;
    int id = g_dma_block_owner[bi];
    if (id == 0) return -1;                      /* not a live allocation */
    /* free the WHOLE run owned by this id (interior/edge addresses of
     * the run are accepted: the run is the allocation) */
    size_t i = bi;
    while (i < WUBU_DMA_POOL_BLOCKS && g_dma_block_owner[i] == id) i++;
    size_t end = i;
    i = bi;
    while (i > 0 && g_dma_block_owner[i - 1] == id) i--;
    for (size_t k = i; k < end; k++) {
        g_dma_block_owner[k] = 0;
        g_dma_blocks_used--;
    }
    return 0;
}

uint64_t wubu_dma_phys(const void *vaddr)
{
    if (!vaddr || !addr_in_pool(vaddr)) return 0;
    /* The pool is identity-mapped kernel .bss: physical == virtual. */
    return (uint64_t)(uintptr_t)vaddr;
}

int wubu_dma_sync_for_dev(void *vaddr)
{
    if (!vaddr || !addr_in_pool(vaddr)) return -1;
    /* x86/q35 is hardware-coherent; the barrier keeps the CPU from
     * reordering touches across the ownership transfer. */
    __asm__ volatile("" ::: "memory");
    return 0;
}

int wubu_dma_sync_for_cpu(void *vaddr)
{
    if (!vaddr || !addr_in_pool(vaddr)) return -1;
    __asm__ volatile("" ::: "memory");
    return 0;
}

size_t wubu_dma_in_use(void)
{
    return (size_t)g_dma_blocks_used * WUBU_DMA_BLOCK_SZ;
}

size_t wubu_dma_max_free_run(void)
{
    return max_free_run_blocks() * WUBU_DMA_BLOCK_SZ;
}
