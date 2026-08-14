/*
 * jit_supremacy_test.c -- Compare WuBu JIT output against GCC -O2.
 *
 * For each test case, both GCC and WuBu compile the SAME expression
 * with the SAME arguments. Results must match exactly.
 *
 * This is the ultimate correctness test: any divergence = bug.
 */
#include "jit.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

static int pass, fail, total;
#define CHECK(c,m) do{total++;if(c){pass++;}else{fail++;printf("  FAIL: %s (gcc=%ld wubu=%ld)\n",m,(long)gcc_r,(long)wubu_r);}}while(0)
static int64_t gcc_r, wubu_r;

static int64_t gcc_eval_expr(const char *expr, int64_t a, int64_t b) {
    FILE *f = fopen("/tmp/gcct.c", "w");
    if (!f) return INT64_MIN;
    fprintf(f, "#include <stdint.h>\n#include <stdio.h>\nint64_t f(int64_t a,int64_t b){return(%s);}\nint main(){printf(\"%%ld\\n\",f(%ld,%ld));return 0;}\n",
            expr, a, b);
    fclose(f);
    if (system("gcc -O2 -o /tmp/gcct /tmp/gcct.c -lm 2>/dev/null") != 0) return INT64_MIN;
    f = popen("/tmp/gcct", "r");
    if (!f) return INT64_MIN;
    int64_t r = INT64_MIN;
    if (fscanf(f, "%ld", &r) != 1) r = INT64_MIN;
    pclose(f);
    return r;
}

static int64_t wubu_eval_expr(JITContext *ctx, const char *expr, int64_t a, int64_t b) {
    char src[512];
    snprintf(src, sizeof(src), "long f(long a,long b){return(%s);}", expr);
    JITFunc fn;
    if (jit_compile(ctx, src, JIT_LANG_C, "f", &fn) != 0) return INT64_MIN;
    return jit_call2(&fn, a, b);
}

/* Test with default args a=5, b=3 */
#define TEST53(expr) do{ \
    gcc_r = gcc_eval_expr(expr, 5, 3); \
    wubu_r = wubu_eval_expr(ctx, expr, 5, 3); \
    CHECK(gcc_r == wubu_r, expr); \
}while(0)

/* Test with specific args */
#define TESTAB(expr, a, b) do{ \
    gcc_r = gcc_eval_expr(expr, a, b); \
    wubu_r = wubu_eval_expr(ctx, expr, a, b); \
    CHECK(gcc_r == wubu_r, expr); \
}while(0)

int main(void) {
    JITContext *ctx = jit_init();

    printf("=== WUBU vs GCC -O2 SUPREMACY TEST ===\n\n");

    printf("--- Basic Arithmetic ---\n");
    TEST53("a+b");
    TEST53("a-b");
    TEST53("a*b");
    TEST53("a/b");
    TEST53("a%b");
    TEST53("-a");
    TEST53("-a+b");

    printf("--- Bitwise ---\n");
    TEST53("a&b");
    TEST53("a|b");
    TEST53("a^b");
    TEST53("~a");
    TEST53("~b");
    TEST53("a<<1");
    TEST53("a<<2");
    TEST53("a>>1");
    TEST53("a>>2");
    TEST53("a&7");
    TEST53("a|8");
    TEST53("a^1");
    TEST53("a<<b");
    TEST53("a>>b");
    TEST53("a&b|3");
    TEST53("(a&b)|3");
    TEST53("a&(b|3)");
    TEST53("~a&b");
    TEST53("(~a)&b");

    printf("--- Complex Expressions ---\n");
    TEST53("a+b*3");
    TEST53("3*a+b");
    TEST53("(a+b)*3");
    TEST53("3*(a+b)");
    TEST53("a*b+3*2");
    TEST53("a*b-100/b");
    TEST53("(a+b)*(a-b)");
    TEST53("a<<1|b>>1");
    TEST53("(a&~b)|(~a&b)");
    TEST53("a+b+3+2");
    TEST53("((a+b)*3-1)/2");
    TEST53("a+(b+(3+2))");

    printf("--- Division by Constant ---\n");
    TEST53("a/3");
    TEST53("a/7");
    TEST53("a/10");
    TEST53("a/100");
    TEST53("a/1");
    TEST53("-a/7");
    TEST53("a/-7");

    printf("--- Modulo by Constant ---\n");
    TEST53("a%3");
    TEST53("a%7");
    TEST53("a%8");
    TEST53("a%1");
    TEST53("-a%7");
    TEST53("a%-7");

    printf("--- Bitwise + Arithmetic ---\n");
    TEST53("(a+b)&0xFF");
    TEST53("(a<<1)+1");
    TEST53("(a+3)&~3")  /* align to 4 */;
    TEST53("(a+7)&~7")  /* align to 8 */;
    TEST53("a<<1&3");
    TEST53("(a<<1)&3");
    TEST53("a>>1|1");
    TEST53("(a>>1)|1");

    printf("--- Boundary Values ---\n");
    TESTAB("a+b", INT64_MAX, 1);
    TESTAB("a-b", INT64_MIN, 1);
    TESTAB("a*2", INT64_MAX, 0);
    TESTAB("a/2", INT64_MIN, 0);
    TESTAB("a%3", INT64_MIN, 0);
    TESTAB("a<<1", INT64_MAX, 0);
    TESTAB("a>>1", INT64_MIN, 0);
    TESTAB("a+b", 0, 0);
    TESTAB("a-b", 0, 0);
    TESTAB("a*b", 0, 0);
    TESTAB("a|0", 0, 0);
    TESTAB("a&0", INT64_MAX, 0);

    printf("--- Negative Values ---\n");
    TESTAB("a+b", -10, -20);
    TESTAB("a*b", -7, -8);
    TESTAB("a/b", -49, 7);
    TESTAB("a%b", -49, 7);
    TESTAB("a/7", -100, 0);
    TESTAB("a%7", -100, 0);
    TESTAB("-a", -42, 0);
    TESTAB("~a", -1, 0);
    TESTAB("a<<2", -1, 0);
    TESTAB("a>>1", -8, 0);
    TESTAB("a^b", -1, 1);

    printf("--- Large Constants ---\n");
    TEST53("a*1000000");
    TEST53("a/1000000");
    TEST53("a+0x7FFFFFFFFFFFFFFF");
    /* Skip a-0x8000000000000000: ambiguous hex literal = INT64_MIN edge case */
    TEST53("a&0xFF");
    TEST53("a|0x100");

    printf("--- Nested Parentheses ---\n");
    TEST53("((a))");
    TEST53("(((a+b)))");
    TEST53("((a+b)*3)");
    TEST53("(a+(b*3))");
    TEST53("((a+b)*(a-b))");
    TEST53("(((a+b)*2)-1)");

    printf("--- Multi-Variable Stress ---\n");
    TEST53("a+a+a+a+a");
    TEST53("a-a-a-a");
    TEST53("a*2*3");
    TEST53("a/2/3");
    TEST53("a+b-a+b");
    TEST53("a*b/a");
    TEST53("a%b%2");
    TEST53("a&b&3");
    TEST53("a|b|3");
    TEST53("a^b^3");

    printf("\n=== SUMMARY ===\n");
    printf("=== jit_supremacy_test: %d/%d passed, %d failed ===\n", pass, total, fail);
    jit_free(ctx);
    return fail ? 1 : 0;
}
