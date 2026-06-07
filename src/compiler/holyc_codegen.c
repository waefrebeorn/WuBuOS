/*
 * holyc_codegen.c — My Seed HolyC Code Generator
 *
 * Emits x86-64 machine code from HolyC AST.
 * Uses our JIT mmap backend for executable memory.
 *
 * Register convention (System V AMD64):
 *   rdi, rsi, rdx, rcx, r8, r9  — argument passing
 *   rax                          — return value
 *   rbx, rbp, r12-r15           — callee-saved
 *   rsp                          — stack pointer
 *
 * Ported from ZealOS/src/Compiler/BackA.ZC + BackB.ZC
 */

#include "holyc.h"
#include "../jit/jit.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>

/* JIT_CALL macro from jit.h — call function pointer with 0 args */
#ifndef JIT_CALL
#define JIT_CALL(fn) ((int64_t(*)(void))(fn))()
#endif

/* ── Code Emission Helpers ──────────────────────────────────────── */

static void emit_byte(HCGen *gen, uint8_t b) {
    if (gen->code_size >= gen->code_cap) {
        gen->code_cap = gen->code_cap ? gen->code_cap * 2 : 256;
        gen->code = (uint8_t *)realloc(gen->code, gen->code_cap);
    }
    gen->code[gen->code_size++] = b;
}

static void emit_word(HCGen *gen, uint16_t w) {
    emit_byte(gen, (uint8_t)(w & 0xFF));
    emit_byte(gen, (uint8_t)((w >> 8) & 0xFF));
}

static void emit_dword(HCGen *gen, uint32_t d) {
    emit_byte(gen, (uint8_t)(d & 0xFF));
    emit_byte(gen, (uint8_t)((d >> 8) & 0xFF));
    emit_byte(gen, (uint8_t)((d >> 16) & 0xFF));
    emit_byte(gen, (uint8_t)((d >> 24) & 0xFF));
}

static void emit_qword(HCGen *gen, uint64_t q) {
    emit_dword(gen, (uint32_t)(q & 0xFFFFFFFF));
    emit_dword(gen, (uint32_t)((q >> 32) & 0xFFFFFFFF));
}

/* ── x86-64 Instruction Patterns ────────────────────────────────── */

/* mov rax, imm64 */
static void emit_mov_rax_imm64(HCGen *gen, int64_t val) {
    emit_byte(gen, 0x48);  /* REX.W */
    emit_byte(gen, 0xB8);  /* mov rax, imm64 */
    emit_qword(gen, (uint64_t)val);
}

/* mov rdi, imm64 (first arg) */
static void emit_mov_rdi_imm64(HCGen *gen, int64_t val) {
    emit_byte(gen, 0x48);  /* REX.W */
    emit_byte(gen, 0xBF);  /* mov rdi, imm64 */
    emit_qword(gen, (uint64_t)val);
}

/* add rax, rdi */
static void emit_add_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x01); emit_byte(gen, 0xF8);
}

/* sub rax, rdi (rax = rax - rdi) */
static void emit_sub_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x29); emit_byte(gen, 0xF8);
}

/* imul rax, rdi */
static void emit_mul_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x0F);
    emit_byte(gen, 0xAF); emit_byte(gen, 0xC7);
}

/* idiv rdi (rdx:rax / rdi → rax=quot, rdx=rem) */
static void emit_div_rax_rdi(HCGen *gen) {
    /* cqo (sign-extend rax into rdx:rax) */
    emit_byte(gen, 0x48); emit_byte(gen, 0x99);
    /* idiv rdi */
    emit_byte(gen, 0x48); emit_byte(gen, 0xF7);
    emit_byte(gen, 0xFF);
}

/* xor rdx, rdx; div rdi (unsigned) */
static void emit_udiv_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x31);
    emit_byte(gen, 0xD2);  /* xor rdx, rdx */
    emit_byte(gen, 0x48); emit_byte(gen, 0xF7);
    emit_byte(gen, 0xF7);  /* div rdi */
}

/* xchg rax, rdi (move arg to rax, old rax to rdi) */
static void emit_xchg_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x87); emit_byte(gen, 0xF8);
}

/* neg rax */
static void emit_neg_rax(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0xF7); emit_byte(gen, 0xD8);
}

/* not rax (bitwise) */
static void emit_not_rax(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0xF7); emit_byte(gen, 0xD0);
}

/* xor rax, rax (set rax = 0) then test+set pattern for logical ops */

/* cmp rax, rdi */
static void emit_cmp_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x39); emit_byte(gen, 0xF8);
}

/* ret */
static void emit_ret(HCGen *gen) {
    emit_byte(gen, 0xC3);
}

