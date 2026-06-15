/*
 * holyc_codegen.c  --  My Seed HolyC Code Generator
 *
 * Emits x86-64 machine code from HolyC AST.
 * Uses our JIT mmap backend for executable memory.
 *
 * Register convention (System V AMD64):
 *   rdi, rsi, rdx, rcx, r8, r9   --  argument passing
 *   rax                           --  return value
 *   rbx, rbp, r12-r15            --  callee-saved
 *   rsp                           --  stack pointer
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

/* JIT_CALL macro from jit.h  --  call function pointer with 0 args */
#ifndef JIT_CALL
#define JIT_CALL(fn) ((int64_t(*)(void))(fn))()
#endif

/* -- Code Emission Helpers ---------------------------------------- */

static void emit_byte(HCGen *gen, uint8_t b) {
    if (gen->code_size >= gen->code_cap) {
        gen->code_cap = gen->code_cap ? gen->code_cap * 2 : 256;
        gen->code = (uint8_t *)realloc(gen->code, gen->code_cap);
    }
    gen->code[gen->code_size++] = b;
}

static void emit_data_byte(HCGen *gen, uint8_t b) {
    if (gen->data_size >= gen->data_cap) {
        gen->data_cap = gen->data_cap ? gen->data_cap * 2 : 256;
        gen->data = (uint8_t *)realloc(gen->data, gen->data_cap);
    }
    gen->data[gen->data_size++] = b;
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

static void emit_data_dword(HCGen *gen, uint32_t d) {
    emit_data_byte(gen, (uint8_t)(d & 0xFF));
    emit_data_byte(gen, (uint8_t)((d >> 8) & 0xFF));
    emit_data_byte(gen, (uint8_t)((d >> 16) & 0xFF));
    emit_data_byte(gen, (uint8_t)((d >> 24) & 0xFF));
}

static void emit_data_qword(HCGen *gen, uint64_t q) {
    emit_data_dword(gen, (uint32_t)(q & 0xFFFFFFFF));
    emit_data_dword(gen, (uint32_t)((q >> 32) & 0xFFFFFFFF));
}

static void emit_qword(HCGen *gen, uint64_t q) {
    emit_dword(gen, (uint32_t)(q & 0xFFFFFFFF));
    emit_dword(gen, (uint32_t)((q >> 32) & 0xFFFFFFFF));
}

/* -- Patch Helpers ------------------------------------------------- */

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

/* -- x86-64 Instruction Patterns ---------------------------------- */

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

/* -- Conditional Set Patterns ------------------------------------- */

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

/* -- Jump Emission (5-byte, always patchable) --------------------- */

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

/* -- Code Gen Init ------------------------------------------------ */

void hc_gen_init(HCGen *gen) {
    memset(gen, 0, sizeof(*gen));
}

/* -- Code Gen for Expressions ------------------------------------- */

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
            /* Store string in data section and emit pointer */
            {
                size_t str_len = strlen(node->str_val);
                size_t str_offset = gen->data_size;
                /* Emit string bytes + null terminator */
                for (size_t i = 0; i < str_len; i++) {
                    emit_data_byte(gen, (uint8_t)node->str_val[i]);
                }
                emit_data_byte(gen, 0); /* null terminator */
                /* Align to 8 bytes */
                while (gen->data_size % 8 != 0) {
                    emit_data_byte(gen, 0);
                }
                /* mov rax, data_section_base + str_offset */
                emit_mov_rax_imm64(gen, (int64_t)(size_t)(gen->data + str_offset));
            }
            break;

        /* Identifiers  --  for now, look up in symbol table */
        case HC_AST_IDENT:
            if (gen->symbols.n_locals > 0) {
                /* Look up variable in symbol table */
                bool found = false;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->ident) == 0) {
                        int off = gen->symbols.locals[i].stack_offset;
                        /* mov rax, [rbp - off] with disp32: 48 8B 85 disp32 */
                        emit_byte(gen, 0x48); /* REX.W */
                        emit_byte(gen, 0x8B); /* mov rax, r/m64 */
                        emit_byte(gen, 0x85); /* modrm: disp32 with rbp */
                        emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    /* Implicit variable - not yet assigned. Return 0. */
                    emit_mov_rax_imm64(gen, 0);
                }
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
            /* Right-hand side → rax, then store to left-hand side */
            gen_expr(gen, node->right);
            if (node->left && node->left->kind == HC_AST_IDENT) {
                /* Look up variable in symbol table */
                bool found = false;
                int off = 0;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->left->ident) == 0) {
                        off = gen->symbols.locals[i].stack_offset;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    /* Implicit variable declaration on first assignment */
                    off = gen->symbols.stack_size + 8;
                    gen->symbols.stack_size += 8;
                    if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                        strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                                node->left->ident, HC_MAX_IDENT_LEN - 1);
                        gen->symbols.locals[gen->symbols.n_locals].stack_offset = off;
                        gen->symbols.n_locals++;
                    }
                }
                /* mov [rbp - off], rax: 48 89 85 disp32 */
                emit_byte(gen, 0x48); /* REX.W */
                emit_byte(gen, 0x89); /* mov */
                emit_byte(gen, 0x85); /* [rbp+disp32] */
                emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
            }
            break;

        /* Compound assignments: x += y means x = x + y */
        case HC_AST_ADD_ASSIGN:
            if (node->left && node->left->kind == HC_AST_IDENT) {
                /* Load current value of left var (with implicit declaration) */
                bool found = false;
                int off = 0;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->left->ident) == 0) {
                        off = gen->symbols.locals[i].stack_offset;
                        /* mov rax, [rbp - off] with disp32: 48 8B 85 disp32 */
                        emit_byte(gen, 0x48);
                        emit_byte(gen, 0x8B);
                        emit_byte(gen, 0x85);
                        emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    /* Implicit declaration - initialize to 0 */
                    off = gen->symbols.stack_size + 8;
                    gen->symbols.stack_size += 8;
                    if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                        strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                                node->left->ident, HC_MAX_IDENT_LEN - 1);
                        gen->symbols.locals[gen->symbols.n_locals].stack_offset = off;
                        gen->symbols.n_locals++;
                    }
                    emit_mov_rax_imm64(gen, 0);
                }
                /* Add right-hand side */
                emit_mov_rdi_rax(gen);
                gen_expr(gen, node->right);
                emit_xchg_rax_rdi(gen);
                emit_add_rax_rdi(gen);
                /* Store back */
                /* mov [rbp - off], rax: 48 89 85 disp32 */
                emit_byte(gen, 0x48); /* REX.W */
                emit_byte(gen, 0x89); /* mov */
                emit_byte(gen, 0x85); /* [rbp+disp32] */
                emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
            }
            break;

        case HC_AST_SUB_ASSIGN:
            if (node->left && node->left->kind == HC_AST_IDENT) {
                bool found = false;
                int off = 0;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->left->ident) == 0) {
                        off = gen->symbols.locals[i].stack_offset;
                        emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x85);
                        emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    off = gen->symbols.stack_size + 8;
                    gen->symbols.stack_size += 8;
                    if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                        strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                                node->left->ident, HC_MAX_IDENT_LEN - 1);
                        gen->symbols.locals[gen->symbols.n_locals].stack_offset = off;
                        gen->symbols.n_locals++;
                    }
                }
                emit_mov_rdi_rax(gen);
                gen_expr(gen, node->right);
                emit_xchg_rax_rdi(gen);
                emit_sub_rax_rdi(gen);
                emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x85);
                emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
            }
            break;

        case HC_AST_MUL_ASSIGN:
            if (node->left && node->left->kind == HC_AST_IDENT) {
                bool found = false;
                int off = 0;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->left->ident) == 0) {
                        off = gen->symbols.locals[i].stack_offset;
                        emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x85);
                        emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    off = gen->symbols.stack_size + 8;
                    gen->symbols.stack_size += 8;
                    if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                        strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                                node->left->ident, HC_MAX_IDENT_LEN - 1);
                        gen->symbols.locals[gen->symbols.n_locals].stack_offset = off;
                        gen->symbols.n_locals++;
                    }
                }
                emit_mov_rdi_rax(gen);
                gen_expr(gen, node->right);
                emit_xchg_rax_rdi(gen);
                emit_mul_rax_rdi(gen);
                emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x85);
                emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
            }
            break;

        case HC_AST_DIV_ASSIGN:
            if (node->left && node->left->kind == HC_AST_IDENT) {
                bool found = false;
                int off = 0;
                for (int i = 0; i < gen->symbols.n_locals; i++) {
                    if (strcmp(gen->symbols.locals[i].name, node->left->ident) == 0) {
                        off = gen->symbols.locals[i].stack_offset;
                        emit_byte(gen, 0x48); emit_byte(gen, 0x8B); emit_byte(gen, 0x85);
                        emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    off = gen->symbols.stack_size + 8;
                    gen->symbols.stack_size += 8;
                    if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                        strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                                node->left->ident, HC_MAX_IDENT_LEN - 1);
                        gen->symbols.locals[gen->symbols.n_locals].stack_offset = off;
                        gen->symbols.n_locals++;
                    }
                }
                emit_mov_rdi_rax(gen);
                gen_expr(gen, node->right);
                emit_xchg_rax_rdi(gen);
                emit_div_rax_rdi(gen);
                emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x85);
                emit_dword(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF));
            }
            break;

        /* Function call */
        case HC_AST_FUNC_CALL:
            /* System V AMD64 ABI: args in rdi, rsi, rdx, rcx, r8, r9 */
            {
                int n_args = node->n_args;
                /* Evaluate all args first (right-to-left for stack, but we use registers) */
                for (int i = n_args - 1; i >= 0; i--) {
                    gen_expr(gen, node->args[i]);
                    switch (i) {
                        case 0: emit_mov_rdi_rax(gen); break;  /* arg0 → rdi */
                        case 1: /* mov rsi, rax: 48 89 C6 */
                                emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xC6); break;
                        case 2: /* mov rdx, rax: 48 89 C2 */
                                emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xC2); break;
                        case 3: /* mov rcx, rax: 48 89 C1 */
                                emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xC1); break;
                        case 4: /* mov r8, rax: 49 89 C0 */
                                emit_byte(gen, 0x49); emit_byte(gen, 0x89); emit_byte(gen, 0xC0); break;
                        case 5: /* mov r9, rax: 49 89 C1 */
                                emit_byte(gen, 0x49); emit_byte(gen, 0x89); emit_byte(gen, 0xC1); break;
                        default: /* TODO: stack args for >6 params */ break;
                    }
                }
                /* Get function address from callee (should be ident or function pointer) */
                if (node->callee && node->callee->kind == HC_AST_IDENT) {
                    /* Look up function in function table */
                    void *func_addr = NULL;
                    for (int i = 0; i < gen->n_functions; i++) {
                        if (strcmp(gen->functions[i].name, node->callee->ident) == 0) {
                            func_addr = gen->functions[i].func_ptr;
                            break;
                        }
                    }
                    if (func_addr) {
                        /* mov rax, func_addr */
                        emit_mov_rax_imm64(gen, (int64_t)func_addr);
                        /* call rax: FF D0 */
                        emit_byte(gen, 0xFF); emit_byte(gen, 0xD0);
                    } else {
                        /* Function not found - emit call to 0 (will crash at runtime) */
                        emit_mov_rax_imm64(gen, 0);
                        emit_byte(gen, 0xFF); emit_byte(gen, 0xD0);
                    }
                } else {
                    emit_mov_rax_imm64(gen, 0);
                }
            }
            break;

        /* Ternary: cond ? then : else */
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

        /* Member access: expr.member */
        case HC_AST_MEMBER: {
            /* Evaluate base expression (struct pointer or value) */
            gen_expr(gen, node->left);
            /* rax now contains base address */
            /* Find member offset */
            if (node->left->type && node->left->type->kind == HC_TYPE_STRUCT) {
                HCType *st = node->left->type;
                bool found = false;
                for (int i = 0; i < st->n_members; i++) {
                    if (strcmp(st->members[i].name, node->ident) == 0) {
                        int off = st->members[i].offset;
                        /* mov rax, [rax + off]: 48 8B 80 disp32 */
                        emit_byte(gen, 0x48); /* REX.W */
                        emit_byte(gen, 0x8B); /* mov */
                        emit_byte(gen, 0x80); /* modrm: rax, [rax+disp32] */
                        emit_dword(gen, (uint32_t)off);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    emit_mov_rax_imm64(gen, 0);
                }
            }
            break;
        }

        /* Arrow access: expr->member (ptr to struct) */
        case HC_AST_ARROW: {
            /* Evaluate base expression (pointer to struct) */
            gen_expr(gen, node->left);
            /* rax now contains base address (already a pointer) */
            if (node->left->type && node->left->type->kind == HC_TYPE_PTR &&
                node->left->type->base && node->left->type->base->kind == HC_TYPE_STRUCT) {
                HCType *st = node->left->type->base;
                bool found = false;
                for (int i = 0; i < st->n_members; i++) {
                    if (strcmp(st->members[i].name, node->ident) == 0) {
                        int off = st->members[i].offset;
                        /* mov rax, [rax + off]: 48 8B 80 disp32 */
                        emit_byte(gen, 0x48); /* REX.W */
                        emit_byte(gen, 0x8B); /* mov */
                        emit_byte(gen, 0x80); /* modrm: rax, [rax+disp32] */
                        emit_dword(gen, (uint32_t)off);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    emit_mov_rax_imm64(gen, 0);
                }
            }
            break;
        }

        default:
            /* Unknown expression  --  emit 0 */
            emit_mov_rax_imm64(gen, 0);
            break;
    }

    return 0;
}

