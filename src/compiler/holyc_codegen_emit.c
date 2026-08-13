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

/* emit_cvt_f64_to_i64: rax currently holds an F64 bit-pattern; convert
 * it (with truncation toward zero) to a signed I64 in rax.
 *   push rax               50
 *   movsd xmm0, [rsp]      F2 0F 10 04 24
 *   add rsp, 8             48 83 C4 08
 *   cvttsd2si rax, xmm0    F2 48 0F 2C C0
 * (NOT movq xmm0,rax — that instruction decodes to a no-op that leaves
 * xmm0 zeroed in the JIT; the stack round-trip is verified reliable.) */
void emit_cvt_f64_to_i64(HCGen *gen) {
    emit_byte(gen, 0x50);                                    /* push rax */
    emit_byte(gen, 0xF2); emit_byte(gen, 0x0F); emit_byte(gen, 0x10);
    emit_byte(gen, 0x04); emit_byte(gen, 0x24);              /* movsd xmm0,[rsp] */
    emit_byte(gen, 0x48); emit_byte(gen, 0x83); emit_byte(gen, 0xC4);
    emit_byte(gen, 0x08);                                    /* add rsp, 8 */
    emit_byte(gen, 0xF2); emit_byte(gen, 0x48); emit_byte(gen, 0x0F);
    emit_byte(gen, 0x2C); emit_byte(gen, 0xC0);              /* cvttsd2si rax, xmm0 */
}

/* emit_cvt_i64_to_f64: rax holds a signed I64; convert to F64 bit-pattern
 * in rax.
 *   push rax               50
 *   cvtsi2sd xmm0,[rsp]    F2 48 0F 2A 04 24
 *   add rsp, 8             48 83 C4 08
 *   movq rax, xmm0         66 48 0F 7E C0   */
void emit_cvt_i64_to_f64(HCGen *gen) {
    emit_byte(gen, 0x50);                                    /* push rax */
    emit_byte(gen, 0xF2); emit_byte(gen, 0x48); emit_byte(gen, 0x0F);
    emit_byte(gen, 0x2A); emit_byte(gen, 0x04); emit_byte(gen, 0x24); /* cvtsi2sd xmm0,[rsp] */
    emit_byte(gen, 0x48); emit_byte(gen, 0x83); emit_byte(gen, 0xC4);
    emit_byte(gen, 0x08);                                    /* add rsp, 8 */
    emit_byte(gen, 0x66); emit_byte(gen, 0x48); emit_byte(gen, 0x0F);
    emit_byte(gen, 0x7E); emit_byte(gen, 0xC0);              /* movq rax, xmm0 */
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

/* emit_mod_rax_rdi: rax %= rdi. cqo; idiv rdi leaves the remainder in
 * rdx; move it back to rax. (Matches HC_AST_MOD's div-then-mov-rdx.) */
void emit_mod_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x99);   /* cqo (rdx:rax = rax) */
    emit_byte(gen, 0x48); emit_byte(gen, 0xF7); emit_byte(gen, 0xFF); /* idiv rdi */
    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xD0); /* mov rax, rdx */
}

/* emit_shl_rax_rdi: rax <<= (rdi & 0x3f). Shift count must be in cl. */
void emit_shl_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xF9); /* mov rcx, rdi */
    emit_byte(gen, 0x48); emit_byte(gen, 0xD3); emit_byte(gen, 0xE0); /* shl rax, cl */
}

/* emit_shr_rax_rdi: rax >>= (rdi & 0x3f). Shift count must be in cl. */
void emit_shr_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x89); emit_byte(gen, 0xF9); /* mov rcx, rdi */
    emit_byte(gen, 0x48); emit_byte(gen, 0xD3); emit_byte(gen, 0xE8); /* shr rax, cl */
}

/* emit_and_rax_rdi: rax &= rdi */
void emit_and_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x21); emit_byte(gen, 0xF8);
}

/* emit_or_rax_rdi: rax |= rdi */
void emit_or_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x09); emit_byte(gen, 0xF8);
}

/* emit_xor_rax_rdi: rax ^= rdi */
void emit_xor_rax_rdi(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x31); emit_byte(gen, 0xF8);
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

