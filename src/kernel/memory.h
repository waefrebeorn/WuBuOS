/*
 * memory.h  --  My Seed Kernel Memory Subsystem
 *
 * Hand-ported from ZealOS Memory design principles:
 *   - Hash table for small allocations (≤MEM_HEAP_HASH_SIZE)
 *   - Page-based allocation for large allocations
 *   - Single-owner, ring-0, no protection
 *   - Signature checking for heap corruption detection
 *
 * The ZealOS original is raw x86-64 assembly (MAllocFree.ZC).
 * This is a clean C11 reimplementation of the same design.
 */

#ifndef MYSEED_MEMORY_H
#define MYSEED_MEMORY_H

#include <stdint.h>
#include <stddef.h>

/* -- Constants ---------------------------------------------------- */

#define MEM_PAG_BITS        12                          /* 4KB pages          */
#define MEM_PAG_SIZE        (1 << MEM_PAG_BITS)         /* 4096               */
#define MEM_HEAP_HASH_SIZE  (MEM_PAG_SIZE / 8)          /* 512 buckets        */
#define MEM_MIN_ALLOC       16                          /* Minimum allocation */

/* Signatures for corruption detection (ZealOS pattern) */
#define HEAP_CTRL_SIGNATURE  0x5EEDC0DE
#define MEM_USED_SIGNATURE   0xDEADBEEF
#define MEM_UNUSED_SIGNATURE 0xCAFEBABE

/* Red zone canary values (ZealOS-style guard bytes) */
#define MEM_RED_ZONE_SIZE     16
#define MEM_CANARY_FRONT      0xBADCAFE0
#define MEM_CANARY_BACK       0xDEADBEE0

/* -- Types -------------------------------------------------------- */

typedef enum {
    HClf_LOCKED = 0,
} HeapCtrlFlag;

/* Free list node */
typedef struct CMemUnused CMemUnused;
struct CMemUnused {
    CMemUnused *next;
    uint32_t     signature;  /* MEM_UNUSED_SIGNATURE when in free list */
};

/* Used allocation header */
typedef struct CMemUsed CMemUsed;
struct CMemUsed {
    uint32_t     signature;  /* MEM_USED_SIGNATURE when allocated */
    uint32_t     size;       /* Total bytes including this header */
    /* User data starts at CMemUsed.start = offsetof(CMemUsed, start) */
    char         start[];
};

/* Memory block (page-based allocation for large allocs) */
typedef struct CMemBlk CMemBlk;
struct CMemBlk {
    CMemBlk    *next;
    uint16_t    pags;        /* Number of pages in this block */
    uint16_t    pad;
    /* Data follows at sizeof(CMemBlk) */
};

/* Heap controller  --  the core heap structure */
typedef struct CHeapCtrl CHeapCtrl;
struct CHeapCtrl {
    uint32_t     hc_signature;          /* HEAP_CTRL_SIGNATURE            */
    uint32_t     locked_flags;          /* Bit 0 = locked (spinlock)     */
    CMemUnused  *heap_hash[MEM_HEAP_HASH_SIZE];  /* Hash table for small allocs */
    CMemUnused   malloc_free_list;      /* Free list for small allocs    */
    CHeapCtrl   *next_heap;            /* Linked list of heaps          */
    size_t       used_size;            /* Total bytes allocated         */
    size_t       max_size;             /* Maximum allowed               */
};

/* -- API ---------------------------------------------------------- */

/* Initialize the kernel heap. Call once at boot. */
int  mem_init(size_t total_bytes);

/* Shutdown (frees all heap memory back to system). */
void mem_shutdown(void);

/* Get the global kernel heap controller. */
CHeapCtrl *mem_heap_ctrl(void);

/* -- Allocation --------------------------------------------------- */

/*
 * Allocate `size` bytes. Returns 16-aligned memory.
 * On failure: returns NULL (unlike ZealOS which throws).
 */
void *mem_alloc(size_t size);

/*
 * Allocate `count * size` bytes, zeroed.
 */
void *mem_calloc(size_t count, size_t size);

/*
 * Reallocate to new size. May move memory.
 */
void *mem_realloc(void *ptr, size_t new_size);

/*
 * Free allocation. Safe on NULL (no-op).
 */
void mem_free(void *ptr);

/* -- Diagnostics -------------------------------------------------- */

/* Total bytes currently allocated. */
size_t mem_used(void);

/* Total bytes available for allocation. */
size_t mem_available(void);

/* Validate heap integrity. Returns 0 if OK, -1 on corruption. */
int mem_validate(void);

/* Heap walk: call `callback` for each allocated block. */
typedef void (*MemWalkFn)(void *ptr, size_t size, void *ctx);
void mem_walk(MemWalkFn callback, void *ctx);

/* Check red zone canaries for a specific allocation. Returns 0 if intact, -1 if corrupted. */
int mem_check_redzones(void *ptr);

/* Count total used and free blocks. Returns number of used blocks. */
int mem_walk_stats(size_t *total_used, size_t *total_free, int *n_used, int *n_free);

/* Bloom filter: scan heap for blocks with a given signature.
 * Calls callback for each match. Fast pre-filter for walk. */
typedef void (*MemBloomFn)(void *block, uint32_t sig, void *ctx);
int mem_bloom_scan(uint32_t target_sig, MemBloomFn callback, void *ctx);

/* Debug dump: print heap state to stderr. Shows block map, stats, corruption. */
void mem_debug_dump(void);

/* Validate all blocks and report any corruption. Returns number of corrupt blocks. */
int mem_validate_all(void);

#endif /* MYSEED_MEMORY_H */
