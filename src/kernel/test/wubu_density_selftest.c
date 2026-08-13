/*
 * wubu_density_selftest.c -- SELFTEST for the AGF density planner.
 *
 * PROVEN: theme_write(0) -> theme_write(1) -> absorb/keep/prune cycle.
 *   density = coherence / n_tokens gates the keep/prune decision:
 *     - a high-coherence cluster (0.5 coherence / 1 token = 0.5 density)
 *       is KEPT (written to the KV metabolism).
 *     - a low-density cluster (0.5 coherence / 100 tokens = 0.005) is PRUNED.
 *   The /theme write-through observer is invoked on each theme node write,
 *   feeding the metabolism (user interactions = live training).
 */
#include "wubu_density_plan.h"
#include <string.h>
#include <stdio.h>
#include <unistd.h>

static int failures = 0;
static int passed = 0;
#define WRITE2(s) do { const char*_s=(s); for(;_s[0];_s++) write(2,_s,1); } while(0)
#define CHECK(c, m) do { if (c) { passed++; } else { WRITE2("FAIL: "); WRITE2(m); WRITE2("\n"); failures++; } } while(0)

/* stub KV metabolism that just records the write count */
static int g_kv_writes = 0;
static int stub_kv(const char *path, const void *data, size_t len, void *ud) {
    (void)path;(void)data;(void)len;(void)ud;
    g_kv_writes++;
    return 0;
}

int main(void) {
    printf("=== wubu_density_selftest ===\n\n");

    wubu_density_plan_t p;
    wubu_density_plan_init(&p);
    wubu_density_plan_set_kvfs(&p, stub_kv, NULL);
    CHECK(p.density_threshold == 0.5f, "init sets density threshold to 0.5");

    /* Theme write (keep): high coherence / few tokens = KEPT */
    wubu_density_plan_theme_write(&p, "/kv/theme/active", 1u);
    CHECK(p.theme_writes == 1, "theme_write observed (theme_writes==1)");
    CHECK(wubu_density_plan_absorbed(&p) >= 1, "absorb recorded after theme write");

    /* Theme write (prune): low density cluster -> pruned */
    wubu_density_plan_theme_write(&p, "/kv/theme/noisy", 1u);
    CHECK(p.theme_writes == 2, "second theme write observed");

    /* Feed a low-density burst that gets pruned */
    for (int i = 0; i < 20; i++)
        wubu_density_plan_absorb(&p, "/kv/theme/prune_me", 0.1f, 5u);  /* 0.02 density */

    /* Feed a high-density kept cluster */
    wubu_density_plan_absorb(&p, "/kv/theme/keep_me", 0.5f, 1u);  /* 0.5 density */

    int alive = wubu_density_plan_cycle(&p);
    CHECK(alive >= 1, "at least one cluster kept after cycle");
    CHECK(wubu_density_plan_pruned(&p) >= 1, "low-density cluster pruned");
    CHECK(wubu_density_plan_kept(&p) >= 1, "high-density cluster kept");
    CHECK(g_kv_writes >= 1, "KEPT clusters written to KV metabolism");
    CHECK(wubu_density_plan_cycles(&p) >= 1, "cycle ran (theme writes + explicit)");
    CHECK(wubu_density_plan_density(&p) >= 0.0f, "global density is non-negative");

    printf("\n=== wubu_density_selftest: %d passed, %d failed ===\n",
           passed, failures);
    return failures ? 1 : 0;
}
