/*
 * memory_test.c  --  Test suite for My Seed Kernel Memory Subsystem
 */

#include "memory.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST(name) printf("  TEST: %-40s ", name)
#define PASS()     printf("✅ PASS\n")
#define FAIL(msg)  do { printf("❌ FAIL: %s\n", msg); failures++; } while(0)

static int failures = 0;

static void test_init_shutdown(void) {
    printf("\n[Init / Shutdown]\n");
    
    TEST("mem_init(1MB) returns 0");
    if (mem_init(1024 * 1024) == 0) PASS(); else FAIL("init failed");
    
    TEST("mem_heap_ctrl returns non-NULL");
    if (mem_heap_ctrl()) PASS(); else FAIL("NULL");
    
    TEST("mem_used() == 0 after init");
    if (mem_used() == 0) PASS(); else FAIL("used not 0");
    
    TEST("mem_available() > 0");
    if (mem_available() > 0) PASS(); else FAIL("no memory");
    
    TEST("mem_validate() == 0");
    if (mem_validate() == 0) PASS(); else FAIL("corrupt");
    
    TEST("mem_shutdown doesn't crash");
    mem_shutdown();
    PASS();
}

static void test_alloc_free(void) {
    printf("\n[Alloc / Free]\n");
    
    mem_init(4 * 1024 * 1024);  /* 4MB heap */
    
    TEST("mem_alloc(64) returns non-NULL");
    void *p1 = mem_alloc(64);
    if (p1) PASS(); else FAIL("NULL");
    
    TEST("mem_alloc(0) returns non-NULL (treated as 1)");
    void *p2 = mem_alloc(0);
    if (p2) PASS(); else FAIL("NULL");
    
    TEST("mem_alloc(4096) returns non-NULL");
    void *p3 = mem_alloc(4096);
    if (p3) PASS(); else FAIL("NULL");
    
    TEST("mem_alloc(65536) returns non-NULL");
    void *p4 = mem_alloc(65536);
    if (p4) PASS(); else FAIL("NULL");
    
    TEST("mem_used() > 0 after allocations");
    size_t used = mem_used();
    if (used > 0) PASS(); else FAIL("used==0");
    
    TEST("mem_free doesn't crash on valid pointers");
    mem_free(p1);
    mem_free(p2);
    mem_free(p3);
    mem_free(p4);
    PASS();
    
    TEST("mem_free(NULL) is no-op");
    mem_free(NULL);
    PASS();
}

static void test_calloc(void) {
    printf("\n[Calloc]\n");
    
    TEST("mem_calloc(10, 64) returns zeroed memory");
    int *p = (int *)mem_calloc(10, 64);
    if (p) {
        int all_zero = 1;
        for (int i = 0; i < 160; i++) {  /* 10*64/4 = 160 ints */
            if (p[i] != 0) { all_zero = 0; break; }
        }
        if (all_zero) PASS(); else FAIL("not zeroed");
        mem_free(p);
    } else FAIL("NULL");
}

static void test_realloc(void) {
    printf("\n[Realloc]\n");
    
    TEST("mem_realloc: grow allocation");
    char *p = (char *)mem_alloc(32);
    if (p) {
        memcpy(p, "hello world", 12);
        char *p2 = (char *)mem_realloc(p, 64);
        if (p2 && memcmp(p2, "hello world", 12) == 0) PASS();
        else FAIL("data lost");
        mem_free(p2);
    } else FAIL("alloc failed");
    
    TEST("mem_realloc(NULL, 64) == mem_alloc(64)");
    void *p3 = mem_realloc(NULL, 64);
    if (p3) PASS(); else FAIL("NULL");
    mem_free(p3);
    
    TEST("mem_realloc(p, 0) == mem_free(p)");
    void *p4 = mem_alloc(64);
    void *p5 = mem_realloc(p4, 0);
    if (p5 == NULL) PASS(); else FAIL("not NULL");
}

static void test_stress(void) {
    printf("\n[Stress Test]\n");
    
    TEST("1000 alloc/free cycles");
    void *ptrs[1000];
    int ok = 1;
    for (int i = 0; i < 1000; i++) {
        ptrs[i] = mem_alloc(16 + (i % 128) * 8);
        if (!ptrs[i]) { ok = 0; break; }
    }
    for (int i = 0; i < 1000; i++) {
        mem_free(ptrs[i]);
    }
    if (ok) PASS(); else FAIL("allocation failed");
    
    TEST("varying sizes 1-8192");
    ok = 1;
    void *v[100];
    for (int i = 0; i < 100; i++) {
        size_t sz = 1 << (i % 14);  /* 1 to 8192 */
        v[i] = mem_alloc(sz);
        if (!v[i]) { ok = 0; break; }
        /* Write a single marker byte only  --  no memset past header */
        ((char *)v[i])[0] = 0xAA;
        if (sz > 1) ((char *)v[i])[sz-1] = 0xBB;
    }
    for (int i = 0; i < 100; i++) {
        mem_free(v[i]);
    }
    if (ok) PASS(); else FAIL("failed");
}

/* -- Red Zone Tests ----------------------------------------------- */

