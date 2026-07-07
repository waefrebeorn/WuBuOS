/*
 * holyc_codegen_emit.c  --  HolyC Code Generator: x86-64 Emission Helpers
 * Low-level byte emission, instruction patterns, patching utilities.
 */

#include "holyc_codegen_internal.h"

/* ====================================================================
 * CODE EMISSION HELPERS
 * ==================================================================== */

void emit_byte(HCGen *gen, uint8_t b) {
    if (gen->code_size >= gen->code_cap) {
        gen->code_cap = gen->code_cap ? gen->code_cap * 2 : 256;
        gen->code = (uint8_t *)realloc(gen->code, gen->code_cap);
    }
    gen->code[gen->code_size++] = b;
}

void emit_data_byte(HCGen *gen, uint8_t b) {
    if (gen->data_size >= gen->data_cap) {
        gen->data_cap = gen->data_cap ? gen->data_cap * 2 : 256;
        gen->data = (uint8_t *)realloc(gen->data, gen->data_cap);
    }
    gen->data[gen->data_size++] = b;
}

void emit_word(HCGen *gen, uint16_t w) {
    emit_byte(gen, (uint8_t)(w & 0xFF));
    emit_byte(gen, (uint8_t)((w >> 8) & 0xFF));
}

void emit_dword(HCGen *gen, uint32_t d) {
    emit_byte(gen, (uint8_t)(d & 0xFF));
    emit_byte(gen, (uint8_t)((d >> 8) & 0xFF));
    emit_byte(gen, (uint8_t)((d >> 16) & 0xFF));
    emit_byte(gen, (uint8_t)((d >> 24) & 0xFF));
}

void emit_data_dword(HCGen *gen, uint32_t d) {
    emit_data_byte(gen, (uint8_t)(d & 0xFF));
    emit_data_byte(gen, (uint8_t)((d >> 8) & 0xFF));
    emit_data_byte(gen, (uint8_t)((d >> 16) & 0xFF));
    emit_data_byte(gen, (uint8_t)((d >> 24) & 0xFF));
}

void emit_data_qword(HCGen *gen, uint64_t q) {
    emit_data_dword(gen, (uint32_t)(q & 0xFFFFFFFF));
    emit_data_dword(gen, (uint32_t)((q >> 32) & 0xFFFFFFFF));
}

void emit_qword(HCGen *gen, uint64_t q) {
    emit_dword(gen, (uint32_t)(q & 0xFFFFFFFF));
    emit_dword(gen, (uint32_t)((q >> 32) & 0xFFFFFFFF));
}

/* ====================================================================
 * PATCH HELPERS
 * ==================================================================== */

void patch_rel32(HCGen *gen, size_t patch_pos, size_t target_pos) {
    int32_t rel = (int32_t)((int64_t)target_pos - (int64_t)(patch_pos + 4));
    gen->code[patch_pos + 0] = (uint8_t)(rel & 0xFF);
    gen->code[patch_pos + 1] = (uint8_t)((rel >> 8) & 0xFF);
    gen->code[patch_pos + 2] = (uint8_t)((rel >> 16) & 0xFF);
    gen->code[patch_pos + 3] = (uint8_t)((rel >> 24) & 0xFF);
}

/* ====================================================================
 * x86-64 INSTRUCTION PATTERNS
 * ==================================================================== */

void emit_mov_rax_imm64(HCGen *gen, int64_t val) {
    emit_byte(gen, 0x48);  /* REX.W */
    emit_byte(gen, 0xB8);  /* mov rax, imm64 */
    emit_qword(gen, (uint64_t)val);
}

void emit_mov_rdi_imm64(HCGen *gen, int64_t val) {
    emit_byte(gen, 0x48);  /* REX.W */
    emit_byte(gen, 0xBF);  /* mov rdi, imm64 */
    emit_qword(gen, (uint64_t)val);
}

void emit_add_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x01); emit_byte(gen, 0xF8);
}

void emit_sub_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x29); emit_byte(gen, 0xF8);
}

void emit_mul_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x0F);
    emit_byte(gen, 0xAF); emit_byte(gen, 0xC7);
}

void emit_div_rax_rdi(HCGen *gen) {
    /* cqo (sign-extend rax into rdx:rax) */
    emit_byte(gen, 0x48); emit_byte(gen, 0x99);
    /* idiv rdi */
    emit_byte(gen, 0x48); emit_byte(gen, 0xF7);
    emit_byte(gen, 0xFF);
}

void emit_udiv_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x31);
    emit_byte(gen, 0xD2);  /* xor rdx, rdx */
    emit_byte(gen, 0x48); emit_byte(gen, 0xF7);
    emit_byte(gen, 0xF7);  /* div rdi */
}

void emit_xchg_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x87); emit_byte(gen, 0xF8);
}

void emit_neg_rax(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0xF7); emit_byte(gen, 0xD8);
}

void emit_not_rax(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0xF7); emit_byte(gen, 0xD0);
}

void emit_cmp_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x39); emit_byte(gen, 0xF8);
}

void emit_test_rax_rax(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x85); emit_byte(gen, 0xC0);
}

void emit_mov_rdi_rax(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xC7);
}

void emit_xor_rax_rax(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x31); emit_byte(gen, 0xC0);
}

void emit_mov_rax_1(HCGen *gen) {
    emit_mov_rax_imm64(gen, 1);
}

void emit_ret(HCGen *gen) {
    emit_byte(gen, 0xC3);
}

void emit_prologue(HCGen *gen) {
    emit_byte(gen, 0x55);                    /* push rbp */
    emit_byte(gen, 0x48); emit_byte(gen, 0x89);
    emit_byte(gen, 0xE5);                    /* mov rbp, rsp */
    /* Reserve a local frame so VAR_DECL slots at [rbp-8..] are owned stack
     * space (not the caller's red zone / return address). 256 bytes covers
     * up to 32 I64 locals, ample for the current tests. */
    emit_byte(gen, 0x48); emit_byte(gen, 0x81); emit_byte(gen, 0xEC);
    emit_dword(gen, 0x00000100);             /* sub rsp, 256 */
    gen->has_prologue = true;
}

void emit_epilogue(HCGen *gen) {
    emit_byte(gen, 0xC9);                    /* leave (mov rsp,rbp; pop rbp) */
    emit_ret(gen);
}

/* ====================================================================
 * CONDITIONAL SET PATTERNS
 * ==================================================================== */

void emit_setcc(HCGen *gen, uint8_t set_op) {
    emit_byte(gen, 0x0F);               /* Two-byte opcode prefix */
    emit_byte(gen, set_op);             /* setcc al */
    emit_byte(gen, 0xC0);              /* al */
    /* movzx rax, al */
    emit_byte(gen, 0x48);              /* REX.W */
    emit_byte(gen, 0x0F);
    emit_byte(gen, 0xB6);
    emit_byte(gen, 0xC0);
}

/* ====================================================================
 * JUMP EMISSION (5-byte, always patchable)
 * ==================================================================== */

size_t emit_jcc_placeholder(HCGen *gen, uint8_t cc) {
    emit_byte(gen, 0x0F);
    emit_byte(gen, 0x80 | (cc & 0x0F));
    size_t patch_pos = gen->code_size;
    emit_dword(gen, 0);  /* placeholder */
    return patch_pos;
}

size_t emit_jmp_placeholder(HCGen *gen) {
    emit_byte(gen, 0xE9);
    size_t patch_pos = gen->code_size;
    emit_dword(gen, 0);  /* placeholder */
    return patch_pos;
}