/*
 * jit_codegen_rv64.c — RISC-V RV64 backend for abstract codegen.
 */
#include "jit_codegen.h"
#include "wubu_rv64.h"
#include <stdlib.h>

static RV64Reg cg_to_rv64(CGReg r) {
    /* CG_REG_0=a0(10), CG_REG_1=a1(11), ..., CG_REG_7=a7(17) */
    /* CG_REG_8+ = t0(5), t1(6), t2(7), t3(28), t4(29), t5(30), t6(31) */
    static const RV64Reg map[] = {
        RV_A0, RV_A1, RV_A2, RV_A3, RV_A4, RV_A5, RV_A6, RV_A7,
        RV_T0, RV_T1, RV_T2, RV_T3, RV_T4, RV_T5, RV_T6, RV_SP,
        RV_SP, RV_S0, RV_T0, RV_X0,
    };
    if (r < 16) return map[r];
    return RV_T0;
}

typedef struct { RV64Enc enc; } RV64Encoder;
static RV64Encoder *rv64_enc(CGEncoder *e) { return (RV64Encoder *)e; }

static void rv64_emit_byte(CGEncoder *e, uint8_t b) { rv64_emit_word(&rv64_enc(e)->enc, b); }
static void rv64_emit_word32(CGEncoder *e, uint32_t w) { rv64_emit_word(&rv64_enc(e)->enc, w); }
static void rv64_emit_word64(CGEncoder *e, uint64_t q) {
    rv64_emit_word(&rv64_enc(e)->enc, (uint32_t)q);
    rv64_emit_word(&rv64_enc(e)->enc, (uint32_t)(q >> 32));
}
static size_t rv64_pos(const CGEncoder *e) { return rv64_enc((CGEncoder *)e)->enc.pos; }
static const uint8_t *rv64_buffer(const CGEncoder *e) { return rv64_enc((CGEncoder *)e)->enc.buf; }

