/*
 * wubu_dma.h -- coherent DMA buffer allocator (DRIVERS #360).
 *
 * A fixed physical pool carved into 4 KiB blocks, handed out as
 * physically-CONTIGUOUS runs (a DMA transfer must not straddle scattered
 * pages). The pool is static .bss inside the kernel's identity-mapped
 * low window, so vaddr == paddr by construction -- wubu_dma_phys() is
 * the (only) place that fact lives.
 *
 * Cache coherence: on x86/QEMU-q35 (this kernel's verified target) DMA
 * memory is hardware-coherent; the sync calls carry the DOCUMENTED
 * ownership-transfer semantics (for_dev = CPU stops touching it,
 * for_cpu = device done) and act as compiler barriers, so a
 * non-coherent platform has one place to add real cache maintenance.
 *
 * C11, freestanding (static pool, no malloc), fail-closed.
 */
#ifndef WUBU_DMA_H
#define WUBU_DMA_H

#include <stddef.h>
#include <stdint.h>

#define WUBU_DMA_BLOCK_SZ      4096
#define WUBU_DMA_POOL_BLOCKS   12   /* 48 KiB pool (fits the 1 MiB kernel
                                     * load budget with real margin --
                                     * kernel.ld ASSERTs that budget) */
#define WUBU_DMA_POOL_SZ       (WUBU_DMA_BLOCK_SZ * WUBU_DMA_POOL_BLOCKS)

/* Reset the pool to fully free (boot + tests). */
void   wubu_dma_init(void);

/* Allocate a physically-contiguous run of >= size bytes, block-aligned.
 * align must be <= WUBU_DMA_BLOCK_SZ (block alignment is inherent);
 * larger alignments are rejected (-1) rather than silently unaligned.
 * Returns the virtual address (= physical, identity-mapped pool) or
 * NULL when no contiguous run fits / bad args. */
void  *wubu_dma_alloc(size_t size, size_t align);

/* Free a live allocation (by its address). 0 ok; -1 not ours / double
 * free / bad args. */
int    wubu_dma_free(void *vaddr);

/* The physical address of a live allocation (identity == vaddr here,
 * but callers must go through this so the mapping fact has one owner).
 * 0 on bad args. */
uint64_t wubu_dma_phys(const void *vaddr);

/* Ownership transfer barriers. 0 ok, -1 bad args. */
int    wubu_dma_sync_for_dev(void *vaddr);
int    wubu_dma_sync_for_cpu(void *vaddr);

/* Diagnostics: bytes in use and the largest contiguous free run. */
size_t wubu_dma_in_use(void);
size_t wubu_dma_max_free_run(void);

#endif /* WUBU_DMA_H */
