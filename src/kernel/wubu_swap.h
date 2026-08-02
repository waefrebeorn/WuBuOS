/*
 * wubu_swap.h  --  demand-page swap (gap B3)
 *
 * The demand region's pages were never evicted: under memory pressure
 * the allocator just failed. This module adds a swap area on the AHCI
 * sim disk: a page can be swapped OUT (written to the swap area, the
 * PTE invalidated, the frame released) and swapped IN on the next
 * fault. The swap slot map is a fixed table (phys-page index -> swap
 * slot); a page is evictable only when it is NOT pinned (the demand
 * pages are the natural victims).
 *
 * Freestanding C11, no heap (fixed tables).
 */
#ifndef WUBU_SWAP_H
#define WUBU_SWAP_H

#include <stdint.h>

#define WUBU_SWAP_SLOTS   256          /* tracked evicted pages */
#define WUBU_SWAP_SECTOR  4096         /* swap area start (LBA, past the
                                          FAT32 volume on the 8MB disk) */
#define WUBU_SWAP_PAGES   (WUBU_SWAP_SECTOR + 4096)  /* 4MB of swap */

/* Swap a page out: write phys to a free swap slot, record the mapping,
 * invalidate the VA's PTE. Returns the swap slot or -1. */
int wubu_swap_out(uint64_t va, uint64_t phys);

/* Swap a page back in: read slot into a fresh frame, map it at va,
 * clear the slot. Returns 0 on success. */
int wubu_swap_in(uint64_t va, uint32_t slot);

/* The number of pages currently swapped out. */
uint32_t wubu_swap_count(void);

/* The slot swapped out at `va`, or -1 (the fault path's lookup). */
int wubu_swap_va_slot(uint64_t va);

/* The slot holding phys's frame, or -1. */
int wubu_swap_slot_of(uint64_t phys);

#endif
