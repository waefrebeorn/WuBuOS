/*
 * wubu_sync.h  --  WuBuOS Synchronization Primitives (freestanding)
 *
 * With timer PREEMPTION live, shared state needs protection. This module
 * provides the two primitives the kernel's drivers/ISRs need:
 *
 *   wubu_spinlock     -- cli/sti + spin (interrupt-safe mutual exclusion)
 *   wubu_fifo         -- fixed-size, ISR-safe producer/consumer FIFO
 *                        (single producer, single consumer -- the
 *                        classic lock-free ring for driver->consumer)
 *
 * No malloc, no hosted APIs.
 */
#ifndef WUBU_SYNC_H
#define WUBU_SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ---- spinlock ------------------------------------------------------ */

typedef struct {
    volatile uint32_t locked;
    uint32_t          irq_state;   /* saved RFLAGS (IF) for irq-safe lock */
    /* Gap D3: priority-inheritance bookkeeping -- the owner task (set
     * at acquire) + the saved pre-boost priority + whether a higher
     * waiter boosted it while the lock was contended. */
    struct CTask     *owner;
    int               owner_prio_saved;
    int               boosted;
} wubu_spinlock_t;

/* Initialize (unlocked). */
void wubu_spin_init(wubu_spinlock_t *l);

/* Acquire: disable interrupts (save state) + spin until free.
 * Nesting: do NOT lock the same spinlock twice. */
void wubu_spin_lock(wubu_spinlock_t *l);

/* Release: clear + restore the saved interrupt state. */
void wubu_spin_unlock(wubu_spinlock_t *l);

/* ---- ISR-safe FIFO (single producer, single consumer) -------------- */

#define WUBU_FIFO_N 32

typedef struct {
    volatile uint32_t head;    /* next write slot (producer) */
    volatile uint32_t tail;    /* next read slot  (consumer) */
    volatile uint32_t count;   /* shared occupancy (atomics) */
    uint32_t          capacity;
    uint32_t          data[WUBU_FIFO_N];
} wubu_fifo_t;

void wubu_fifo_init(wubu_fifo_t *f, uint32_t cap);  /* cap <= WUBU_FIFO_N */

/* Producer (ISR-safe: one producer only). Returns 0 on success, -1 full. */
int wubu_fifo_push(wubu_fifo_t *f, uint32_t v);

/* Consumer. Returns 0 on success, -1 empty. */
int wubu_fifo_pop(wubu_fifo_t *f, uint32_t *out);

/* Non-destructive peek of the next value. Returns 0/-1. */
int wubu_fifo_peek(wubu_fifo_t *f, uint32_t *out);

uint32_t wubu_fifo_count(wubu_fifo_t *f);
bool     wubu_fifo_full(wubu_fifo_t *f);
bool     wubu_fifo_empty(wubu_fifo_t *f);

#endif /* WUBU_SYNC_H */
