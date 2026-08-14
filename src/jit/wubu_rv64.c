/*
 * wubu_rv64.c — RISC-V RV64I + M extension instruction encoder.
 *
 * RISC-V is beautifully simple: fixed 32-bit instructions, regular encoding.
 * R-type: funct7[31:25] | rs2[24:20] | rs1[19:15] | funct3[14:12] | rd[11:7] | opcode[6:0]
 * I-type: imm[11:0] | rs1[19:15] | funct3[14:12] | rd[11:7] | opcode[6:0]
 * S-type: imm[11:5] | rs2[24:20] | rs1[19:15] | funct3[14:12] | imm[4:0] | opcode[6:0]
 * B-type: imm[12|10:5] | rs2 | rs1 | funct3 | imm[4:1|11] | opcode
 * U-type: imm[31:12] | rd | opcode
 * J-type: imm[20|10:1|11|19:12] | rd | opcode
 */
#include "wubu_rv64.h"
#include <stdlib.h>
#include <string.h>

/* Opcodes */
#define RV_OP_LUI    0x37
#define RV_OP_JAL    0x6F
#define RV_OP_JALR   0x67
#define RV_OP_BRANCH 0x63
#define RV_OP_LOAD   0x03
#define RV_OP_STORE  0x23
#define RV_OP_OP_IMM 0x13
#define RV_OP_OP     0x33
#define RV_OP_OP_IMM_32 0x1B
#define RV_OP_OP_32  0x3B

/* Funct3 values */
#define RV_F3_ADD    0x0
#define RV_F3_SLL    0x1
#define RV_F3_SLT    0x2
#define RV_F3_XOR    0x4
#define RV_F3_SRL    0x5
#define RV_F3_OR     0x6
#define RV_F3_AND    0x7
#define RV_F3_SUB    0x0
#define RV_F3_BEQ    0x0
#define RV_F3_BNE    0x1
#define RV_F3_BLT    0x4
#define RV_F3_BGE    0x5
#define RV_F3_LD     0x3
#define RV_F3_SD     0x3

/* Funct7 values */
#define RV_F7_ADD    0x00
#define RV_F7_SUB    0x20
#define RV_F7_MUL    0x01
#define RV_F7_DIV    0x01
#define RV_F7_REM    0x01

static void rv64_emit(RV64Enc *e, uint32_t w) {
    if (e->pos + 4 <= e->cap) {
        /* Little-endian */
        e->buf[e->pos++] = (uint8_t)(w & 0xFF);
        e->buf[e->pos++] = (uint8_t)((w >> 8) & 0xFF);
        e->buf[e->pos++] = (uint8_t)((w >> 16) & 0xFF);
        e->buf[e->pos++] = (uint8_t)((w >> 24) & 0xFF);
    }
}

static uint32_t rv_r_type(uint8_t funct7, RV64Reg rs2, RV64Reg rs1, uint8_t funct3, RV64Reg rd, uint8_t opcode) {
    return ((funct7 & 0x7F) << 25) | ((rs2 & 0x1F) << 20) | ((rs1 & 0x1F) << 15) |
           ((funct3 & 0x7) << 12) | ((rd & 0x1F) << 7) | opcode;
}

static uint32_t rv_i_type(int16_t imm, RV64Reg rs1, uint8_t funct3, RV64Reg rd, uint8_t opcode) {
    return ((imm & 0xFFF) << 20) | ((rs1 & 0x1F) << 15) | ((funct3 & 0x7) << 12) | ((rd & 0x1F) << 7) | opcode;
}

static uint32_t rv_s_type(int16_t imm, RV64Reg rs2, RV64Reg rs1, uint8_t funct3, uint8_t opcode) {
    uint32_t hi = (imm >> 5) & 0x7F;
    uint32_t lo = imm & 0x1F;
    return (hi << 25) | ((rs2 & 0x1F) << 20) | ((rs1 & 0x1F) << 15) | ((funct3 & 0x7) << 12) | (lo << 7) | opcode;
}

static uint32_t rv_b_type(int16_t imm, RV64Reg rs2, RV64Reg rs1, uint8_t funct3, uint8_t opcode) {
    /* B-immediate: imm[12|10:5] | rs2 | rs1 | funct3 | imm[4:1|11] | opcode */
    int16_t off = imm >> 1;  /* encoded in multiples of 2 */
    uint32_t b12  = (off >> 11) & 1;
    uint32_t b11  = (off >> 10) & 1;
    uint32_t b10_5= (off >> 4) & 0x3F;
    uint32_t b4_1 = (off >> 0) & 0xF;
    return (b12 << 31) | (b10_5 << 25) | ((rs2 & 0x1F) << 20) | ((rs1 & 0x1F) << 15) |
           ((funct3 & 0x7) << 12) | (b4_1 << 8) | (b11 << 7) | opcode;
}

