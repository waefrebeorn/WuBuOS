/*
 * memory.c — My Seed Kernel Memory Subsystem Implementation
 *
 * Clean C11 reimplementation of ZealOS heap design.
 * Simplified from the original for correctness first;
 * will optimize to match ZealOS perf characteristics later.
 *
 * Original: ZealOS/src/Kernel/Memory/MAllocFree.ZC (514 lines asm)
 */

#include "memory.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ── Internal: Spinlock ─────────────────────────────────────────── */

static inline void heap_lock(CHeapCtrl *hc) {
    while (__atomic_test_and_set(&hc->locked_flags, __ATOMIC_ACQUIRE))
        ;
}

static inline void heap_unlock(CHeapCtrl *hc) {
    __atomic_clear(&hc->locked_flags, __ATOMIC_RELEASE);
}

/* ── Global Heap ────────────────────────────────────────────────── */

static CHeapCtrl *g_heap = NULL;
static void       *g_heap_base = NULL;
static size_t      g_heap_total = 0;
static uint8_t    *g_next_page = NULL;
static uint8_t    *g_heap_end  = NULL;

static void *alloc_pages(size_t n_pages) {
    size_t bytes = n_pages * MEM_PAG_SIZE;
    if (g_next_page + bytes > g_heap_end)
        return NULL;
    void *p = g_next_page;
    g_next_page += bytes;
    return p;
}

/* ── Init / Shutdown ────────────────────────────────────────────── */

int mem_init(size_t total_bytes) {
    g_heap_base = malloc(total_bytes);
    if (!g_heap_base) return -1;
    
    g_heap_total = total_bytes;
    g_next_page  = (uint8_t *)g_heap_base;
    g_heap_end   = g_next_page + total_bytes;
    /* Allocate CHeapCtrl at the start — may need multiple pages */
    size_t hc_pages = (sizeof(CHeapCtrl) + MEM_PAG_SIZE - 1) / MEM_PAG_SIZE;
    g_heap = (CHeapCtrl *)alloc_pages(hc_pages);
    if (!g_heap) return -1;
    
    memset(g_heap, 0, sizeof(CHeapCtrl));
    g_heap->hc_signature = HEAP_CTRL_SIGNATURE;
    g_heap->locked_flags = 0;
    g_heap->used_size = 0;
    g_heap->max_size = total_bytes - MEM_PAG_SIZE;
    
    return 0;
}

void mem_shutdown(void) {
    if (g_heap_base) {
        free(g_heap_base);
        g_heap_base = NULL;
        g_heap = NULL;
        g_next_page = NULL;
        g_heap_end = NULL;
    }
}

CHeapCtrl *mem_heap_ctrl(void) {
    return g_heap;
}

/* ── Allocation ─────────────────────────────────────────────────── */

void *mem_alloc(size_t size) {
    if (!g_heap) return NULL;
    if (size == 0) size = 1;
    
    /* Round up to 16-byte alignment */
    size_t total = size + sizeof(CMemUsed);
    total = (total + 15) & ~(size_t)15;
    
    CHeapCtrl *hc = g_heap;
    
    heap_lock(hc);
    
    CMemUsed *mu = NULL;
    
    if (total < MEM_HEAP_HASH_SIZE) {
        /* Small allocation: try hash table (ZealOS fast path) */
        CMemUnused *u = hc->heap_hash[total];
        if (u) {
            hc->heap_hash[total] = u->next;
            mu = (CMemUsed *)u;
            goto found;
        }
        
        /* Try free list */
        CMemUnused **prev_ptr = &hc->malloc_free_list.next;
        CMemUnused  *cur = *prev_ptr;
        while (cur) {
            CMemUsed *cand = (CMemUsed *)cur;
            if (cand->size >= total) {
                *prev_ptr = cur->next;
                mu = cand;
                goto found;
            }
            prev_ptr = &cur->next;
            cur = cur->next;
        }
    }
    
    /* Need fresh memory */
    {
        size_t n_pages = (total + MEM_PAG_SIZE - 1) / MEM_PAG_SIZE;
        void *block = alloc_pages(n_pages);
        if (!block) {
            heap_unlock(hc);
            return NULL;
        }
        mu = (CMemUsed *)block;
        mu->size = n_pages * MEM_PAG_SIZE;
    }
    
found:
    mu->signature = MEM_USED_SIGNATURE;
    
    /* If block is much larger, split and add remainder to free list */
    if (mu->size > total + sizeof(CMemUsed) + 16) {
        size_t rem_size = mu->size - total;
        CMemUsed *rem = (CMemUsed *)((uint8_t *)mu + total);
        rem->signature = MEM_UNUSED_SIGNATURE;
        rem->size = rem_size;
        
        CMemUnused *u = (CMemUnused *)rem;
        u->next = hc->malloc_free_list.next;
        hc->malloc_free_list.next = u;
        
        mu->size = total;
    }
    
    hc->used_size += mu->size;
    heap_unlock(hc);
    
    return mu->start;
}

void *mem_calloc(size_t count, size_t size) {
    size_t total = count * size;
    void *p = mem_alloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void mem_free(void *ptr) {
    if (!ptr || !g_heap) return;
    
    CHeapCtrl *hc = g_heap;
    
    /* Get header from user pointer */
    CMemUsed *mu = (CMemUsed *)((uint8_t *)ptr - offsetof(CMemUsed, start));
    
    if (mu->signature != MEM_USED_SIGNATURE) {
        fprintf(stderr, "mem_free: BAD SIGNATURE %p (got 0x%08X)\n",
                ptr, mu->signature);
        return;
    }
    
    heap_lock(hc);
    
    hc->used_size -= mu->size;
    mu->signature = MEM_UNUSED_SIGNATURE;
    
    CMemUnused *u = (CMemUnused *)mu;
    u->signature = MEM_UNUSED_SIGNATURE;
    
    if (mu->size < MEM_HEAP_HASH_SIZE) {
        /* Small: return to hash bucket */
        u->next = hc->heap_hash[mu->size];
        hc->heap_hash[mu->size] = u;
    } else {
        /* Large: return to free list */
        u->next = hc->malloc_free_list.next;
        hc->malloc_free_list.next = u;
    }
    
    heap_unlock(hc);
}

void *mem_realloc(void *ptr, size_t new_size) {
    if (!ptr) return mem_alloc(new_size);
    if (new_size == 0) { mem_free(ptr); return NULL; }
    
    CMemUsed *mu = (CMemUsed *)((uint8_t *)ptr - offsetof(CMemUsed, start));
    size_t old_usable = mu->size - offsetof(CMemUsed, start);
    size_t copy_size = old_usable < new_size ? old_usable : new_size;
    
    void *new_ptr = mem_alloc(new_size);
    if (!new_ptr) return NULL;
    
    memcpy(new_ptr, ptr, copy_size);
    mem_free(ptr);
    
    return new_ptr;
}

/* ── Diagnostics ────────────────────────────────────────────────── */

size_t mem_used(void) {
    return g_heap ? g_heap->used_size : 0;
}

size_t mem_available(void) {
    if (!g_heap) return 0;
    return g_heap->max_size - g_heap->used_size;
}

int mem_validate(void) {
    if (!g_heap) return -1;
    if (g_heap->hc_signature != HEAP_CTRL_SIGNATURE) return -1;
    return 0;
}

void mem_walk(MemWalkFn callback, void *ctx) {
    (void)callback; (void)ctx;
    /* TODO: full heap walk */
}
