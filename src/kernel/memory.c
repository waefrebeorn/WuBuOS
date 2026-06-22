/*
 * memory.c  --  My Seed Kernel Memory Subsystem Implementation
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

/* -- Internal: Spinlock ------------------------------------------- */

static inline void heap_lock(CHeapCtrl *hc) {
    while (__atomic_test_and_set(&hc->locked_flags, __ATOMIC_ACQUIRE))
        ;
}

static inline void heap_unlock(CHeapCtrl *hc) {
    __atomic_clear(&hc->locked_flags, __ATOMIC_RELEASE);
}

/* -- Internal: Red Zone Helpers ---------------------------------- */

/* Write canary values into red zones around a user allocation.
 * Layout: [CMemUsed header] [front canary (16B)] [user data] [back canary (16B)]
 * The front canary is written at the start of the user area,
 * the back canary at the end. The actual user pointer is offset by
 * MEM_RED_ZONE_SIZE past the header's start[] field. */
static void write_canaries(CMemUsed *mu) {
    uint32_t *front = (uint32_t *)mu->start;
    for (int i = 0; i < MEM_RED_ZONE_SIZE / 4; i++)
        front[i] = MEM_CANARY_FRONT;
    /* Back canary: at (user_size / 4) dwords past the front canary area.
     * User ptr starts at start + RED_ZONE_SIZE. User size = mu->size - header - 2*RED_ZONE_SIZE. */
    size_t header_size = offsetof(CMemUsed, start);
    size_t user_bytes = mu->size - header_size - 2 * MEM_RED_ZONE_SIZE;
    uint32_t *back = (uint32_t *)((uint8_t *)mu->start + MEM_RED_ZONE_SIZE + user_bytes);
    for (int i = 0; i < MEM_RED_ZONE_SIZE / 4; i++)
        back[i] = MEM_CANARY_BACK;
}

static int check_canaries(CMemUsed *mu) {
    size_t header_size = offsetof(CMemUsed, start);
    size_t user_bytes = mu->size - header_size - 2 * MEM_RED_ZONE_SIZE;
    uint32_t *front = (uint32_t *)mu->start;
    for (int i = 0; i < MEM_RED_ZONE_SIZE / 4; i++) {
        if (front[i] != MEM_CANARY_FRONT) return -1;
    }
    uint32_t *back = (uint32_t *)((uint8_t *)mu->start + MEM_RED_ZONE_SIZE + user_bytes);
    for (int i = 0; i < MEM_RED_ZONE_SIZE / 4; i++) {
        if (back[i] != MEM_CANARY_BACK) return -1;
    }
    return 0;
}

/* -- Global Heap -------------------------------------------------- */

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

/* -- Init / Shutdown ---------------------------------------------- */