/* push rbp; mov rbp, rsp (function prologue) */
static void emit_prologue(HCGen *gen) {
    emit_byte(gen, 0x55);                    /* push rbp */
    emit_byte(gen, 0x48); emit_byte(gen, 0x89);
    emit_byte(gen, 0xE5);                    /* mov rbp, rsp */
}

/* pop rbp; ret (function epilogue) */
static void emit_epilogue(HCGen *gen) {
    emit_byte(gen, 0x5D);  /* pop rbp */
    emit_ret(gen);
}

/* mov rdi, rax (copy result to first-arg position for binary op) */
static void emit_mov_rdi_rax(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xC7);
}

/* ── Conditional Set Patterns ───────────────────────────────────── */

/* After cmp rax, rdi: set al based on condition, then movzx rax, al */
static void emit_setcc(HCGen *gen, uint8_t set_op) {
    emit_byte(gen, 0x0F);               /* Two-byte opcode prefix */
    emit_byte(gen, set_op);             /* setcc al */
    emit_byte(gen, 0xC0);              /* al */
    /* movzx rax, al */
    emit_byte(gen, 0x48);              /* REX.W */
    emit_byte(gen, 0x0F);
    emit_byte(gen, 0xB6);
    emit_byte(gen, 0xC0);
}

/* ── Code Gen Init ──────────────────────────────────────────────── */

void hc_gen_init(HCGen *gen) {
    memset(gen, 0, sizeof(*gen));
}

/* ── Code Gen for Expressions ───────────────────────────────────── */

static int gen_expr(HCGen *gen, const HCASTNode *node) {
    if (!node) return 0;

    switch (node->kind) {
        /* Literals → mov rax, imm64 */
        case HC_AST_INT_LIT:
            emit_mov_rax_imm64(gen, node->int_val);
            break;

        case HC_AST_FLOAT_LIT:
            /* For now, store as I64 bit pattern */
            {
                union { double d; int64_t i; } u;
                u.d = node->float_val;
                emit_mov_rax_imm64(gen, u.i);
            }
            break;

        case HC_AST_BOOL_LIT:
            emit_mov_rax_imm64(gen, node->int_val ? 1 : 0);
            break;

        /* Identifiers — for now, just load 0 (TODO: symbol resolution) */
        case HC_AST_IDENT:
            /* Would look up in symbol table and load from stack */
            emit_mov_rax_imm64(gen, 0);
            break;

        /* Negation */
        case HC_AST_NEG:
            gen_expr(gen, node->child);
            emit_neg_rax(gen);
            break;

        /* Logical NOT: test rax, rax; setz al; movzx rax, al */
        case HC_AST_NOT:
            gen_expr(gen, node->child);
            /* test rax, rax */
            emit_byte(gen, 0x48); emit_byte(gen, 0x85);
            emit_byte(gen, 0xC0);
            /* sete al (set if zero = NOT) */
            emit_setcc(gen, 0x94);
            break;

        /* Bitwise NOT */
        case HC_AST_BITNOT:
            gen_expr(gen, node->child);
            emit_not_rax(gen);
            break;

        /* Binary operations: eval left → rax, save to rdi, eval right → rax, swap, op */
        case HC_AST_ADD:
            gen_expr(gen, node->left);     /* left → rax */
            emit_mov_rdi_rax(gen);         /* save left to rdi */
            gen_expr(gen, node->right);    /* right → rax */
            emit_xchg_rax_rdi(gen);        /* left→rax, right→rdi */
            emit_add_rax_rdi(gen);
            break;

        case HC_AST_SUB:
            gen_expr(gen, node->left);
            emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right);
            emit_xchg_rax_rdi(gen);
            emit_sub_rax_rdi(gen);
            break;

        case HC_AST_MUL:
            gen_expr(gen, node->left);
            emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right);
            emit_xchg_rax_rdi(gen);
            emit_mul_rax_rdi(gen);
            break;

        case HC_AST_DIV:
            gen_expr(gen, node->left);
            emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right);
            emit_xchg_rax_rdi(gen);
            emit_div_rax_rdi(gen);
            break;

        case HC_AST_MOD:
            gen_expr(gen, node->left);
            emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right);
            emit_xchg_rax_rdi(gen);
            emit_div_rax_rdi(gen);
            /* Remainder is in rdx, move to rax */
            emit_byte(gen, 0x48); emit_byte(gen, 0x89);
            emit_byte(gen, 0xD0); /* mov rax, rdx */
            break;

        /* Comparison ops: cmp rax, rdi then setcc */
        case HC_AST_EQ:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x94); /* sete */
            break;

        case HC_AST_NE:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x95); /* setne */
            break;

        case HC_AST_LT:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x9C); /* setl */
            break;

        case HC_AST_LE:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x9E); /* setle */
            break;

        case HC_AST_GT:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x9F); /* setg */
            break;

        case HC_AST_GE:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            emit_cmp_rax_rdi(gen);
            emit_setcc(gen, 0x9D); /* setge */
            break;

        /* For AND/OR, we use short-circuit with jumps */
        case HC_AST_AND: {
            gen_expr(gen, node->left);    /* left → rax */
            /* test rax, rax; jz false; eval right; test rax, rax; setne al; jmp end; false: xor rax,rax; end: */
            emit_byte(gen, 0x48); emit_byte(gen, 0x85); emit_byte(gen, 0xC0); /* test rax, rax */
            emit_byte(gen, 0x74); emit_byte(gen, 0x07); /* jz +7 (skip to xor) */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x48); emit_byte(gen, 0x85); emit_byte(gen, 0xC0);
            emit_setcc(gen, 0x95); /* setne al */
            emit_byte(gen, 0xEB); emit_byte(gen, 0x01); /* jmp +1 */
            emit_byte(gen, 0x48); emit_byte(gen, 0x31); emit_byte(gen, 0xC0); /* xor rax, rax */
            break;
        }

        case HC_AST_OR: {
            gen_expr(gen, node->left);
            emit_byte(gen, 0x48); emit_byte(gen, 0x85); emit_byte(gen, 0xC0);
            emit_byte(gen, 0x75); emit_byte(gen, 0x07); /* jnz +7 (skip to mov 1) */
            gen_expr(gen, node->right);
            emit_byte(gen, 0x48); emit_byte(gen, 0x85); emit_byte(gen, 0xC0);
            emit_setcc(gen, 0x95); /* setne al */
            emit_byte(gen, 0xEB); emit_byte(gen, 0x06); /* jmp +6 */
            emit_mov_rax_imm64(gen, 1); /* true */
            break;
        }

        case HC_AST_ASSIGN:
            /* Right-hand side → rax, then store (TODO: stack slot lookup) */
            gen_expr(gen, node->right);
            break;

        /* Function call */
        case HC_AST_FUNC_CALL:
            /* For now: if 0 args, just call. If 1 arg, pass in rdi. */
            if (node->n_args >= 1) {
                gen_expr(gen, node->args[0]);  /* first arg → rax */
                emit_mov_rdi_rax(gen);          /* move arg to rdi */
            }
            /* TODO: actual call instruction with address */
            emit_mov_rax_imm64(gen, 0);
            break;

        /* Ternary: cond ? then : else */
        case HC_AST_TERNARY: {
            gen_expr(gen, node->cond);
            emit_byte(gen, 0x48); emit_byte(gen, 0x85); emit_byte(gen, 0xC0); /* test rax, rax */
            emit_byte(gen, 0x74); emit_byte(gen, 0x05); /* jz +5 (skip to else) */
            gen_expr(gen, node->then_branch);
            emit_byte(gen, 0xEB); emit_byte(gen, 0x03); /* jmp +3 */
            gen_expr(gen, node->else_branch);
            break;
        }

        default:
            /* Unknown expression — emit 0 */
            emit_mov_rax_imm64(gen, 0);
            break;
    }

    return 0;
}

