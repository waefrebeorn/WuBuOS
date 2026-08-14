/*
 * wubu_rv64.h — RISC-V RV64GC encoder interface.
 *
 * RV64I base + M extension (mul/div). 32 registers x0-x32.
 * Calling convention (RV64 SysV):
 *   Args: a0-a7 (x10-x17) → CG_REG maps to x10=a0, etc.
 *   Return: a0 (x10)
 *   Temp: t0-t6 (x5-x7, x28-x31)
 *   Zero: x0 (hardwired)
 */
#ifndef WUBU_RV64_H
#define WUBU_RV64_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RV_X0 = 0,  RV_X1,  RV_X2,  RV_X3,  RV_X4,  RV_X5,  RV_X6,  RV_X7,
    RV_X8,  RV_X9,  RV_X10, RV_X11, RV_X12, RV_X13, RV_X14, RV_X15,
    RV_X16, RV_X17, RV_X18, RV_X19, RV_X20, RV_X21, RV_X22, RV_X23,
    RV_X24, RV_X25, RV_X26, RV_X27, RV_X28, RV_X29, RV_X30, RV_X31,
    /* ABI names */
    RV_ZERO = 0, RV_RA = 1, RV_SP = 2, RV_GP = 3, RV_TP = 4,
    RV_T0 = 5, RV_T1 = 6, RV_T2 = 7,
    RV_S0 = 8, RV_S1 = 9,
    RV_A0 = 10, RV_A1 = 11, RV_A2 = 12, RV_A3 = 13, RV_A4 = 14, RV_A5 = 15, RV_A6 = 16, RV_A7 = 17,
    RV_S2 = 18, RV_S3 = 19, RV_S4 = 20, RV_S5 = 21, RV_S6 = 22, RV_S7 = 23,
    RV_S8 = 24, RV_S9 = 25, RV_S10 = 26, RV_S11 = 27,
    RV_T3 = 28, RV_T4 = 29, RV_T5 = 30, RV_T6 = 31,
} RV64Reg;

typedef struct {
    uint8_t *buf;
    size_t   pos;
    size_t   cap;
    int      owns_buf;
} RV64Enc;

void rv64_enc_init(RV64Enc *e, uint8_t *buf, size_t cap);
void rv64_enc_init_dynamic(RV64Enc *e, size_t initial_cap);
void rv64_enc_free(RV64Enc *e);
void rv64_emit_word(RV64Enc *e, uint32_t w);
size_t rv64_enc_pos(const RV64Enc *e);

/* -- Data processing (R-type) -------------------------------------- */
void rv64_add(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2);
void rv64_sub(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2);
void rv64_mul(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2);
void rv64_div(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2);
void rv64_rem(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2);
void rv64_and(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2);
void rv64_or(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2);
void rv64_xor(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2);
void rv64_sll(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2);
void rv64_srl(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2);
void rv64_sra(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2);
void rv64_slt(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2);
void rv64_sltu(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2);

/* -- Data processing (I-type) -------------------------------------- */
void rv64_addi(RV64Enc *e, RV64Reg rd, RV64Reg rs1, int16_t imm12);
void rv64_ori(RV64Enc *e, RV64Reg rd, RV64Reg rs1, int16_t imm12);
void rv64_xori(RV64Enc *e, RV64Reg rd, RV64Reg rs1, int16_t imm12);
void rv64_andi(RV64Enc *e, RV64Reg rd, RV64Reg rs1, int16_t imm12);
void rv64_slti(RV64Enc *e, RV64Reg rd, RV64Reg rs1, int16_t imm12);
void rv64_sltiu(RV64Enc *e, RV64Reg rd, RV64Reg rs1, int16_t imm12);
void rv64_slli(RV64Enc *e, RV64Reg rd, RV64Reg rs1, uint8_t shamt);
void rv64_srli(RV64Enc *e, RV64Reg rd, RV64Reg rs1, uint8_t shamt);
void rv64_srai(RV64Enc *e, RV64Reg rd, RV64Reg rs1, uint8_t shamt);

/* -- Loads -------------------------------------------------------- */
void rv64_ld(RV64Enc *e, RV64Reg rd, RV64Reg base, int16_t off12);

/* -- Stores ------------------------------------------------------- */
void rv64_sd(RV64Enc *e, RV64Reg rs2, RV64Reg base, int16_t off12);

/* -- Branches ----------------------------------------------------- */
void rv64_beq(RV64Enc *e, RV64Reg rs1, RV64Reg rs2, int16_t off13);
void rv64_bne(RV64Enc *e, RV64Reg rs1, RV64Reg rs2, int16_t off13);
void rv64_blt(RV64Enc *e, RV64Reg rs1, RV64Reg rs2, int16_t off13);
void rv64_bge(RV64Enc *e, RV64Reg rs1, RV64Reg rs2, int16_t off13);

/* -- JAL / JALR --------------------------------------------------- */
void rv64_jal(RV64Enc *e, RV64Reg rd, int32_t off21);
void rv64_jalr(RV64Enc *e, RV64Reg rd, RV64Reg rs1, int16_t off12);
void rv64_ret(RV64Enc *e);

/* -- LUI (load upper immediate) ----------------------------------- */
void rv64_lui(RV64Enc *e, RV64Reg rd, int32_t imm20);

/* -- Pseudo-instructions ------------------------------------------ */
void rv64_li(RV64Enc *e, RV64Reg rd, int64_t imm);
void rv64_mv(RV64Enc *e, RV64Reg rd, RV64Reg rs);

/* -- Branch fixups ------------------------------------------------ */
size_t rv64_branch_pos(const RV64Enc *e);
void rv64_patch_branch(RV64Enc *e, size_t pos, size_t target);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_RV64_H */