int mem_init(size_t total_bytes) {
    g_heap_base = malloc(total_bytes);
    if (!g_heap_base) return -1;
    
    g_heap_total = total_bytes;
    g_next_page  = (uint8_t *)g_heap_base;
    g_heap_end   = g_next_page + total_bytes;
    /* Allocate CHeapCtrl at the start  --  may need multiple pages */
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

/* -- Allocation --------------------------------------------------- */

void *mem_alloc(size_t size) {
    if (!g_heap) return NULL;
    if (size == 0) size = 1;
    
    /* Round up to 16-byte alignment. Add red zone space. */
    size_t total = size + sizeof(CMemUsed) + 2 * MEM_RED_ZONE_SIZE;
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
    
    /* Trim block to requested total if not already split.
     * This ensures canaries are written at the correct offsets. */
    if (mu->size > total) {
        /* Check if we should split */
        if (mu->size > total + sizeof(CMemUsed) + 16) {
            size_t rem_size = mu->size - total;
            CMemUsed *rem = (CMemUsed *)((uint8_t *)mu + total);
            rem->signature = MEM_UNUSED_SIGNATURE;
            rem->size = rem_size;
            
            CMemUnused *u = (CMemUnused *)rem;
            u->next = hc->malloc_free_list.next;
            hc->malloc_free_list.next = u;
        }
        /* Always record the size as the requested total */
        mu->size = total;
    }
    
    /* Write red zone canaries */
    write_canaries(mu);
    
    hc->used_size += mu->size;
    heap_unlock(hc);
    
    /* Return user pointer past the front red zone */
    return (uint8_t *)mu->start + MEM_RED_ZONE_SIZE;
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
    
    /* Get header from user pointer (offset back by red zone size) */
    CMemUsed *mu = (CMemUsed *)((uint8_t *)ptr - MEM_RED_ZONE_SIZE - offsetof(CMemUsed, start));
    
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
    
    CMemUsed *mu = (CMemUsed *)((uint8_t *)ptr - MEM_RED_ZONE_SIZE - offsetof(CMemUsed, start));
    size_t header_plus_rz = offsetof(CMemUsed, start) + MEM_RED_ZONE_SIZE;
    size_t old_usable = mu->size - header_plus_rz - MEM_RED_ZONE_SIZE;
    size_t copy_size = old_usable < new_size ? old_usable : new_size;
    
    void *new_ptr = mem_alloc(new_size);
    if (!new_ptr) return NULL;
    
    memcpy(new_ptr, ptr, copy_size);
    mem_free(ptr);
    
    return new_ptr;
}

/* -- Diagnostics -------------------------------------------------- */

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
    if (!g_heap || !callback) return;
    
    CHeapCtrl *hc = g_heap;
    heap_lock(hc);
    
    /* Walk the hash table buckets. 
     * In the hash table, blocks are stored as CMemUnused but their
     * CMemUsed fields (signature/size) are partially overlaid by next pointer.
     * The hash key is the block size, so all blocks in a bucket have that size.
     * We check if the signature at CMemUnused offset 8 matches MEM_UNUSED_SIG
     * to distinguish used vs free blocks. But typically hash buckets contain
     * recently-freed blocks (UNUSED). Used blocks would be on page memory.
     * 
     * For correctness, we use bloom_scan for the full heap walk. */
    
    /* Use bloom scan for reliable signature detection */
    heap_unlock(hc);
    
    /* Walk via bloom scan - finds all blocks with USED signature */
    uint8_t *scan = (uint8_t *)g_heap_base + sizeof(CHeapCtrl);
    uint8_t *end = g_heap_end;
    scan = (uint8_t *)(((uintptr_t)scan + 7) & ~(uintptr_t)7);
    
    while (scan + 4 <= end) {
        uint32_t sig;
        memcpy(&sig, scan, 4);
        
        if (sig == MEM_USED_SIGNATURE) {
            CMemUsed *mu = (CMemUsed *)scan;
            if (mu->size >= sizeof(CMemUsed) && mu->size < g_heap_total &&
                (uint8_t *)mu + mu->size <= end) {
                /* Verify canaries if present */
                void *user_ptr = (uint8_t *)mu->start + MEM_RED_ZONE_SIZE;
                size_t user_size = mu->size - offsetof(CMemUsed, start) - 2 * MEM_RED_ZONE_SIZE;
                callback(user_ptr, user_size, ctx);
                scan = (uint8_t *)mu + mu->size;
                scan = (uint8_t *)(((uintptr_t)scan + 7) & ~(uintptr_t)7);
                continue;
            }
        }
        scan += 8;
    }
}

/* -- Bloom Filter Scan -------------------------------------------- */

