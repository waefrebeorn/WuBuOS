/*
 * wubu_as.h  --  per-address-space isolation (gap B7)
 *
 * The kernel and its future user space shared one CR3: no isolation.
 * This module introduces address spaces: an AS owns a private PML4
 * tree (the kernel's mappings are cloned at creation; the user window
 * is distinct per AS), a task binds to an AS, and the scheduler
 * switches CR3 on a context switch. The kernel AS is the singleton
 * the boot already has.
 *
 * Freestanding C11, no heap (tables come from the page allocator).
 */
#ifndef WUBU_AS_H
#define WUBU_AS_H

#include <stdint.h>
#include <stddef.h>

typedef struct wubu_as wubu_as_t;

/* The kernel's singleton AS (the boot's own CR3). */
wubu_as_t *wubu_as_kernel(void);

/* Create a new AS: a private PML4 with the kernel window cloned.
 * Returns NULL on alloc failure. */
wubu_as_t *wubu_as_create(void);

/* Bind the current task to an AS (the scheduler uses this at switch
 * time). Returns 0 on success. */
int wubu_as_bind(wubu_as_t *as);

/* Switch the CPU to an AS (reload CR3). */
void wubu_as_switch(wubu_as_t *as);

/* Destroy an AS + its private tables (not the kernel AS). */
void wubu_as_destroy(wubu_as_t *as);

/* The AS a task is currently bound to, or NULL. */
wubu_as_t *wubu_as_current(void);

/* Diagnostic: the number of live ASes. */
uint32_t wubu_as_count(void);

#endif
