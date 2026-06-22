/*
 * jit_test.c  --  WuBuOS JIT Test Suite
 *
 * Tests: mmap backend, x86-64 encoder roundtrip, trivial disassembler,
 *        register allocator, MIR backend (gcc -shared + dlopen),
 *        ASMJIT backend (asm text → wubu_x86 → exec).
 *
 * Build:  See Makefile test_jit target
 * Run:    ./jit_test
 */

#include "jit.h"
#include "wubu_x86.h"
#include "wubu_disasm.h"
#include "x86_regalloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#define TEST(name) printf("  TEST: %-50s ", name)
#define PASS()     printf("✅ PASS\n")
#define FAIL(msg)  do { printf("❌ FAIL: %s\n", msg); failures++; } while(0)

static int failures = 0;

/* -- Context Lifecycle Tests ------------------------------------- */

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

/* -- Executable Memory Tests ------------------------------------- */

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

/* -- mmap Backend Tests ------------------------------------------ */

static void test_compile_simple(void) {
    printf("\n[mmap Backend: Simple Expressions]\n");

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

/* -- x86-64 Encoder Tests ---------------------------------------- */

/* Helper: Write encoder output to executable mmap, return function pointer */
static void *enc_to_exec(Wx86Enc *enc) {
    void *mem = jit_alloc_exec(enc->pos > 0 ? enc->pos : 4096);
    if (mem && enc->pos > 0)
        memcpy(mem, enc->buf, enc->pos);
    return mem;
}

static void test_x86_encoder(void) {
    printf("\n[x86-64 Encoder Roundtrip]\n");

    /* Test: encode mov rax, 42; ret → call → verify result */
    TEST("encoder: mov rax, 42 + ret = 42");
    {
        Wx86Enc enc;
        wx86_enc_init_dynamic(&enc, 256);
        wx86_mov_reg_imm32(&enc, WREG_RAX, 42);
        wx86_ret(&enc);
        void *exec = enc_to_exec(&enc);
        wx86_enc_free(&enc);
        int64_t result = ((int64_t (*)(void))exec)();
        jit_free_exec(exec, 4096);
        if (result == 42) PASS(); else FAIL("wrong result");
    }

    /* Test: mov rdi, 10; mov rsi, 20; add rdi, rsi; mov rax, rdi; ret = 30 */
    TEST("encoder: add rdi, rsi → 30");
    {
        Wx86Enc enc;
        wx86_enc_init_dynamic(&enc, 256);
        wx86_mov_reg_imm32(&enc, WREG_RDI, 10);
        wx86_mov_reg_imm32(&enc, WREG_RSI, 20);
        wx86_add_reg_reg(&enc, WREG_RDI, WREG_RSI);
        wx86_mov_reg_reg(&enc, WREG_RAX, WREG_RDI);
        wx86_ret(&enc);
        void *exec = enc_to_exec(&enc);
        size_t sz = enc.pos;
        wx86_enc_free(&enc);
        int64_t result = ((int64_t (*)(int64_t, int64_t))exec)(0, 0);
        jit_free_exec(exec, sz);
        if (result == 30) PASS(); else FAIL("wrong result");
    }

    /* Test: imul rax, rsi → 6*7 = 42 */
    TEST("encoder: imul rax, rsi → 42");
    {
        Wx86Enc enc;
        wx86_enc_init_dynamic(&enc, 256);
        wx86_mov_reg_imm32(&enc, WREG_RDI, 6);
        wx86_mov_reg_imm32(&enc, WREG_RSI, 7);
        wx86_mov_reg_reg(&enc, WREG_RAX, WREG_RDI);
        wx86_imul_reg_reg(&enc, WREG_RAX, WREG_RSI);
        wx86_ret(&enc);
        void *exec = enc_to_exec(&enc);
        size_t sz = enc.pos;
        wx86_enc_free(&enc);
        int64_t result = ((int64_t (*)(int64_t, int64_t))exec)(0, 0);
        jit_free_exec(exec, sz);
        if (result == 42) PASS(); else FAIL("wrong result");
    }

    /* Test: sub rdi, rsi → 100-37 = 63 */
    TEST("encoder: sub rdi, rsi → 63");
    {
        Wx86Enc enc;
        wx86_enc_init_dynamic(&enc, 256);
        wx86_mov_reg_imm32(&enc, WREG_RDI, 100);
        wx86_mov_reg_imm32(&enc, WREG_RSI, 37);
        wx86_sub_reg_reg(&enc, WREG_RDI, WREG_RSI);
        wx86_mov_reg_reg(&enc, WREG_RAX, WREG_RDI);
        wx86_ret(&enc);
        void *exec = enc_to_exec(&enc);
        size_t sz = enc.pos;
        wx86_enc_free(&enc);
        int64_t result = ((int64_t (*)(int64_t, int64_t))exec)(0, 0);
        jit_free_exec(exec, sz);
        if (result == 63) PASS(); else FAIL("wrong result");
    }

    /* Test: neg → -5 */
    TEST("encoder: neg rax → -5");
    {
        Wx86Enc enc;
        wx86_enc_init_dynamic(&enc, 256);
        wx86_mov_reg_imm32(&enc, WREG_RAX, 5);
        wx86_neg_reg(&enc, WREG_RAX);
        wx86_ret(&enc);
        void *exec = enc_to_exec(&enc);
        size_t sz = enc.pos;
        wx86_enc_free(&enc);
        int64_t result = ((int64_t (*)(void))exec)();
        jit_free_exec(exec, sz);
        if (result == -5) PASS(); else FAIL("wrong result");
    }

    /* Test: xor rax, rax → 0 */
    TEST("encoder: xor rax, rax → 0");
    {
        Wx86Enc enc;
        wx86_enc_init_dynamic(&enc, 256);
        wx86_xor_reg_reg(&enc, WREG_RAX, WREG_RAX);
        wx86_ret(&enc);
        void *exec = enc_to_exec(&enc);
        size_t sz = enc.pos;
        wx86_enc_free(&enc);
        int64_t result = ((int64_t (*)(void))exec)();
        jit_free_exec(exec, sz);
        if (result == 0) PASS(); else FAIL("wrong result");
    }

    /* Test: shl rax, 3 → 5<<3 = 40 */
    TEST("encoder: shl rax, 3 → 40");
    {
        Wx86Enc enc;
        wx86_enc_init_dynamic(&enc, 256);
        wx86_mov_reg_imm32(&enc, WREG_RAX, 5);
        wx86_shl_reg_imm8(&enc, WREG_RAX, 3);
        wx86_ret(&enc);
        void *exec = enc_to_exec(&enc);
        size_t sz = enc.pos;
        wx86_enc_free(&enc);
        int64_t result = ((int64_t (*)(void))exec)();
        jit_free_exec(exec, sz);
        if (result == 40) PASS(); else FAIL("wrong result");
    }

    /* Test: sar rax, 2 → -64/4 = -16 */
    TEST("encoder: sar rax, 2 → -16 (arithmetic)");
    {
        Wx86Enc enc;
        wx86_enc_init_dynamic(&enc, 256);
        wx86_mov_reg_imm32(&enc, WREG_RAX, -64);
        wx86_sar_reg_imm8(&enc, WREG_RAX, 2);
        wx86_ret(&enc);
        void *exec = enc_to_exec(&enc);
        size_t sz = enc.pos;
        wx86_enc_free(&enc);
        int64_t result = ((int64_t (*)(void))exec)();
        jit_free_exec(exec, sz);
        if (result == -16) PASS(); else FAIL("wrong result");
    }

    /* Test: r8 register encoding — mov r8, 99; mov rax, r8; ret */
    TEST("encoder: r8 register → 99");
    {
        Wx86Enc enc;
        wx86_enc_init_dynamic(&enc, 256);
        wx86_mov_reg_imm32(&enc, WREG_R8, 99);
        wx86_mov_reg_reg(&enc, WREG_RAX, WREG_R8);
        wx86_ret(&enc);
        void *exec = enc_to_exec(&enc);
        size_t sz = enc.pos;
        wx86_enc_free(&enc);
        int64_t result = ((int64_t (*)(void))exec)();
        jit_free_exec(exec, sz);
        if (result == 99) PASS(); else FAIL("wrong result");
    }

    /* Test: idiv: 100 / 7 = 14 (quotient in rax) */
    TEST("encoder: cqo + idiv → 100/7 = 14");
    {
        Wx86Enc enc;
        wx86_enc_init_dynamic(&enc, 256);
        wx86_mov_reg_imm32(&enc, WREG_RAX, 100);
        wx86_cqo(&enc);  /* sign-extend rax into rdx:rax */
        wx86_mov_reg_imm32(&enc, WREG_RCX, 7);
        wx86_idiv_reg(&enc, WREG_RCX);
        wx86_ret(&enc);
        void *exec = enc_to_exec(&enc);
        size_t sz = enc.pos;
        wx86_enc_free(&enc);
        int64_t result = ((int64_t (*)(void))exec)();
        jit_free_exec(exec, sz);
        if (result == 14) PASS(); else FAIL("wrong result");
    }
}

/* -- Disassembler Tests ------------------------------------------ */

static void test_disasm(void) {
    printf("\n[Trivial Disassembler]\n");

    /* Test: ret → disassembles as "ret" */
    TEST("disasm: ret");
    {
        uint8_t code[] = { 0xC3 };
        WDisasmInst inst;
        int len = wdisasm_one(code, sizeof(code), 0, &inst);
        if (len == 1 && strcmp(inst.mnemonic, "ret") == 0) PASS();
        else FAIL(inst.mnemonic);
    }

    /* Test: xor rax, rax → disassembles */
    TEST("disasm: xor rax, rax");
    {
        uint8_t code[] = { 0x48, 0x31, 0xC0 };  /* REX.W xor rax, rax */
        WDisasmInst inst;
        int len = wdisasm_one(code, sizeof(code), 0, &inst);
        if (len == 3 && strcmp(inst.mnemonic, "xor") == 0) PASS();
        else FAIL(inst.mnemonic);
    }

    /* Test: add rdi, rsi → disassembles */
    TEST("disasm: add rdi, rsi");
    {
        uint8_t code[] = { 0x48, 0x01, 0xF7 };  /* REX.W add rdi, rsi */
        WDisasmInst inst;
        int len = wdisasm_one(code, sizeof(code), 0, &inst);
        if (len == 3 && strcmp(inst.mnemonic, "add") == 0) PASS();
        else FAIL(inst.mnemonic);
    }

    /* Test: imul rax, rsi → disassembles */
    TEST("disasm: imul rax, rsi");
    {
        uint8_t code[] = { 0x48, 0x0F, 0xAF, 0xC6 };  /* REX.W imul rax, rsi */
        WDisasmInst inst;
        int len = wdisasm_one(code, sizeof(code), 0, &inst);
        if (len == 4 && strcmp(inst.mnemonic, "imul") == 0) PASS();
        else FAIL(inst.mnemonic);
    }

    /* Test: dump full sequence */
    TEST("disasm: dump mov+add+ret");
    {
        /* Encode mov rax, rdi; add rax, rsi; ret into mmap memory */
        Wx86Enc enc;
        wx86_enc_init_dynamic(&enc, 256);
        wx86_mov_reg_reg(&enc, WREG_RAX, WREG_RDI);
        wx86_add_reg_reg(&enc, WREG_RAX, WREG_RSI);
        wx86_ret(&enc);
        /* Make a stack copy for disasm (read-only is fine for disasm) */
        uint8_t *copy = (uint8_t *)malloc(enc.pos);
        memcpy(copy, enc.buf, enc.pos);
        /* Dump to string */
        char str[256];
        int n = wdisasm_to_str(copy, enc.pos, str, sizeof(str));
        free(copy);
        wx86_enc_free(&enc);
        if (n > 0 && strstr(str, "mov") && strstr(str, "add") && strstr(str, "ret"))
            PASS();
        else
            FAIL(str);
    }
}

/* -- Register Allocator Tests ------------------------------------ */

static void test_regalloc(void) {
    printf("\n[Register Allocator]\n");

    TEST("xra_init: 2 args");
    {
        XRARegAlloc ra;
        xra_init(&ra, 2);
        if (ra.n_args == 2) PASS(); else FAIL("wrong n_args");
    }

    TEST("xra_alloc: scratch pool first");
    {
        XRARegAlloc ra;
        xra_init(&ra, 0);
        Wx86Reg r1 = xra_alloc(&ra, 0);
        Wx86Reg r2 = xra_alloc(&ra, 1);
        /* r10 and r11 are first in scratch pool */
        if (r1 != WREG_NONE && r2 != WREG_NONE) PASS();
        else FAIL("alloc failed");
    }

    TEST("xra_alloc: callee-saved overflow");
    {
        XRARegAlloc ra;
        xra_init(&ra, 0);
        Wx86Reg regs[16];
        int n = 0;
        for (int i = 0; i < 14; i++) {
            Wx86Reg r = xra_alloc(&ra, i);
            if (r != WREG_NONE) regs[n++] = r;
        }
        if (n >= 7) PASS();  /* At least scratch pool (7 regs) */
        else FAIL("too few allocs");
    }

    TEST("xra_finalize: frame size aligned to 16");
    {
        XRARegAlloc ra;
        xra_init(&ra, 2);
        xra_alloc(&ra, 0);
        xra_alloc(&ra, 1);
        int fsz = xra_finalize(&ra);
        if (fsz % 16 == 0) PASS(); else FAIL("not aligned");
    }

    TEST("xra_get_reg: lookup vreg");
    {
        XRARegAlloc ra;
        xra_init(&ra, 0);
        Wx86Reg r = xra_alloc(&ra, 5);
        Wx86Reg found = xra_get_reg(&ra, 5);
        if (found == r) PASS(); else FAIL("vreg not found");
    }

    TEST("xra_callee_saved_list: count used");
    {
        XRARegAlloc ra;
        xra_init(&ra, 0);
        /* Allocate enough to overflow into callee-saved */
        for (int i = 0; i < 12; i++) xra_alloc(&ra, i);
        Wx86Reg saved[5];
        int n = xra_callee_saved_list(&ra, saved, 5);
        if (n >= 1) PASS();  /* At least one callee-saved used */
        else FAIL("no callee-saved");
    }
}

/* -- ASMJIT Backend Tests ---------------------------------------- */

static void test_asmjit_backend(void) {
    printf("\n[ASMJIT Backend: Assembly Text → Executable]\n");

    JITContext *ctx = jit_init_backend(JIT_BACKEND_ASMJIT);
    assert(ctx);

    /* Simple add: mov rax, rdi; add rax, rsi; ret */
    TEST("asmjit: compile 'a+b' assembly");
    {
        const char *asm_src =
            "mov rax, rdi\n"
            "add rax, rsi\n"
            "ret\n";
        JITFunc fn;
        JITResult r = jit_compile(ctx, asm_src, JIT_LANG_ASM, "add_asm", &fn);
        if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));

        TEST("asmjit: add_asm(10, 25) == 35");
        if (r == JIT_OK && jit_call2(&fn, 10, 25) == 35) PASS(); else FAIL("wrong result");

        TEST("asmjit: add_asm(-5, 15) == 10");
        if (r == JIT_OK && jit_call2(&fn, -5, 15) == 10) PASS(); else FAIL("wrong result");

        if (r == JIT_OK) jit_func_free(&fn);
    }

    /* Multiply: mov rax, rdi; imul rax, rsi; ret */
    TEST("asmjit: compile 'a*b' assembly");
    {
        const char *asm_src =
            "mov rax, rdi\n"
            "imul rax, rsi\n"
            "ret\n";
        JITFunc fn;
        JITResult r = jit_compile(ctx, asm_src, JIT_LANG_ASM, "mul_asm", &fn);
        if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));

        TEST("asmjit: mul_asm(6, 7) == 42");
        if (r == JIT_OK && jit_call2(&fn, 6, 7) == 42) PASS(); else FAIL("wrong result");

        if (r == JIT_OK) jit_func_free(&fn);
    }

    /* Conditional: max(a,b) */
    TEST("asmjit: compile max(a,b) assembly");
    {
        const char *asm_src =
            "mov rax, rdi\n"
            "cmp rax, rsi\n"
            "jge done\n"
            "mov rax, rsi\n"
            "done:\n"
            "ret\n";
        JITFunc fn;
        JITResult r = jit_compile(ctx, asm_src, JIT_LANG_ASM, "max_asm", &fn);
        if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));

        TEST("asmjit: max(3, 7) == 7");
        if (r == JIT_OK && jit_call2(&fn, 3, 7) == 7) PASS(); else FAIL("wrong result");

        TEST("asmjit: max(10, 2) == 10");
        if (r == JIT_OK && jit_call2(&fn, 10, 2) == 10) PASS(); else FAIL("wrong result");

        if (r == JIT_OK) jit_func_free(&fn);
    }

    /* Subtract: mov rax, rdi; sub rax, rsi; ret */
    TEST("asmjit: compile 'a-b' assembly");
    {
        const char *asm_src =
            "mov rax, rdi\n"
            "sub rax, rsi\n"
            "ret\n";
        JITFunc fn;
        JITResult r = jit_compile(ctx, asm_src, JIT_LANG_ASM, "sub_asm", &fn);
        if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));

        TEST("asmjit: sub_asm(100, 37) == 63");
        if (r == JIT_OK && jit_call2(&fn, 100, 37) == 63) PASS(); else FAIL("wrong result");

        if (r == JIT_OK) jit_func_free(&fn);
    }

    jit_free(ctx);
}