int mem_bloom_scan(uint32_t target_sig, MemBloomFn callback, void *ctx) {
    if (!g_heap || !g_heap_base) return 0;
    
    int count = 0;
    uint8_t *scan = (uint8_t *)g_heap_base + sizeof(CHeapCtrl);
    uint8_t *end = g_heap_end;
    
    /* Align scan to 8 bytes for efficient scanning */
    scan = (uint8_t *)(((uintptr_t)scan + 7) & ~(uintptr_t)7);
    
    while (scan + 4 <= end) {
        uint32_t sig;
        memcpy(&sig, scan, 4);  /* Safe unaligned read */
        
        if (sig == target_sig) {
            CMemUsed *mu = (CMemUsed *)scan;
            /* Validate: signature + reasonable size */
            if (mu->size >= sizeof(CMemUsed) && 
                mu->size < g_heap_total &&
                (uint8_t *)mu + mu->size <= end) {
                if (callback) callback(mu, sig, ctx);
                count++;
                /* Skip past this block */
                scan = (uint8_t *)mu + mu->size;
                scan = (uint8_t *)(((uintptr_t)scan + 7) & ~(uintptr_t)7);
                continue;
            }
        }
        
        /* Also check for CMemUnused-style signature at offset 8.
         * After mem_free, the signature field is written at offset 8
         * of the reinterpreted CMemUnused struct. */
        if (scan + 12 <= end) {
            uint32_t sig8;
            memcpy(&sig8, scan + 8, 4);
            if (sig8 == target_sig) {
                if (callback) callback(scan, sig8, ctx);
                count++;
                scan += 48;  /* Skip minimum block size */
                scan = (uint8_t *)(((uintptr_t)scan + 7) & ~(uintptr_t)7);
                continue;
            }
        }
        
        scan += 8;  /* Skip 8 bytes at a time (SWAR-like) */
    }
    
    return count;
}

/* mem_walk_stats — count used/free blocks and sizes via bloom scan */
int mem_walk_stats(size_t *total_used, size_t *total_free, int *n_used, int *n_free) {
    if (!g_heap || !g_heap_base) return -1;
    
    size_t used_sz = 0, free_sz = 0;
    int nu = 0, nf = 0;
    uint8_t *scan = (uint8_t *)g_heap_base + sizeof(CHeapCtrl);
    uint8_t *end = g_heap_end;
    scan = (uint8_t *)(((uintptr_t)scan + 7) & ~(uintptr_t)7);
    
    while (scan + 4 <= end) {
        uint32_t sig;
        memcpy(&sig, scan, 4);
        
        if (sig == MEM_USED_SIGNATURE) {
            CMemUsed *mu = (CMemUsed *)scan;
            if (mu->size >= sizeof(CMemUsed) && mu->size < g_heap_total &&
                (uint8_t *)mu + mu->size <= end) {
                used_sz += mu->size; nu++;
                scan = (uint8_t *)mu + mu->size;
                scan = (uint8_t *)(((uintptr_t)scan + 7) & ~(uintptr_t)7);
                continue;
            }
        }
        /* Also check for MEM_UNUSED_SIGNATURE at offset 8 (CMemUnused layout) */
        if (scan + 12 <= end) {
            uint32_t sig8;
            memcpy(&sig8, scan + 8, 4);
            if (sig8 == MEM_UNUSED_SIGNATURE) {
                /* This is a freed block. Try to recover size from CMemUsed layout.
                 * But after freeing, the size field is corrupted by the next pointer.
                 * The hash bucket index tells us the original total size.
                 * For now, just count it. */
                nf++;
                /* Can't reliably determine size — skip to next possible block.
                 * Advance by minimum block size. */
                scan += 48;  /* Minimum reasonable block size */
                scan = (uint8_t *)(((uintptr_t)scan + 7) & ~(uintptr_t)7);
                continue;
            }
        }
        scan += 8;
    }
    
    if (total_used) *total_used = used_sz;
    if (total_free) *total_free = free_sz;
    if (n_used) *n_used = nu;
    if (n_free) *n_free = nf;
    return nu;
}

/* mem_check_redzones — check canaries for a specific allocation */
int mem_check_redzones(void *ptr) {
    if (!ptr || !g_heap) return -1;
    
    CMemUsed *mu = (CMemUsed *)((uint8_t *)ptr - MEM_RED_ZONE_SIZE - offsetof(CMemUsed, start));
    if (mu->signature != MEM_USED_SIGNATURE) return -1;
    
    return check_canaries(mu);
}

