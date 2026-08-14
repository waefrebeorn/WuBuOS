/*
 * jit_codegen_arm64.c — ARM64 backend for the abstract code generator.
 *
 * Wraps WArm64Enc + wubu_arm64.h functions behind the CodeGenVTable.
 *
 * ARM64 is a 3-address architecture (ADD Xd, Xn, Xm), so the mapping
 * from neutral CGReg to WArm64Reg is straightforward.
 *
 * Calling convention (AArch64 SysV):
 *   Args: X0-X7 → CG_REG_0-CG_REG_7
 *   Return: X0 → CG_REG_0
 *   Callee-saved: X19-X28
 *   Scratch: X9-X18, X29 (FP), X30 (LR)
 */
#include "jit_codegen.h"
#include "wubu_arm64.h"
#include <stdlib.h>
#include <string.h>

/* -- Register mapping: CGReg → WArm64Reg --------------------------- */
static WArm64Reg cg_to_arm64(CGReg r) {
    if (r <= 15) return (WArm64Reg)r;  /* Direct mapping: CG_REG_0→X0, etc. */
    if (r == CG_REG_SP) return WREG_SP;
    if (r == CG_REG_FP) return WREG_FP;
    if (r == CG_REG_LR) return WREG_LR;
    if (r == CG_REG_XZR) return WREG_XZR;
    return WREG_X0;
}

/* -- Condition mapping: CGCC → WArm64CC ---------------------------- */
static WArm64CC cg_to_arm64cc(CGCC cc) {
    return (WArm64CC)cc;  /* Direct mapping — neutral codes match ARM64 */
}

/* -- Encoder wrapper ----------------------------------------------- */
typedef struct {
    WArm64Enc enc;
} Arm64Encoder;

static Arm64Encoder *arm64_enc(CGEncoder *e) { return (Arm64Encoder *)e; }

/* -- VTable implementations ---------------------------------------- */

static void arm64_emit_byte(CGEncoder *e, uint8_t b) {
    warm64_emit_byte(&arm64_enc(e)->enc, b);
}

static void arm64_emit_word32(CGEncoder *e, uint32_t w) {
    warm64_emit_word(&arm64_enc(e)->enc, w);
}

static void arm64_emit_word64(CGEncoder *e, uint64_t q) {
    warm64_emit_qword(&arm64_enc(e)->enc, q);
}

static size_t arm64_pos(const CGEncoder *e) {
    return arm64_enc((CGEncoder *)e)->enc.pos;
}

static const uint8_t *arm64_buffer(const CGEncoder *e) {
    return arm64_enc((CGEncoder *)e)->enc.buf;
}

static void arm64_add_imm(CGEncoder *e, CGReg rd, CGReg rn, uint32_t imm) {
    /* ARM64 ADD (immediate) supports imm12 (0-4095) */
    if (imm <= 0xFFF) {
        warm64_add_imm(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64(rn), (uint16_t)imm, 1);
    } else {
        /* Large immediate: load into scratch reg, then add */
        /* Use X16 (IP0) as scratch — caller-saved */
        warm64_movz_imm(&arm64_enc(e)->enc, WREG_X10, (uint16_t)(imm & 0xFFFF), 0, 1);
        warm64_movz_imm(&arm64_enc(e)->enc, WREG_X11, (uint16_t)((imm >> 16) & 0xFFFF), 1, 1);
        warm64_add_reg(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64(rn), WREG_X10, 1);
    }
}

static void arm64_sub_imm(CGEncoder *e, CGReg rd, CGReg rn, uint32_t imm) {
    if (imm <= 0xFFF) {
        warm64_sub_imm(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64(rn), (uint16_t)imm, 1);
    } else {
        warm64_movz_imm(&arm64_enc(e)->enc, WREG_X10, (uint16_t)(imm & 0xFFFF), 0, 1);
        warm64_movz_imm(&arm64_enc(e)->enc, WREG_X11, (uint16_t)((imm >> 16) & 0xFFFF), 1, 1);
        warm64_sub_reg(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64(rn), WREG_X10, 1);
    }
}

static void arm64_add_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    warm64_add_reg(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64(rn), cg_to_arm64(rm), 1);
}

static void arm64_sub_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    warm64_sub_reg(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64(rn), cg_to_arm64(rm), 1);
}

static void arm64_mul_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    warm64_mul_reg(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64(rn), cg_to_arm64(rm));
}

static void arm64_div_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    warm64_sdiv_reg(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64(rn), cg_to_arm64(rm));
}

static void arm64_mod_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    /* rd = rn - (rn/rm)*rm */
    /* Use X10 as scratch (caller-saved) */
    warm64_sdiv_reg(&arm64_enc(e)->enc, WREG_X10, cg_to_arm64(rn), cg_to_arm64(rm));
    warm64_mul_reg(&arm64_enc(e)->enc, WREG_X10, WREG_X10, cg_to_arm64(rm));
    warm64_sub_reg(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64(rn), WREG_X10, 1);
}

static void arm64_and_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    warm64_and_reg(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64(rn), cg_to_arm64(rm), 1);
}

