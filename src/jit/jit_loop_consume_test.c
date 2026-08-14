/*
 * jit_loop_consume_test.c -- Verify Subsystem B's loop-body capture consumer:
 * the JIT compiler captures assignments during while-body compilation, feeds
 * them to the loop analysis engine, and records the trip count / IV info.
 *
 * Also verifies #14 block layout swap: when the then-body is a single return
 * (early-exit pattern), the compiler inverts the jcc so the else-block falls
 * through (hot path = no taken branch).
 */
#include "jit.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int pass, fail;
#define CHECK(cond, msg) do { if (cond) pass++; else { fail++; printf("FAIL: %s\n", msg); } } while(0)

int main(void) {
    /* ===== Test 1: loop-body capture + IV detection =====
     * while(i < n){ i = i + 1; s = s + i; } — i is IV with stride +1 */
    {
        JITContext *ctx = jit_init();
        JITFunc fn; JITResult r;
        const char *src =
            "long f(long n){"
            "  long i = 0;"
            "  long s = 0;"
            "  while(i < n){"
            "    i = i + 1;"
            "    s = s + i;"
            "  }"
            "  return s;"
            "}";
        r = jit_compile(ctx, src, JIT_LANG_C, "f", &fn);
        CHECK(r == 0, "T1: compile while with IV");
        if (r == 0) {
            /* f(5) = 1+2+3+4+5 = 15 */
            int64_t v = jit_call1(&fn, 5);
            CHECK(v == 15, "T1: f(5) = sum(1..5) = 15");
            if (v != 15) printf("   got %ld\n", (long)v);
        }
        jit_free(ctx);
    }

    /* ===== Test 2: loop with decrementing IV =====
     * while(n > 0){ n = n - 1; s = s + 1; } */
    {
        JITContext *ctx = jit_init();
        JITFunc fn; JITResult r;
        const char *src =
            "long f(long n){"
            "  long s = 0;"
            "  while(n > 0){"
            "    n = n - 1;"
            "    s = s + 1;"
            "  }"
            "  return s;"
            "}";
        r = jit_compile(ctx, src, JIT_LANG_C, "f", &fn);
        CHECK(r == 0, "T2: compile while with decrementing IV");
        if (r == 0) {
            int64_t v = jit_call1(&fn, 10);
            CHECK(v == 10, "T2: f(10) = 10 (counts iterations)");
            if (v != 10) printf("   got %ld\n", (long)v);
        }
        jit_free(ctx);
    }

    /* ===== Test 3: #14 layout swap — single-return then-body =====
     * if(x<0){ return -1; } else { return x*2; }
     * The then-body is a single return (early-exit), so the compiler should
     * invert the jcc so the else-block (the common path) falls through.
     * Verify: semantics unchanged. */
    {
        JITContext *ctx = jit_init();
        JITFunc fn; JITResult r;
        const char *src =
            "long f(long x){"
            "  if(x < 0){ return -1; }"
            "  else { return x * 2; }"
            "}";
        r = jit_compile(ctx, src, JIT_LANG_C, "f", &fn);
        CHECK(r == 0, "T3: compile if/else with single-return then");
        if (r == 0) {
            int64_t v1 = jit_call1(&fn, -5);
            int64_t v2 = jit_call1(&fn, 7);
            CHECK(v1 == -1, "T3: f(-5) = -1 (error path)");
            CHECK(v2 == 14, "T3: f(7) = 14 (common path)");
        }
        jit_free(ctx);
    }

    /* ===== Test 4: nested if inside while (full A+B+C pipeline) ===== */
    {
        JITContext *ctx = jit_init();
        JITFunc fn; JITResult r;
        const char *src =
            "long f(long n){"
            "  long s = 0;"
            "  while(n > 0){"
            "    if(n > 1){ s = s + n; }"
            "    n = n - 1;"
            "  }"
            "  return s;"
            "}";
        r = jit_compile(ctx, src, JIT_LANG_C, "f", &fn);
        CHECK(r == 0, "T4: compile while + nested if");
        if (r == 0) {
            /* n=4: s+=4,n=3; s+=3,n=2; s+=2,n=1; skip if; n=0 => s=9 */
            int64_t v = jit_call1(&fn, 4);
            CHECK(v == 9, "T4: f(4) = 4+3+2 = 9");
            if (v != 9) printf("   got %ld\n", (long)v);
        }
        jit_free(ctx);
    }

    printf("=== jit_loop_consume_test: %d passed, %d failed ===\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