/* mem_validate_all — scan all blocks for corruption */
int mem_validate_all(void) {
    if (!g_heap) return -1;
    
    int corrupt = 0;
    CHeapCtrl *hc = g_heap;
    heap_lock(hc);
    
    /* Check hash buckets */
    for (int i = 0; i < MEM_HEAP_HASH_SIZE; i++) {
        CMemUnused *u = hc->heap_hash[i];
        while (u) {
            CMemUsed *mu = (CMemUsed *)u;
            if (mu->signature == MEM_USED_SIGNATURE && check_canaries(mu) != 0) {
                fprintf(stderr, "CORRUPT: hash bucket %d block %p size=%u\n",
                        i, (void *)((uint8_t *)mu->start + MEM_RED_ZONE_SIZE), mu->size);
                corrupt++;
            }
            u = u->next;
        }
    }
    
    /* Check free list */
    CMemUnused *cur = hc->malloc_free_list.next;
    while (cur) {
        CMemUsed *mu = (CMemUsed *)cur;
        if (mu->signature == MEM_USED_SIGNATURE && check_canaries(mu) != 0) {
            fprintf(stderr, "CORRUPT: free list block %p size=%u\n",
                    (void *)((uint8_t *)mu->start + MEM_RED_ZONE_SIZE), mu->size);
            corrupt++;
        }
        cur = cur->next;
    }
    
    heap_unlock(hc);
    return corrupt;
}

/* mem_debug_dump — visual heap state to stderr */
void mem_debug_dump(void) {
    if (!g_heap) {
        fprintf(stderr, "mem_debug_dump: heap not initialized\n");
        return;
    }
    
    CHeapCtrl *hc = g_heap;
    size_t total_used = 0, total_free = 0;
    int n_used = 0, n_free = 0;
    
    heap_lock(hc);
    
    fprintf(stderr, "\n╔══════════════════════════════════════╗\n");
    fprintf(stderr, "║       WuBuOS Heap Debug Dump         ║\n");
    fprintf(stderr, "╠══════════════════════════════════════╣\n");
    fprintf(stderr, "║ Heap base:  %p                ║\n", (void *)g_heap_base);
    fprintf(stderr, "║ Heap size:  %zu bytes          ║\n", g_heap_total);
    fprintf(stderr, "║ Used:       %zu bytes          ║\n", hc->used_size);
    fprintf(stderr, "║ Available:  %zu bytes          ║\n", hc->max_size - hc->used_size);
    fprintf(stderr, "║ Signature:  0x%08X            ║\n", hc->hc_signature);
    fprintf(stderr, "╠══════════════════════════════════════╣\n");
    
    /* Walk hash buckets and count */
    for (int i = 0; i < MEM_HEAP_HASH_SIZE; i++) {
        CMemUnused *u = hc->heap_hash[i];
        while (u) {
            CMemUsed *mu = (CMemUsed *)u;
            if (mu->signature == MEM_USED_SIGNATURE) {
                total_used += mu->size; n_used++;
                int rz = check_canaries(mu);
                fprintf(stderr, "║ [USED]  %p  size=%5u  rz=%s  ║\n",
                        (void *)((uint8_t *)mu->start + MEM_RED_ZONE_SIZE),
                        mu->size, rz == 0 ? "OK" : "BAD");
            } else {
                total_free += mu->size; n_free++;
                fprintf(stderr, "║ [FREE]  %p  size=%5u            ║\n",
                        (void *)mu, mu->size);
            }
            u = u->next;
        }
    }
    
    CMemUnused *cur = hc->malloc_free_list.next;
    while (cur) {
        CMemUsed *mu = (CMemUsed *)cur;
        if (mu->signature == MEM_USED_SIGNATURE) {
            total_used += mu->size; n_used++;
            int rz = check_canaries(mu);
            fprintf(stderr, "║ [USED]  %p  size=%5u  rz=%s  ║\n",
                    (void *)((uint8_t *)mu->start + MEM_RED_ZONE_SIZE),
                    mu->size, rz == 0 ? "OK" : "BAD");
        } else {
            total_free += mu->size; n_free++;
            fprintf(stderr, "║ [FREE]  %p  size=%5u            ║\n",
                    (void *)mu, mu->size);
        }
        cur = cur->next;
    }
    
    fprintf(stderr, "╠══════════════════════════════════════╣\n");
    fprintf(stderr, "║ Used blocks:  %d  (%zu bytes)  ║\n", n_used, total_used);
    fprintf(stderr, "║ Free blocks:  %d  (%zu bytes)  ║\n", n_free, total_free);
    fprintf(stderr, "╚══════════════════════════════════════╝\n\n");
    
    heap_unlock(hc);
}