static uint32_t rv_j_type(int32_t imm, RV64Reg rd, uint8_t opcode) {
    /* J-immediate: imm[20|10:1|11|19:12] */
    int32_t off = imm >> 1;
    uint32_t j20   = (off >> 19) & 1;
    uint32_t j10_1 = (off >> 9) & 0x3FF;
    uint32_t j11   = (off >> 8) & 1;
    uint32_t j19_12= (off >> 0) & 0xFF;
    return (j20 << 31) | (j19_12 << 12) | (j11 << 20) | (j10_1 << 21) | ((rd & 0x1F) << 7) | opcode;
}

void rv64_enc_init(RV64Enc *e, uint8_t *buf, size_t cap) {
    e->buf = buf; e->pos = 0; e->cap = cap; e->owns_buf = 0;
}

void rv64_enc_init_dynamic(RV64Enc *e, size_t initial_cap) {
    if (initial_cap == 0) initial_cap = 256;
    e->buf = (uint8_t *)malloc(initial_cap);
    e->cap = e->buf ? initial_cap : 0;
    e->pos = 0;
    e->owns_buf = 1;
}

void rv64_enc_free(RV64Enc *e) {
    if (e && e->owns_buf && e->buf) { free(e->buf); e->buf = NULL; }
}

void rv64_emit_word(RV64Enc *e, uint32_t w) { rv64_emit(e, w); }
size_t rv64_enc_pos(const RV64Enc *e) { return e->pos; }

/* -- R-type instructions ------------------------------------------- */
void rv64_add(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2) {
    rv64_emit(e, rv_r_type(RV_F7_ADD, rs2, rs1, RV_F3_ADD, rd, RV_OP_OP));
}
void rv64_sub(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2) {
    rv64_emit(e, rv_r_type(RV_F7_SUB, rs2, rs1, RV_F3_SUB, rd, RV_OP_OP));
}
void rv64_mul(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2) {
    rv64_emit(e, rv_r_type(RV_F7_MUL, rs2, rs1, RV_F3_ADD, rd, RV_OP_OP));
}
void rv64_div(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2) {
    rv64_emit(e, rv_r_type(RV_F7_DIV, rs2, rs1, RV_F3_XOR, rd, RV_OP_OP));
}
void rv64_rem(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2) {
    rv64_emit(e, rv_r_type(RV_F7_REM, rs2, rs1, RV_F3_XOR, rd, RV_OP_OP));
}
void rv64_and(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2) {
    rv64_emit(e, rv_r_type(RV_F7_ADD, rs2, rs1, RV_F3_AND, rd, RV_OP_OP));
}
void rv64_or(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2) {
    rv64_emit(e, rv_r_type(RV_F7_ADD, rs2, rs1, RV_F3_OR, rd, RV_OP_OP));
}
void rv64_xor(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2) {
    rv64_emit(e, rv_r_type(RV_F7_ADD, rs2, rs1, RV_F3_XOR, rd, RV_OP_OP));
}
void rv64_sll(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2) {
    rv64_emit(e, rv_r_type(RV_F7_ADD, rs2, rs1, RV_F3_SLL, rd, RV_OP_OP));
}
void rv64_srl(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2) {
    rv64_emit(e, rv_r_type(RV_F7_ADD, rs2, rs1, RV_F3_SRL, rd, RV_OP_OP));
}
void rv64_sra(RV64Enc *e, RV64Reg rd, RV64Reg rs1, RV64Reg rs2) {
    rv64_emit(e, rv_r_type(RV_F7_SUB, rs2, rs1, RV_F3_SRL, rd, RV_OP_OP));
}

/* -- I-type instructions ------------------------------------------- */
void rv64_addi(RV64Enc *e, RV64Reg rd, RV64Reg rs1, int16_t imm) {
    rv64_emit(e, rv_i_type(imm, rs1, RV_F3_ADD, rd, RV_OP_OP_IMM));
}
void rv64_ori(RV64Enc *e, RV64Reg rd, RV64Reg rs1, int16_t imm) {
    rv64_emit(e, rv_i_type(imm, rs1, RV_F3_OR, rd, RV_OP_OP_IMM));
}
void rv64_xori(RV64Enc *e, RV64Reg rd, RV64Reg rs1, int16_t imm) {
    rv64_emit(e, rv_i_type(imm, rs1, RV_F3_XOR, rd, RV_OP_OP_IMM));
}
void rv64_andi(RV64Enc *e, RV64Reg rd, RV64Reg rs1, int16_t imm) {
    rv64_emit(e, rv_i_type(imm, rs1, RV_F3_AND, rd, RV_OP_OP_IMM));
}
void rv64_slli(RV64Enc *e, RV64Reg rd, RV64Reg rs1, uint8_t shamt) {
    rv64_emit(e, rv_i_type(shamt & 0x3F, rs1, RV_F3_SLL, rd, RV_OP_OP_IMM));
}
void rv64_srli(RV64Enc *e, RV64Reg rd, RV64Reg rs1, uint8_t shamt) {
    rv64_emit(e, rv_i_type(shamt & 0x3F, rs1, RV_F3_SRL, rd, RV_OP_OP_IMM));
}
void rv64_srai(RV64Enc *e, RV64Reg rd, RV64Reg rs1, uint8_t shamt) {
    rv64_emit(e, rv_i_type((shamt & 0x3F) | 0x400, rs1, RV_F3_SRL, rd, RV_OP_OP_IMM));
}

