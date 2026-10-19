/* test_timer_wheel.c -- wake accuracy across the full 2^32-tick span. */
#include "timer_wheel.h"
#include <stdio.h>
#include <stdlib.h>

static int checks, fails;
#define CHECK(c, m) do { checks++; if (!(c)) { printf("  [FAIL] %s\n", m); fails++; } } while (0)

int main(void)
{
    tw_t w;
    tw_init(&w);

    /* Deltas spanning 2 ticks to 2^32 ticks. */
    const uint64_t deltas[] = {2, 63, 64, 4095, 4096, 262143, 262144,
                               16777215ull, 16777216ull, 4294967296ull};
    const int n = (int)(sizeof deltas / sizeof deltas[0]);
    for (int i = 0; i < n; i++)
        CHECK(tw_add(&w, (uint64_t)(i + 1), w.now + deltas[i]) == 0, "entry added");

    /* Walk the clock to each expiry and check we wake on the exact tick. */
    uint64_t out[8];
    int idx = 0;
    uint64_t furthest = w.now + 4294967296ull;
    int fired_ok = 0, fired_early = 0, fired_late = 0;

    while (w.now <= furthest) {
        int f = tw_advance(&w, out, 8);
        for (int k = 0; k < f; k++) {
            uint64_t id = out[k];
            uint64_t want = deltas[id - 1];
            if (w.now == want) fired_ok++;
            else if (w.now < want) fired_early++;
            else fired_late++;
            idx++;
        }
        if (idx == n) break;
    }

    printf("  fired=%d exact=%d early=%d late=%d cascades=%llu\n",
           idx, fired_ok, fired_early, fired_late, (unsigned long long)w.cascades);
    CHECK(idx == n, "every entry fired");
    CHECK(fired_early == 0, "no entry woke early");
    CHECK(fired_late == 0, "no entry woke late");
    CHECK(fired_ok == n, "every wake landed on its exact tick");
    CHECK(w.cascades > 0, "coarse entries cascaded as the clock advanced");
    CHECK(w.pending == 0, "wheel is empty at the end");

    /* Ordering: the earliest expiry is reported. */
    tw_t o;
    tw_init(&o);
    tw_add(&o, 1, 500);
    tw_add(&o, 2, 100);
    tw_add(&o, 3, 900);
    CHECK(tw_next(&o) == 100, "tw_next reports the earliest expiry");

    printf("\n=== test_timer_wheel: %d checks, %d failures ===\n", checks, fails);
    return fails ? 1 : 0;
}
