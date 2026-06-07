/*
 * memory_test.c — Test suite for My Seed Kernel Memory Subsystem
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
        /* Write a single marker byte only — no memset past header */
        ((char *)v[i])[0] = 0xAA;
        if (sz > 1) ((char *)v[i])[sz-1] = 0xBB;
    }
    for (int i = 0; i < 100; i++) {
        mem_free(v[i]);
    }
    if (ok) PASS(); else FAIL("failed");
}

int main(void) {
    printf("╔══════════════════════════════╗\n");
    printf("║  My Seed Memory Test Suite   ║\n");
    printf("╚══════════════════════════════╝\n");
    
    test_init_shutdown();
    test_alloc_free();
    test_calloc();
    test_realloc();
    test_stress();
    
    mem_shutdown();
    
    printf("\n══════════════════════════════\n");
    if (failures == 0)
        printf("All tests passed! ✅\n");
    else
        printf("%d test(s) FAILED ❌\n", failures);
    
    return failures ? 1 : 0;
}