/* ── Code Gen for Statements ────────────────────────────────────── */

static int gen_stmt(HCGen *gen, const HCASTNode *node) {
    if (!node) return 0;

    switch (node->kind) {
        case HC_AST_EXPR_STMT:
            return gen_expr(gen, node->child);

        case HC_AST_RETURN:
            if (node->child)
                gen_expr(gen, node->child);
            else
                emit_mov_rax_imm64(gen, 0);
            emit_epilogue(gen);
            break;

        case HC_AST_BLOCK:
            for (int i = 0; i < node->n_stmts; i++)
                gen_stmt(gen, node->stmts[i]);
            break;

        case HC_AST_IF: {
            gen_expr(gen, node->cond);
            emit_byte(gen, 0x48); emit_byte(gen, 0x85); emit_byte(gen, 0xC0); /* test rax, rax */
            emit_byte(gen, 0x74); emit_byte(gen, 0x05); /* jz +5 (skip to else) */
            gen_stmt(gen, node->then_branch);
            if (node->else_branch) {
                emit_byte(gen, 0xEB); emit_byte(gen, 0x03); /* jmp +3 (skip else) */
                gen_stmt(gen, node->else_branch);
            }
            break;
        }

        case HC_AST_WHILE: {
            /* Simplified: emit condition check in a loop.
             * Without proper label support, this is a skeleton. */
            size_t loop_start = gen->code_size;
            gen_expr(gen, node->cond);
            emit_byte(gen, 0x48); emit_byte(gen, 0x85); emit_byte(gen, 0xC0); /* test */
            emit_byte(gen, 0x74); emit_byte(gen, 0x05); /* jz +5 (break) */
            gen_stmt(gen, node->body);
            /* Jump back to loop_start (requires relative offset) */
            {
                int64_t rel = (int64_t)loop_start - (int64_t)(gen->code_size + 2);
                emit_byte(gen, 0xEB);
                emit_byte(gen, (uint8_t)(rel & 0xFF));
            }
            break;
        }

        case HC_AST_VAR_DECL:
            /* TODO: Allocate stack slot, store init value */
            if (node->init)
                gen_expr(gen, node->init);
            break;

        case HC_AST_FUNC_DECL:
            /* Generate function body */
            emit_prologue(gen);
            if (node->body)
                gen_stmt(gen, node->body);
            emit_epilogue(gen);
            break;

        default:
            /* Try as expression */
            return gen_expr(gen, node);
    }

    return 0;
}

