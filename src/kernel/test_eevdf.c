/* test_eevdf.c -- EEVDF fairness and lag bounds. */
#include "sched_eevdf.h"
#include <stdio.h>
#include <stdlib.h>

static int checks, fails;
#define CHECK(c, m) do { checks++; if (!(c)) { printf("  [FAIL] %s\n", m); fails++; } } while (0)

int main(void)
{
    eevdf_rq_t rq;
    eevdf_init(&rq);
    CHECK(eevdf_add(&rq, 1, EEVDF_NICE_0) == 0, "task 1 added");
    CHECK(eevdf_add(&rq, 2, EEVDF_NICE_0) == 0, "task 2 added");
    CHECK(eevdf_add(&rq, 3, 0) == -1, "zero weight rejected");

    const int TICKS = 10000;
    int64_t worst_lag = 0;
    for (int i = 0; i < TICKS; i++) {
        int idx = eevdf_pick(&rq);
        if (idx < 0) { CHECK(0, "a task was always runnable"); break; }
        eevdf_charge(&rq, idx, 1);
        for (int k = 0; k < rq.count; k++) {
            int64_t lag = eevdf_lag(&rq, k);
            if (lag > worst_lag) worst_lag = lag;
            if (-lag > worst_lag) worst_lag = -lag;
        }
    }

    double share1 = (double)rq.tasks[0].ran / TICKS;
    double share2 = (double)rq.tasks[1].ran / TICKS;
    printf("  split: %.4f / %.4f over %d ticks\n", share1, share2, TICKS);
    printf("  worst |lag|: %lld ticks, slice %u\n", (long long)worst_lag, rq.tasks[0].slice);

    CHECK(share1 > 0.48 && share1 < 0.52, "equal-weight tasks split 50/50 within 2%");
    CHECK(share2 > 0.48 && share2 < 0.52, "both sides of the split are within 2%");
    CHECK(rq.tasks[0].ran + rq.tasks[1].ran == (uint64_t)TICKS, "every tick was charged");
    CHECK(worst_lag <= (int64_t)rq.tasks[0].slice * 4,
          "lag stays within a small multiple of the slice");

    /* Weighted: 2:1 weights should give 2:1 CPU within 5%. */
    eevdf_rq_t w;
    eevdf_init(&w);
    eevdf_add(&w, 10, EEVDF_NICE_0);
    eevdf_add(&w, 11, EEVDF_NICE_0 / 2);
    for (int i = 0; i < TICKS; i++) {
        int idx = eevdf_pick(&w);
        if (idx < 0) break;
        eevdf_charge(&w, idx, 1);
    }
    double ratio = (double)w.tasks[0].ran / (double)(w.tasks[1].ran ? w.tasks[1].ran : 1);
    printf("  weighted ratio (2:1 expected): %.3f\n", ratio);
    CHECK(ratio > 1.9 && ratio < 2.1, "weights of 2:1 split CPU 2:1 within 5%");

    printf("\n=== test_eevdf: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
