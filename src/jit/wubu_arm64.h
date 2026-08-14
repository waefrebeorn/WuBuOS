/*
 * wubu_arm64.h — ARM64/A64 encoder interface.
 *
 * Mirrors the Wx86Enc API so the minic compiler can target both
 * x86-64 and ARM64 with a compile-time or runtime switch.
 *
 * ARM64 registers: X0-X30 (64-bit), W0-W30 (32-bit)
 * SysV calling convention:
 *   Args: X0-X7, Return: X0
 *   Callee-saved: X19-X28
 *   Scratch: X9-X18, X29 (FP), X30 (LR)
 *   SP = X31, XZR = X31
 */
#ifndef WUBU_ARM64_H
#define WUBU_ARM64_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ARM64 register encoding (5-bit field) */
typedef enum {
    WREG_X0 = 0,  WREG_X1,  WREG_X2,  WREG_X3,
    WREG_X4,  WREG_X5,  WREG_X6,  WREG_X7,
    WREG_X8,  WREG_X9,  WREG_X10, WREG_X11,
    WREG_X12, WREG_X13, WREG_X14, WREG_X15,
    WREG_X16, WREG_X17, WREG_X18, WREG_X19,
    WREG_X20, WREG_X21, WREG_X22, WREG_X23,
    WREG_X24, WREG_X25, WREG_X26, WREG_X27,
    WREG_X28, WREG_X29, WREG_X30, WREG_X31,
    /* Aliases */
    WREG_SP  = 31,   /* Stack pointer (X31) */
    WREG_XZR = 31,   /* Zero register (X31) */
    WREG_FP  = 29,   /* Frame pointer (X29) */
    WREG_LR  = 30,   /* Link register (X30) */
    WREG_W0  = 32,   /* 32-bit aliases start at 32 */
} WArm64Reg;

/* Condition codes for conditional branches */
typedef enum {
    WCC_EQ = 0,  /* Equal */
    WCC_NE,      /* Not equal */
    WCC_CS,      /* Carry set (HS) */
    WCC_CC,      /* Carry clear (LO) */
    WCC_MI,      /* Minus/negative */
    WCC_PL,      /* Plus/positive */
    WCC_VS,      /* Overflow */
    WCC_VC,      /* No overflow */
    WCC_HI,      /* Unsigned higher */
    WCC_LS,      /* Unsigned lower or same */
    WCC_GE,      /* Signed >= */
    WCC_LT,      /* Signed < */
    WCC_GT,      /* Signed > */
    WCC_LE,      /* Signed <= */
    WCC_AL,      /* Always */
} WArm64CC;

/* Encoder buffer */
typedef struct {
    uint8_t *buf;
    size_t   pos;
    size_t   cap;
    int      owns_buf;
} WArm64Enc;

/* Buffer management */
void warm64_enc_init(WArm64Enc *e, uint8_t *buf, size_t cap);
void warm64_enc_init_dynamic(WArm64Enc *e, size_t initial_cap);
void warm64_enc_free(WArm64Enc *e);
void warm64_emit_byte(WArm64Enc *e, uint8_t b);
void warm64_emit_word(WArm64Enc *e, uint32_t w);
void warm64_emit_dword(WArm64Enc *e, uint32_t d);
void warm64_emit_qword(WArm64Enc *e, uint64_t q);
size_t warm64_enc_pos(WArm64Enc *e);

/* -- Data processing (immediate) ----------------------------------- */
void warm64_add_imm(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, uint16_t imm12, int sf);
void warm64_sub_imm(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, uint16_t imm12, int sf);
void warm64_add_reg(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, WArm64Reg rm, int sf);
void warm64_sub_reg(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, WArm64Reg rm, int sf);
void warm64_mul_reg(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, WArm64Reg rm);
void warm64_sdiv_reg(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, WArm64Reg rm);
void warm64_mov_imm(WArm64Enc *e, WArm64Reg rd, uint16_t imm16);
void warm64_movz_imm(WArm64Enc *e, WArm64Reg rd, uint16_t imm16, int hw, int sf);
void warm64_mov_reg(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn);
void warm64_mov_sp(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn);

/* -- Bitwise (register) ------------------------------------------- */
void warm64_and_reg(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, WArm64Reg rm, int sf);
void warm64_orr_reg(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, WArm64Reg rm, int sf);
void warm64_eor_reg(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, WArm64Reg rm, int sf);

/* -- Shift -------------------------------------------------------- */
void warm64_lsl_imm(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, uint8_t shift, int sf);
void warm64_lsr_imm(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, uint8_t shift, int sf);
void warm64_asr_imm(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, uint8_t shift, int sf);

/* -- Load / Store ------------------------------------------------- */
void warm64_ldr_imm(WArm64Enc *e, WArm64Reg rt, WArm64Reg rn, int32_t imm12, int sf);
void warm64_str_imm(WArm64Enc *e, WArm64Reg rt, WArm64Reg rn, int32_t imm12, int sf);
void warm64_ldr_reg(WArm64Enc *e, WArm64Reg rt, WArm64Reg rn, WArm64Reg rm, int sf);
void warm64_str_reg(WArm64Enc *e, WArm64Reg rt, WArm64Reg rn, WArm64Reg rm, int sf);

/* -- Branch ------------------------------------------------------- */
void warm64_b_uncond(WArm64Enc *e, int32_t imm26);
void warm64_b_cond(WArm64Enc *e, int32_t imm19, WArm64CC cc);
void warm64_b_reg(WArm64Enc *e, WArm64Reg rn);  /* BR (jump to register) */
void warm64_bl_reg(WArm64Enc *e, WArm64Reg rn); /* BLR (call register) */
void warm64_ret(WArm64Enc *e, WArm64Reg rn);

/* -- Comparison --------------------------------------------------- */
void warm64_cmp_imm(WArm64Enc *e, WArm64Reg rn, uint16_t imm12, int sf);
void warm64_cmp_reg(WArm64Enc *e, WArm64Reg rn, WArm64Reg rm, int sf);
void warm64_cset(WArm64Enc *e, WArm64Reg rd, WArm64CC cc);

/* -- Branch fixups (for forward jumps) ---------------------------- */
size_t warm64_branch_pos(WArm64Enc *e);  /* pos of last branch imm field */
void warm64_patch_branch(WArm64Enc *e, size_t branch_pos, size_t target);

/* -- Stack frame -------------------------------------------------- */
void warm64_push(WArm64Enc *e, WArm64Reg rt);
void warm64_pop(WArm64Enc *e, WArm64Reg rt);
void warm64_stp_pre(WArm64Enc *e, WArm64Reg rt1, WArm64Reg rt2, WArm64Reg rn, int32_t imm7);
void warm64_ldp_post(WArm64Enc *e, WArm64Reg rt1, WArm64Reg rt2, WArm64Reg rn, int32_t imm7);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_ARM64_H */