/* ── Public API ─────────────────────────────────────────────────── */

int hc_gen_node(HCGen *gen, const HCASTNode *node) {
    if (!node) return -1;
    if (node->kind == HC_AST_BLOCK || node->kind == HC_AST_FUNC_DECL)
        return gen_stmt(gen, node);
    return gen_expr(gen, node);
}

int hc_gen_function(HCGen *gen, const HCASTNode *func) {
    if (!func || func->kind != HC_AST_FUNC_DECL) return -1;
    return gen_stmt(gen, func);
}

const uint8_t *hc_gen_code(const HCGen *gen, size_t *out_size) {
    if (out_size) *out_size = gen->code_size;
    return gen->code;
}

/* ── Top-Level Compile and Execute ──────────────────────────────── */

void *hc_compile(const char *source, size_t *out_size) {
    HCLexer lex;
    hc_lex_init(&lex, source);

    if (lex.has_error) return NULL;

    HCParser parse;
    hc_parse_init(&parse, &lex);

    HCASTNode *ast = hc_parse_expr(&parse);
    if (parse.has_error) {
        hc_ast_free(ast);
        return NULL;
    }

    HCGen gen;
    hc_gen_init(&gen);
    gen_expr(&gen, ast);
    emit_ret(&gen);

    hc_ast_free(ast);

    if (gen.code_size == 0 || gen.has_error) {
        free(gen.code);
        return NULL;
    }

    /* Allocate executable memory */
    void *exec = jit_alloc_exec(gen.code_size);
    if (!exec) { free(gen.code); return NULL; }

    memcpy(exec, gen.code, gen.code_size);
    if (out_size) *out_size = gen.code_size;
    free(gen.code);

    return exec;
}

int64_t hc_eval(const char *source) {
    size_t code_size;
    void *code = hc_compile(source, &code_size);
    if (!code) return 0;

    int64_t result = JIT_CALL(code);
    jit_free_exec(code, code_size);
    return result;
}

void *hc_compile_func(const char *source, const char *func_name) {
    /* Parse entire compilation unit, find the named function, compile it */
    (void)func_name; /* TODO: multi-function compilation */

    HCLexer lex;
    hc_lex_init(&lex, source);
    if (lex.has_error) return NULL;

    HCParser parse;
    hc_parse_init(&parse, &lex);
    HCASTNode *ast = hc_parse_compilation_unit(&parse);
    if (parse.has_error || !ast) {
        hc_ast_free(ast);
        return NULL;
    }

    /* Find the first function declaration */
    HCASTNode *func = NULL;
    for (int i = 0; i < ast->n_stmts; i++) {
        if (ast->stmts[i]->kind == HC_AST_FUNC_DECL) {
            func = ast->stmts[i];
            break;
        }
    }

    if (!func) {
        /* No function found — treat as expression */
        hc_ast_free(ast);

        HCLexer lex2;
        hc_lex_init(&lex2, source);
        HCParser parse2;
        hc_parse_init(&parse2, &lex2);
        HCASTNode *expr = hc_parse_expr(&parse2);

        HCGen gen;
        hc_gen_init(&gen);
        emit_prologue(&gen);
        gen_expr(&gen, expr);
        emit_epilogue(&gen);
        hc_ast_free(expr);

        if (gen.code_size == 0) { free(gen.code); return NULL; }

        void *exec = jit_alloc_exec(gen.code_size);
        if (!exec) { free(gen.code); return NULL; }
        memcpy(exec, gen.code, gen.code_size);
        free(gen.code);
        return exec;
    }

    /* Generate the function */
    HCGen gen;
    hc_gen_init(&gen);
    hc_gen_function(&gen, func);
    hc_ast_free(ast);

    if (gen.code_size == 0) { free(gen.code); return NULL; }

    void *exec = jit_alloc_exec(gen.code_size);
    if (!exec) { free(gen.code); return NULL; }
    memcpy(exec, gen.code, gen.code_size);
    free(gen.code);
    return exec;
}