static void rv64_add_imm(CGEncoder *e, CGReg rd, CGReg rn, uint32_t imm) {
    if (imm <= 0xFFF) {
        rv64_addi(&rv64_enc(e)->enc, cg_to_rv64(rd), cg_to_rv64(rn), (int16_t)imm);
    } else {
        rv64_li(&rv64_enc(e)->enc, RV_T6, imm);
        rv64_add(&rv64_enc(e)->enc, cg_to_rv64(rd), cg_to_rv64(rn), RV_T6);
    }
}
static void rv64_sub_imm(CGEncoder *e, CGReg rd, CGReg rn, uint32_t imm) {
    if (imm <= 0x1000) {
        rv64_addi(&rv64_enc(e)->enc, cg_to_rv64(rd), cg_to_rv64(rn), (int16_t)(-imm));
    } else {
        rv64_li(&rv64_enc(e)->enc, RV_T6, imm);
        rv64_sub(&rv64_enc(e)->enc, cg_to_rv64(rd), cg_to_rv64(rn), RV_T6);
    }
}
static void rv64_add_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    rv64_add(&rv64_enc(e)->enc, cg_to_rv64(rd), cg_to_rv64(rn), cg_to_rv64(rm));
}
static void rv64_sub_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    rv64_sub(&rv64_enc(e)->enc, cg_to_rv64(rd), cg_to_rv64(rn), cg_to_rv64(rm));
}
static void rv64_mul_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    rv64_mul(&rv64_enc(e)->enc, cg_to_rv64(rd), cg_to_rv64(rn), cg_to_rv64(rm));
}
static void rv64_div_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    rv64_div(&rv64_enc(e)->enc, cg_to_rv64(rd), cg_to_rv64(rn), cg_to_rv64(rm));
}
static void rv64_mod_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    rv64_rem(&rv64_enc(e)->enc, cg_to_rv64(rd), cg_to_rv64(rn), cg_to_rv64(rm));
}
static void rv64_and_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    rv64_and(&rv64_enc(e)->enc, cg_to_rv64(rd), cg_to_rv64(rn), cg_to_rv64(rm));
}
static void rv64_orr_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    rv64_or(&rv64_enc(e)->enc, cg_to_rv64(rd), cg_to_rv64(rn), cg_to_rv64(rm));
}
static void rv64_eor_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    rv64_xor(&rv64_enc(e)->enc, cg_to_rv64(rd), cg_to_rv64(rn), cg_to_rv64(rm));
}
static void rv64_lsl_imm(CGEncoder *e, CGReg rd, CGReg rn, uint8_t s) {
    rv64_slli(&rv64_enc(e)->enc, cg_to_rv64(rd), cg_to_rv64(rn), s);
}
static void rv64_lsr_imm(CGEncoder *e, CGReg rd, CGReg rn, uint8_t s) {
    rv64_srli(&rv64_enc(e)->enc, cg_to_rv64(rd), cg_to_rv64(rn), s);
}
static void rv64_asr_imm(CGEncoder *e, CGReg rd, CGReg rn, uint8_t s) {
    rv64_srai(&rv64_enc(e)->enc, cg_to_rv64(rd), cg_to_rv64(rn), s);
}
static void rv64_mov_imm(CGEncoder *e, CGReg rd, int64_t imm) {
    rv64_li(&rv64_enc(e)->enc, cg_to_rv64(rd), imm);
}
static void rv64_mov_reg(CGEncoder *e, CGReg rd, CGReg rn) {
    rv64_mv(&rv64_enc(e)->enc, cg_to_rv64(rd), cg_to_rv64(rn));
}
static void rv64_load(CGEncoder *e, CGReg rt, CGReg base, int32_t off) {
    rv64_ld(&rv64_enc(e)->enc, cg_to_rv64(rt), cg_to_rv64(base), (int16_t)off);
}
static void rv64_store(CGEncoder *e, CGReg rt, CGReg base, int32_t off) {
    rv64_sd(&rv64_enc(e)->enc, cg_to_rv64(rt), cg_to_rv64(base), (int16_t)off);
}
static void rv64_cmp_imm(CGEncoder *e, CGReg rn, uint32_t imm) {
    rv64_li(&rv64_enc(e)->enc, RV_T6, imm);
    rv64_sub(&rv64_enc(e)->enc, RV_T6, cg_to_rv64(rn), RV_T6);
}
static void rv64_cmp_reg(CGEncoder *e, CGReg rn, CGReg rm) {
    rv64_sub(&rv64_enc(e)->enc, RV_T6, cg_to_rv64(rn), cg_to_rv64(rm));
}
static void rv64_cset(CGEncoder *e, CGReg rd, CGCC cc) {
    /* RISC-V comparison: T6 holds the comparison result from cmp_imm/cmp_reg.
     * We need to set rd = 1 if the condition is true, 0 otherwise.
     * Use sltiu/slt/sltu on T6 to produce 0/1. */
    switch (cc) {
        case CG_CC_NE:
            /* rd = (T6 != 0) ? 1 : 0 → sltiu rd, T6, 1 */
            rv64_sltiu(&rv64_enc(e)->enc, cg_to_rv64(rd), RV_T6, 1);
            break;
        case CG_CC_EQ:
            /* rd = (T6 == 0) ? 1 : 0 → sltiu rd, T6, 1 then xor 1 */
            rv64_sltiu(&rv64_enc(e)->enc, RV_T6, RV_T6, 1);  /* T6 = (T6 == 0) */
            rv64_addi(&rv64_enc(e)->enc, cg_to_rv64(rd), RV_T6, 0);
            break;
        case CG_CC_LT:
            /* rd = (T6 < 0) ? 1 : 0 → slli rd, T6, 63; srai rd, rd, 63 (arithmetic shift) */
            /* Simpler: slt rd, T6, x0 (signed less than 0) */
            rv64_slt(&rv64_enc(e)->enc, cg_to_rv64(rd), RV_T6, RV_X0);
            break;
        case CG_CC_GE:
            /* rd = (T6 >= 0) ? 1 : 0 → slt rd, T6, x0; xori rd, rd, 1 */
            rv64_slt(&rv64_enc(e)->enc, RV_T6, RV_T6, RV_X0);
            rv64_xori(&rv64_enc(e)->enc, cg_to_rv64(rd), RV_T6, 1);
            break;
        default:
            rv64_addi(&rv64_enc(e)->enc, cg_to_rv64(rd), RV_X0, 0);
            break;
    }
}
static void rv64_b_uncond(CGEncoder *e, int32_t off) {
    rv64_jal(&rv64_enc(e)->enc, RV_X0, off);
}
static void rv64_b_cond(CGEncoder *e, int32_t offset, CGCC cc) {
    (void)offset;
    switch (cc) {
        case CG_CC_NE: rv64_bne(&rv64_enc(e)->enc, RV_T6, RV_X0, 0); break;
        default: rv64_beq(&rv64_enc(e)->enc, RV_T6, RV_X0, 0); break;
    }
}
static void rv64_b_reg(CGEncoder *e, CGReg rn) {
    rv64_jalr(&rv64_enc(e)->enc, RV_X0, cg_to_rv64(rn), 0);
}
static void rv64_do_ret(CGEncoder *e) { rv64_ret(&rv64_enc(e)->enc); }
static size_t rv64_get_branch_pos(CGEncoder *e) { return rv64_branch_pos(&rv64_enc(e)->enc); }
static void rv64_do_patch_branch(CGEncoder *e, size_t pos, size_t target) {
    rv64_patch_branch(&rv64_enc(e)->enc, pos, target);
}
static void rv64_noop(CGEncoder *e) { (void)e; }
static void rv64_push(CGEncoder *e, CGReg rt) {
    rv64_addi(&rv64_enc(e)->enc, RV_SP, RV_SP, -8);
    rv64_sd(&rv64_enc(e)->enc, cg_to_rv64(rt), RV_SP, 0);
}
static void rv64_pop(CGEncoder *e, CGReg rt) {
    rv64_ld(&rv64_enc(e)->enc, cg_to_rv64(rt), RV_SP, 0);
    rv64_addi(&rv64_enc(e)->enc, RV_SP, RV_SP, 8);
}
static void rv64_prologue(CGEncoder *e, int n_args, int stack_slots) {
    (void)n_args;
    /* Leaf function: don't save RA (return address from JAL) */
    if (stack_slots > 0) {
        int alloc = ((stack_slots * 8) + 15) & ~15;
        rv64_addi(&rv64_enc(e)->enc, RV_SP, RV_SP, -(alloc));
    }
}
static void rv64_epilogue(CGEncoder *e, int stack_slots) {
    if (stack_slots > 0) {
        int alloc = ((stack_slots * 8) + 15) & ~15;
        rv64_addi(&rv64_enc(e)->enc, RV_SP, RV_SP, alloc);
    }
    rv64_ret(&rv64_enc(e)->enc);
}

