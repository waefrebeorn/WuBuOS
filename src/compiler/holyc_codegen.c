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
 * All conditional jumps use 5-byte encoding (0F 8x + rel32)
 * to guarantee patchability for label-based backpatching.
 * Unconditional jumps use 5-byte (E9 + rel32).
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

/* ── Patch Helpers ───────────────────────────────────────────────── */

/* Patch a 4-byte relative offset at 'patch_pos' to jump to 'target_pos'.
 * The offset is relative to the instruction AFTER the patch position.
 * i.e., offset = target - (patch_pos + 4)
 */
static void patch_rel32(HCGen *gen, size_t patch_pos, size_t target_pos) {
    int32_t rel = (int32_t)((int64_t)target_pos - (int64_t)(patch_pos + 4));
    gen->code[patch_pos + 0] = (uint8_t)(rel & 0xFF);
    gen->code[patch_pos + 1] = (uint8_t)((rel >> 8) & 0xFF);
    gen->code[patch_pos + 2] = (uint8_t)((rel >> 16) & 0xFF);
    gen->code[patch_pos + 3] = (uint8_t)((rel >> 24) & 0xFF);
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

/* idiv rdi (rdx:hax / rdi → rax=quot, rdx=rem) */
static void emit_div_rax_rdi(HCGen *gen) {
    /* cqo (sign-extend rax into rdx:hax) */
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

/* cmp rax, rdi */
static void emit_cmp_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x39); emit_byte(gen, 0xF8);
}

/* test rax, rax (3 bytes: 48 85 C0) */
static void emit_test_rax_rax(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x85); emit_byte(gen, 0xC0);
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

/* xor rax, rax (set rax = 0) */
static void emit_xor_rax_rax(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x31); emit_byte(gen, 0xC0);
}

