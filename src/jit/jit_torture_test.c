/*
 * jit_torture_test.c -- WuBu-style torture tests.
 *
 * Generates 50+ stress patterns that probe every operator, precedence,
 * nesting depth, and boundary condition. Each test compiles with both
 * WuBu JIT and GCC -O2, and results must match.
 *
 * This is our equivalent of the GCC torture suite, tailored to the
 * Mini-C subset we actually support.
 */
#include "jit.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

static int pass, fail, total;
static int64_t gcc_r, wubu_r;

static int64_t gcc_eval_expr(const char *expr) {
    FILE *f = fopen("/tmp/gcct.c", "w");
    if (!f) return INT64_MIN;
    fprintf(f, "#include <stdint.h>\n#include <stdio.h>\nint main(){printf(\"%%ld\\n\",(int64_t)(%s));return 0;}\n", expr);
    fclose(f);
    if (system("gcc -O2 -o /tmp/gcct /tmp/gcct.c -lm 2>/dev/null") != 0) return INT64_MIN;
    f = popen("/tmp/gcct", "r");
    if (!f) return INT64_MIN;
    int64_t r = INT64_MIN;
    if (fscanf(f, "%ld", &r) != 1) r = INT64_MIN;
    pclose(f);
    return r;
}

static int64_t wubu_eval_expr(JITContext *ctx, const char *expr) {
    char src[512];
    snprintf(src, sizeof(src), "long f(){return(%s);}", expr);
    JITFunc fn;
    if (jit_compile(ctx, src, JIT_LANG_C, "f", &fn) != 0) return INT64_MIN;
    return jit_call0(&fn);
}

#define TEST(expr) do{ \
    gcc_r = gcc_eval_expr(expr); \
    wubu_r = wubu_eval_expr(ctx, expr); \
    total++; \
    if(gcc_r == wubu_r){pass++;} \
    else{fail++;printf("  FAIL: %-40s gcc=%ld wubu=%ld\n",expr,(long)gcc_r,(long)wubu_r);} \
}while(0)

int main(void) {
    JITContext *ctx = jit_init();

    printf("=== WUBU TORTURE TEST ===\n\n");

    printf("--- Constants ---");
    TEST("0");
    TEST("1");
    TEST("-1");
    TEST("42");
    TEST("0xFF");
    TEST("0x7FFFFFFFFFFFFFFF");
    TEST("-9223372036854775807");
    printf("\n");

    printf("--- Arithmetic ---");
    TEST("1+2");
    TEST("10-3");
    TEST("6*7");
    TEST("20/4");
    TEST("17%5");
    TEST("1+2+3+4+5");
    TEST("100-10-20-30");
    TEST("2*3*4");
    TEST("120/2/3/4");
    TEST("100%7%3");
    TEST("1+2*3");
    TEST("(1+2)*3");
    TEST("10-6/2");
    TEST("(10-6)/2");
    TEST("2+3*4+5");
    TEST("100/10/2");
    TEST("(100/10)/2");
    TEST("2*3+4*5");
    TEST("(2*3)+(4*5)");
    TEST("10-3-2");
    TEST("10-(3-2)");
    TEST("100%7");
    TEST("-100%7");
    TEST("100/-7");
    TEST("-100/-7");
    printf("\n");

    printf("--- Bitwise ---");
    TEST("5&3");
    TEST("5|3");
    TEST("5^3");
    TEST("~0");
    TEST("~5");
    TEST("~(-1)");
    TEST("1<<1");
    TEST("1<<10");
    TEST("1024>>1");
    TEST("1024>>10");
    TEST("1<<0");
    TEST("42>>0");
    TEST("0xFF&0x0F");
    TEST("0x0F|0xF0");
    TEST("0xFF^0x0F");
    TEST("5&3|1");
    TEST("5&(3|1)");
    TEST("(5&3)|1");
    TEST("1<<2&3");
    TEST("(1<<2)&3");
    TEST("1<<(2&3)");
    TEST("~1&2");
    TEST("(~1)&2");
    TEST("5^3^1");
    TEST("(5^3)^1");
    TEST("5^(3^1)");
    printf("\n");

    printf("--- Comparison ---");
    TEST("1==1");
    TEST("1==2");
    TEST("1!=2");
    TEST("1!=1");
    TEST("1<2");
    TEST("2<1");
    TEST("1<=1");
    TEST("1<=0");
    TEST("1>0");
    TEST("0>1");
    TEST("1>=1");
    TEST("0>=1");
    printf("\n");

    printf("--- Complex ---");
    TEST("(1+2)*(3+4)");
    TEST("((1+2)*(3+4))+(5*6)");
    TEST("10-(3*2)");
    TEST("(10-(3*2))-(1+1)");
    TEST("1+2*3+4");
    TEST("(1+2)*(3+4)+5*6");
    TEST("20-4-2-1");
    TEST("20-(4-2)-1");
    TEST("20-(4-(2-1))");
    TEST("100/5/2");
        TEST("100%7%3");
    TEST("100%(7%3)");
    TEST("1<<2<<1");
        TEST("8>>2>>1");
        TEST("~0xFF&0x0F");
    TEST("~(0xFF&0x0F)");
    TEST("~0xFF|~0x0F");
    TEST("5^3|1");
    TEST("5^(3|1)");
    TEST("5&3^1");
    TEST("(5&3)^1");
        TEST("(1<2)==1");
    TEST("1<2&3");
    printf("\n");

    printf("--- Boundary ---");
    TEST("0x7FFFFFFFFFFFFFFF+1");
    TEST("-9223372036854775807-1");
    TEST("0x7FFFFFFFFFFFFFFF*2");
    TEST("0x7FFFFFFFFFFFFFFF/2");
    TEST("0x7FFFFFFFFFFFFFFF%7");
    TEST("0x7FFFFFFFFFFFFFFF&0xFF");
    TEST("0x7FFFFFFFFFFFFFFF|0xFF");
    TEST("0x7FFFFFFFFFFFFFFF^0xFF");
    TEST("0x7FFFFFFFFFFFFFFF>>1");
        TEST("0x4000000000000000");  /* 1<<62 */
        printf("\n");

    printf("--- Negative ---");
    TEST("-1+1");
    TEST("-1*1");
    TEST("-1/1");
    TEST("-1%1");
    TEST("-10+5");
    TEST("-10-5");
    TEST("-10*5");
    TEST("-10/5");
    TEST("-10%5");
    TEST("(-10)/(-5)");
    TEST("(-10)%(-5)");
    TEST("-(1+2)");
    TEST("-(1*2)");
    TEST("-(10/3)");
    TEST("-(10%3)");
    TEST("~(-1)");
    TEST("~(-10)");
    TEST("-1<<1");
    TEST("-10>>1");
    printf("\n");

    printf("--- Precedence Stress ---");
    TEST("1+2*3-4/5%6");
    TEST("(1+2)*(3-4)/(5%6)");
    TEST("1<<1+1");
    TEST("(1<<1)+1");
    TEST("1+1<<1");
    TEST("(1+1)<<1");
    TEST("~1+1");
    TEST("(~1)+1");
    TEST("~1*2");
    TEST("(~1)*2");
    TEST("-1+2*3");
    TEST("(-1+2)*3");
    TEST("1-2-3-4-5");
    TEST("1-(2-(3-(4-5)))");
    TEST("100/10/2/5");
        printf("\n");

    printf("\n=== SUMMARY ===\n");
    printf("=== jit_torture_test: %d/%d passed, %d failed ===\n", pass, total, fail);
    jit_free(ctx);
    return fail ? 1 : 0;
}
