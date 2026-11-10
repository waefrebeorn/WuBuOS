/*
 * test_heap_stress.c -- devil's-advocate churn test for the kernel heap
 * (memory.c: hash buckets + free list + coalescing + red-zone canaries).
 *
 * Cases: alloc/free churn with live-pattern verification, red-zone canaries,
 * realloc grow/shrink, double-free, free(NULL), alloc(0), size-boundary
 * allocations around MEM_HEAP_HASH_SIZE, and the post-churn leak check
 * (mem_used == 0) plus mem_validate / mem_validate_all.
 *
 * Build (hosted):
 *   gcc -O2 -g -Wall -Isrc/kernel -Isrc/runtime src/kernel/memory.c \
 *       src/kernel/test/test_heap_stress.c -o build/test_heap_stress
 */
#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HEAP_BYTES (64u * 1024u * 1024u)
#define MAX_LIVE   512
#define OPS        20000

static int g_fail;
#define CHECK(c, msg) do { if (!(c)) { printf("  FAIL: %s\n", msg); g_fail++; } } while (0)

static unsigned g_seed;
static unsigned rnd(void) { g_seed = g_seed * 1103515245u + 12345u; return (g_seed >> 8); }

static void fill(unsigned char *p, size_t n, unsigned seed) {
    for (size_t i = 0; i < n; i++) p[i] = (unsigned char)(seed + i * 7);
}
static long g_first_bad = -1;
static int verify(const unsigned char *p, size_t n, unsigned seed) {
    for (size_t i = 0; i < n; i++) if (p[i] != (unsigned char)(seed + i * 7)) {
        if (g_first_bad < 0) g_first_bad = (long)i;
        return 0;
    }
    return 1;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    g_seed = 0x12345678u;
    if (mem_init(HEAP_BYTES) != 0) { printf("FAIL: mem_init\n"); return 1; }

    void *live[MAX_LIVE];
    size_t sz[MAX_LIVE];
    unsigned sd[MAX_LIVE];
    int nlive = 0;

    long allocs = 0, frees = 0;
    char hist[48][96];
    int hn = 0;
#define HIST(...) do { snprintf(hist[hn % 48], sizeof hist[0], __VA_ARGS__); hn++; } while (0)

    for (int op = 0; op < OPS; op++) {
        int do_alloc = (rnd() % 3) != 0;                 /* ~2/3 alloc */
        if (do_alloc && nlive < MAX_LIVE) {
            /* mix of small (hash), boundary, and large (free-list) sizes */
            size_t s;
            switch (rnd() % 4) {
                case 0: s = 1 + rnd() % 64; break;        /* tiny */
                case 1: s = 400 + rnd() % 200; break;     /* around the hash boundary */
                case 2: s = 1024 + rnd() % 4096; break;   /* large */
                default: s = 1 + rnd() % 256; break;
            }
            unsigned char *p = (unsigned char *)mem_alloc(s);
            if (!p) { printf("FAIL op %d: alloc(%zu) NULL\n", op, s); g_fail++; break; }
            unsigned seed = rnd();
            HIST("op %d alloc %zu -> %p", op, s, (void *)p);
            fill(p, s, seed);
            CHECK(mem_check_redzones(p) == 0, "red zones intact after alloc");
            if (g_fail) break;
            live[nlive] = p; sz[nlive] = s; sd[nlive] = seed; nlive++;
            allocs++;
        } else if (nlive > 0) {
            int k = rnd() % nlive;
            void *p = live[k];
            CHECK(mem_check_redzones(p) == 0, "red zones intact before free");
            if (g_fail) break;
            mem_free(p);
            HIST("op %d free %p (size %zu)", op, p, sz[k]);
            live[k] = live[nlive - 1]; sz[k] = sz[nlive - 1]; sd[k] = sd[nlive - 1];
            nlive--;
            frees++;
        }
        /* every live block must still hold its pattern and its red zones */
        for (int i = 0; i < nlive; i++) {
            if (!verify((unsigned char *)live[i], sz[i], sd[i])) {
                printf("FAIL op %d: pattern corrupted on block %d (%zu bytes) ptr=%p first_bad_off=%ld\n",
                       op, i, sz[i], live[i], g_first_bad);
                for (int j = 0; j < nlive; j++)
                    printf("   live[%d] ptr=%p size=%zu %s\n", j, live[j], sz[j],
                           j == i ? "<-- corrupted" : "");
                for (int h = 0; h < 48 && h < hn; h++)
                    printf("   hist: %s\n", hist[(hn - 1 - h) % 48]);
                g_fail++; break;
            }
            if (mem_check_redzones(live[i]) != 0) {
                printf("FAIL op %d: red zones clobbered on block %d (%zu bytes)\n",
                       op, i, sz[i]);
                g_fail++; break;
            }
        }
        if (g_fail) break;
        /* linear contiguity walk: catches an overlap at the op that creates it */
        long w = mem_debug_walk();
        if (w != 0) {
            printf("FAIL op %d: heap walk desync at block offset %ld\n", op, w);
            for (int h = 0; h < 48 && h < hn; h++)
                printf("   hist: %s\n", hist[(hn - 1 - h) % 48]);
            g_fail++; break;
        }
    }

    /* drain */
    for (int i = 0; i < nlive; i++) mem_free(live[i]);
    nlive = 0;

    CHECK(mem_used() == 0, "mem_used == 0 after draining (no leak)");
    CHECK(mem_validate() == 0, "mem_validate OK");
    {
        int corrupt = mem_validate_all();
        CHECK(corrupt == 0, "mem_validate_all: no corrupt blocks");
        size_t used = 0, freeb = 0; int nu = 0, nf = 0;
        mem_walk_stats(&used, &freeb, &nu, &nf);
        printf("  stats: used=%zu free=%zu n_used=%d n_free=%d coalesce_pairs=%d\n",
               used, freeb, nu, nf, mem_validate_coalescing());
    }

    /* edge cases */
    void *p0 = mem_alloc(0);
    CHECK(p0 != NULL, "alloc(0) returns a usable pointer");
    mem_free(p0);
    printf("  after alloc(0)/free: used=%zu\n", mem_used());
    mem_free(NULL);                                     /* no-op */
    void *pd = mem_alloc(64);
    CHECK(pd != NULL, "alloc for double-free");
    mem_free(pd);
    printf("  after alloc(64)/free: used=%zu\n", mem_used());
    mem_free(pd);                                       /* double free: must be a safe no-op */

    /* realloc grow + shrink + content preservation */
    unsigned char *r = (unsigned char *)mem_alloc(32);
    CHECK(r != NULL, "realloc source alloc");
    fill(r, 32, 777u);
    r = (unsigned char *)mem_realloc(r, 4096);
    CHECK(r != NULL, "realloc grow");
    CHECK(verify(r, 32, 777u), "realloc preserves content on grow");
    r = (unsigned char *)mem_realloc(r, 16);
    CHECK(r != NULL, "realloc shrink");
    CHECK(verify(r, 16, 777u), "realloc preserves content on shrink");
    mem_free(r);
    CHECK(mem_used() == 0, "mem_used == 0 after edge cases");

    mem_shutdown();
    printf("heap stress: %ld allocs, %ld frees, %d ops -> %s\n",
           allocs, frees, OPS, g_fail ? "FAIL" : "PASS");
    return g_fail ? 1 : 0;
}
