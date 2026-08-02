/*
 * wubu_sync.c  --  WuBuOS Synchronization Primitives (freestanding)
 *
 * See wubu_sync.h. The spinlock saves/restores RFLAGS so the caller's
 * interrupt state is preserved (lock in an ISR -> unlock restores IF=0).
 * The FIFO is a single-producer/single-consumer ring with volatile
 * indices -- the standard lock-free pattern for driver->consumer.
 */

#include "wubu_sync.h"

static inline uint64_t read_rflags(void)
{
    uint64_t f;
    __asm__ __volatile__("pushfq\n popq %0" : "=r"(f) :: "memory");
    return f;
}

void wubu_spin_init(wubu_spinlock_t *l)
{
    if (!l) return;
    l->locked = 0;
    l->irq_state = 0;
}

void wubu_spin_lock(wubu_spinlock_t *l)
{
    if (!l) return;
    uint64_t flags = read_rflags();
    __asm__ __volatile__("cli" ::: "memory");   /* irq-safe acquire */
    while (__atomic_test_and_set(&l->locked, __ATOMIC_ACQUIRE)) {
        /* spin; IF is already off, so no IRQ can arrive mid-spin */
        __asm__ __volatile__("pause");
    }
    l->irq_state = (uint32_t)(flags & 0x200u);  /* keep the IF bit */
}

void wubu_spin_unlock(wubu_spinlock_t *l)
{
    if (!l) return;
    __atomic_clear(&l->locked, __ATOMIC_RELEASE);
    if (l->irq_state & 0x200u)
        __asm__ __volatile__("sti" ::: "memory");
}

void wubu_fifo_init(wubu_fifo_t *f, uint32_t cap)
{
    if (!f) return;
    if (cap > WUBU_FIFO_N) cap = WUBU_FIFO_N;
    f->head = 0;
    f->tail = 0;
    f->count = 0;
    f->capacity = cap;
}

int wubu_fifo_push(wubu_fifo_t *f, uint32_t v)
{
    if (!f || f->count >= f->capacity) return -1;
    f->data[f->head] = v;
    f->head = (f->head + 1) % f->capacity;
    __atomic_add_fetch(&f->count, 1, __ATOMIC_RELEASE);
    return 0;
}

int wubu_fifo_pop(wubu_fifo_t *f, uint32_t *out)
{
    if (!f || f->count == 0) return -1;
    uint32_t v = f->data[f->tail];
    f->tail = (f->tail + 1) % f->capacity;
    __atomic_sub_fetch(&f->count, 1, __ATOMIC_ACQUIRE);
    if (out) *out = v;
    return 0;
}

int wubu_fifo_peek(wubu_fifo_t *f, uint32_t *out)
{
    if (!f || f->count == 0) return -1;
    if (out) *out = f->data[f->tail];
    return 0;
}

uint32_t wubu_fifo_count(wubu_fifo_t *f)
{
    return f ? f->count : 0;
}

bool wubu_fifo_full(wubu_fifo_t *f)  { return f && f->count >= f->capacity; }
bool wubu_fifo_empty(wubu_fifo_t *f) { return !f || f->count == 0; }
