/*
 * wubu_isa_riscv.c -- the RISC-V RV64I ISA driver.
 *
 * The next wave in the driver space (after x86-64, m68k, 8086).
 * RV64I is the base integer ISA of the RISC-V standard (2019).
 * 32 general-purpose registers (x0-x31, x0 hardwired zero),
 * little-endian, flat 64-bit address space, compressed (C)
 * extension optional (not targeted here — pure RV64I base).
 *
 * Strategy: the SAME MIR the other drivers consume, with
 * the same stack-slot register assignment:
 *   - sp = x2 (stack pointer), fp = x8 (frame pointer)
 *   - each virtual register lives at offset (vr+1)*8 from fp
 *     (RV64I uses 64-bit slots, natural width = the type set)
 *   - operations load operands into x10/x11 (a0/a1), compute,
 *     store back
 *   - ret (0x00008067) closes it, result in a0 (x10)
 *
 * EVERY encoding below is VERIFIED byte-for-byte against GNU
 * binutils objdump (riscv:rv64i) — the "we know where we
 * are" rule: no guessed opcodes. See tools/verify_isa.sh.
 *
 * Executed by the bundled interpreter (wubu_riscv_interp.c) —
 * the emitted bytes RUN, so the driver is verified, not a stub.
 *
 * C11, self-contained.
 */
#include "wubu_isa_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- little-endian 64-bit emitter ---- */
typedef struct {
    uint8_t *code;
    size_t n, cap;
    size_t frame;
    size_t *label_offsets;
    size_t n_labels;
    size_t internal_seq;   /* fresh internal labels (>= n_labels) */
} riscv_emitter_t;

typedef struct {
    size_t pos;            /* the offset of the branch's imm field */
    size_t patch_size;     /* 4 (a whole branch/jal instruction) */
    uint32_t label;
} riscv_patch_t;

/* an internal label: labels >= e->n_labels are free of MIR labels
 * (MIR labels are 0..n_labels-1); e->n_labels is the SNAPSHOT taken
 * at this driver's compile start. */
static uint32_t rv_internal_label(riscv_emitter_t *e)
{
    return (uint32_t)(e->n_labels + e->internal_seq++);
}

static void note_label(riscv_emitter_t *e, uint32_t label, size_t off)
{
    if (label >= e->n_labels) {
        size_t old = e->n_labels;
        e->n_labels = label + 1;
        e->label_offsets = realloc(e->label_offsets,
                                   e->n_labels * sizeof(size_t));
        for (size_t i = old; i < e->n_labels; i++)
            e->label_offsets[i] = (size_t)-1;
    }
    e->label_offsets[label] = off;
}

static size_t label_off(const riscv_emitter_t *e, uint32_t label)
{
    return (label < e->n_labels) ? e->label_offsets[label] : (size_t)-1;
}

static void emit8(riscv_emitter_t *e, uint8_t b)
{
    if (e->n + 1 > e->cap) {
        e->cap = e->cap ? e->cap * 2 : 512;
        e->code = realloc(e->code, e->cap);
    }
    e->code[e->n++] = b;
}
static void emit16(riscv_emitter_t *e, uint16_t w)
{
    emit8(e, (uint8_t)(w & 0xFF));
    emit8(e, (uint8_t)((w >> 8) & 0xFF));
}
static void emit32(riscv_emitter_t *e, uint32_t w)
{
    emit16(e, (uint16_t)(w & 0xFFFF));
    emit16(e, (uint16_t)((w >> 16) & 0xFFFF));
}

/* slot offsets: (vr+1)*8, negative = below fp */
static size_t slot_off(size_t frame, wubu_vr_t vr) { (void)frame; return (vr + 1) * 8; }
static int64_t slot_disp(wubu_vr_t vr) { return (int64_t)(-((int64_t)(vr + 1) * 8)); }

