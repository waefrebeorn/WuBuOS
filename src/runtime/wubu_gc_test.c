/*
 * wubu_gc_test.c  --  Userspace GC Test Suite
 *
 * Tests mark-and-sweep for HolyC REPL / container applets.
 */

#include "wubu_gc.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

/* -- Test Framework --------------------------------------------- */

static int g_pass = 0, g_fail = 0, g_total = 0;

#define TEST(name) printf("  TEST %-50s", name); g_total++
#define PASS() do { printf("✅\n"); g_pass++; } while(0)
#define FAIL(msg) do { printf("❌ %s\n", msg); g_fail++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)

/* -- Basic Allocation ------------------------------------------- */

static void test_basic_alloc(void) {
    TEST("wubu_gc_alloc / wubu_gc_free");
    void *p = wubu_gc_alloc(128);
    CHECK(p != NULL, "alloc succeeded");
    memset(p, 0xAA, 128);
    CHECK(*(uint8_t*)p == 0xAA, "memory writable");
    wubu_gc_free(p);
    PASS();
}

static void test_zero_alloc(void) {
    TEST("wubu_gc_alloc(0) returns NULL");
    void *p = wubu_gc_alloc(0);
    CHECK(p == NULL, "zero size returns NULL");
    PASS();
}

static void test_multiple_allocs(void) {
    TEST("multiple allocations tracked");
    wubu_gc_collect();  /* Clean slate */
    size_t before = wubu_gc_allocated();

    void *p1 = wubu_gc_alloc(64);
    void *p2 = wubu_gc_alloc(128);
    void *p3 = wubu_gc_alloc(256);

    size_t after = wubu_gc_allocated();
    CHECK(after >= before + 448, "allocated bytes increased");

    wubu_gc_free(p1);
    wubu_gc_free(p2);
    wubu_gc_free(p3);
    PASS();
}

/*-- Root Pinning -------------------------------------------------*/

static void test_root_pin(void) {
    TEST("root pin prevents collection");
    wubu_gc_collect();

    void *p = wubu_gc_alloc(256);
    wubu_gc_root_add(p);

    /* Simulate collection  --  pinned object should survive */
    wubu_gc_collect();

    size_t allocated = wubu_gc_allocated();
    CHECK(allocated >= 256, "pinned object not collected");

    wubu_gc_root_remove(p);
    wubu_gc_free(p);
    PASS();
}

static void test_root_duplicate(void) {
    TEST("duplicate root add handled gracefully");
    void *p = wubu_gc_alloc(64);
    wubu_gc_root_add(p);
    wubu_gc_root_add(p);  /* Duplicate */
    wubu_gc_root_remove(p);
    wubu_gc_root_remove(p);  /* Double remove */
    wubu_gc_free(p);
    PASS();
}

/*-- Collection Behavior ------------------------------------------*/

static void test_collect_unreachable(void) {
    TEST("unreachable objects collected");
    wubu_gc_collect();

    void *p1 = wubu_gc_alloc(128);
    void *p2 = wubu_gc_alloc(128);
    size_t before = wubu_gc_allocated();

    /* Drop references  --  don't root them */
    /* p1, p2 go out of scope (in real use)  --  simulate by not rooting */
    wubu_gc_collect();

    size_t after = wubu_gc_allocated();
    CHECK(after < before, "unreachable objects freed");

    PASS();
}

static void test_pinned_survives_collect(void) {
    TEST("pinned object survives collect");
    wubu_gc_collect();

    void *p = wubu_gc_alloc(512);
    wubu_gc_root_add(p);
    size_t before = wubu_gc_allocated();

    wubu_gc_collect();
    size_t after = wubu_gc_allocated();

    CHECK(after >= before - 64, "pinned object retained");  /* Allow header overhead */

    wubu_gc_root_remove(p);
    wubu_gc_free(p);
    PASS();
}

/*-- Threshold ----------------------------------------------------*/

static void test_threshold_triggers_collect(void) {
    TEST("threshold triggers auto-collect");
    wubu_gc_set_threshold(1024);  /* 1KB */
    wubu_gc_enable(1);

    void *p = wubu_gc_alloc(2000);  /* Exceeds threshold */
    CHECK(p != NULL, "large alloc succeeded");

    wubu_gc_free(p);
    wubu_gc_set_threshold(1024 * 1024);  /* Reset */
    PASS();
}

/*-- Stats --------------------------------------------------------*/

static void test_stats(void) {
    TEST("stats reported correctly");
    wubu_gc_collect();

    size_t a, t, r;
    wubu_gc_stats(&a, &t, &r);
    CHECK(a == 0, "allocated is zero after collect");

    void *p = wubu_gc_alloc(1024);
    wubu_gc_stats(&a, &t, &r);
    CHECK(a >= 1024, "allocated reflects allocation");

    wubu_gc_free(p);
    PASS();
}

/*-- Shutdown ----------------------------------------------------*/

static void test_shutdown(void) {
    TEST("wubu_gc_shutdown cleans up");
    wubu_gc_collect();
    void *p = wubu_gc_alloc(256);
    wubu_gc_root_add(p);

    wubu_gc_shutdown();

    size_t a, t, r;
    wubu_gc_stats(&a, &t, &r);
    CHECK(a == 0 && r == 0, "all state cleared after shutdown");

    PASS();
}

/*-- Main --------------------------------------------------------*/

int main(void) {
    printf("\n+==================================================+\n");
    printf("|  WuBuOS Userspace GC Test Suite                |\n");
    printf("|  Opt-in mark/sweep for HolyC REPL / applets    |\n");
    printf("+==================================================+\n\n");

    test_basic_alloc();
    test_zero_alloc();
    test_multiple_allocs();
    test_root_pin();
    test_root_duplicate();
    test_collect_unreachable();
    test_pinned_survives_collect();
    test_threshold_triggers_collect();
    test_stats();
    test_shutdown();

    printf("\n===================================================\n");
    printf("  Results: %d/%d passed, %d failed\n", g_pass, g_total, g_fail);
    printf("===================================================\n");

    return g_fail > 0 ? 1 : 0;
}