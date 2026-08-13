/* tools/probe/hedge_verify.c
 * Prove the Tailslayer DRAM-hedge prefetch is emitted before EVERY class of
 * memory load the JIT generates:
 *   - module global (RIP-relative)          -> emit_prefetch_rip
 *   - stack local                          -> emit_prefetch_rbp
 *   - struct member / union                -> emit_prefetch_rax_off
 *   - array element (INDEX)                -> emit_prefetch_rdi
 *   - pointer dereference (DEREF)          -> emit_prefetch_rax
 * wubu_hedge_prefetch_count counts every prefetchnta emitted. Running each
 * load-bearing construct via hc_eval must bump it. A pure-constant construct
 * must NOT.
 */
#include <stdio.h>
#include "holyc.h"
#include "holyc_codegen.h"

extern unsigned long wubu_hedge_prefetch_count;

/* returns prefetches emitted by running src */
static unsigned long run_evals(const char *const *srcs, int n) {
    unsigned long a = wubu_hedge_prefetch_count;
    for (int i = 0; i < n; i++) hc_eval(srcs[i]);
    return wubu_hedge_prefetch_count - a;
}

int main(void) {
    const char *global[] = { "int g=42; g+1;", "int g=42; int x=g+1; x;" };
    const char *local[]  = { "int f(int n){ int x=n*2; return x+n; } f(14);" };
    const char *member[] = { "struct S{ int a; int b; }; struct S s; s.a=42; s.a;",
                             "struct A{ int x; }; struct B{ struct A a; }; struct B b; b.a.x=42; b.a.x;" };
    const char *index[]  = { "int a[5]; a[2]=42; a[2];", "\"hello\"[0];" };
    const char *deref[]  = { "int x=42; int* p=&x; *p;",
                             "struct S{ int a; }; struct S s; s.a=42; struct S* q=&s; q->a;" };

    unsigned long pg = run_evals(global, 2);
    unsigned long pl = run_evals(local, 1);
    unsigned long pm = run_evals(member, 2);
    unsigned long pi = run_evals(index, 2);
    unsigned long pd = run_evals(deref, 2);

    const char *const consts[] = { "2+3*4-6;", "42;", "(1)?42:0;" };
    unsigned long pc = run_evals(consts, 3);

    printf("hedge prefetch count by load class:\n");
    printf("  global RIP : %lu\n", pg);
    printf("  stack local: %lu\n", pl);
    printf("  member/union: %lu\n", pm);
    printf("  array index: %lu\n", pi);
    printf("  ptr deref  : %lu\n", pd);
    printf("  constants  : %lu (expect 0)\n", pc);

    int pass = (pg > 0 && pl > 0 && pm > 0 && pi > 0 && pd > 0 && pc == 0);
    printf("RESULT: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