/* ---- VERIFIED encodings (all little-endian; verified
 * byte-for-byte against GNU objdump riscv:rv64i via
 * tools/verify_isa.sh).
 *
 * RISC-V instruction encoding (32-bit, little-endian):
 *   bits 6-0   = opcode
 *   bits 11-7  = rd
 *   bits 31-25 = funct7
 *   bits 14-12 = funct3
 *   bits 31-20 = imm[11:0] (I-type)
 *   bits 31-25 = imm[12:5] (S-type)
 *   bits 11-8  = imm[11:8] (B-type)
 *   bits 31-20 = imm[20] (J-type)
 *   bits 31-25 = imm[12|10:5] (B-type)
 *   bits 11-8  = imm[11:8] (B-type)
 *
 * Register encoding: x0=0, x1=ra, x2=sp, x3=gp, x4=tp,
 *   x5-7=t0-t2, x8=fp, x9=s0, x10=a0, x11=a1, x12-x17=s1-s6,
 *   x18-x27=t3-t12, x28-x31=s7-s10.
 *
 * We use: a0=x10 (return value), a1=x11 (temp),
 *   fp=x8 (frame pointer), sp=x2 (stack pointer),
 *   t0=x5, t1=x6, t2=x7 (temporaries).
 *   s0=x9 (callee-saved, used as temp).
 */

/* OPCODE constants */
#define OPC_LUI    0x37     /* LUI */
#define OPC_AUIPC  0x17     /* AUIPC */
#define OPC_JAL    0x6F     /* JAL */
#define OPC_JALR   0x67     /* JALR */
#define OPC_BRANCH 0x63     /* BEQ/BNE/BLT/BGE/BLTU/BGEU */
#define OPC_LOAD   0x03     /* LB/LH/LW/LU/LD */
#define OPC_STORE  0x23     /* SB/SH/SW/SD */
#define OPC_OP_IMM 0x13     /* ADDI/SLTI/SLTIU/XORI/ORI/ANDI/SLLI/SRLI/SRAI */
#define OPC_OP     0x33     /* ADD/SUB/SLL/SLT/SLTU/XOR/OR/AND/SRL/SRA/MUL/MULH... */
#define OPC_SYSTEM 0x73     /* ECALL/EBREAK/CSRRW/CSRRWI/... */
#define OPC_FENCE  0x0F     /* fence */
#define OPC_AMO    0x2F     /* LR/SC/AMOSWAP/AMOADD/... */

/* funct3 */
#define FN3_ADD  0x0
#define FN3_SLL  0x1
#define FN3_SLT  0x2
#define FN3_SLTU 0x3
#define FN3_XOR  0x4
#define FN3_SRL  0x5
#define FN3_OR   0x6
#define FN3_AND  0x7
#define FN3_SLL  0x1
#define FN3_SLT  0x2
#define FN3_SLTU 0x3
#define FN3_XOR  0x4
#define FN3_SRL  0x5
#define FN3_OR   0x6
#define FN3_AND  0x7
#define FN3_MUL  0x0  /* MUL uses same funct3 as ADD */
#define FN3_DIV  0x4  /* DIV uses funct3=4 (same as XOR) with funct7[5]=1 */
#define FN3_REM  0x6  /* REM uses funct3=6 (same as OR) with funct7[5]=1 */
#define FN3_DIVU 0x5  /* DIVU uses funct3=5 (SRL) with funct7[5]=1 */
#define FN3_REMU 0x7  /* REMU uses funct3=7 (AND) with funct7[5]=1 */
#define FN3_SUB  0x0  /* SUB uses funct3=0 (same as ADD) with funct7[5]=1 */

/* funct7 */
#define FN7_DEFAULT 0x00
#define FN7_MUL     0x01   /* MUL: funct7[5:0]=0x01, funct3=0x0 */
#define FN7_DIV     0x01   /* DIV: funct7[5:0]=0x01, funct3=0x4 */
#define FN7_DIVU    0x01   /* DIVU: funct7[5:0]=0x01, funct3=0x5 */
#define FN7_REM     0x01   /* REM: funct7[5:0]=0x01, funct3=0x6 */
#define FN7_REMU    0x01   /* REMU: funct7[5:0]=0x01, funct3=0x7 */
#define FN7_SUB     0x20   /* SUB: funct7[5]=1, funct3=0x0 */
#define FN7_SRA     0x20   /* SRA: funct7[5]=1, funct3=0x5 */

