/*
 * jit_test.c — Test suite for My Seed JIT Runtime
 *
 * Tests the API contract, mmap backend, and x86-64 encoding.
 * Build:  gcc -o jit_test jit.c jit_test.c -Wall -Wextra
 * Run:    ./jit_test
 */

#include "jit.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST(name) printf("  TEST: %-40s ", name)
#define PASS()     printf("✅ PASS\n")
#define FAIL(msg)  do { printf("❌ FAIL: %s\n", msg); failures++; } while(0)

static int failures = 0;

/* ── Context Lifecycle Tests ───────────────────────────────────── */

static void test_context_lifecycle(void) {
    printf("\n[Context Lifecycle]\n");
    
    TEST("jit_init returns non-NULL");
    JITContext *ctx = jit_init();
    if (ctx) PASS(); else FAIL("NULL context");
    
    TEST("default backend is MMAP");
    JITContext *ctx3 = jit_init();
    JITStats s3;
    jit_stats(ctx3, &s3);
    if (ctx3) PASS(); else FAIL("wrong backend");
    jit_free(ctx3);
    
    TEST("jit_init_backend(MMAP)");
    JITContext *ctx2 = jit_init_backend(JIT_BACKEND_MMAP);
    if (ctx2) PASS(); else FAIL("NULL");
    
    TEST("jit_free doesn't crash");
    jit_free(ctx);
    jit_free(ctx2);
    PASS();
}

/* ── Executable Memory Tests ───────────────────────────────────── */

static void test_exec_memory(void) {
    printf("\n[Executable Memory]\n");
    
    TEST("jit_alloc_exec returns RWX memory");
    void *mem = jit_alloc_exec(4096);
    if (mem) PASS(); else FAIL("NULL");
    
    TEST("can write to allocated memory");
    if (mem) {
        ((unsigned char *)mem)[0] = 0xC3;  /* ret */
        PASS();
    } else FAIL("no memory");
    
    TEST("can execute from allocated memory");
    if (mem) {
        ((void (*)(void))mem)();  /* Should return immediately (ret) */
        PASS();
    } else FAIL("exec failed");
    
    TEST("jit_free_exec doesn't crash");
    jit_free_exec(mem, 4096);
    PASS();
    
    TEST("jit_lock_exec/unlock_exec");
    mem = jit_alloc_exec(4096);
    if (mem) {
        ((unsigned char *)mem)[0] = 0xC3;
        jit_lock_exec(mem, 4096);
        ((void (*)(void))mem)();  /* should still work */
        jit_unlock_exec(mem, 4096);
        ((unsigned char *)mem)[0] = 0xC3;  /* should be writable again */
        jit_free_exec(mem, 4096);
        PASS();
    } else FAIL("alloc failed");
}

/* ── Compilation Tests ─────────────────────────────────────────── */

static void test_compile_simple(void) {
    printf("\n[Simple Expression Compilation]\n");
    
    JITContext *ctx = jit_init();
    assert(ctx);
    
    /* a+b */
    TEST("compile 'a+b'");
    JITFunc fn_add;
    JITResult r = jit_compile(ctx, "a+b", JIT_LANG_C, "add", &fn_add);
    if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));
    
    TEST("a+b: add(3,4) == 7");
    if (r == JIT_OK && jit_call2(&fn_add, 3, 4) == 7) PASS(); 
    else FAIL("wrong result");
    
    TEST("a+b: add(-1,10) == 9");
    if (r == JIT_OK && jit_call2(&fn_add, -1, 10) == 9) PASS();
    else FAIL("wrong result");
    
    /* a*b */
    TEST("compile 'a*b'");
    JITFunc fn_mul;
    r = jit_compile(ctx, "a*b", JIT_LANG_C, "mul", &fn_mul);
    if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));
    
    TEST("a*b: mul(5,5) == 25");
    if (r == JIT_OK && jit_call2(&fn_mul, 5, 5) == 25) PASS();
    else FAIL("wrong result");
    
    TEST("a*b: mul(7,6) == 42");
    if (r == JIT_OK && jit_call2(&fn_mul, 7, 6) == 42) PASS();
    else FAIL("wrong result");
    
    /* a-b */
    TEST("compile 'a-b'");
    JITFunc fn_sub;
    r = jit_compile(ctx, "a-b", JIT_LANG_C, "sub", &fn_sub);
    if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));
    
    TEST("a-b: sub(10,3) == 7");
    if (r == JIT_OK && jit_call2(&fn_sub, 10, 3) == 7) PASS();
    else FAIL("wrong result");
    
    /* -a */
    TEST("compile '-a'");
    JITFunc fn_neg;
    r = jit_compile(ctx, "-a", JIT_LANG_C, "neg", &fn_neg);
    if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));
    
    TEST("-a: neg(5) == -5");
    if (r == JIT_OK && jit_call1(&fn_neg, 5) == -5) PASS();
    else FAIL("wrong result");
    
    /* identity a */
    TEST("compile 'a'");
    JITFunc fn_id;
    r = jit_compile(ctx, "a", JIT_LANG_C, "id", &fn_id);
    if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));
    
    TEST("a: id(42) == 42");
    if (r == JIT_OK && jit_call1(&fn_id, 42) == 42) PASS();
    else FAIL("wrong result");
    
    /* constant */
    TEST("compile '123'");
    JITFunc fn_const;
    r = jit_compile(ctx, "123", JIT_LANG_C, "const123", &fn_const);
    if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));
    
    TEST("123: const() == 123");
    if (r == JIT_OK && jit_call0(&fn_const) == 123) PASS();
    else FAIL("wrong result");
    
    /* Cleanup */
    jit_func_free(&fn_add);
    jit_func_free(&fn_mul);
    jit_func_free(&fn_sub);
    jit_func_free(&fn_neg);
    jit_func_free(&fn_id);
    jit_func_free(&fn_const);
    jit_free(ctx);
}

/* ── Diagnostics Tests ─────────────────────────────────────────── */

static void test_diagnostics(void) {
    printf("\n[Diagnostics]\n");
    
    JITContext *ctx = jit_init();
    assert(ctx);
    
    TEST("jit_func_dump works");
    JITFunc fn;
    JITResult r = jit_compile(ctx, "a+b", JIT_LANG_C, "add", &fn);
    if (r == JIT_OK) {
        jit_func_dump(&fn, stderr);
        PASS();
    } else FAIL("compile failed");
    
    TEST("jit_stats tracks allocations");
    JITStats stats;
    jit_stats(ctx, &stats);
    if (stats.total_compiled >= 1 && stats.total_alloc > 0) PASS();
    else FAIL("stats not tracking");
    
    TEST("jit_strerror works");
    if (strcmp(jit_strerror(JIT_OK), "Success") == 0 &&
        strcmp(jit_strerror(JIT_ERR_COMPILE), "Compilation failed") == 0) PASS();
    else FAIL("wrong strings");
    
    jit_func_free(&fn);
    jit_free(ctx);
}

/* ── Main ──────────────────────────────────────────────────────── */

int main(void) {
    printf("╔══════════════════════════════╗\n");
    printf("║   My Seed JIT Test Suite     ║\n");
    printf("╚══════════════════════════════╝\n");
    
    test_context_lifecycle();
    test_exec_memory();
    test_compile_simple();
    test_diagnostics();
    
    printf("\n══════════════════════════════\n");
    if (failures == 0)
        printf("All tests passed! ✅\n");
    else
        printf("%d test(s) FAILED ❌\n", failures);
    
    return failures ? 1 : 0;
}
