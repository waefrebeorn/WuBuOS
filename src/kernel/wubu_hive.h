/*
 * wubu_hive.h -- C11 "luddite hive": linked fixed blocks + skipfield +
 *                freelist.  (plf::hive-style, pure C11, no templates.)
 *
 * The middle ground the hand-drawn diagram spells out:
 *
 *   Vector  -- contiguous, cache-hot, but erase/middle-insert moves memory
 *              and pointers are unstable.
 *   List    -- stable nodes, but every element is its own allocation; cache
 *              cold, poor locality, pointer-chase per step.
 *   Hive    -- elements live in small FIXED-CAPACITY blocks linked together
 *              (vector-like locality inside a block, list-like growth).
 *              Each block carries a bit skipfield marking erased slots;
 *              erased slots are chained on a per-block freelist.
 *
 *   - erase:  mark skip bit + freelist push          -> O(1), no moves
 *   - insert: reuse a free slot, else append a block -> O(1) amortized
 *   - iterate: jump skipped slots, walk blocks       -> cache > list
 *   - stable element pointers (the caller's pointer never moves)
 *
 * Freestanding C11: all storage comes from caller-provided alloc/free
 * callbacks (kernel: mem_alloc/mem_free; hosted: malloc/free). No globals,
 * no templates, no hidden state.
 */
#ifndef WUBU_HIVE_H
#define WUBU_HIVE_H

#include <stddef.h>
#include <stdint.h>

/* One fixed-capacity block.  The freelist lives INSIDE the slots array:
 * a free slot's slots[i] holds the index of the next free slot (as
 * uintptr_t), chained from free_head.  No extra storage needed. */
typedef struct wubu_hive_block {
    void   **slots;        /* cap element pointers (free slots chain indices) */
    uint8_t *skip;         /* bit skipfield: bit i set = slot i is dead/free */
    size_t   live;         /* live elements in this block */
    size_t   cap;          /* slot capacity (fixed for the whole hive) */
    size_t   free_head;    /* first free slot index, NONE if block is full */
    struct wubu_hive_block *next;
} wubu_hive_block_t;

typedef struct wubu_hive {
    wubu_hive_block_t *head;
    wubu_hive_block_t *tail;
    size_t   block_cap;    /* fixed capacity of every block */
    size_t   size;         /* live elements across all blocks */
    size_t   capacity;     /* total slot count across all blocks */
    void *(*alloc)(size_t);
    void  (*free_)(void *);
} wubu_hive_t;

/* Iterator: a block + slot index.  Valid while the hive lives; erase_at()
 * on the current slot is safe (next() then jumps the dead slot). */
typedef struct wubu_hive_iter {
    wubu_hive_block_t *block;
    size_t idx;
} wubu_hive_iter_t;

#define WUBU_HIVE_NONE ((size_t)-1)

/* Create a hive.  block_cap: slots per block (0 -> default 32).
 * alloc/free_: storage callbacks (both required).  Returns NULL on failure. */
wubu_hive_t *wubu_hive_new(size_t block_cap,
                           void *(*alloc)(size_t),
                           void (*free_)(void *));

/* Destroy the hive and all its blocks/slots (elements are NOT freed --
 * the caller owns the payloads). */
void wubu_hive_destroy(wubu_hive_t *h);

size_t wubu_hive_size(const wubu_hive_t *h);
size_t wubu_hive_capacity(const wubu_hive_t *h);
size_t wubu_hive_block_count(const wubu_hive_t *h);
int    wubu_hive_empty(const wubu_hive_t *h);

/* Insert an element pointer (stable: it is stored as-is, never moved).
 * Returns elem on success, NULL on alloc failure. */
void *wubu_hive_insert(wubu_hive_t *h, void *elem);

/* Erase by pointer: scans live slots (O(live) worst case), marks skip +
 * pushes the slot on the freelist.  Returns elem if found+erased, else NULL. */
void *wubu_hive_erase(wubu_hive_t *h, void *elem);

/* Erase the slot the iterator currently points at (O(1)).  Safe to call
 * inside the first()/next() loop: the following next() jumps the dead slot.
 * Returns 1 if a live slot was erased, 0 if already dead/invalid. */
int wubu_hive_erase_at(wubu_hive_t *h, wubu_hive_iter_t *it);

/* Mark every slot free, keeping the blocks (capacity is retained). */
void wubu_hive_clear(wubu_hive_t *h);

/* Iteration:  for (e = wubu_hive_first(h, &it); e; e = wubu_hive_next(h, &it)) */
void  *wubu_hive_first(const wubu_hive_t *h, wubu_hive_iter_t *it);
void  *wubu_hive_next(const wubu_hive_t *h, wubu_hive_iter_t *it);

#endif /* WUBU_HIVE_H */
