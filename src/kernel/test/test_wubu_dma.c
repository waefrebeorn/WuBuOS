/*
 * test_wubu_dma.c -- asserts the coherent DMA allocator contract
 * (DRIVERS #360): contiguous runs, fragmentation honesty, ownership
 * barriers, fail-closed paths.
 */
#include "wubu_dma.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(cond) do { \
    if (!(cond)) { fails++; printf("FAIL: %s (line %d)\n", #cond, __LINE__); } \
} while (0)

int main(void)
{
    wubu_dma_init();
    CHECK(wubu_dma_in_use() == 0);
    CHECK(wubu_dma_max_free_run() == WUBU_DMA_POOL_SZ);

    /* ---- contiguous run + block alignment + identity phys ---- */
    void *a = wubu_dma_alloc(3 * 4096, 4096);   /* 3 blocks */
    CHECK(a != NULL);
    CHECK(((uintptr_t)a % 4096) == 0);
    CHECK(wubu_dma_phys(a) == (uint64_t)(uintptr_t)a);   /* identity */
    CHECK(wubu_dma_in_use() == 3 * 4096);
    CHECK(wubu_dma_max_free_run() == WUBU_DMA_POOL_SZ - 3 * 4096);

    /* ---- bad args fail closed ---- */
    CHECK(wubu_dma_alloc(0, 4096) == NULL);
    CHECK(wubu_dma_alloc(4096, 0) == NULL);
    CHECK(wubu_dma_alloc(4096, 8192) == NULL);     /* > block alignment */
    CHECK(wubu_dma_alloc(WUBU_DMA_POOL_SZ + 1, 4096) == NULL);
    CHECK(wubu_dma_free(NULL) == -1);
    CHECK(wubu_dma_free((void *)(uintptr_t)0x1) == -1);  /* not the pool */
    CHECK(wubu_dma_phys(NULL) == 0);

    /* ---- fragmentation honesty ---- */
    void *b = wubu_dma_alloc(2 * 4096, 4096);      /* blocks 3..4 */
    void *c = wubu_dma_alloc(2 * 4096, 4096);      /* blocks 5..6 */
    CHECK(b && c);
    CHECK(wubu_dma_in_use() == 7 * 4096);
    CHECK(wubu_dma_max_free_run() == (WUBU_DMA_POOL_BLOCKS - 7) * 4096);
    CHECK(wubu_dma_free(b) == 0);                 /* hole at 3..4 */
    CHECK(wubu_dma_in_use() == 5 * 4096);
    /* a 3-block run does not fit in the 2-block hole or past c */
    CHECK(wubu_dma_alloc(3 * 4096, 4096) != NULL); /* fits in the tail */
    /* allocate until exhausted, then honest failure */
    wubu_dma_init();
    void *big = wubu_dma_alloc(WUBU_DMA_POOL_SZ, 4096);  /* whole pool */
    CHECK(big != NULL);
    CHECK(wubu_dma_in_use() == WUBU_DMA_POOL_SZ);
    CHECK(wubu_dma_max_free_run() == 0);
    CHECK(wubu_dma_alloc(4096, 4096) == NULL);    /* full pool */
    CHECK(wubu_dma_free(big) == 0);
    CHECK(wubu_dma_in_use() == 0);
    CHECK(wubu_dma_max_free_run() == WUBU_DMA_POOL_SZ);

    /* ---- double free rejected; ownership barriers ---- */
    void *d = wubu_dma_alloc(4096, 4096);
    CHECK(d != NULL);
    CHECK(wubu_dma_free(d) == 0);
    CHECK(wubu_dma_free(d) == -1);                /* double free */
    CHECK(wubu_dma_sync_for_dev(d) == 0);
    CHECK(wubu_dma_sync_for_cpu(d) == 0);
    CHECK(wubu_dma_sync_for_dev(NULL) == -1);
    CHECK(wubu_dma_sync_for_cpu(NULL) == -1);

    /* ---- partial-size allocations round up to blocks ---- */
    wubu_dma_init();
    void *e = wubu_dma_alloc(100, 64);             /* tiny -> 1 block */
    CHECK(e != NULL);
    CHECK(wubu_dma_in_use() == 4096);
    /* writable through the whole promised region */
    memset(e, 0xAB, 100);
    CHECK(((uint8_t *)e)[0] == 0xAB && ((uint8_t *)e)[99] == 0xAB);

    if (fails == 0) printf("ALL PASSED\n");
    else printf("%d FAILURES\n", fails);
    return fails ? 1 : 0;
}
