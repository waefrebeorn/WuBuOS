/* JIT code auditor: compiles representative patterns and dumps disasm
 * so we can spot optimization opportunities and bugs. */
#include "jit.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void dump(const char *label, const char *src) {
    JITContext *ctx = jit_init();
    JITFunc fn; JITResult r;
    r = jit_compile(ctx, src, JIT_LANG_C, "f", &fn);
    if (r != 0) { printf("=== %s: COMPILE FAIL rc=%d ===\n\n", label, r); jit_free(ctx); return; }
    printf("=== %s (%zu bytes) ===\n", label, fn.code_size);
    jit_func_disasm(&fn, stdout);
    printf("\n");
    jit_free(ctx);
}

int main(void) {
    /* Canonical patterns to audit */
    dump("if/else fusion", "long f(long x){ if(x>0){ return x; } else { return -x; } }");
    dump("while IV", "long f(long n){ long i=0; long s=0; while(i<n){ s=s+i; i=i+1; } return s; }");
    dump("div-by-const", "long f(long x){ return x/7; }");
    dump("mul-by-const", "long f(long x){ return x*5; }");
    dump("branchless abs", "long f(long x){ if(x<0){ x=-x; } return x; }");
    dump("struct access", "struct S{ long x; U8 c; }; long f(struct S* p){ return p->x + p->c; }");
    dump("nested if", "long f(long a, long b){ if(a>0){ if(b>0){ return 1; } } return 0; }");
    dump("loop invariant", "long f(long n){ long c=n*10; long s=0; while(s<c){ s=s+1; } return s; }");
    return 0;
}
