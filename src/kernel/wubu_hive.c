/*
 * wubu_hive.c -- C11 "luddite hive" (see wubu_hive.h for the design).
 *
 * Linked fixed-capacity blocks + bit skipfield + per-block freelist.
 * Erase = mark + freelist push (O(1), no moves).  Insert reuses a free
 * slot or appends a new block.  Iteration jumps skipped slots.
 *
 * The per-block freelist is chained inside the slots array: a free slot
 * stores the index of the next free slot as a uintptr_t.  The skipfield
 * is one bit per slot (cap/8 bytes per block).  Freestanding C11 --
 * storage comes from the alloc/free callbacks the caller passed in.
 */
#include "wubu_hive.h"
#include <string.h>

#define WUBU_HIVE_DEFAULT_CAP 32

/* ---- skipfield (1 bit per slot) ------------------------------------ */

static inline int  skip_get(const wubu_hive_block_t *b, size_t i)
{ return (b->skip[i >> 3] >> (i & 7)) & 1; }

static inline void skip_set(wubu_hive_block_t *b, size_t i)
{ b->skip[i >> 3] |= (uint8_t)(1u << (i & 7)); }

static inline void skip_clr(wubu_hive_block_t *b, size_t i)
{ b->skip[i >> 3] &= (uint8_t)~(1u << (i & 7)); }

/* ---- block lifecycle ------------------------------------------------ */

/* Append a fresh block to the hive.  Every slot is free; the freelist
 * chain fills the slots array (slot i -> i+1, last -> NONE). */
static wubu_hive_block_t *wubu_hive_block_new(wubu_hive_t *h)
{
    const size_t cap = h->block_cap;
    wubu_hive_block_t *b = (wubu_hive_block_t *)h->alloc(sizeof(*b));
    if (!b) return NULL;
    b->slots = (void **)h->alloc(cap * sizeof(void *));
    if (!b->slots) { h->free_(b); return NULL; }
    b->skip = (uint8_t *)h->alloc((cap + 7) / 8);
    if (!b->skip) { h->free_(b->slots); h->free_(b); return NULL; }

    /* A fresh block: EVERY slot is dead/free.  skip bit set = dead, so
     * fill the skipfield with 0xFF (insert clears the bit on reuse). */
    memset(b->skip, 0xFF, (cap + 7) / 8);
    for (size_t i = 0; i < cap; i++)
        b->slots[i] = (void *)(uintptr_t)((i + 1 < cap) ? (i + 1) : WUBU_HIVE_NONE);
    b->live = 0;
    b->cap  = cap;
    b->free_head = 0;
    b->next = NULL;

    if (h->tail) h->tail->next = b; else h->head = b;
    h->tail = b;
    h->capacity += cap;
    return b;
}

/* ---- public API ----------------------------------------------------- */

wubu_hive_t *wubu_hive_new(size_t block_cap,
                           void *(*alloc)(size_t),
                           void (*free_)(void *))
{
    if (!alloc || !free_) return NULL;
    if (block_cap == 0) block_cap = WUBU_HIVE_DEFAULT_CAP;

    wubu_hive_t *h = (wubu_hive_t *)alloc(sizeof(*h));
    if (!h) return NULL;
    h->head = NULL;
    h->tail = NULL;
    h->block_cap = block_cap;
    h->size = 0;
    h->capacity = 0;
    h->alloc = alloc;
    h->free_ = free_;
    return h;
}

void wubu_hive_destroy(wubu_hive_t *h)
{
    if (!h) return;
    wubu_hive_block_t *b = h->head;
    while (b) {
        wubu_hive_block_t *next = b->next;
        h->free_(b->skip);
        h->free_(b->slots);
        h->free_(b);
        b = next;
    }
    h->free_(h);
}