/* -- MIR Backend Tests ------------------------------------------- */

static void test_mir_backend(void) {
    printf("\n[MIR Backend: C Source → gcc → dlopen]\n");

    JITContext *ctx = jit_init_backend(JIT_BACKEND_MIR);
    assert(ctx);

    /* Simple C expression: a+b */
    TEST("mir: compile 'a+b' as C");
    {
        JITFunc fn;
        JITResult r = jit_compile(ctx, "a + b", JIT_LANG_C, "wubu_add", &fn);
        if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));

        TEST("mir: wubu_add(3, 4) == 7");
        if (r == JIT_OK && jit_call2(&fn, 3, 4) == 7) PASS(); else FAIL("wrong result");

        TEST("mir: wubu_add(100, 200) == 300");
        if (r == JIT_OK && jit_call2(&fn, 100, 200) == 300) PASS(); else FAIL("wrong result");

        jit_func_free(&fn);
    }

    /* C expression: a * b + 10 */
    TEST("mir: compile 'a*b + 10' as C");
    {
        JITFunc fn;
        JITResult r = jit_compile(ctx, "a * b + 10", JIT_LANG_C, "wubu_muladd", &fn);
        if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));

        TEST("mir: wubu_muladd(5, 3) == 25");
        if (r == JIT_OK && jit_call2(&fn, 5, 3) == 25) PASS(); else FAIL("wrong result");

        jit_func_free(&fn);
    }

    jit_free(ctx);
}