/* ====================================================================
 * Tailslayer DRAM-refresh hedge — software-prefetch load hedging
 * ====================================================================
 * DRAM refresh (tREFI ~7.8us) stalls cold reads by ~150-750ns. A
 * `prefetchnta` issued just before the actual load primes the cache so
 * the read overlaps the refresh — the tailslayer trick made implicit
 * in every compiled load. Encoding: 0F 18 /0 = prefetchnta, with the
 * ModRM matching the load's addressing mode. All are 3 bytes + disp32
 * (same shape as the 48 8B/89 05 RIP loads), so the shared global
 * patch formula applies verbatim.
 */

/* Diagnostics: total prefetch instructions ever emitted (for the hedge
 * verification probe). Monotonic across compilations — the probe checks
 * >0 after compiling a load-bearing function. */
unsigned long wubu_hedge_prefetch_count = 0;

static void emit_prefetch_rax_disp32(HCGen *gen, uint32_t disp, uint8_t modrm) {
    emit_byte(gen, 0x0F);
    emit_byte(gen, 0x18);
    emit_byte(gen, modrm);            /* e.g. 0x80 = [rax+disp32] */
    emit_dword(gen, disp);
    wubu_hedge_prefetch_count++;
}

/* prefetchnta [rdi] — array-element loads (INDEX path holds base in rdi) */
void emit_prefetch_rdi(HCGen *gen) {
    if (gen->hedge_loads) {
        emit_byte(gen, 0x0F);
        emit_byte(gen, 0x18);
        emit_byte(gen, 0x07);            /* modrm 07 = [rdi] */
        wubu_hedge_prefetch_count++;
    }
}

/* prefetchnta [rax] — plain pointer dereference loads */
void emit_prefetch_rax(HCGen *gen) {
    if (gen->hedge_loads) {
        emit_byte(gen, 0x0F);
        emit_byte(gen, 0x18);
        emit_byte(gen, 0x00);            /* modrm 00 = [rax] */
        wubu_hedge_prefetch_count++;
    }
}

/* prefetchnta [rax+disp32] — used by sized/element loads */
void emit_prefetch_rax_off(HCGen *gen, int32_t off) {
    emit_prefetch_rax_disp32(gen, (uint32_t)off, 0x80);
}

/* prefetchnta [rbp - off] — stack-local loads */
void emit_prefetch_rbp(HCGen *gen, int32_t off) {
    emit_prefetch_rax_disp32(gen, (uint32_t)(-(int32_t)off & 0xFFFFFFFF), 0x85);
}

/* prefetchnta [rip + disp32] — RIP-relative global loads.
 * Records a global_patch so the disp32 is fixed up to the data section. */
void emit_prefetch_rip(HCGen *gen, size_t global_offset) {
    size_t patch_pos = gen->code_size + 3;   /* disp32 start */
    emit_prefetch_rax_disp32(gen, 0, 0x05);
    if (gen->n_global_patches < 128) {
        gen->global_patches[gen->n_global_patches].code_patch_pos = patch_pos;
        gen->global_patches[gen->n_global_patches].global_offset = global_offset;
        gen->n_global_patches++;
    }
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

/* Emit `rep movsb` with RCX bytes from [RSI] to [RDI]. Caller must set
 * rsi=src, rdi=dst, rcx=count BEFORE calling this. Used for struct-by-value
 * return memcpy (the RETURN path sets up these regs from the operand). */
void emit_rep_movsb(HCGen *gen) {
    emit_byte(gen, 0xF3);   /* REP */
    emit_byte(gen, 0x48);   /* REX.W */
    emit_byte(gen, 0xA4);   /* MOVSB */
}

/* switch dispatch helpers */
void emit_push_rax(HCGen *gen)     { emit_byte(gen, 0x50); }
void emit_pop_rax(HCGen *gen)      { emit_byte(gen, 0x58); }
/* cmp rax, [rsp]  — 48 39 04 24 */
void emit_cmp_rax_mem_rsp(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x39);
    emit_byte(gen, 0x04); emit_byte(gen, 0x24);
}
/* mov rax, [rsp]  — 48 8B 04 24 */
void emit_mov_rax_mem_rsp(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x8B);
    emit_byte(gen, 0x04); emit_byte(gen, 0x24);
}
/* add rsp, 8  — 48 83 C4 08 */
void emit_add_rsp_8(HCGen *gen) {
    emit_byte(gen, 0x48); emit_byte(gen, 0x83);
    emit_byte(gen, 0xC4); emit_byte(gen, 0x08);
}