static void arm64_orr_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    warm64_orr_reg(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64(rn), cg_to_arm64(rm), 1);
}

static void arm64_eor_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    warm64_eor_reg(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64(rn), cg_to_arm64(rm), 1);
}

static void arm64_lsl_imm(CGEncoder *e, CGReg rd, CGReg rn, uint8_t shift) {
    warm64_lsl_imm(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64(rn), shift, 1);
}

static void arm64_lsr_imm(CGEncoder *e, CGReg rd, CGReg rn, uint8_t shift) {
    warm64_lsr_imm(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64(rn), shift, 1);
}

static void arm64_asr_imm(CGEncoder *e, CGReg rd, CGReg rn, uint8_t shift) {
    warm64_asr_imm(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64(rn), shift, 1);
}

static void arm64_mov_imm(CGEncoder *e, CGReg rd, int64_t imm) {
    /* ARM64 MOV (wide immediate) uses MOVZ + MOVK for large values.
     * For small values (16-bit), MOVZ is enough.
     * For larger values, we need up to 4 MOVZ/MOVK pairs. */
    uint16_t w0 = (uint16_t)(imm & 0xFFFF);
    uint16_t w1 = (uint16_t)((imm >> 16) & 0xFFFF);
    uint16_t w2 = (uint16_t)((imm >> 32) & 0xFFFF);
    uint16_t w3 = (uint16_t)((imm >> 48) & 0xFFFF);

    if (imm >= 0 && imm <= 0xFFFF) {
        warm64_movz_imm(&arm64_enc(e)->enc, cg_to_arm64(rd), w0, 0, 1);
    } else if (imm >= 0 && imm <= 0xFFFFFFFF) {
        warm64_movz_imm(&arm64_enc(e)->enc, cg_to_arm64(rd), w0, 0, 1);
        if (w1) warm64_movz_imm(&arm64_enc(e)->enc, cg_to_arm64(rd), w1, 1, 1);
    } else {
        warm64_movz_imm(&arm64_enc(e)->enc, cg_to_arm64(rd), w0, 0, 1);
        if (w1) warm64_movz_imm(&arm64_enc(e)->enc, cg_to_arm64(rd), w1, 1, 1);
        if (w2) warm64_movz_imm(&arm64_enc(e)->enc, cg_to_arm64(rd), w2, 2, 1);
        if (w3) warm64_movz_imm(&arm64_enc(e)->enc, cg_to_arm64(rd), w3, 3, 1);
    }
}

static void arm64_mov_reg(CGEncoder *e, CGReg rd, CGReg rn) {
    warm64_mov_reg(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64(rn));
}

static void arm64_load(CGEncoder *e, CGReg rt, CGReg base, int32_t offset) {
    /* ARM64 LDR (unsigned offset): imm12 is scaled by 8 for 64-bit */
    if (offset >= 0 && (offset >> 3) <= 0xFFF) {
        warm64_ldr_imm(&arm64_enc(e)->enc, cg_to_arm64(rt), cg_to_arm64(base), offset, 1);
    } else {
        /* Large offset: load address into scratch, then load */
        arm64_mov_imm(e, CG_REG_10, offset);
        warm64_ldr_reg(&arm64_enc(e)->enc, cg_to_arm64(rt), cg_to_arm64(base), WREG_X10, 1);
    }
}

static void arm64_store(CGEncoder *e, CGReg rt, CGReg base, int32_t offset) {
    if (offset >= 0 && (offset >> 3) <= 0xFFF) {
        warm64_str_imm(&arm64_enc(e)->enc, cg_to_arm64(rt), cg_to_arm64(base), offset, 1);
    } else {
        arm64_mov_imm(e, CG_REG_10, offset);
        warm64_str_reg(&arm64_enc(e)->enc, cg_to_arm64(rt), cg_to_arm64(base), WREG_X10, 1);
    }
}

static void arm64_cmp_imm(CGEncoder *e, CGReg rn, uint32_t imm) {
    if (imm <= 0xFFF) {
        warm64_cmp_imm(&arm64_enc(e)->enc, cg_to_arm64(rn), (uint16_t)imm, 1);
    } else {
        arm64_mov_imm(e, CG_REG_10, imm);
        warm64_cmp_reg(&arm64_enc(e)->enc, cg_to_arm64(rn), WREG_X10, 1);
    }
}

static void arm64_cmp_reg(CGEncoder *e, CGReg rn, CGReg rm) {
    warm64_cmp_reg(&arm64_enc(e)->enc, cg_to_arm64(rn), cg_to_arm64(rm), 1);
}

static void arm64_cset(CGEncoder *e, CGReg rd, CGCC cc) {
    warm64_cset(&arm64_enc(e)->enc, cg_to_arm64(rd), cg_to_arm64cc(cc));
}

static void arm64_b_uncond(CGEncoder *e, int32_t offset) {
    warm64_b_uncond(&arm64_enc(e)->enc, offset);
}