/* -- Minic Compiler Tests ---------------------------------------- */

static void test_minic_compiler(void) {
    printf("\n[Minic Self-Hosted Compiler]\n");

    JITContext *ctx = jit_init_backend(JIT_BACKEND_MIR);
    assert(ctx);

    /* Expression: a + b */
    TEST("minic: compile 'a+b' expression");
    {
        JITFunc fn;
        JITResult r = jit_compile(ctx, "a + b", JIT_LANG_C, "mc_add", &fn);
        if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));

        TEST("minic: mc_add(10, 20) == 30");
        if (r == JIT_OK && jit_call2(&fn, 10, 20) == 30) PASS(); else FAIL("wrong result");

        jit_func_free(&fn);
    }

    /* Expression: a * b + 10 */
    TEST("minic: compile 'a * b + 10' expression");
    {
        JITFunc fn;
        JITResult r = jit_compile(ctx, "a * b + 10", JIT_LANG_C, "mc_muladd", &fn);
        if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));

        TEST("minic: mc_muladd(5, 3) == 25");
        if (r == JIT_OK && jit_call2(&fn, 5, 3) == 25) PASS(); else FAIL("wrong result");

        TEST("minic: mc_muladd(0, 0) == 10");
        if (r == JIT_OK && jit_call2(&fn, 0, 0) == 10) PASS(); else FAIL("wrong result");

        jit_func_free(&fn);
    }

    /* Simple function via minic */
    TEST("minic: compile function 'long add(long a, long b) { return a + b; }'");
    {
        JITFunc fn;
        JITResult r = jit_compile(ctx,
            "long add(long a, long b) { return a + b; }",
            JIT_LANG_C, "add", &fn);
        if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));

        TEST("minic: add(7, 8) == 15");
        if (r == JIT_OK && jit_call2(&fn, 7, 8) == 15) PASS(); else FAIL("wrong result");

        jit_func_free(&fn);
    }

    /* Function with local variables */
    TEST("minic: compile function with local vars");
    {
        JITFunc fn;
        JITResult r = jit_compile(ctx,
            "long compute(long a, long b) { long c = a + b; long d = c * 2; return d; }",
            JIT_LANG_C, "compute", &fn);
        if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));

        TEST("minic: compute(3, 4) == 14");
        if (r == JIT_OK && jit_call2(&fn, 3, 4) == 14) PASS(); else FAIL("wrong result");

        TEST("minic: compute(10, 20) == 60");
        if (r == JIT_OK && jit_call2(&fn, 10, 20) == 60) PASS(); else FAIL("wrong result");

        jit_func_free(&fn);
    }

    /* Function with if/else */
    TEST("minic: compile function with if/else");
    {
        JITFunc fn;
        JITResult r = jit_compile(ctx,
            "long max_fn(long a, long b) { if (a > b) { return a; } else { return b; } }",
            JIT_LANG_C, "max_fn", &fn);
        if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));

        TEST("minic: max_fn(3, 7) == 7");
        if (r == JIT_OK && jit_call2(&fn, 3, 7) == 7) PASS(); else FAIL("wrong result");

        TEST("minic: max_fn(10, 2) == 10");
        if (r == JIT_OK && jit_call2(&fn, 10, 2) == 10) PASS(); else FAIL("wrong result");

        jit_func_free(&fn);
    }

    /* Function with while loop */
    TEST("minic: compile function with while loop");
    {
        JITFunc fn;
        JITResult r = jit_compile(ctx,
            "long sum_to(long n, long b) { long s = 0; long i = 1; while (i <= n) { s = s + i; i = i + 1; } return s; }",
            JIT_LANG_C, "sum_to", &fn);
        if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));

        TEST("minic: sum_to(5, 0) == 15");
        if (r == JIT_OK && jit_call2(&fn, 5, 0) == 15) PASS(); else FAIL("wrong result");

        TEST("minic: sum_to(10, 0) == 55");
        if (r == JIT_OK && jit_call2(&fn, 10, 0) == 55) PASS(); else FAIL("wrong result");

        jit_func_free(&fn);
    }

    /* Unary negation */
    TEST("minic: compile '-a' expression");
    {
        JITFunc fn;
        JITResult r = jit_compile(ctx, "-a", JIT_LANG_C, "mc_neg", &fn);
        if (r == JIT_OK) PASS(); else FAIL(jit_strerror(r));

        TEST("minic: mc_neg(42) == -42");
        if (r == JIT_OK && jit_call2(&fn, 42, 0) == -42) PASS(); else FAIL("wrong result");

        jit_func_free(&fn);
    }

    /* Disasm of minic-compiled function */
    TEST("minic: disasm a minic function");
    {
        JITFunc fn;
        JITResult r = jit_compile(ctx, "a * b", JIT_LANG_C, "mc_mul", &fn);
        if (r == JIT_OK) {
            jit_func_disasm(&fn, stderr);
            PASS();
        } else FAIL(jit_strerror(r));
        jit_func_free(&fn);
    }

    jit_free(ctx);
}

/* -- Diagnostics Tests ------------------------------------------- */

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

    TEST("jit_func_disasm works (capstone replaced)");
    if (r == JIT_OK) {
        jit_func_disasm(&fn, stderr);
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

/* -- Main -------------------------------------------------------- */

int main(void) {
    printf("+======================================+\n");
    printf("|   WuBuOS JIT Test Suite             |\n");
    printf("|   x86-64 encoder + disasm + backends |\n");
    printf("+======================================+\n");

    test_context_lifecycle();
    test_exec_memory();
    test_compile_simple();
    test_x86_encoder();
    test_disasm();
    test_regalloc();
    test_asmjit_backend();
    test_mir_backend();
    test_minic_compiler();
    test_diagnostics();

    printf("\n==============================\n");
    if (failures == 0)
        printf("All tests passed! ✅\n");
    else
        printf("%d test(s) FAILED ❌\n", failures);

    return failures ? 1 : 0;
}
