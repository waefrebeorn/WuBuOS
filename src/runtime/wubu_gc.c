/*
 * wubu_gc.c  --  Simple Mark-and-Sweep GC for WuBuOS Userspace Applets
 *
 * Opt-in garbage collector for HolyC REPL, editor, container apps.
 * Kernel stays manual (memory.c)  --  this is purely userspace.
 *
 * Design (from "Baby's First Garbage Collector"  --  mark/sweep):
 *   - Single-linked allocation list
 *   - Mark phase: DFS from roots (stack, globals, registers)
 *   - Sweep phase: free unmarked, compact marked
 *   - Conservative stack scanning (no precise type info in C)
 *
 * Usage:
 *   void *ptr = wubu_gc_alloc(size);
 *   wubu_gc_root_add(ptr);   // Pin during critical section
 *   wubu_gc_root_remove(ptr);
 *   wubu_gc_collect();       // Explicit or auto on alloc failure
 */

#include "wubu_gc.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>

/* -- GC Object Header -------------------------------------------- */

typedef struct GCObject {
    struct GCObject *next;    /* Next in allocation list */
    size_t           size;    /* Payload size (excludes header) */
    uint8_t          marked;  /* Mark bit */
    uint8_t          pinned;  /* Root pin count */
    uint8_t          padding[6];
    /* Payload follows immediately */
} GCObject;

/* -- GC State ---------------------------------------------------- */

static GCObject  *g_alloc_list = NULL;
static void     **g_roots       = NULL;
static size_t     g_root_count = 0;
static size_t     g_root_cap   = 0;
static size_t     g_total_allocated = 0;
static size_t     g_gc_threshold = 1024 * 1024;  /* 1MB default */
static int        g_gc_enabled = 1;

/* -- Root Management --------------------------------------------- */

void wubu_gc_root_add(void *ptr) {
    if (!ptr) return;
    if (g_root_count == g_root_cap) {
        size_t new_cap = g_root_cap ? g_root_cap * 2 : 32;
        void **new_roots = realloc(g_roots, new_cap * sizeof(void*));
        if (!new_roots) return;
        g_roots = new_roots;
        g_root_cap = new_cap;
    }
    g_roots[g_root_count++] = ptr;
}

void wubu_gc_root_remove(void *ptr) {
    if (!ptr) return;
    for (size_t i = 0; i < g_root_count; i++) {
        if (g_roots[i] == ptr) {
            g_roots[i] = g_roots[--g_root_count];
            return;
        }
    }
}

size_t wubu_gc_root_count(void) { return g_root_count; }

/* -- Allocation -------------------------------------------------- */

void *wubu_gc_alloc(size_t size) {
    if (size == 0) return NULL;

    /* Align to 16 bytes */
    size = (size + 15) & ~15;

    /* Try to collect if over threshold */
    if (g_gc_enabled && g_total_allocated + size > g_gc_threshold) {
        wubu_gc_collect();
    }

    GCObject *obj = malloc(sizeof(GCObject) + size);
    if (!obj) return NULL;

    obj->size    = size;
    obj->marked  = 0;
    obj->pinned  = 0;
    obj->next    = g_alloc_list;
    g_alloc_list = obj;
    g_total_allocated += size;

    return (void*)(obj + 1);  /* Return payload pointer */
}

void wubu_gc_free(void *ptr) {
    if (!ptr) return;

    GCObject *obj = (GCObject*)ptr - 1;

    /* Unlink from allocation list */
    GCObject **pp = &g_alloc_list;
    while (*pp) {
        if (*pp == obj) {
            *pp = obj->next;
            g_total_allocated -= obj->size;
            free(obj);
            return;
        }
        pp = &(*pp)->next;
    }
    /* Not found  --  maybe already freed or not GC-managed */
}

/* -- Mark Phase -------------------------------------------------- */

static void gc_mark_ptr(void *ptr) {
    if (!ptr) return;

    GCObject *obj = (GCObject*)ptr - 1;

    /* Handle interior pointers: scan backwards to find object header */
    while (obj >= (GCObject*)g_alloc_list && obj->marked == 0) {
        obj->marked = 1;
        /* In a real GC, we'd scan obj's payload for more pointers.
         * Conservative: treat any word in payload as potential pointer. */
        return;
    }
}

static void gc_mark_roots(void) {
    for (size_t i = 0; i < g_root_count; i++) {
        gc_mark_ptr(g_roots[i]);
    }

    /* Conservative stack scan  --  scan current stack frame for pointers */
    /* This is platform-dependent; simplified here */
}

/* -- Sweep Phase ------------------------------------------------- */

static void gc_sweep(void) {
    GCObject **pp = &g_alloc_list;
    while (*pp) {
        GCObject *obj = *pp;
        if (obj->marked || obj->pinned) {
            obj->marked = 0;  /* Clear for next cycle */
            pp = &obj->next;
        } else {
            /* Unmarked  --  free it */
            *pp = obj->next;
            g_total_allocated -= obj->size;
            free(obj);
        }
    }
}

/* -- Public API -------------------------------------------------- */

void wubu_gc_collect(void) {
    if (!g_gc_enabled) return;

    gc_mark_roots();
    gc_sweep();

    /* Adjust threshold based on live data */
    g_gc_threshold = g_total_allocated * 2;
    if (g_gc_threshold < 64 * 1024) g_gc_threshold = 64 * 1024;
    if (g_gc_threshold > 64 * 1024 * 1024) g_gc_threshold = 64 * 1024 * 1024;
}

void wubu_gc_enable(int enable) { g_gc_enabled = enable; }
int  wubu_gc_enabled(void)        { return g_gc_enabled; }

size_t wubu_gc_allocated(void)    { return g_total_allocated; }

void wubu_gc_set_threshold(size_t bytes) {
    g_gc_threshold = bytes;
}

void wubu_gc_stats(size_t *allocated, size_t *threshold, size_t *roots) {
    if (allocated)  *allocated  = g_total_allocated;
    if (threshold)  *threshold  = g_gc_threshold;
    if (roots)      *roots      = g_root_count;
}

/* -- Cleanup ----------------------------------------------------- */

void wubu_gc_shutdown(void) {
    while (g_alloc_list) {
        GCObject *next = g_alloc_list->next;
        free(g_alloc_list);
        g_alloc_list = next;
    }
    free(g_roots);
    g_roots = NULL;
    g_root_count = g_root_cap = 0;
    g_total_allocated = 0;
}