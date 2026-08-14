/*
 * jit_branch_test.c  --  Verify #4 cmp/jcc macro-fusion + #2 flag-dependency.
 *
 * When an `if`/`while` condition is a comparison (a<b, a==0, ...), the codegen
 * used to emit  cmp; setcc; movzx; test; jcc  — the cmp already set the flags,
 * then setcc/movzx re-encoded the result as 0/1, then test re-derived ZF for the
 * branch. The fusion drops the setcc+movzx+test and branches DIRECTLY on the
 * compare's flags with the inverted cc.
 *
 * Discriminators:
 *   - correctness: every fused if/else/while returns the C-correct value
 *   - byte-level: a compare-conditioned if/while emits NO setcc (0F 90..9F)
 *     and NO test rax,rax (48 85 C0) after the compare — only the cmp + jcc.
 *   - robustness: a NON-comparison condition (bare `if(a)`) still works (uses
 *     test rax,rax; jne) — fusion only engages for real comparisons.
 */
#include "jit.h"
#include "wubu_x86.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>

static int pass, fail;
#define CHECK(cond, msg) do { \
    if (cond) { pass++; } \
    else { fail++; printf("FAIL: %s\n", msg); } \
} while (0)

static int has_setcc(unsigned char *p, size_t n) {
    for (size_t i = 0; i + 1 < n; i++)
        if (p[i] == 0x0F && (p[i+1] & 0xF0) == 0x90) return 1;
    return 0;
}
static int has_test_raxrax(unsigned char *p, size_t n) {
    for (size_t i = 0; i + 2 < n; i++)
        if (p[i] == 0x48 && p[i+1] == 0x85 && p[i+2] == 0xC0) return 1;
    return 0;
}
static int has_cmp(unsigned char *p, size_t n) {
    for (size_t i = 0; i + 1 < n; i++)
        if (p[i] == 0x48 && p[i+1] == 0x39) return 1;
    return 0;
}