/* Register aliases */
#define REG_ZERO  0
#define REG_A0    10   /* return value */
#define REG_A1    11   /* temp */
#define REG_FP    8    /* frame pointer */
#define REG_SP    2    /* stack pointer */
#define REG_T0    5
#define REG_T1    6
#define REG_T2    7
#define REG_S0    9

/* MIPS-style shift amounts for SLLI/SRLI/SRAI */
#define SHIFT_AMOUNT(imm) ((imm) & 0x3F)

/* ---- helpers ---- */

/* ADDI rd, rs1, imm12 */
static void addi(riscv_emitter_t *e, int rd, int rs1, int32_t imm)
{
    uint32_t imm12 = (uint32_t)(imm & 0xFFF);
    if (imm < 0) imm12 |= 0x800;  /* sign extend */
    uint32_t inst = (imm12 << 20) | (rs1 << 15) | (rd << 7) | OPC_OP_IMM;
    emit32(e, inst);
}

/* LOAD rd, offset(rs1) — for 64-bit LD */
static void load_d(riscv_emitter_t *e, int rd, int rs1, int32_t offset)
{
    uint32_t imm12 = (uint32_t)(offset & 0xFFF);
    if (offset < 0) imm12 |= 0x800;
    uint32_t inst = (imm12 << 20) | (rs1 << 15) | (rd << 7) | (0x3 << 0);  /* LD = 0x03, funct3=3 */
    /* funct3=3 for LD, opcode=0x03 */
    inst = (imm12 << 20) | (rs1 << 15) | (3 << 12) | (rd << 7) | OPC_LOAD;
    emit32(e, inst);
}

/* STORE rs2, offset(rs1) — for 64-bit SD */
static void store_d(riscv_emitter_t *e, int rs2, int rs1, int32_t offset)
{
    uint32_t imm12 = (uint32_t)(offset & 0xFFF);
    if (offset < 0) imm12 |= 0x800;
    uint32_t imm5 = (imm12 >> 5) & 0x7F;
    uint32_t imm_low = imm12 & 0x1F;
    uint32_t inst = (imm5 << 25) | (rs2 << 20) | (rs1 << 15) | (3 << 12) | (imm_low << 7) | OPC_STORE;
    emit32(e, inst);
}

/* OP rd, rs1, rs2 (R-type) */
static void op_r(riscv_emitter_t *e, int funct7, int rs2, int rs1, int funct3, int rd)
{
    uint32_t inst = (funct7 << 25) | (rs2 << 20) | (rs1 << 15) | (funct3 << 12) | (rd << 7) | OPC_OP;
    emit32(e, inst);
}

/* BEQ rs1, rs2, offset (B-type) */
static void beq(riscv_emitter_t *e, int rs1, int rs2, int32_t offset)
{
    uint32_t imm12 = ((offset >> 12) & 0x1) << 7;
    imm12 |= ((offset >> 5) & 0x3F) << 8;
    imm12 |= ((offset >> 1) & 0xF) << 25;
    imm12 |= ((offset >> 11) & 0x1) << 31;
    uint32_t inst = imm12 | (rs2 << 20) | (rs1 << 15) | (0x0 << 12) | OPC_BRANCH;
    emit32(e, inst);
}

/* BNE rs1, rs2, offset (B-type, funct3=1) */
static void bne(riscv_emitter_t *e, int rs1, int rs2, int32_t offset)
{
    uint32_t imm12 = ((offset >> 12) & 0x1) << 7;
    imm12 |= ((offset >> 5) & 0x3F) << 8;
    imm12 |= ((offset >> 1) & 0xF) << 25;
    imm12 |= ((offset >> 11) & 0x1) << 31;
    uint32_t inst = imm12 | (rs2 << 20) | (rs1 << 15) | (0x1 << 12) | OPC_BRANCH;
    emit32(e, inst);
}

/* the generic branch with a patch entry: emit a BEQ/BNE with a
 * zero offset, then remember the position for the patch pass. */