static void test_redzones(void) {
    printf("\n[Red Zones / Canaries]\n");
    
    mem_shutdown();
    mem_init(4 * 1024 * 1024);
    
    TEST("mem_check_redzones returns 0 for valid allocation");
    void *p = mem_alloc(64);
    if (p && mem_check_redzones(p) == 0) PASS();
    else FAIL("red zone check failed on valid block");
    
    TEST("red zones detect buffer overflow (front corruption)");
    /* Intentionally corrupt front canary */
    uint32_t *front = (uint32_t *)((uint8_t *)p - MEM_RED_ZONE_SIZE);
    uint32_t saved = front[0];
    front[0] = 0x41414141;  /* Overwrite front canary */
    if (mem_check_redzones(p) != 0) PASS();
    else FAIL("corrupted front canary not detected");
    front[0] = saved;  /* Restore for clean free */
    
    TEST("red zones detect buffer underflow (back corruption)");
    /* Get back canary pointer using the block size */
    CMemUsed *mu = (CMemUsed *)((uint8_t *)p - MEM_RED_ZONE_SIZE - offsetof(CMemUsed, start));
    size_t user_bytes = mu->size - offsetof(CMemUsed, start) - 2 * MEM_RED_ZONE_SIZE;
    uint32_t *back = (uint32_t *)((uint8_t *)p + user_bytes);
    uint32_t saved_back = back[0];
    back[0] = 0x42424242;
    if (mem_check_redzones(p) != 0) PASS();
    else FAIL("corrupted back canary not detected");
    back[0] = saved_back;
    
    mem_free(p);
    
    TEST("mem_validate_all returns 0 when heap is clean");
    void *a = mem_alloc(32);
    void *b = mem_alloc(128);
    if (mem_validate_all() == 0) PASS();
    else FAIL("validate_all reports false corruption");
    mem_free(a);
    mem_free(b);
}

/* -- Heap Walk Tests ---------------------------------------------- */

static int walk_count;
static void walk_counter(void *ptr, size_t size, void *ctx) {
    (void)ptr; (void)ctx;
    walk_count++;
    (void)size;
}

static void test_heap_walk(void) {
    printf("\n[Heap Walk]\n");
    
    mem_shutdown();
    mem_init(4 * 1024 * 1024);
    
    TEST("mem_walk with no allocations returns 0 blocks");
    walk_count = 0;
    mem_walk(walk_counter, NULL);
    if (walk_count == 0) PASS();
    else FAIL("walk found blocks in empty heap");
    
    TEST("mem_walk counts allocated blocks");
    void *p1 = mem_alloc(32);
    void *p2 = mem_alloc(64);
    void *p3 = mem_alloc(128);
    walk_count = 0;
    mem_walk(walk_counter, NULL);
    if (walk_count == 3) PASS();
    else FAIL("wrong block count");
    mem_free(p1); mem_free(p2); mem_free(p3);
    
    TEST("mem_walk_stats reports correct totals");
    mem_alloc(32);
    mem_alloc(64);
    size_t used, freed;
    int nu, nf;
    mem_walk_stats(&used, &freed, &nu, &nf);
    if (nu >= 2 && used > 0) PASS();
    else FAIL("wrong stats");
    
    TEST("mem_debug_dump doesn't crash");
    mem_debug_dump();
    PASS();
}

/* -- Bloom Filter Scan Tests ------------------------------------- */

static int bloom_count;
static void bloom_counter(void *block, uint32_t sig, void *ctx) {
    (void)block; (void)sig; (void)ctx;
    bloom_count++;
}

static void test_bloom_scan(void) {
    printf("\n[Bloom Filter Scan]\n");
    
    mem_shutdown();
    mem_init(4 * 1024 * 1024);
    
    TEST("bloom scan finds MEM_USED_SIGNATURE blocks");
    void *p1 = mem_alloc(64);
    void *p2 = mem_alloc(128);
    bloom_count = 0;
    int n = mem_bloom_scan(MEM_USED_SIGNATURE, bloom_counter, NULL);
    (void)p1; (void)p2;
    if (n >= 2) PASS();
    else FAIL("bloom scan missed blocks");
    
    TEST("bloom scan finds MEM_UNUSED_SIGNATURE blocks");
    mem_free(p1);
    bloom_count = 0;
    n = mem_bloom_scan(MEM_UNUSED_SIGNATURE, bloom_counter, NULL);
    if (n >= 1) PASS();
    else FAIL("bloom scan missed free blocks");
    
    mem_free(p2);
}

int main(void) {
    printf("+==============================+\n");
    printf("|  My Seed Memory Test Suite   |\n");
    printf("+==============================+\n");
    
    test_init_shutdown();
    test_alloc_free();
    test_calloc();
    test_realloc();
    test_stress();
    test_redzones();
    test_heap_walk();
    test_bloom_scan();
    
    mem_shutdown();
    
    printf("\n==============================\n");
    if (failures == 0)
        printf("All tests passed! ✅\n");
    else
        printf("%d test(s) FAILED ❌\n", failures);
    
    return failures ? 1 : 0;
}