/* -- Code Gen for Statements -------------------------------------- */

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
            int depth = gen->loop_depth;
            gen_expr(gen, node->cond);
            emit_test_rax_rax(gen);
            size_t jz_patch = emit_jcc_placeholder(gen, CC_E); /* jz loop_end */
            gen->loop_depth++;
            gen->n_break_patches[depth] = 0;
            gen->n_continue_patches[depth] = 0;
            gen_stmt(gen, node->body);
            /* Continue target is loop_top (condition check) */
            size_t continue_label = loop_top;
            /* jmp loop_top (back jump) */
            size_t jmp_patch = emit_jmp_placeholder(gen);
            patch_rel32(gen, jmp_patch, loop_top);
            size_t loop_end = gen->code_size;
            patch_rel32(gen, jz_patch, loop_end);
            /* Patch all break statements in this loop to jump to loop_end */
            for (int i = 0; i < gen->n_break_patches[depth]; i++) {
                patch_rel32(gen, gen->break_patches[depth][i], loop_end);
            }
            /* Patch all continue statements to jump to continue_label */
            for (int i = 0; i < gen->n_continue_patches[depth]; i++) {
                patch_rel32(gen, gen->continue_patches[depth][i], continue_label);
            }
            gen->loop_depth--;
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
            int depth = gen->loop_depth;
            gen->loop_depth++;
            gen->n_break_patches[depth] = 0;
            gen->n_continue_patches[depth] = 0;
            gen_stmt(gen, node->body);
            /* Continue target is the condition check */
            size_t continue_label = gen->code_size;
            gen_expr(gen, node->cond);
            emit_test_rax_rax(gen);
            size_t jnz_patch = emit_jcc_placeholder(gen, CC_NE); /* jnz loop_top */
            patch_rel32(gen, jnz_patch, loop_top);
            size_t loop_end = gen->code_size;
            /* Patch all break statements in this loop to jump to loop_end */
            for (int i = 0; i < gen->n_break_patches[depth]; i++) {
                patch_rel32(gen, gen->break_patches[depth][i], loop_end);
            }
            /* Patch all continue statements to jump to continue_label */
            for (int i = 0; i < gen->n_continue_patches[depth]; i++) {
                patch_rel32(gen, gen->continue_patches[depth][i], continue_label);
            }
            gen->loop_depth--;
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
            int depth = gen->loop_depth;
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

            gen->loop_depth++;
            gen->n_break_patches[depth] = 0;
            gen->n_continue_patches[depth] = 0;

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
            /* Patch all break statements in this loop to jump to loop_end */
            for (int i = 0; i < gen->n_break_patches[depth]; i++) {
                patch_rel32(gen, gen->break_patches[depth][i], loop_end);
            }
            /* Patch all continue statements to jump to continue_label */
            for (int i = 0; i < gen->n_continue_patches[depth]; i++) {
                patch_rel32(gen, gen->continue_patches[depth][i], continue_label);
            }
            gen->loop_depth--;
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
            /* Generate function body and save function pointer */
            /* Save current code state */
            uint8_t *saved_code = gen->code;
            size_t saved_code_size = gen->code_size;
            size_t saved_code_cap = gen->code_cap;
            HCSymTab saved_symbols = gen->symbols;
            int saved_n_functions = gen->n_functions;
            HCFunction saved_functions[HC_MAX_FUNCTIONS];
            memcpy(saved_functions, gen->functions, sizeof(HCFunction) * HC_MAX_FUNCTIONS);
            
            gen->code = NULL;
            gen->code_size = 0;
            gen->code_cap = 0;
            /* Reset symbols but keep functions */
            memset(&gen->symbols, 0, sizeof(HCSymTab));
            
            emit_prologue(gen);
            
            /* Add function parameters to symbol table before compiling body */
            for (int i = 0; i < node->n_params; i++) {
                int offset = gen->symbols.stack_size + 8;
                gen->symbols.stack_size += 8;
                if (gen->symbols.n_locals < HC_MAX_LOCALS) {
                    strncpy(gen->symbols.locals[gen->symbols.n_locals].name,
                            node->param_names[i], HC_MAX_IDENT_LEN - 1);
                    gen->symbols.locals[gen->symbols.n_locals].stack_offset = offset;
                    gen->symbols.n_locals++;
                }
                /* Store parameter from register to stack slot */
                switch (i) {
                    case 0: /* rdi -> [rbp - offset] */
                        /* mov [rbp - offset], rdi: 48 89 BD disp32 */
                        emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xBD);
                        emit_dword(gen, (uint32_t)(-(int32_t)offset & 0xFFFFFFFF));
                        break;
                    case 1: /* rsi -> [rbp - offset] */
                        /* mov [rbp - offset], rsi: 48 89 B5 disp32 */
                        emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xB5);
                        emit_dword(gen, (uint32_t)(-(int32_t)offset & 0xFFFFFFFF));
                        break;
                    case 2: /* rdx -> [rbp - offset] */
                        /* mov [rbp - offset], rdx: 48 89 95 disp32 */
                        emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x95);
                        emit_dword(gen, (uint32_t)(-(int32_t)offset & 0xFFFFFFFF));
                        break;
                    case 3: /* rcx -> [rbp - offset] */
                        /* mov [rbp - offset], rcx: 48 89 8D disp32 */
                        emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0x8D);
                        emit_dword(gen, (uint32_t)(-(int32_t)offset & 0xFFFFFFFF));
                        break;
                    case 4: /* r8 -> [rbp - offset] */
                        /* mov [rbp - offset], r8: 4C 89 85 disp32 */
                        emit_byte(gen, 0x4C); emit_byte(gen, 0x89); emit_byte(gen, 0x85);
                        emit_dword(gen, (uint32_t)(-(int32_t)offset & 0xFFFFFFFF));
                        break;
                    case 5: /* r9 -> [rbp - offset] */
                        /* mov [rbp - offset], r9: 4C 89 8D disp32 */
                        emit_byte(gen, 0x4C); emit_byte(gen, 0x89); emit_byte(gen, 0x8D);
                        emit_dword(gen, (uint32_t)(-(int32_t)offset & 0xFFFFFFFF));
                        break;
                }
            }
            
            /* Allocate stack frame for locals */
            if (node->body)
                gen_stmt(gen, node->body);
            emit_epilogue(gen);

            /* Allocate executable memory for this function */
            if (gen->code_size > 0 && gen->n_functions < HC_MAX_FUNCTIONS) {
                void *exec = jit_alloc_exec(gen->code_size);
                if (exec) {
                    memcpy(exec, gen->code, gen->code_size);
                    /* Restore main code buffer */
                    gen->code = saved_code;
                    gen->code_size = saved_code_size;
                    gen->code_cap = saved_code_cap;
                    gen->symbols = saved_symbols;
                    gen->n_functions = saved_n_functions;
                    memcpy(gen->functions, saved_functions, sizeof(HCFunction) * HC_MAX_FUNCTIONS);
                    
                    strncpy(gen->functions[gen->n_functions].name,
                            node->ident, HC_MAX_IDENT_LEN - 1);
                    gen->functions[gen->n_functions].func_ptr = exec;
                    gen->functions[gen->n_functions].n_params = node->n_params;
                    gen->n_functions++;
                } else {
                    /* Restore on failure */
                    free(gen->code);
                    gen->code = saved_code;
                    gen->code_size = saved_code_size;
                    gen->code_cap = saved_code_cap;
                    gen->symbols = saved_symbols;
                }
            }
            break;

        case HC_AST_BREAK:
            /* Emit jump to loop end - will be patched when loop ends */
            if (gen->loop_depth > 0 && gen->loop_depth <= 10) {
                int depth = gen->loop_depth - 1;
                if (gen->n_break_patches[depth] < 16) {
                    size_t patch = emit_jmp_placeholder(gen);
                    gen->break_patches[depth][gen->n_break_patches[depth]++] = patch;
                }
            }
            break;

        case HC_AST_CONTINUE:
            /* Emit jump to loop continue/condition - will be patched when loop ends */
            if (gen->loop_depth > 0 && gen->loop_depth <= 10) {
                int depth = gen->loop_depth - 1;
                if (gen->n_continue_patches[depth] < 16) {
                    size_t patch = emit_jmp_placeholder(gen);
                    gen->continue_patches[depth][gen->n_continue_patches[depth]++] = patch;
                }
            }
            break;

        default:
            /* Try as expression */
            return gen_expr(gen, node);
    }

    return 0;
}