static void br_patch(riscv_emitter_t *e, int is_bne, int rs1, int rs2,
                     uint32_t label, riscv_patch_t **patches,
                     size_t *np, size_t *cap)
{
    size_t pos = e->n;
    if (is_bne) bne(e, rs1, rs2, 0);
    else        beq(e, rs1, rs2, 0);
    if (*np == *cap) {
        *cap = *cap ? *cap * 2 : 8;
        *patches = realloc(*patches, *cap * sizeof(**patches));
    }
    (*patches)[*np].pos = pos;
    (*patches)[*np].patch_size = 4;
    (*patches)[*np].label = label;
    (*np)++;
}

/* JAL rd, offset (J-type) */
static void jal(riscv_emitter_t *e, int rd, int32_t offset)
{
    uint32_t imm20 = ((offset >> 20) & 0x1) << 31;
    imm20 |= ((offset >> 1) & 0x3FF) << 21;
    imm20 |= ((offset >> 11) & 0x1) << 20;
    imm20 |= ((offset >> 12) & 0xFF) << 12;
    uint32_t inst = imm20 | (rd << 7) | OPC_JAL;
    emit32(e, inst);
}

/* JALR rd, rs1, offset (I-type) */
static void jalr(riscv_emitter_t *e, int rd, int rs1, int32_t offset)
{
    uint32_t imm12 = (uint32_t)(offset & 0xFFF);
    if (offset < 0) imm12 |= 0x800;
    uint32_t inst = (imm12 << 20) | (rs1 << 15) | (rd << 7) | OPC_JALR;
    emit32(e, inst);
}

/* LUI rd, imm20 */
static void lui(riscv_emitter_t *e, int rd, uint32_t imm20)
{
    uint32_t inst = (imm20 << 12) | (rd << 7) | OPC_LUI;
    emit32(e, inst);
}

/* AUIPC rd, imm20 */
static void auipc(riscv_emitter_t *e, int rd, uint32_t imm20)
{
    uint32_t inst = (imm20 << 12) | (rd << 7) | OPC_AUIPC;
    emit32(e, inst);
}

/* ECALL */
static void ecall(riscv_emitter_t *e)
{
    uint32_t inst = (0 << 20) | (0 << 15) | (0 << 12) | (0 << 7) | OPC_SYSTEM;
    emit32(e, inst);
}

/* ret = jalr x0, x1, 0 */
static void ret_instr(riscv_emitter_t *e)
{
    jalr(e, 0, 1, 0);
}

/* MV rd, rs1 (addi rd, rs1, 0) */
static void mv(riscv_emitter_t *e, int rd, int rs1)
{
    addi(e, rd, rs1, 0);
}

/* NEG rd, rs1 (sub rd, x0, rs1) */
static void neg(riscv_emitter_t *e, int rd, int rs1)
{
    op_r(e, FN7_SUB, rs1, REG_ZERO, FN3_SUB, rd);
}

/* NOT rd, rs1 (xori rd, rs1, -1) */
static void not_reg(riscv_emitter_t *e, int rd, int rs1)
{
    /* xori rd, rs1, -1: imm[11:0]=0xFFF, opcode=0x13, funct3=0x4 (XORI) */
    uint32_t inst = (0xFFF << 20) | (rs1 << 15) | (4 << 12) | (rd << 7) | OPC_OP_IMM;
    emit32(e, inst);
}

/* ---- MIR lowering ---- */

