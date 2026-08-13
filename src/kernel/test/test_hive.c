/*
 * test_hive.c -- wubu_hive (C11 luddite hive) unit tests.
 *
 * Verifies the three-way tradeoff the hand-drawn diagram spells out:
 *   - block structure: elements land in fixed blocks (vector locality)
 *   - skipfield: erase marks + iteration jumps dead slots
 *   - freelist: insert reuses freed slots without growing capacity
 * Plus stable pointers, multi-block growth, iterator erase, and zero leaks
 * (counting allocator).
 */
#include "wubu_hive.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_allocs, g_frees;
static int g_fail;

static void *t_alloc(size_t n) { g_allocs++; return malloc(n); }
static void  t_free(void *p)   { g_frees++;  free(p); }

#define CHECK(cond, name) do {                                          \
    if (!(cond)) { printf("  FAIL: %s\n", name); g_fail++; }            \
    else        { printf("  ok:   %s\n", name); }                       \
} while (0)

static size_t iter_count(wubu_hive_t *h)
{
    wubu_hive_iter_t it;
    size_t n = 0;
    for (void *e = wubu_hive_first(h, &it); e; e = wubu_hive_next(h, &it)) n++;
    return n;
}

int main(void)
{
    printf("== wubu_hive tests ==\n");

    /* ---- 1. empty lifecycle, no leaks ---- */
    g_allocs = g_frees = 0;
    wubu_hive_t *h = wubu_hive_new(4, t_alloc, t_free);
    CHECK(h != NULL, "hive_new(4)");
    CHECK(wubu_hive_empty(h), "empty hive is empty");
    CHECK(wubu_hive_size(h) == 0, "size == 0");
    CHECK(wubu_hive_capacity(h) == 0, "capacity == 0");
    CHECK(wubu_hive_block_count(h) == 0, "block_count == 0");
    wubu_hive_destroy(h);
    CHECK(g_allocs == g_frees, "destroy frees every allocation");

    /* ---- 2. block growth: cap-4 fills 2 blocks, iteration spans them ---- */
    h = wubu_hive_new(4, t_alloc, t_free);
    const char *in[8] = { "A","B","C","D","E","F","G","H" };
    for (int i = 0; i < 8; i++)
        CHECK(wubu_hive_insert(h, (void *)in[i]) == (void *)in[i],
              "insert returns the stable pointer");
    CHECK(wubu_hive_size(h) == 8, "size == 8");
    CHECK(wubu_hive_capacity(h) == 8, "capacity == 8 (2 blocks x 4)");
    CHECK(wubu_hive_block_count(h) == 2, "block_count == 2");

    /* iteration order: block 0 then block 1, in slot order */
    {
        wubu_hive_iter_t it;
        const char *expect[] = { "A","B","C","D","E","F","G","H" };
        int i = 0, ok = 1;
        for (void *e = wubu_hive_first(h, &it); e; e = wubu_hive_next(h, &it)) {
            if ((const char *)e != expect[i]) ok = 0;
            i++;
        }
        CHECK(ok && i == 8, "iteration yields all 8 in block/slot order");
    }

    /* ---- 3. erase by pointer: mark + freelist push, no moves ---- */
    CHECK(wubu_hive_erase(h, (void *)"C") == (void *)"C", "erase returns elem");
    CHECK(wubu_hive_erase(h, (void *)"F") == (void *)"F", "erase returns elem 2");
    CHECK(wubu_hive_size(h) == 6, "size == 6 after two erases");
    CHECK(wubu_hive_capacity(h) == 8, "capacity unchanged (no block freed)");
    CHECK(wubu_hive_erase(h, (void *)"nope") == NULL, "erase unknown -> NULL");
    CHECK(wubu_hive_size(h) == 6, "size unchanged after failed erase");

    /* iteration jumps the dead slots */
    {
        wubu_hive_iter_t it;
        const char *expect[] = { "A","B","D","E","G","H" };
        int i = 0, ok = 1;
        for (void *e = wubu_hive_first(h, &it); e; e = wubu_hive_next(h, &it)) {
            if ((const char *)e != expect[i]) ok = 0;
            i++;
        }
        CHECK(ok && i == 6, "iteration jumps skipped slots (C and F gone)");
    }

    /* ---- 4. insert reuses the freed slots (freelist) ---- */
    CHECK(wubu_hive_insert(h, (void *)"X") == (void *)"X", "insert X");
    CHECK(wubu_hive_insert(h, (void *)"Y") == (void *)"Y", "insert Y");
    CHECK(wubu_hive_size(h) == 8, "size == 8 again");
    CHECK(wubu_hive_capacity(h) == 8, "capacity STILL 8 (slots reused, no new block)");
    CHECK(wubu_hive_block_count(h) == 2, "block_count still 2");
    {
        wubu_hive_iter_t it;
        /* X/Y landed in the freed slots of block 0 (C) and block 1 (F) */
        const char *expect[] = { "A","B","X","D","E","Y","G","H" };
        int i = 0, ok = 1;
        for (void *e = wubu_hive_first(h, &it); e; e = wubu_hive_next(h, &it)) {
            if ((const char *)e != expect[i]) ok = 0;
            i++;
        }
        CHECK(ok && i == 8, "X and Y reused the erased slots");
    }

    /* ---- 5. iterator erase (erase_at), all elements ---- */
    {
        wubu_hive_iter_t it;
        int erased = 0;
        for (void *e = wubu_hive_first(h, &it); e; e = wubu_hive_next(h, &it)) {
            if (erased++ % 2 == 0) CHECK(wubu_hive_erase_at(h, &it) == 1,
                                         "erase_at on current slot");
        }
    }
    CHECK(wubu_hive_size(h) == 4, "size == 4 after iterator erases");
    CHECK(wubu_hive_empty(h) == 0, "not empty");

    /* ---- 6. erase all -> empty, then reuse works ---- */
    {
        wubu_hive_iter_t it;
        void *e = wubu_hive_first(h, &it);
        while (e) {
            wubu_hive_erase_at(h, &it);
            e = wubu_hive_next(h, &it);
        }
    }
    CHECK(wubu_hive_empty(h), "empty after erasing everything");
    CHECK(wubu_hive_capacity(h) == 8, "capacity retained after empty");
    CHECK(wubu_hive_insert(h, (void *)"Z") != NULL, "reuse works after empty");
    CHECK(wubu_hive_size(h) == 1, "size == 1");
    wubu_hive_destroy(h);

    /* ---- 7. scale + leak check: 10k inserts, erase 1/3, iterate ---- */
    g_allocs = g_frees = 0;
    h = wubu_hive_new(64, t_alloc, t_free);
    {
        char *buf = malloc(10001);
        for (int i = 0; i < 10000; i++) wubu_hive_insert(h, buf + i);
        CHECK(wubu_hive_size(h) == 10000, "10k inserts");
        CHECK(wubu_hive_capacity(h) >= 10000, "capacity covers 10k");
        CHECK(wubu_hive_block_count(h) == 157, "157 blocks (ceil 10000/64)");

        for (int i = 0; i < 10000; i += 3)   /* erase 3334 */
            wubu_hive_erase(h, buf + i);
        CHECK(wubu_hive_size(h) == 6666, "6666 live after erase every 3rd");
        CHECK(iter_count(h) == 6666, "iteration count matches live size");

        /* stable pointers: every live pointer is still findable */
        {
            int ok = 1;
            wubu_hive_iter_t it;
            for (void *e = wubu_hive_first(h, &it); e; e = wubu_hive_next(h, &it)) {
                if (wubu_hive_erase(h, e) != e) { ok = 0; break; }
            }
            CHECK(ok && wubu_hive_empty(h), "erase-by-pointer finds every live elem");
        }

        /* refill after mass erase (freelist depth) */
        for (int i = 0; i < 7000; i++) wubu_hive_insert(h, buf + i);
        CHECK(wubu_hive_size(h) == 7000, "refill after mass erase");
        /* the erase-all loop kept all 157 blocks; 7000 fits in the 10048
         * retained slots -> no growth (the hive's capacity is retained) */
        CHECK(wubu_hive_capacity(h) == 10048, "refill reuses retained slots, no new blocks");
        CHECK(wubu_hive_block_count(h) == 157, "block count unchanged after refill");
        free(buf);
    }
    wubu_hive_destroy(h);
    CHECK(g_allocs == g_frees, "no leaks across 10k-scale workload");

    /* ---- 8. guards ---- */
    CHECK(wubu_hive_new(0, NULL, NULL) == NULL, "NULL allocators rejected");
    h = wubu_hive_new(0, t_alloc, t_free);   /* cap 0 -> default 32 */
    CHECK(h && wubu_hive_insert(h, (void *)"A") != NULL, "cap 0 defaults to 32");
    wubu_hive_destroy(h);

    if (g_fail) { printf("\n%d TEST(S) FAILED\n", g_fail); return 1; }
    printf("\nALL HIVE TESTS PASSED\n");
    return 0;
}