/* -- Loads / Stores ------------------------------------------------ */
void rv64_ld(RV64Enc *e, RV64Reg rd, RV64Reg base, int16_t off) {
    rv64_emit(e, rv_i_type(off, base, RV_F3_LD, rd, RV_OP_LOAD));
}
void rv64_sd(RV64Enc *e, RV64Reg rs2, RV64Reg base, int16_t off) {
    rv64_emit(e, rv_s_type(off, rs2, base, RV_F3_SD, RV_OP_STORE));
}

/* -- Branches ------------------------------------------------------ */
void rv64_beq(RV64Enc *e, RV64Reg rs1, RV64Reg rs2, int16_t off) {
    rv64_emit(e, rv_b_type(off, rs2, rs1, RV_F3_BEQ, RV_OP_BRANCH));
}
void rv64_bne(RV64Enc *e, RV64Reg rs1, RV64Reg rs2, int16_t off) {
    rv64_emit(e, rv_b_type(off, rs2, rs1, RV_F3_BNE, RV_OP_BRANCH));
}
void rv64_blt(RV64Enc *e, RV64Reg rs1, RV64Reg rs2, int16_t off) {
    rv64_emit(e, rv_b_type(off, rs2, rs1, RV_F3_BLT, RV_OP_BRANCH));
}
void rv64_bge(RV64Enc *e, RV64Reg rs1, RV64Reg rs2, int16_t off) {
    rv64_emit(e, rv_b_type(off, rs2, rs1, RV_F3_BGE, RV_OP_BRANCH));
}

/* -- JAL / JALR ---------------------------------------------------- */
void rv64_jal(RV64Enc *e, RV64Reg rd, int32_t off) {
    rv64_emit(e, rv_j_type(off, rd, RV_OP_JAL));
}
void rv64_jalr(RV64Enc *e, RV64Reg rd, RV64Reg rs1, int16_t off) {
    rv64_emit(e, rv_i_type(off, rs1, 0, rd, RV_OP_JALR));
}
void rv64_ret(RV64Enc *e) {
    rv64_emit(e, rv_i_type(0, RV_RA, 0, RV_X0, RV_OP_JALR));
}

/* -- LUI ----------------------------------------------------------- */
void rv64_lui(RV64Enc *e, RV64Reg rd, int32_t imm20) {
    rv64_emit(e, ((imm20 & 0xFFFFF) << 12) | ((rd & 0x1F) << 7) | RV_OP_LUI);
}

/* -- Pseudo-instructions ------------------------------------------- */
void rv64_li(RV64Enc *e, RV64Reg rd, int64_t imm) {
    /* Load 64-bit immediate using LUI + ADDI + SLLI + ORI sequence */
    int32_t lo = (int32_t)(imm & 0xFFF);
    if (lo & 0x800) lo |= ~0xFFF;  /* sign extend */
    int32_t hi = (int32_t)((imm + 0x800) >> 12);  /* upper bits */
    int32_t hi20 = (imm >> 12) & 0xFFFFF;

    if (hi20 != 0) {
        rv64_lui(e, rd, hi20 << 12);
        if (lo != 0) {
            rv64_addi(e, rd, rd, (int16_t)lo);
        }
    } else {
        rv64_addi(e, rd, RV_X0, (int16_t)lo);
    }
}

void rv64_mv(RV64Enc *e, RV64Reg rd, RV64Reg rs) {
    rv64_addi(e, rd, rs, 0);
}

/* -- Branch fixups ------------------------------------------------- */
size_t rv64_branch_pos(const RV64Enc *e) {
    return e->pos - 4;
}

void rv64_patch_branch(RV64Enc *e, size_t pos, size_t target) {
    if (pos + 4 > e->pos) return;
    int32_t offset = (int32_t)((intptr_t)target - (intptr_t)pos);
    uint32_t *p = (uint32_t *)(e->buf + pos);
    uint32_t ins = *p;
    uint8_t opcode = ins & 0x7F;

    if (opcode == RV_OP_BRANCH) {
        *p = rv_b_type((int16_t)offset, (RV64Reg)((ins >> 20) & 0x1F),
                       (RV64Reg)((ins >> 15) & 0x1F), (ins >> 12) & 0x7, RV_OP_BRANCH);
    } else if (opcode == RV_OP_JAL) {
        *p = rv_j_type(offset, (RV64Reg)((ins >> 7) & 0x1F), RV_OP_JAL);
    }
}