/* -- Public API --------------------------------------------------- */

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

/* -- Top-Level Compile and Execute -------------------------------- */

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
        free(gen.data);
        return NULL;
    }

    /* Allocate executable memory */
    void *exec = jit_alloc_exec(gen.code_size + gen.data_size);
    if (!exec) { free(gen.code); free(gen.data); return NULL; }

    memcpy(exec, gen.code, gen.code_size);
    /* Copy data section after code for string literals */
    if (gen.data_size > 0) {
        memcpy((uint8_t *)exec + gen.code_size, gen.data, gen.data_size);
    }
    if (out_size) *out_size = gen.code_size + gen.data_size;
    free(gen.code);
    free(gen.data);

    return exec;
}

int64_t hc_eval(const char *source) {
    HCLexer lex;
    hc_lex_init(&lex, source);

    if (lex.has_error) return 0;

    HCParser parse;
    hc_parse_init(&parse, &lex);

    /* Try parsing as expression first */
    HCASTNode *ast = hc_parse_expr(&parse);

    /* If expression parsing failed or didn't consume all input, try block parsing */
    if (parse.has_error || (hc_parse_peek(&parse) != HC_TOK_EOF && hc_parse_peek(&parse) != HC_TOK_SEMI)) {
        hc_ast_free(ast);
        parse.has_error = false;
        parse.n_errors = 0;
        /* Re-lex from start */
        hc_lex_init(&lex, source);
        hc_parse_init(&parse, &lex);
        
        /* Check if source starts with a control keyword */
        const char *p = source;
        while (*p && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) p++;
        bool starts_with_keyword = false;
        if (strncmp(p, "if ", 3) == 0 || strncmp(p, "while ", 6) == 0 ||
            strncmp(p, "for ", 4) == 0 || strncmp(p, "do ", 3) == 0 ||
            strncmp(p, "return", 6) == 0 || strncmp(p, "break", 5) == 0 ||
            strncmp(p, "continue", 8) == 0 || *p == '{') {
            starts_with_keyword = true;
        }
        
        /* Wrap in braces to parse as block if it contains semicolons AND doesn't start with keyword */
        bool has_semicolon = false;
        p = source;
        while (*p) {
            if (*p == ';') { has_semicolon = true; break; }
            p++;
        }
        
        if (has_semicolon && !starts_with_keyword) {
            /* Create a temporary buffer with braces */
            size_t len = strlen(source);
            char *wrapped = malloc(len + 3);
            sprintf(wrapped, "{ %s }", source);
            hc_lex_init(&lex, wrapped);
            hc_parse_init(&parse, &lex);
            /* hc_lex_init already primes the first token (which is `{`) */
            ast = parse_block(&parse);
            free(wrapped);
        } else {
            ast = hc_parse_stmt(&parse);
        }
    }

    if (parse.has_error || !ast) {
        hc_ast_free(ast);
        return 0;
    }

    HCGen gen;
    hc_gen_init(&gen);
    emit_prologue(&gen);

    if (ast->kind == HC_AST_BLOCK) {
        gen_stmt(&gen, ast);
    } else if (ast->kind == HC_AST_EXPR_STMT || ast->kind == HC_AST_RETURN ||
        ast->kind == HC_AST_IF || ast->kind == HC_AST_WHILE ||
        ast->kind == HC_AST_FOR || ast->kind == HC_AST_DO_WHILE ||
        ast->kind == HC_AST_VAR_DECL || ast->kind == HC_AST_FUNC_DECL) {
        gen_stmt(&gen, ast);
    } else {
        gen_expr(&gen, ast);
    }
    emit_epilogue(&gen);

    hc_ast_free(ast);

    if (gen.code_size == 0 || gen.has_error) {
        free(gen.code);
        free(gen.data);
        return 0;
    }

    void *exec = jit_alloc_exec(gen.code_size + gen.data_size);
    if (!exec) { free(gen.code); free(gen.data); return 0; }

    memcpy(exec, gen.code, gen.code_size);
    /* Copy data section after code - string literals need to be readable */
    if (gen.data_size > 0) {
        memcpy((uint8_t *)exec + gen.code_size, gen.data, gen.data_size);
    }
    int64_t result = JIT_CALL(exec);
    jit_free_exec(exec, gen.code_size + gen.data_size);
    free(gen.code);
    free(gen.data);

    return result;
}

void *hc_compile_func(const char *source, const char *func_name) {
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
        /* No function found  --  treat as expression */
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

    if (gen.code_size == 0) { free(gen.code); free(gen.data); return NULL; }

    void *exec = jit_alloc_exec(gen.code_size + gen.data_size);
    if (!exec) { free(gen.code); free(gen.data); return NULL; }
    memcpy(exec, gen.code, gen.code_size);
    if (gen.data_size > 0) {
        memcpy((uint8_t *)exec + gen.code_size, gen.data, gen.data_size);
    }
    free(gen.code);
    free(gen.data);
    return exec;
}