/* mov rax, 1 */
static void emit_mov_rax_1(HCGen *gen) {
    emit_mov_rax_imm64(gen, 1);
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

/* ── Jump Emission (5-byte, always patchable) ───────────────────── */

/* Emit Jcc (conditional jump) with placeholder rel32.
 * cc is the condition code (0-15): 0=JO, 4=JE/JZ, 5=JNE/JNZ, etc.
 * Returns the position of the 4-byte rel32 for later patching.
 */
static size_t emit_jcc_placeholder(HCGen *gen, uint8_t cc) {
    emit_byte(gen, 0x0F);
    emit_byte(gen, 0x80 | (cc & 0x0F));
    size_t patch_pos = gen->code_size;
    emit_dword(gen, 0);  /* placeholder */
    return patch_pos;
}

/* Emit unconditional JMP with placeholder rel32.
 * Returns position of 4-byte rel32 for patching.
 */
static size_t emit_jmp_placeholder(HCGen *gen) {
    emit_byte(gen, 0xE9);
    size_t patch_pos = gen->code_size;
    emit_dword(gen, 0);  /* placeholder */
    return patch_pos;
}

/* Condition codes for Jcc */
#define CC_O  0
#define CC_NO 1
#define CC_B  2   /* below (unsigned <) */
#define CC_NB 3   /* not below */
#define CC_E  4   /* equal / zero */
#define CC_NE 5   /* not equal / not zero */
#define CC_BE 6   /* below or equal */
#define CC_NBE 7  /* not below or equal (above) */
#define CC_S  8   /* sign */
#define CC_NS 9   /* not sign */
#define CC_L  12  /* less (signed <) */
#define CC_NL 13  /* not less */
#define CC_LE 14  /* less or equal */
#define CC_NLE 15 /* not less or equal (greater) */

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

        case HC_AST_STRING_LIT:
            /* TODO: string literal in data section. For now, load address placeholder. */
            emit_mov_rax_imm64(gen, 0);
            break;

        /* Identifiers — for now, look up in symbol table */
        case HC_AST_IDENT:
            if (gen->symbols.n_locals > 0) {
                /* Look up variable in symbol table */
                bool found = false;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->ident) == 0) {
                        int off = gen->symbols.locals[i].stack_offset;
                        /* mov rax, [rbp - off] */
                        emit_byte(gen, 0x48); /* REX.W */
                        emit_byte(gen, 0x8B); /* mov */
                        emit_byte(gen, 0x85); /* rax, [rbp+disp8] */
                        emit_byte(gen, (uint8_t)(-off & 0xFF));
                        found = true;
                        break;
                    }
                }
                if (!found) emit_mov_rax_imm64(gen, 0);
            } else {
                emit_mov_rax_imm64(gen, 0);
            }
            break;

        /* Negation */
        case HC_AST_NEG:
            gen_expr(gen, node->child);
            emit_neg_rax(gen);
            break;

        /* Logical NOT: test rax, rax; setz al; movzx rax, al */
        case HC_AST_NOT:
            gen_expr(gen, node->child);
            emit_test_rax_rax(gen);
            emit_setcc(gen, 0x94); /* sete */
            break;

        /* Bitwise NOT */
        case HC_AST_BITNOT:
            gen_expr(gen, node->child);
            emit_not_rax(gen);
            break;

        /* Binary operations: eval left → rax, save to rdi, eval right → rax, swap, op */
        case HC_AST_ADD:
            gen_expr(gen, node->left);
            emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right);
            emit_xchg_rax_rdi(gen);
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

        /* Bitwise AND, OR, XOR */
        case HC_AST_BITAND:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            /* and rax, rdi: 48 21 F8 */
            emit_byte(gen, 0x48); emit_byte(gen, 0x21); emit_byte(gen, 0xF8);
            break;

        case HC_AST_BITOR:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            /* or rax, rdi: 48 09 F8 */
            emit_byte(gen, 0x48); emit_byte(gen, 0x09); emit_byte(gen, 0xF8);
            break;

        case HC_AST_XOR:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            /* xor rax, rdi: 48 31 F8 */
            emit_byte(gen, 0x48); emit_byte(gen, 0x31); emit_byte(gen, 0xF8);
            break;

        /* Shift left/right */
        case HC_AST_SHL:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            /* mov rcx, rdi (shift count must be in cl): 48 89 F9 */
            emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xF9);
            emit_byte(gen, 0x48); emit_byte(gen, 0xD3); emit_byte(gen, 0xE0); /* shl rax, cl */
            break;

        case HC_AST_SHR:
            gen_expr(gen, node->left); emit_mov_rdi_rax(gen);
            gen_expr(gen, node->right); emit_xchg_rax_rdi(gen);
            emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xF9);
            emit_byte(gen, 0x48); emit_byte(gen, 0xD3); emit_byte(gen, 0xE8); /* shr rax, cl */
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

        /* Logical AND with short-circuit and proper backpatching:
         *   eval left → rax
         *   test rax, rax
         *   jz false_label          (5 bytes, placeholder)
         *   eval right → rax
         *   test rax, rax
         *   setne al; movzx rax, al  (convert to bool)
         *   jmp end_label            (5 bytes, placeholder)
         * false_label:
         *   xor rax, rax             (rax = 0)
         * end_label:
         */
        case HC_AST_AND: {
            gen_expr(gen, node->left);
            emit_test_rax_rax(gen);
            size_t jz_patch = emit_jcc_placeholder(gen, CC_E); /* jz false */
            gen_expr(gen, node->right);
            emit_test_rax_rax(gen);
            emit_setcc(gen, 0x95); /* setne al; movzx rax, al → bool result */
            size_t jmp_patch = emit_jmp_placeholder(gen); /* jmp end */
            size_t false_label = gen->code_size;
            emit_xor_rax_rax(gen);
            size_t end_label = gen->code_size;
            patch_rel32(gen, jz_patch, false_label);
            patch_rel32(gen, jmp_patch, end_label);
            break;
        }

        /* Logical OR with short-circuit and proper backpatching:
         *   eval left → rax
         *   test rax, rax
         *   jnz true_label           (5 bytes, placeholder)
         *   eval right → rax
         *   test rax, rax
         *   setne al; movzx rax, al   (convert to bool)
         *   jmp end_label             (5 bytes, placeholder)
         * true_label:
         *   mov rax, 1                (rax = 1)
         * end_label:
         */
        case HC_AST_OR: {
            gen_expr(gen, node->left);
            emit_test_rax_rax(gen);
            size_t jnz_patch = emit_jcc_placeholder(gen, CC_NE); /* jnz true */
            gen_expr(gen, node->right);
            emit_test_rax_rax(gen);
            emit_setcc(gen, 0x95); /* setne al; movzx → bool */
            size_t jmp_patch = emit_jmp_placeholder(gen); /* jmp end */
            size_t true_label = gen->code_size;
            emit_mov_rax_1(gen);
            size_t end_label = gen->code_size;
            patch_rel32(gen, jnz_patch, true_label);
            patch_rel32(gen, jmp_patch, end_label);
            break;
        }

        case HC_AST_ASSIGN:
            /* Right-hand side → rax, then store (TODO: stack slot lookup) */
            gen_expr(gen, node->right);
            /* TODO: if left is an IDENT, store rax to its stack slot */
            break;

        /* Compound assignments */
        case HC_AST_ADD_ASSIGN:
        case HC_AST_SUB_ASSIGN:
        case HC_AST_MUL_ASSIGN:
        case HC_AST_DIV_ASSIGN:
            /* TODO: load current value, apply op, store back */
            gen_expr(gen, node->right);
            break;

        /* Function call */
        case HC_AST_FUNC_CALL:
            /* For now: if 0 args, just call. If args, pass in rdi, rsi, rdx, rcx. */
            {
                int n_args = node->n_args;
                if (n_args >= 1) {
                    gen_expr(gen, node->args[0]);  /* first arg → rax → rdi */
                    emit_mov_rdi_rax(gen);
                }
                if (n_args >= 2) {
                    gen_expr(gen, node->args[1]);  /* second arg → rax → rsi */
                    /* mov rsi, rax: 48 89 C6 */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xC6);
                }
                if (n_args >= 3) {
                    gen_expr(gen, node->args[2]);  /* third arg → rax → rdx */
                    /* mov rdx, rax: 48 89 C2 */
                    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xC2);
                }
                /* TODO: actual call instruction with address */
                emit_mov_rax_imm64(gen, 0);
            }
            break;

        /* Ternary: cond ? then : else
         *   eval cond → rax
         *   test rax, rax
         *   jz else_label             (5 bytes, placeholder)
         *   eval then_branch → rax
         *   jmp end_label             (5 bytes, placeholder)
         * else_label:
         *   eval else_branch → rax
         * end_label:
         */
        case HC_AST_TERNARY: {
            gen_expr(gen, node->cond);
            emit_test_rax_rax(gen);
            size_t jz_patch = emit_jcc_placeholder(gen, CC_E); /* jz else */
            gen_expr(gen, node->then_branch);
            size_t jmp_patch = emit_jmp_placeholder(gen); /* jmp end */
            size_t else_label = gen->code_size;
            gen_expr(gen, node->else_branch);
            size_t end_label = gen->code_size;
            patch_rel32(gen, jz_patch, else_label);
            patch_rel32(gen, jmp_patch, end_label);
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

        /* if (cond) then [else else]
         *   eval cond → rax
         *   test rax, rax
         *   jz else_label             (5 bytes, placeholder)
         *   gen then_branch
         *   jmp end_label             (5 bytes, placeholder)  [if else exists]
         * else_label:
         *   gen else_branch           [if exists]
         * end_label:
         */
        case HC_AST_IF: {
            gen_expr(gen, node->cond);
            emit_test_rax_rax(gen);
            size_t jz_patch = emit_jcc_placeholder(gen, CC_E); /* jz else */
            gen_stmt(gen, node->then_branch);
            if (node->else_branch) {
                size_t jmp_patch = emit_jmp_placeholder(gen); /* jmp end */
                size_t else_label = gen->code_size;
                gen_stmt(gen, node->else_branch);
                size_t end_label = gen->code_size;
                patch_rel32(gen, jz_patch, else_label);
                patch_rel32(gen, jmp_patch, end_label);
            } else {
                size_t end_label = gen->code_size;
                patch_rel32(gen, jz_patch, end_label);
            }
            break;
        }

        /* while (cond) body
         * loop_top:
         *   eval cond → rax
         *   test rax, rax
         *   jz loop_end               (5 bytes, placeholder)
         *   gen body
         *   jmp loop_top              (5 bytes, back jump)
         * loop_end:
         */
        case HC_AST_WHILE: {
            size_t loop_top = gen->code_size;
            gen_expr(gen, node->cond);
            emit_test_rax_rax(gen);
            size_t jz_patch = emit_jcc_placeholder(gen, CC_E); /* jz loop_end */
            int prev_break = gen->break_label;
            int prev_continue = gen->continue_label;
            gen->loop_depth++;
            gen_stmt(gen, node->body);
            /* jmp loop_top (back jump) */
            size_t jmp_patch = emit_jmp_placeholder(gen);
            patch_rel32(gen, jmp_patch, loop_top);
            size_t loop_end = gen->code_size;
            patch_rel32(gen, jz_patch, loop_end);
            gen->loop_depth--;
            gen->break_label = prev_break;
            gen->continue_label = prev_continue;
            break;
        }

        /* do body while (cond)
         * loop_top:
         *   gen body
         *   eval cond → rax
         *   test rax, rax
         *   jnz loop_top              (5 bytes, back jump)
         * loop_end:                   (break target)
         */
        case HC_AST_DO_WHILE: {
            size_t loop_top = gen->code_size;
            int prev_break = gen->break_label;
            int prev_continue = gen->continue_label;
            gen->loop_depth++;
            gen_stmt(gen, node->body);
            gen_expr(gen, node->cond);
            emit_test_rax_rax(gen);
            size_t jnz_patch = emit_jcc_placeholder(gen, CC_NE); /* jnz loop_top */
            patch_rel32(gen, jnz_patch, loop_top);
            gen->loop_depth--;
            gen->break_label = prev_break;
            gen->continue_label = prev_continue;
            break;
        }

        /* for (init; cond; update) body
         *   gen init
         * loop_top:
         *   eval cond → rax
         *   test rax, rax
         *   jz loop_end               (5 bytes, placeholder)
         *   gen body
         * continue_label:
         *   gen update
         *   jmp loop_top              (5 bytes, back jump)
         * loop_end:
         */
        case HC_AST_FOR: {
            /* init */
            if (node->init_expr)
                gen_expr(gen, node->init_expr);

            size_t loop_top = gen->code_size;

            /* condition */
            if (node->cond)
                gen_expr(gen, node->cond);
            else
                emit_mov_rax_imm64(gen, 1); /* no cond = infinite loop (true) */

            emit_test_rax_rax(gen);
            size_t jz_patch = emit_jcc_placeholder(gen, CC_E); /* jz loop_end */

            int prev_break = gen->break_label;
            int prev_continue = gen->continue_label;
            gen->loop_depth++;

            /* body */
            if (node->body)
                gen_stmt(gen, node->body);

            /* continue target = update */
            size_t continue_label = gen->code_size;

            /* update */
            if (node->update)
                gen_expr(gen, node->update);

            /* jmp loop_top */
            size_t jmp_patch = emit_jmp_placeholder(gen);
            patch_rel32(gen, jmp_patch, loop_top);

            size_t loop_end = gen->code_size;
            patch_rel32(gen, jz_patch, loop_end);
            gen->loop_depth--;
            gen->break_label = prev_break;
            gen->continue_label = prev_continue;
            break;
        }

        case HC_AST_VAR_DECL:
            /* Allocate stack slot for variable */
            if (node->init) {
                gen_expr(gen, node->init);
                /* Store rax to stack: mov [rbp - offset], rax */
                int offset = gen->symbols.stack_size + 8;
                gen->symbols.stack_size += 8;
                if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                    strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                            node->ident, HC_MAX_IDENT_LEN - 1);
                    gen->symbols.locals[gen->symbols.n_locals].stack_offset = offset;
                    gen->symbols.n_locals++;
                }
                /* mov [rbp - offset], rax: 48 89 85 disp32 */
                emit_byte(gen, 0x48); /* REX.W */
                emit_byte(gen, 0x89); /* mov */
                emit_byte(gen, 0x85); /* [rbp+disp32] */
                emit_dword(gen, (uint32_t)(-(int32_t)offset & 0xFFFFFFFF));
            }
            break;

        case HC_AST_FUNC_DECL:
            /* Generate function body */
            emit_prologue(gen);
            /* Allocate stack frame for locals */
            if (node->body)
                gen_stmt(gen, node->body);
            emit_epilogue(gen);
            break;

        case HC_AST_BREAK:
            /* TODO: emit jump to break label */
            break;

        case HC_AST_CONTINUE:
            /* TODO: emit jump to continue label */
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
    HCLexer lex;
    hc_lex_init(&lex, source);

    if (lex.has_error) return 0;

    HCParser parse;
    hc_parse_init(&parse, &lex);

    /* Try parsing as expression first, then as statement */
    HCASTNode *ast = hc_parse_expr(&parse);

    /* If expression parsing failed or didn't consume all input, try statement */
    if (parse.has_error || (hc_parse_peek(&parse) != HC_TOK_EOF && hc_parse_peek(&parse) != HC_TOK_SEMI)) {
        hc_ast_free(ast);
        parse.has_error = false;
        parse.n_errors = 0;
        /* Re-lex from start */
        hc_lex_init(&lex, source);
        hc_parse_init(&parse, &lex);
        ast = hc_parse_stmt(&parse);
    }

    if (parse.has_error || !ast) {
        hc_ast_free(ast);
        return 0;
    }

    HCGen gen;
    hc_gen_init(&gen);
    emit_prologue(&gen);

    if (ast->kind == HC_AST_EXPR_STMT || ast->kind == HC_AST_RETURN ||
        ast->kind == HC_AST_IF || ast->kind == HC_AST_WHILE ||
        ast->kind == HC_AST_FOR || ast->kind == HC_AST_DO_WHILE ||
        ast->kind == HC_AST_BLOCK || ast->kind == HC_AST_VAR_DECL ||
        ast->kind == HC_AST_FUNC_DECL) {
        gen_stmt(&gen, ast);
        emit_epilogue(&gen);
    } else {
        gen_expr(&gen, ast);
        emit_epilogue(&gen);
    }

    hc_ast_free(ast);

    if (gen.code_size == 0 || gen.has_error) {
        free(gen.code);
        return 0;
    }

    void *exec = jit_alloc_exec(gen.code_size);
    if (!exec) { free(gen.code); return 0; }

    memcpy(exec, gen.code, gen.code_size);
    int64_t result = JIT_CALL(exec);
    jit_free_exec(exec, gen.code_size);
    free(gen.code);

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