static void arm64_b_cond(CGEncoder *e, int32_t offset, CGCC cc) {
    warm64_b_cond(&arm64_enc(e)->enc, offset, cg_to_arm64cc(cc));
}

static void arm64_b_reg(CGEncoder *e, CGReg rn) {
    warm64_b_reg(&arm64_enc(e)->enc, cg_to_arm64(rn));
}

static void arm64_ret(CGEncoder *e) {
    warm64_ret(&arm64_enc(e)->enc, WREG_LR);
}

static size_t arm64_branch_pos(CGEncoder *e) {
    return warm64_branch_pos(&arm64_enc(e)->enc);
}

static void arm64_patch_branch(CGEncoder *e, size_t pos, size_t target) {
    warm64_patch_branch(&arm64_enc(e)->enc, pos, target);
}

static void arm64_push(CGEncoder *e, CGReg rt) {
    warm64_push(&arm64_enc(e)->enc, cg_to_arm64(rt));
}

static void arm64_pop(CGEncoder *e, CGReg rt) {
    warm64_pop(&arm64_enc(e)->enc, cg_to_arm64(rt));
}

static void arm64_prologue(CGEncoder *e, int n_args, int stack_slots) {
    /* ARM64 prologue: save FP+LR, set up frame, allocate stack */
    /* STP X29, X30, [SP, #-16]!  (push FP, LR) */
    warm64_stp_pre(&arm64_enc(e)->enc, WREG_X29, WREG_X30, WREG_SP, -16);
    /* MOV X29, SP */
    warm64_mov_reg(&arm64_enc(e)->enc, WREG_X29, WREG_SP);
    /* SUB SP, SP, #stack_slots*8 (aligned to 16) */
    if (stack_slots > 0) {
        int alloc = ((stack_slots * 8) + 15) & ~15;  /* align to 16 */
        if (alloc <= 0xFFF) {
            warm64_sub_imm(&arm64_enc(e)->enc, WREG_SP, WREG_SP, (uint16_t)alloc, 1);
        } else {
            /* Large allocation: load immediate */
            /* For now, assume small stacks */
            warm64_sub_imm(&arm64_enc(e)->enc, WREG_SP, WREG_SP, (uint16_t)(alloc & 0xFFF), 1);
        }
    }
}

static void arm64_epilogue(CGEncoder *e, int stack_slots) {
    /* ARM64 epilogue: restore SP, restore FP+LR, ret */
    /* MOV SP, X29 */
    warm64_mov_reg(&arm64_enc(e)->enc, WREG_SP, WREG_X29);
    /* LDP X29, X30, [SP], #16 (pop FP, LR) */
    warm64_ldp_post(&arm64_enc(e)->enc, WREG_X29, WREG_X30, WREG_SP, 16);
    /* RET X30 */
    warm64_ret(&arm64_enc(e)->enc, WREG_LR);
}

/* -- VTable ------------------------------------------------------- */
static const CodeGenVTable arm64_vtable = {
    .name = "arm64",
    .buffer = arm64_buffer,
    .emit_byte = arm64_emit_byte,
    .emit_word32 = arm64_emit_word32,
    .emit_word64 = arm64_emit_word64,
    .pos = arm64_pos,
    .add_imm = arm64_add_imm,
    .sub_imm = arm64_sub_imm,
    .add_reg = arm64_add_reg,
    .sub_reg = arm64_sub_reg,
    .mul_reg = arm64_mul_reg,
    .div_reg = arm64_div_reg,
    .mod_reg = arm64_mod_reg,
    .and_reg = arm64_and_reg,
    .orr_reg = arm64_orr_reg,
    .eor_reg = arm64_eor_reg,
    .lsl_imm = arm64_lsl_imm,
    .lsr_imm = arm64_lsr_imm,
    .asr_imm = arm64_asr_imm,
    .mov_imm = arm64_mov_imm,
    .mov_reg = arm64_mov_reg,
    .load = arm64_load,
    .store = arm64_store,
    .cmp_imm = arm64_cmp_imm,
    .cmp_reg = arm64_cmp_reg,
    .cset = arm64_cset,
    .b_uncond = arm64_b_uncond,
    .b_cond = arm64_b_cond,
    .b_reg = arm64_b_reg,
    .ret = arm64_ret,
    .branch_pos = arm64_branch_pos,
    .patch_branch = arm64_patch_branch,
    .push = arm64_push,
    .pop = arm64_pop,
    .prologue = arm64_prologue,
    .epilogue = arm64_epilogue,
};

/* -- Factory ------------------------------------------------------ */
CodeGen *cg_create_arm64(void) {
    CodeGen *cg = (CodeGen *)calloc(1, sizeof(CodeGen));
    Arm64Encoder *enc = (Arm64Encoder *)calloc(1, sizeof(Arm64Encoder));
    if (!cg || !enc) { free(cg); free(enc); return NULL; }
    cg->vt = &arm64_vtable;
    cg->enc = (CGEncoder *)enc;
    cg->backend = 1;
    warm64_enc_init_dynamic(&enc->enc, 4096);
    return cg;
}
