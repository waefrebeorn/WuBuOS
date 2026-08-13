/* test_sync.c -- host tests for wubu_sync (spinlock + ISR-safe FIFO). */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>

#include "wubu_sync.h"
#include "wubu_sync.c"

/* Gap D3 stubs: the real lock's priority-inheritance calls these (the
 * host test has no tasking). */
struct CTask;
struct CTask *task_current(void) { return NULL; }
int task_prio_get(const struct CTask *t) { (void)t; return 0; }
void task_prio_set(struct CTask *t, int p) { (void)t; (void)p; }

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
    printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; \
} } while (0)

/* ---- FIFO under concurrency: ONE producer + ONE consumer (true SPSC) ---- */
static wubu_fifo_t g_f;
static volatile int g_prod_done = 0;
#define TOTAL 20000

static void *prod(void *arg)
{
    (void)arg;
    for (int i = 0; i < TOTAL; i++) {
        uint32_t v = (uint32_t)i;
        while (wubu_fifo_push(&g_f, v) != 0) { /* full: spin; consumer drains */
        }
    }
    __atomic_store_n(&g_prod_done, 1, __ATOMIC_RELEASE);
    return NULL;
}

static void *cons(void *arg)
{
    (void)arg;
    int got = 0;
    uint32_t expect = 0;
    int missing = 0;
    while (!__atomic_load_n(&g_prod_done, __ATOMIC_ACQUIRE) ||
           !wubu_fifo_empty(&g_f)) {
        uint32_t v;
        if (wubu_fifo_pop(&g_f, &v) == 0) {
            if (v != expect) {      /* SPSC preserves order */
                printf("  OUT-OF-ORDER: got %u expected %u\n", v, expect);
                missing = 1;
            }
            expect++;
            got++;
        }
    }
    printf("  consumed %d values\n", got);
    if (!missing && got == TOTAL) printf("  all %d values present, in order\n", TOTAL);
    else printf("  VERIFY FAILED (got=%d missing=%d)\n", got, missing);
    return NULL;
}

int main(void)
{
    /* ---- FIFO basics ---- */
    wubu_fifo_init(&g_f, 8);
    CHECK(wubu_fifo_empty(&g_f));
    CHECK(wubu_fifo_push(&g_f, 42) == 0);
    CHECK(wubu_fifo_count(&g_f) == 1);
    uint32_t v = 0;
    CHECK(wubu_fifo_peek(&g_f, &v) == 0 && v == 42);
    CHECK(wubu_fifo_pop(&g_f, &v) == 0 && v == 42);
    CHECK(wubu_fifo_empty(&g_f));
    CHECK(wubu_fifo_pop(&g_f, &v) == -1);       /* empty */

    /* wrap-around: fill 8, pop 3, fill 3 */
    for (int i = 0; i < 8; i++) CHECK(wubu_fifo_push(&g_f, (uint32_t)i) == 0);
    CHECK(wubu_fifo_full(&g_f));
    CHECK(wubu_fifo_push(&g_f, 99) == -1);      /* full */
    for (int i = 0; i < 3; i++) CHECK(wubu_fifo_pop(&g_f, &v) == 0 && v == (uint32_t)i);
    for (int i = 8; i < 11; i++) CHECK(wubu_fifo_push(&g_f, (uint32_t)i) == 0);
    for (int i = 3; i < 11; i++) CHECK(wubu_fifo_pop(&g_f, &v) == 0 && v == (uint32_t)i);
    CHECK(wubu_fifo_empty(&g_f));

    /* ---- FIFO under concurrency: 1 producer + 1 consumer ---- */
    wubu_fifo_init(&g_f, WUBU_FIFO_N);
    g_prod_done = 0;
    pthread_t pt, ct;
    pthread_create(&pt, NULL, prod, NULL);
    pthread_create(&ct, NULL, cons, NULL);
    pthread_join(pt, NULL);
    pthread_join(ct, NULL);
    CHECK(wubu_fifo_empty(&g_f));

    /* ---- spinlock: init only (lock/unlock use cli -- RING-0 ONLY, so
     * they are metal-verified, not host-testable; a CPL-3 cli #GPs) ---- */
    wubu_spinlock_t l;
    wubu_spin_init(&l);
    CHECK(l.locked == 0);

    if (failures == 0) printf("test_sync: ALL PASS\n");
    else printf("test_sync: %d FAILURES\n", failures);
    return failures ? 1 : 0;
}
