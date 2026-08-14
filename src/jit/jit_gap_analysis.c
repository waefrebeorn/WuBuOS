/*
 * jit_gap_analysis.c -- Comprehensive self-assessment of compiler capabilities.
 * Compiles every pattern we support and reports what's missing.
 */
#include "jit.h"
#include <stdio.h>
#include <string.h>
#include <stdint.h>

static int pass, fail;
#define CHECK(c,m) do{if(c)pass++;else{fail++;printf("  MISSING: %s\n",m);}}while(0)

static int64_t run1(JITContext *ctx, const char *src, int64_t a) {
    JITFunc fn; JITResult r = jit_compile(ctx, src, JIT_LANG_C, "f", &fn);
    if (r != 0) return INT64_MIN;
    return jit_call1(&fn, a);
}
static int64_t run2(JITContext *ctx, const char *src, int64_t a, int64_t b) {
    JITFunc fn; JITResult r = jit_compile(ctx, src, JIT_LANG_C, "f", &fn);
    if (r != 0) return INT64_MIN;
    return jit_call2(&fn, a, b);
}
static int compiles(JITContext *ctx, const char *src) {
    JITFunc fn; JITResult r = jit_compile(ctx, src, JIT_LANG_C, "f", &fn);
    return (r == 0);
}

int main(void) {
    JITContext *ctx = jit_init();
    int64_t v;

    printf("=== CAPABILITY MAP: WHAT WE KNOW ===\n\n");

    printf("--- Arithmetic ---\n");
    CHECK(run1(ctx,"long f(long x){ return x+1; }",5)==6, "add constant");
    CHECK(run1(ctx,"long f(long x){ return x-1; }",5)==4, "sub constant");
    CHECK(run1(ctx,"long f(long x){ return x*5; }",7)==35, "mul constant");
    CHECK(run1(ctx,"long f(long x){ return x/7; }",49)==7, "div constant");
    CHECK(run1(ctx,"long f(long x){ return -x; }",5)==-5, "negation");
    CHECK(run1(ctx,"long f(long x){ return x*0; }",42)==0, "mul by zero");

    printf("--- Comparisons ---\n");
    CHECK(run1(ctx,"long f(long x){ return x==0; }",0)==1, "eq zero");
    CHECK(run1(ctx,"long f(long x){ return x!=0; }",5)==1, "neq zero");
    CHECK(run1(ctx,"long f(long x){ return x<10; }",5)==1, "lt");
    CHECK(run1(ctx,"long f(long x){ return x>10; }",15)==1, "gt");
    CHECK(run1(ctx,"long f(long x){ return x<=10; }",10)==1, "le");
    CHECK(run1(ctx,"long f(long x){ return x>=10; }",10)==1, "ge");

    printf("--- Control Flow ---\n");
    CHECK(run1(ctx,"long f(long x){ if(x>0) return 1; return 0; }",5)==1, "if");
    CHECK(run1(ctx,"long f(long x){ if(x>0) return 1; else return 0; }",5)==1, "if/else");
    CHECK(run1(ctx,"long f(long n){ long s=0; while(n>0){ s=s+1; n=n-1; } return s; }",10)==10, "while");
    CHECK(run1(ctx,"long f(long x){ if(x>0){ if(x>1){ return 3; } } return 0; }",5)==3, "nested if");

    printf("--- Variables ---\n");
    CHECK(run1(ctx,"long f(long x){ long y=x+1; return y; }",5)==6, "local var");
    CHECK(run2(ctx,"long f(long a, long b){ return a+b; }",3,4)==7, "two args");
    CHECK(run1(ctx,"long f(long x){ long a=x+1; long b=a+1; return b; }",5)==7, "chained locals");

    printf("--- Structs ---\n");
    CHECK(compiles(ctx,"struct S{ long x; }; long f(struct S* p){ return p->x; }"), "struct decl");
    CHECK(compiles(ctx,"struct S{ long x; U8 c; }; long f(struct S* p){ return p->x+p->c; }"), "struct member access");

    printf("\n=== CAPABILITY MAP: WHAT WE DON'T KNOW (GAPS) ===\n\n");

    printf("--- Missing Language Features ---\n");
    CHECK(compiles(ctx,"long f(long x){ return x%3; }"), "modulo operator");
    CHECK(compiles(ctx,"long f(long x){ return x&1; }"), "bitwise AND");
    CHECK(compiles(ctx,"long f(long x){ return x|1; }"), "bitwise OR");
    CHECK(compiles(ctx,"long f(long x){ return x^1; }"), "bitwise XOR");
    CHECK(compiles(ctx,"long f(long x){ return x<<2; }"), "left shift");
    CHECK(compiles(ctx,"long f(long x){ return x>>2; }"), "right shift");
    CHECK(compiles(ctx,"long f(long x){ return ~x; }"), "bitwise NOT");
    CHECK(compiles(ctx,"long f(long x){ return !x; }"), "logical NOT");
    CHECK(compiles(ctx,"long f(long x){ long arr[10]; arr[0]=x; return arr[0]; }"), "array indexing");
    CHECK(compiles(ctx,"long f(long x){ return x>0 ? 1 : 0; }"), "ternary operator");
    CHECK(compiles(ctx,"void f(long x){ }"), "void return type");
    CHECK(compiles(ctx,"long f(){ return 42; }"), "no arguments");
    CHECK(compiles(ctx,"long f(long a, long b, long c, long d, long e, long f){ return a+b+c+d+e+f; }"), "six args");

    printf("--- Missing Optimizations ---\n");
    CHECK(run1(ctx,"long f(long x){ return x*3; }",7)==21, "mul by 3 (lea)");
    CHECK(run1(ctx,"long f(long x){ return x*9; }",7)==63, "mul by 9 (lea)");
    CHECK(run1(ctx,"long f(long x){ return x*15; }",7)==105, "mul by 15 (lea)");
    CHECK(run1(ctx,"long f(long x){ return x/3; }",21)==7, "div by 3 (magic)");
    CHECK(run1(ctx,"long f(long x){ return x/5; }",50)==10, "div by 5 (magic)");
    CHECK(run1(ctx,"long f(long x){ return x/9; }",81)==9, "div by 9 (magic)");
    CHECK(run1(ctx,"long f(long x){ return x%3; }",7)==1, "mod by constant");
    CHECK(run1(ctx,"long f(long x){ return x%8; }",15)==7, "mod by power of 2");
    CHECK(run1(ctx,"long f(long x){ return x&7; }",15)==7, "and (mask)");
    CHECK(run1(ctx,"long f(long x){ return (x+3)&~3; }",5)==8, "align up");

    printf("--- Missing Advanced Features ---\n");
    CHECK(compiles(ctx,"long f(long x){ long* p=&x; return *p; }"), "address-of / pointer deref");
    CHECK(compiles(ctx,"long f(long x){ for(long i=0;i<x;i=i+1){ } return x; }"), "for loop");
    CHECK(compiles(ctx,"long f(long x){ switch(x){ case 1: return 10; default: return 0; } }"), "switch");
    CHECK(compiles(ctx,"long f(long x){ return x+=1; }"), "compound assign (+=)");
    CHECK(compiles(ctx,"long f(long x){ return ++x; }"), "pre-increment");
    CHECK(compiles(ctx,"long f(long x){ long a[4]={1,2,3,4}; return a[2]; }"), "array literal");

    printf("\n=== SUMMARY: %d/%d capabilities present, %d gaps found ===\n", pass, pass+fail, fail);
    jit_free(ctx);
    return fail ? 1 : 0;
}