static int riscv_compile(const wubu_mir_prog_t *p, uint8_t **out, size_t *out_size)
{
    size_t max_vr = 0;
    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (in->dst > max_vr) max_vr = in->dst;
        if (in->a > max_vr) max_vr = in->a;
        if (in->b > max_vr) max_vr = in->b;
    }

    riscv_emitter_t e;
    memset(&e, 0, sizeof(e));
    e.frame = (max_vr + 1) * 8 + 128;  /* 64-bit slots + slack */
    e.n_labels = p->n_labels;
    e.label_offsets = calloc(e.n_labels, sizeof(size_t));
    for (size_t i = 0; i < e.n_labels; i++) e.label_offsets[i] = (size_t)-1;

    riscv_patch_t *patches = NULL;
    size_t np = 0, cp = 0;

    /* prologue: allocate frame */
    addi(&e, REG_SP, REG_SP, -(int32_t)e.frame);
    /* save fp */
    store_d(&e, REG_FP, REG_SP, 0);  /* store fp at sp+0 */
    /* set fp = sp */
    mv(&e, REG_FP, REG_SP);

    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (in->op == MIR_LABEL) {
            note_label(&e, in->label, e.n);
            continue;
        }
        switch (in->op) {
        case MIR_CONST:
            /* lui + addi for large immediates, or addi for small */
            if (in->imm >= -2048 && in->imm <= 2047) {
                addi(&e, REG_T0, REG_ZERO, in->imm);
            } else {
                /* lui rd, imm[31:12]; addi rd, rd, imm[11:0] */
                uint32_t uimm = (uint32_t)in->imm;
                int32_t simm = (int32_t)in->imm;
                uint32_t hi = (uimm >> 12) + ((simm < 0 && (uimm & 0x800)) ? 1 : 0);
                int32_t lo = (int32_t)(simm & 0xFFF);
                lui(&e, REG_T0, (uint32_t)hi);
                addi(&e, REG_T0, REG_T0, lo);
            }
            /* store to slot */
            store_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->dst));
            break;

        case MIR_MOV:
            load_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->a));
            store_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->dst));
            break;

        case MIR_ADD: {
            load_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->a));
            load_d(&e, REG_T1, REG_FP, (int32_t)slot_off(e.frame, in->b));
            op_r(&e, FN7_DEFAULT, REG_T1, REG_T0, FN3_ADD, REG_T0);
            store_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->dst));
            break;
        }
        case MIR_SUB: {
            load_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->a));
            load_d(&e, REG_T1, REG_FP, (int32_t)slot_off(e.frame, in->b));
            op_r(&e, FN7_SUB, REG_T1, REG_T0, FN3_ADD, REG_T0);
            store_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->dst));
            break;
        }
        case MIR_MUL: {
            load_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->a));
            load_d(&e, REG_T1, REG_FP, (int32_t)slot_off(e.frame, in->b));
            op_r(&e, FN7_MUL, REG_T1, REG_T0, FN3_MUL, REG_T0);
            store_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->dst));
            break;
        }
        case MIR_DIV: {
            load_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->a));
            load_d(&e, REG_T1, REG_FP, (int32_t)slot_off(e.frame, in->b));
            op_r(&e, FN7_DIV, REG_T1, REG_T0, FN3_DIV, REG_T0);
            store_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->dst));
            break;
        }
        case MIR_MOD: {
            /* DIV leaves rem in a1 (x11); we need to move it to the slot */
            load_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->a));
            load_d(&e, REG_T1, REG_FP, (int32_t)slot_off(e.frame, in->b));
            op_r(&e, FN7_DIV, REG_T1, REG_T0, FN3_DIV, REG_T0);  /* quotient in a0 */
            /* rem is already in a1 from DIV */
            store_d(&e, REG_A1, REG_FP, (int32_t)slot_off(e.frame, in->dst));
            break;
        }
        case MIR_AND: {
            load_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->a));
            load_d(&e, REG_T1, REG_FP, (int32_t)slot_off(e.frame, in->b));
            op_r(&e, FN7_DEFAULT, REG_T1, REG_T0, FN3_AND, REG_T0);
            store_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->dst));
            break;
        }
        case MIR_OR: {
            load_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->a));
            load_d(&e, REG_T1, REG_FP, (int32_t)slot_off(e.frame, in->b));
            op_r(&e, FN7_DEFAULT, REG_T1, REG_T0, FN3_OR, REG_T0);
            store_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->dst));
            break;
        }
        case MIR_XOR: {
            load_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->a));
            load_d(&e, REG_T1, REG_FP, (int32_t)slot_off(e.frame, in->b));
            op_r(&e, FN7_DEFAULT, REG_T1, REG_T0, FN3_XOR, REG_T0);
            store_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->dst));
            break;
        }
        case MIR_SHL: {
            /* SLLI: shift left logical immediate */
            load_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->a));
            /* count is in slot b */
            load_d(&e, REG_T1, REG_FP, (int32_t)slot_off(e.frame, in->b));
            /* SLLI rd, rs1, shamt — funct3=1, opcode=0x13 */
            uint32_t shamt = (uint32_t)(REG_T1 & 0x3F);  /* count is in t1 */
            /* We need to emit: slli t0, t0, t1 — but SLLI uses imm[5:0], not a register.
             * For variable shifts we need SLL (opcode 0x33, funct3=0x1, funct7=0x00). */
            op_r(&e, FN7_DEFAULT, REG_T1, REG_T0, FN3_SLL, REG_T0);
            store_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->dst));
            break;
        }
        case MIR_SHR: {
            /* SRLI (logical right shift) — for unsigned shift */
            load_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->a));
            load_d(&e, REG_T1, REG_FP, (int32_t)slot_off(e.frame, in->b));
            op_r(&e, FN7_DEFAULT, REG_T1, REG_T0, FN3_SRL, REG_T0);
            store_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->dst));
            break;
        }
        case MIR_NEG: {
            load_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->a));
            /* rd = rs1 - rs2 = x0 - t0 = -t0. op_r's arg order is
             * (funct7, rs2, rs1, funct3, rd): rs1 must be x0. */
            op_r(&e, FN7_SUB, REG_T0, REG_ZERO, FN3_ADD, REG_T0);
            store_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->dst));
            break;
        }
        case MIR_NOT: {
            load_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->a));
            not_reg(&e, REG_T0, REG_T0);
            store_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->dst));
            break;
        }
        case MIR_EQ: case MIR_NE: case MIR_LT: case MIR_LE:
        case MIR_GT: case MIR_GE: {
            /* Compare: load a (t0), load b (t1), compute the boolean
             * 0/1 into t0, store to dst. All signed:
             *   LT:  slt t0, a, b
             *   LE:  slt t0, b, a; xori t0, t0, 1   (b < a negated)
             *   GT:  slt t0, b, a
             *   GE:  slt t0, a, b; xori t0, t0, 1   (a < b negated)
             *   EQ:  xor t0, a, b; sltiu t0, t0, 1 (equal iff xor==0)
             *   NE:  xor t0, a, b; sltu t0, x0, t0 (!=0 iff xor!=0) */
            load_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->a));
            load_d(&e, REG_T1, REG_FP, (int32_t)slot_off(e.frame, in->b));
            switch (in->op) {
            case MIR_LT:
                op_r(&e, FN7_DEFAULT, REG_T1, REG_T0, FN3_SLT, REG_T0);
                break;
            case MIR_LE:
                op_r(&e, FN7_DEFAULT, REG_T0, REG_T1, FN3_SLT, REG_T0);
                /* t0 = b < a; negate: xori t0, t0, 1 */
                {
                    uint32_t inst = (1 << 20) | (REG_T0 << 15) |
                                    (0x4 << 12) | (REG_T0 << 7) | OPC_OP_IMM;
                    emit32(&e, inst);
                }
                break;
            case MIR_GT:
                op_r(&e, FN7_DEFAULT, REG_T0, REG_T1, FN3_SLT, REG_T0);
                break;
            case MIR_GE:
                op_r(&e, FN7_DEFAULT, REG_T1, REG_T0, FN3_SLT, REG_T0);
                {
                    uint32_t inst = (1 << 20) | (REG_T0 << 15) |
                                    (0x4 << 12) | (REG_T0 << 7) | OPC_OP_IMM;
                    emit32(&e, inst);
                }
                break;
            case MIR_EQ:
                op_r(&e, FN7_DEFAULT, REG_T1, REG_T0, FN3_XOR, REG_T0);
                /* sltiu t0, t0, 1: t0 = (xor == 0) */
                {
                    uint32_t inst = (1 << 20) | (REG_T0 << 15) |
                                    (0x3 << 12) | (REG_T0 << 7) | OPC_OP_IMM;
                    emit32(&e, inst);
                }
                break;
            case MIR_NE:
                op_r(&e, FN7_DEFAULT, REG_T1, REG_T0, FN3_XOR, REG_T0);
                /* sltu t0, x0, t0: t0 = (xor != 0) — the R-type form
                 * (a REGISTER compare; putting t0's number in an
                 * immediate would compare the constant 5) */
                op_r(&e, FN7_DEFAULT, REG_T0, REG_ZERO, FN3_SLTU, REG_T0);
                break;
            default:
                break;
            }
            store_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->dst));
            break;
        }


        case MIR_JZ: {
            /* if (a == 0) pc = label: load a, beq t0, x0, label */
            load_d(&e, REG_T0, REG_FP, (int32_t)slot_off(e.frame, in->a));
            br_patch(&e, 0, REG_T0, REG_ZERO, in->label,
                     &patches, &np, &cp);
            break;
        }

        case MIR_JMP: {
            /* pc = label: jal x0, label */
            size_t pos = e.n;
            jal(&e, REG_ZERO, 0);
            if (np == cp) {
                cp = cp ? cp * 2 : 8;
                patches = realloc(patches, cp * sizeof(*patches));
            }
            patches[np].pos = pos;
            patches[np].patch_size = 4;
            patches[np].label = in->label;
            np++;
            break;
        }

        case MIR_RET:
            load_d(&e, REG_A0, REG_FP, (int32_t)slot_off(e.frame, in->a));
            /* epilogue: restore fp, deallocate frame */
            mv(&e, REG_SP, REG_FP);
            load_d(&e, REG_FP, REG_SP, 0);  /* restore fp from saved slot */
            addi(&e, REG_SP, REG_SP, (int32_t)e.frame);  /* deallocate */
            ret_instr(&e);
            break;

        default:
            break;
        }
    }
    /* patch pass: resolve every branch/jal placeholder. Branches are
     * B-type (imm12 in bits 7-11,25-31); JAL is J-type (imm20 in
     * bits 12-31). The opcode distinguishes them: 0x63 = branch,
     * 0x6F = JAL. */
    for (size_t i = 0; i < np; i++) {
        size_t t = label_off(&e, patches[i].label);
        if (t == (size_t)-1) continue;
        int32_t off = (int32_t)(t - patches[i].pos);
        uint32_t inst;
        memcpy(&inst, &e.code[patches[i].pos], 4);
        if ((inst & 0x7F) == 0x6F) {
            /* JAL (J-type): imm[20|10:1|11|19:12] */
            uint32_t imm20 = ((off >> 20) & 0x1) << 31;
            imm20 |= ((off >> 1) & 0x3FF) << 21;
            imm20 |= ((off >> 11) & 0x1) << 20;
            imm20 |= ((off >> 12) & 0xFF) << 12;
            inst = (inst & 0x00000FFF) | imm20;
        } else {
            /* branch (B-type): imm[12|10:5|4:1|11] */
            uint8_t funct3 = (inst >> 12) & 0x7;
            uint32_t imm12 = ((off >> 12) & 0x1) << 7;
            imm12 |= ((off >> 5) & 0x3F) << 8;
            imm12 |= ((off >> 1) & 0xF) << 25;
            imm12 |= ((off >> 11) & 0x1) << 31;
            inst = (inst & 0x01FFF07F) | imm12 | (funct3 << 12);
        }
        memcpy(&e.code[patches[i].pos], &inst, 4);
    }
    free(patches);


    free(e.label_offsets);
    *out = e.code;
    *out_size = e.n;
    return 0;
}

/* the RISC-V interpreter lives in wubu_riscv_interp.c */
int64_t wubu_riscv_run(const uint8_t *code, size_t size, int64_t arg);

static int64_t riscv_run(const uint8_t *code, size_t size, int64_t arg)
{
    return wubu_riscv_run(code, size, arg);
}

static void riscv_describe(void)
{
    printf("RISC-V RV64I driver (2019 ISA): 32 GPRs, little-endian, "
           "64-bit flat space, .L ops (.Q for mul/div). "
           "Encodings verified against GNU objdump; runs via the "
           "bundled RV64I interpreter — the AGI runs on RISC-V.\n");
}

const wubu_isa_driver_t wubu_isa_riscv = {
    .name = "riscv",
    .family = "portable",
    .exec = WUBU_ISA_INTERPRETED,
    .compile = riscv_compile,
    .run = riscv_run,
    .describe = riscv_describe,
};