int main(void) {
    JITContext *ctx = jit_init();
    JITFunc fn; JITResult r; int64_t v;

    /* --- correctness: if / else / nested / while with fused compares --- */
    struct { const char *e; int64_t a, b, exp; int na; } t[] = {
        {"long f(long a,long b){ if(a<b){ return 1; } return 0; }", 3, 5, 1, 2},
        {"long f(long a,long b){ if(a<b){ return 1; } return 0; }", 5, 3, 0, 2},
        {"long f(long a,long b){ if(a==b){ return 7; } else { return 8; } }", 4, 4, 7, 2},
        {"long f(long a,long b){ if(a==b){ return 7; } else { return 8; } }", 4, 5, 8, 2},
        {"long f(long a,long b){ if(a!=b){ return 9; } return 0; }", 4, 5, 9, 2},
        {"long f(long a,long b){ if(a>=b){ return 3; } return 0; }", 5, 3, 3, 2},
        {"long f(long a,long b){ if(a>b){ return 8; } return 0; }", 5, 3, 8, 2},
        {"long f(long a,long b){ if(a<=b){ return 5; } return 0; }", 3, 3, 5, 2},
        {"long f(long a){ if(a==0){ return 1; } return 0; }", 0, 0, 1, 1},
        {"long f(long a){ if(a==0){ return 1; } return 0; }", 5, 0, 0, 1},
        {"long f(long a,long b){ if(a<b){ if(a>0){ return 9; } return 1; } return 0; }", 3, 5, 9, 2},
        {"long f(long a,long b){ if(a<b){ if(a>0){ return 9; } return 1; } return 0; }", -3, 5, 1, 2},
        {"long f(long n){ long s=0; while(n>0){ s=s+n; n=n-1; } return s; }", 5, 0, 15, 1},
        {"long f(long n){ long s=0; while(n>0){ s=s+n; n=n-1; } return s; }", 10, 0, 55, 1},
        /* non-comparison condition: MUST NOT fuse (uses test), still correct */
        {"long f(long a){ if(a){ return 2; } return 0; }", 5, 0, 2, 1},
        {"long f(long a){ if(a){ return 2; } return 0; }", 0, 0, 0, 1},
    };
    for (int i = 0; i < 16; i++) {
        r = jit_compile(ctx, t[i].e, JIT_LANG_C, "f", &fn);
        CHECK(r == 0, t[i].e);
        if (r == 0) {
            v = (t[i].na == 1) ? jit_call1(&fn, t[i].a) : jit_call2(&fn, t[i].a, t[i].b);
            CHECK(v == t[i].exp, t[i].e);
        }
    }

    /* --- byte-level: fused compare-condition if has NO setcc / NO test --- */
    r = jit_compile(ctx, "long f(long a,long b){ if(a<b){ return 1; } return 0; }",
                    JIT_LANG_C, "f", &fn);
    CHECK(r == 0 && has_cmp((unsigned char*)fn.code, fn.code_size), "fused if has cmp");
    CHECK(r == 0 && !has_setcc((unsigned char*)fn.code, fn.code_size), "fused if has NO setcc");
    CHECK(r == 0 && !has_test_raxrax((unsigned char*)fn.code, fn.code_size), "fused if has NO test");

    /* non-comparison if(a): must KEEP the test (it's the branch generator) */
    r = jit_compile(ctx, "long f(long a){ if(a){ return 2; } return 0; }",
                    JIT_LANG_C, "f", &fn);
    CHECK(r == 0 && has_test_raxrax((unsigned char*)fn.code, fn.code_size), "non-cmp if keeps test");

    /* fused while: no setcc in the loop condition */
    r = jit_compile(ctx, "long f(long n){ long s=0; while(n>0){ s=s+n; n=n-1; } return s; }",
                    JIT_LANG_C, "f", &fn);
    CHECK(r == 0 && !has_setcc((unsigned char*)fn.code, fn.code_size), "fused while has NO setcc");

    /* #15 branch alignment: the loop back-edge target is 16-byte aligned, and
     * the f(5)=15 result is preserved. */
    if (r == 0) {
        unsigned char *p = (unsigned char*)fn.code;
        int aligned = 0;
        for (size_t i = 0; i + 4 < fn.code_size; i++) {
            if (p[i] == 0xE9) {
                int32_t d = p[i+1]|(p[i+2]<<8)|(p[i+3]<<16)|((int32_t)p[i+4]<<24);
                size_t tgt = i + 5 + (size_t)d;
                if (tgt < fn.code_size && (tgt & 15) == 0) aligned = 1;
            }
        }
        CHECK(aligned, "while loop back-edge target is 16-byte aligned");
        CHECK(jit_call1(&fn, 5) == 15, "aligned while still computes f(5)=15");
    }

    jit_free(ctx);

    /* --- #10 / #22 / #1 encoder bytes (movnti, adc/sbb/clc/stc, mem-add) --- */
    {
        uint8_t buf[64]; Wx86Enc e; wx86_enc_init(&e, buf, sizeof(buf));
        /* movnti [rbp-8], rax = 48 0f c3 45 f8 (non-temporal store, #22) */
        e.pos = 0; wx86_movnti_mem_reg(&e, WREG_RBP, -8, WREG_RAX);
        CHECK(e.pos == 5 && memcmp(buf, (unsigned char[]){0x48,0x0f,0xc3,0x45,0xf8},5)==0,
              "movnti [rbp-8],rax encoding");
        /* adc rax,rcx = 48 11 c8 ; sbb r10,r11 = 4d 19 da (#1 carry chain) */
        e.pos = 0; wx86_adc_reg_reg(&e, WREG_RAX, WREG_RCX);
        CHECK(e.pos == 3 && memcmp(buf, (unsigned char[]){0x48,0x11,0xc8},3)==0,
              "adc rax,rcx encoding");
        e.pos = 0; wx86_sbb_reg_reg(&e, WREG_R10, WREG_R11);
        CHECK(e.pos == 3 && memcmp(buf, (unsigned char[]){0x4d,0x19,0xda},3)==0,
              "sbb r10,r11 encoding");
        /* clc=f8, stc=f9 (carry flag control for the multiword prologue) */
        e.pos = 0; wx86_clc(&e); CHECK(e.pos==1 && buf[0]==0xf8, "clc encoding");
        e.pos = 0; wx86_stc(&e); CHECK(e.pos==1 && buf[0]==0xf9, "stc encoding");
        /* add rax,[rbp-24] = 48 03 45 e8 (#10 memory-operand fusion) */
        e.pos = 0; wx86_add_rax_mem(&e, WREG_RBP, -24);
        CHECK(e.pos == 4 && memcmp(buf, (unsigned char[]){0x48,0x03,0x45,0xe8},4)==0,
              "add rax,[rbp-24] encoding");
        /* add rax,[r10] (disp0, r10) = 49 03 02 */
        e.pos = 0; wx86_add_rax_mem(&e, WREG_R10, 0);
        CHECK(e.pos == 3 && memcmp(buf, (unsigned char[]){0x49,0x03,0x02},3)==0,
              "add rax,[r10] encoding");
    }

    printf("=== jit_branch_test: %d passed, %d failed ===\n", pass, fail);
    return fail == 0 ? 0 : 1;
}