size_t wubu_hive_size(const wubu_hive_t *h)      { return h ? h->size : 0; }
size_t wubu_hive_capacity(const wubu_hive_t *h)  { return h ? h->capacity : 0; }
size_t wubu_hive_block_count(const wubu_hive_t *h)
{
    size_t n = 0;
    if (h) for (wubu_hive_block_t *b = h->head; b; b = b->next) n++;
    return n;
}
int wubu_hive_empty(const wubu_hive_t *h)        { return !h || h->size == 0; }

void *wubu_hive_insert(wubu_hive_t *h, void *elem)
{
    if (!h || !elem) return NULL;

    /* Reuse a free slot in some block, else append a new block. */
    wubu_hive_block_t *b = h->head;
    for (; b && b->free_head == WUBU_HIVE_NONE; b = b->next) {}
    if (!b) {
        b = wubu_hive_block_new(h);
        if (!b) return NULL;
    }

    const size_t idx = b->free_head;
    b->free_head = (size_t)(uintptr_t)b->slots[idx];   /* pop freelist */
    b->slots[idx] = elem;
    skip_clr(b, idx);
    b->live++;
    h->size++;
    return elem;
}

int wubu_hive_erase_at(wubu_hive_t *h, wubu_hive_iter_t *it)
{
    if (!h || !it || !it->block) return 0;
    wubu_hive_block_t *b = it->block;
    if (it->idx >= b->cap || skip_get(b, it->idx)) return 0;

    b->slots[it->idx] = (void *)(uintptr_t)b->free_head;  /* freelist push */
    b->free_head = it->idx;
    skip_set(b, it->idx);
    b->live--;
    h->size--;
    return 1;
}

void *wubu_hive_erase(wubu_hive_t *h, void *elem)
{
    if (!h || !elem) return NULL;
    wubu_hive_iter_t it;
    for (void *e = wubu_hive_first(h, &it); e; e = wubu_hive_next(h, &it)) {
        if (e == elem) {
            wubu_hive_erase_at(h, &it);
            return elem;
        }
    }
    return NULL;
}

void wubu_hive_clear(wubu_hive_t *h)
{
    if (!h) return;
    for (wubu_hive_block_t *b = h->head; b; b = b->next) {
        memset(b->skip, 0xFF, (b->cap + 7) / 8);   /* every slot dead again */
        for (size_t i = 0; i < b->cap; i++)
            b->slots[i] = (void *)(uintptr_t)((i + 1 < b->cap) ? (i + 1) : WUBU_HIVE_NONE);
        b->live = 0;
        b->free_head = 0;
    }
    h->size = 0;
}

/* ---- iteration ------------------------------------------------------ */

static size_t wubu_hive_block_first_live(const wubu_hive_block_t *b)
{
    for (size_t i = 0; i < b->cap; i++)
        if (!skip_get(b, i)) return i;
    return WUBU_HIVE_NONE;
}

void *wubu_hive_first(const wubu_hive_t *h, wubu_hive_iter_t *it)
{
    if (!h || !it) return NULL;
    for (wubu_hive_block_t *b = h->head; b; b = b->next) {
        size_t i = wubu_hive_block_first_live(b);
        if (i != WUBU_HIVE_NONE) {
            it->block = b;
            it->idx = i;
            return b->slots[i];
        }
    }
    it->block = NULL;
    it->idx = 0;
    return NULL;
}

void *wubu_hive_next(const wubu_hive_t *h, wubu_hive_iter_t *it)
{
    if (!h || !it || !it->block) return NULL;

    /* Same block: jump any skipped slots after idx. */
    wubu_hive_block_t *b = it->block;
    for (size_t i = it->idx + 1; i < b->cap; i++) {
        if (!skip_get(b, i)) { it->idx = i; return b->slots[i]; }
    }
    /* Next blocks. */
    for (b = b->next; b; b = b->next) {
        size_t i = wubu_hive_block_first_live(b);
        if (i != WUBU_HIVE_NONE) {
            it->block = b;
            it->idx = i;
            return b->slots[i];
        }
    }
    it->block = NULL;
    it->idx = 0;
    return NULL;
}
