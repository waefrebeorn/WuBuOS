#include <stdio.h>
#include "holyd.h"
#include "holyd_codegen.h"
int main(void) {
    printf("1: %lld (expect 3)\n", (long long)hc_eval("int add(int a,int b){return a+b;} add(1,2);"));
    printf("2: %lld (expect 42)\n", (long long)hc_eval("int add(int a,int b){return a+b;} int (*op)(int,int)=add; op(20,22);"));
    printf("3: %lld (expect 5)\n", (long long)hc_eval("int f(int x){int (*sq)(int)=f; return x;} f(5);"));
    return 0;
}
