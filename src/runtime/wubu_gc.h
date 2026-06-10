/*
 * wubu_gc.h — Userspace Mark-and-Sweep GC API
 *
 * Opt-in garbage collector for HolyC REPL, editor, container apps.
 * Kernel uses manual memory.c — this is purely userspace.
 *
 * Usage:
 *   void *ptr = wubu_gc_alloc(256);
 *   wubu_gc_root_add(ptr);      // Pin during critical section
 *   wubu_gc_root_remove(ptr);
 *   wubu_gc_collect();          // Explicit collection
 */

#ifndef WUBU_GC_H
#define WUBU_GC_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*── Allocation ────────────────────────────────────────────────────*/

/* Allocate GC-managed memory. Returns NULL on OOM.
 * Memory is zeroed. Call wubu_gc_free() to release early,
 * or let wubu_gc_collect() reclaim when unreferenced. */
void *wubu_gc_alloc(size_t size);

/* Explicit free — optional, since GC reclaims automatically.
 * Use for deterministic release of large buffers. */
void wubu_gc_free(void *ptr);

/*── Root Pinning ──────────────────────────────────────────────────*/

/* Add pointer to root set — prevents collection during critical section.
 * Must be matched with wubu_gc_root_remove(). */
void wubu_gc_root_add(void *ptr);

/* Remove pointer from root set. */
void wubu_gc_root_remove(void *ptr);

/* Current root count. */
size_t wubu_gc_root_count(void);

/*── Collection Control ────────────────────────────────────────────*/

/* Trigger mark-and-sweep collection.
 * Also runs automatically when allocation exceeds threshold. */
void wubu_gc_collect(void);

/* Enable/disable automatic collection on allocation. */
void wubu_gc_enable(int enable);
int  wubu_gc_enabled(void);

/* Set collection threshold (bytes). Default 1MB. */
void wubu_gc_set_threshold(size_t bytes);

/*── Statistics ────────────────────────────────────────────────────*/

size_t wubu_gc_allocated(void);

void wubu_gc_stats(size_t *allocated, size_t *threshold, size_t *roots);

/*── Lifecycle ─────────────────────────────────────────────────────*/

/* Free all GC memory. Call on applet shutdown. */
void wubu_gc_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_GC_H */