static const CodeGenVTable rv64_vtable = {
    .name = "rv64",
    .buffer = rv64_buffer,
    .pos = rv64_pos,
    .emit_byte = rv64_emit_byte,
    .emit_word32 = rv64_emit_word32,
    .emit_word64 = rv64_emit_word64,
    .add_imm = rv64_add_imm,
    .sub_imm = rv64_sub_imm,
    .add_reg = rv64_add_reg,
    .sub_reg = rv64_sub_reg,
    .mul_reg = rv64_mul_reg,
    .div_reg = rv64_div_reg,
    .mod_reg = rv64_mod_reg,
    .and_reg = rv64_and_reg,
    .orr_reg = rv64_orr_reg,
    .eor_reg = rv64_eor_reg,
    .lsl_imm = rv64_lsl_imm,
    .lsr_imm = rv64_lsr_imm,
    .asr_imm = rv64_asr_imm,
    .mov_imm = rv64_mov_imm,
    .mov_reg = rv64_mov_reg,
    .load = rv64_load,
    .store = rv64_store,
    .cmp_imm = rv64_cmp_imm,
    .cmp_reg = rv64_cmp_reg,
    .cset = rv64_cset,
    .b_uncond = rv64_b_uncond,
    .b_cond = rv64_b_cond,
    .b_reg = rv64_b_reg,
    .ret = rv64_do_ret,
    .branch_pos = rv64_get_branch_pos,
    .patch_branch = rv64_do_patch_branch,
    .push = rv64_push,
    .pop = rv64_pop,
    .drop = rv64_noop,
    .prologue = rv64_prologue,
    .epilogue = rv64_epilogue,
};

CodeGen *cg_create_rv64(void) {
    CodeGen *cg = (CodeGen *)calloc(1, sizeof(CodeGen));
    RV64Encoder *enc = (RV64Encoder *)calloc(1, sizeof(RV64Encoder));
    if (!cg || !enc) { free(cg); free(enc); return NULL; }
    cg->vt = &rv64_vtable;
    cg->enc = (CGEncoder *)enc;
    cg->backend = 2;
    rv64_enc_init_dynamic(&enc->enc, 4096);
    return cg;
}
