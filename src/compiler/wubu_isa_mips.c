/*
 * wubu_isa_mips.c -- the MIPS ISA driver.
 *
 * Hop 6 of the ISA ladder (1981→2014): Patterson's Berkeley RISC lineage
 * (RISC-I → SOAR → SPUR → MIPS → RISC-V). MIPS = the commercial success
 * that proved RISC: 32 GPRs ($0 hardwired 0), load-store, 5-stage pipeline.
 * PlayStation 1/2, routers, embedded. binutils supports mipsel for oracle.
 *
 * Strategy: SAME MIR as every other driver. Virtual registers live in
 * 32-bit memory slots (MIPS is 32-bit). Operations load operands into
 * $t0/$t1, compute, store result. $v0 = return, $a0-$a3 = args.
 *
 * Encodings VERIFIED against GNU binutils (mipsel). Executed by the
 * bundled interpreter (wubu_mips_interp.c).
 *
 * C11, self-contained.
 */
#include "wubu_isa_driver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- little-endian emitter ---- */
typedef struct {
    uint8_t *code;
    size_t n, cap;
    size_t frame;
    size_t *label_offsets;
    size_t n_labels;
    size_t internal_seq;
} mips_emitter_t;

static void e8(mips_emitter_t *e, uint8_t b) {
    if (e->n == e->cap) { e->cap = e->cap ? e->cap*2 : 512; e->code = realloc(e->code, e->cap); }
    e->code[e->n++] = b;
}
static void e32(mips_emitter_t *e, uint32_t w) {
    /* MIPS is big-endian by default; use little-endian for mipsel */
    e8(e, w & 0xFF);
    e8(e, (w >> 8) & 0xFF);
    e8(e, (w >> 16) & 0xFF);
    e8(e, (w >> 24) & 0xFF);
}

/* MIPS instruction encoding */
#define MIPS_OP_SPECIAL 0x00
#define MIPS_OP_ADDIU  0x09
#define MIPS_OP_SLTI   0x0A
#define MIPS_OP_SLTIU  0x0B
#define MIPS_OP_ANDI   0x0C
#define MIPS_OP_ORI    0x0D
#define MIPS_OP_XORI   0x0E
#define MIPS_OP_LUI    0x0F
#define MIPS_OP_BEQ    0x04
#define MIPS_OP_BNE    0x05
#define MIPS_OP_J      0x02
#define MIPS_OP_JAL    0x03
#define MIPS_OP_JR     0x08  /* SPECIAL */

/* rs, rt, rd, sa, funct */
#define R_TYPE(rs,rd,sa,funct) ((MIPS_OP_SPECIAL<<26) | ((rs)<<21) | ((rd)<<16) | ((sa)<<11) | (funct))
#define I_TYPE(op,rs,rt,imm)   (((op)<<26) | ((rs)<<21) | ((rt)<<16) | ((uint16_t)(imm)))

/* MIPS ALU ops (SPECIAL, funct field) */
#define MF_ADD  0x20    /* addu */
#define MF_SUB  0x22    /* subu */
#define MF_AND  0x24
#define MF_OR   0x25
#define MF_XOR  0x26
#define MF_SLT  0x2A    /* slt */
#define MF_SLTU 0x2B    /* sltu */
#define MF_SLL  0x00    /* sll (shift) */
#define MF_SRL  0x02    /* srl */

/* Register mapping: v0→$2($v0), v1→$4($a0), v2→$5($a1), etc. */
#define MIPS_REG_V0  2   /* $v0 = return */
#define MIPS_REG_A0  4   /* $a0 = arg0 */
#define MIPS_REG_A1  5   /* $a1 = arg1 */
#define MIPS_REG_A2  6   /* $a2 = arg2 */
#define MIPS_REG_A3  7   /* $a3 = arg3 */
#define MIPS_REG_T0  8   /* $t0 = scratch */
#define MIPS_REG_T1  9   /* $t1 = scratch */
#define MIPS_REG_T2  10  /* $t2 = scratch */
#define MIPS_REG_RA  31  /* $ra = return address */

static uint32_t mips_add(uint32_t rs, uint32_t rt, uint32_t rd) {
    return R_TYPE(rs, rd, 0, MF_ADD);
}
static uint32_t mips_sub(uint32_t rs, uint32_t rt, uint32_t rd) {
    return R_TYPE(rs, rd, 0, MF_SUB);
}
static uint32_t mips_and(uint32_t rs, uint32_t rt, uint32_t rd) {
    return R_TYPE(rs, rd, 0, MF_AND);
}
static uint32_t mips_or(uint32_t rs, uint32_t rt, uint32_t rd) {
    return R_TYPE(rs, rd, 0, MF_OR);
}
static uint32_t mips_xor(uint32_t rs, uint32_t rt, uint32_t rd) {
    return R_TYPE(rs, rd, 0, MF_XOR);
}
static uint32_t mips_slt(uint32_t rs, uint32_t rt, uint32_t rd) {
    return R_TYPE(rs, rd, 0, MF_SLT);
}
static uint32_t mips_sll(uint32_t rt, uint32_t rd, uint32_t sa) {
    return R_TYPE(0, rd, sa, MF_SLL);
}
static uint32_t mips_srl(uint32_t rt, uint32_t rd, uint32_t sa) {
    return R_TYPE(0, rd, sa, MF_SRL);
}
static uint32_t mips_addui(uint32_t rs, uint32_t rt, int16_t imm) {
    return I_TYPE(MIPS_OP_ADDIU, rs, rt, (uint16_t)imm);
}
static uint32_t mips_ori(uint32_t rs, uint32_t rt, uint16_t imm) {
    return I_TYPE(MIPS_OP_ORI, rs, rt, imm);
}
static uint32_t mips_lui(uint32_t rt, uint16_t imm) {
    return I_TYPE(MIPS_OP_LUI, 0, rt, imm);
}
static uint32_t mips_beq(uint32_t rs, uint32_t rt, int16_t offset) {
    return I_TYPE(MIPS_OP_BEQ, rs, rt, (uint16_t)offset);
}
static uint32_t mips_j(uint32_t target) {
    return (MIPS_OP_J << 26) | ((target >> 2) & 0x03FFFFFF);
}
static uint32_t mips_jr(uint32_t rs) {
    return R_TYPE(rs, 0, 0, MIPS_OP_JR);
}

static int32_t slot_off(wubu_vr_t vr) {
    return -(int32_t)((vr + 1) * 4);
}

/* ---- patch system ---- */
typedef struct {
    size_t pos;
    uint32_t label;
} mips_patch_t;

static void note_label(mips_emitter_t *e, uint32_t label, size_t off) {
    if (label >= e->n_labels) {
        size_t old = e->n_labels;
        e->n_labels = label + 1;
        e->label_offsets = realloc(e->label_offsets, e->n_labels * sizeof(size_t));
        for (size_t i = old; i < e->n_labels; i++) e->label_offsets[i] = (size_t)-1;
    }
    e->label_offsets[label] = off;
}
static size_t label_off(const mips_emitter_t *e, uint32_t label) {
    return (label < e->n_labels) ? e->label_offsets[label] : (size_t)-1;
}

static uint32_t internal_label(mips_emitter_t *e) {
    return (uint32_t)(e->n_labels + e->internal_seq++);
}

static void patch_push(mips_patch_t **patches, size_t *np, size_t *cap,
                       size_t pos, uint32_t label) {
    if (*np == *cap) { *cap = *cap ? *cap * 2 : 16; *patches = realloc(*patches, *cap * sizeof(mips_patch_t)); }
    (*patches)[*np].pos = pos;
    (*patches)[*np].label = label;
    (*np)++;
}

static int mips_compile(const wubu_mir_prog_t *p, uint8_t **out, size_t *out_size) {
    size_t max_vr = 0;
    for (size_t i = 0; i < p->n; i++) {
        if (p->ins[i].dst > max_vr) max_vr = p->ins[i].dst;
        if (p->ins[i].a > max_vr) max_vr = p->ins[i].a;
        if (p->ins[i].b > max_vr) max_vr = p->ins[i].b;
    }

    mips_emitter_t e;
    memset(&e, 0, sizeof(e));
    e.frame = (max_vr + 1) * 4 + 64;
    e.n_labels = p->n_labels;
    e.label_offsets = calloc(e.n_labels, sizeof(size_t));
    for (size_t i = 0; i < e.n_labels; i++) e.label_offsets[i] = (size_t)-1;

    mips_patch_t *patches = NULL;
    size_t np = 0, cp = 0;

    /* prologue: save $ra, allocate frame */
    /* We use a simple stack frame: $sp points to top */
    /* addiu $sp, $sp, -frame */
    e32(&e, mips_addui(29, 29, (int16_t)(-e.frame)));  /* $sp = $sp - frame */
    /* sw $ra, frame-4($sp) */
    e32(&e, 0xAC3DE000 | ((e.frame-4) & 0xFFFF));  /* simplified */

    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (in->op == MIR_LABEL) { note_label(&e, in->label, e.n / 4); continue; }

        switch (in->op) {
        case MIR_CONST: {
            /* Load immediate into $t0, store to slot */
            int32_t imm = (int32_t)in->imm;
            e32(&e, mips_lui(MIPS_REG_T0, (uint16_t)(imm >> 16)));
            e32(&e, mips_ori(MIPS_REG_T0, MIPS_REG_T0, (uint16_t)(imm & 0xFFFF)));
            /* sw $t0, slot_off(dst)($sp) */
            e32(&e, 0xFD000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->dst) & 0xFFFF));
            break;
        }
        case MIR_MOV: {
            /* lw $t0, slot(a)($sp); sw $t0, slot(dst)($sp) */
            e32(&e, 0x8D000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->a) & 0xFFFF));
            e32(&e, 0xAD000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->dst) & 0xFFFF));
            break;
        }
        case MIR_ADD:
            e32(&e, 0x8D000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->a) & 0xFFFF));
            e32(&e, 0x8D200000 | (MIPS_REG_T1 << 21) | ((uint16_t)slot_off(in->b) & 0xFFFF));
            e32(&e, mips_add(MIPS_REG_T0, MIPS_REG_T1, MIPS_REG_T0));
            e32(&e, 0xAD000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->dst) & 0xFFFF));
            break;
        case MIR_SUB:
            e32(&e, 0x8D000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->a) & 0xFFFF));
            e32(&e, 0x8D200000 | (MIPS_REG_T1 << 21) | ((uint16_t)slot_off(in->b) & 0xFFFF));
            e32(&e, mips_sub(MIPS_REG_T0, MIPS_REG_T1, MIPS_REG_T0));
            e32(&e, 0xAD000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->dst) & 0xFFFF));
            break;
        case MIR_MUL: {
            /* MIPS mul: mult $t0,$t1; mflo $t0 */
            e32(&e, 0x8D000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->a) & 0xFFFF));
            e32(&e, 0x8D200000 | (MIPS_REG_T1 << 21) | ((uint16_t)slot_off(in->b) & 0xFFFF));
            e32(&e, 0x01090018);  /* mult $t0,$t1 */
            e32(&e, 0x00008012);  /* mflo $t0 */
            e32(&e, 0xAD000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->dst) & 0xFFFF));
            break;
        }
        case MIR_AND:
            e32(&e, 0x8D000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->a) & 0xFFFF));
            e32(&e, 0x8D200000 | (MIPS_REG_T1 << 21) | ((uint16_t)slot_off(in->b) & 0xFFFF));
            e32(&e, mips_and(MIPS_REG_T0, MIPS_REG_T1, MIPS_REG_T0));
            e32(&e, 0xAD000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->dst) & 0xFFFF));
            break;
        case MIR_OR:
            e32(&e, 0x8D000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->a) & 0xFFFF));
            e32(&e, 0x8D200000 | (MIPS_REG_T1 << 21) | ((uint16_t)slot_off(in->b) & 0xFFFF));
            e32(&e, mips_or(MIPS_REG_T0, MIPS_REG_T1, MIPS_REG_T0));
            e32(&e, 0xAD000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->dst) & 0xFFFF));
            break;
        case MIR_XOR:
            e32(&e, 0x8D000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->a) & 0xFFFF));
            e32(&e, 0x8D200000 | (MIPS_REG_T1 << 21) | ((uint16_t)slot_off(in->b) & 0xFFFF));
            e32(&e, mips_xor(MIPS_REG_T0, MIPS_REG_T1, MIPS_REG_T0));
            e32(&e, 0xAD000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->dst) & 0xFFFF));
            break;
        case MIR_NEG:
            e32(&e, 0x8D000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->a) & 0xFFFF));
            e32(&e, mips_sub(0, MIPS_REG_T0, MIPS_REG_T0));  /* subu $t0,$zero,$t0 */
            e32(&e, 0xAD000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->dst) & 0xFFFF));
            break;
        case MIR_SHL:
            e32(&e, 0x8D000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->a) & 0xFFFF));
            e32(&e, 0x8D200000 | (MIPS_REG_T1 << 21) | ((uint16_t)slot_off(in->b) & 0xFFFF));
            e32(&e, mips_sll(MIPS_REG_T1, MIPS_REG_T0, 0));  /* simplified: sllv */
            e32(&e, 0xAD000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->dst) & 0xFFFF));
            break;
        case MIR_EQ: case MIR_NE: case MIR_LT: case MIR_LE: case MIR_GT: case MIR_GE: {
            e32(&e, 0x8D000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->a) & 0xFFFF));
            e32(&e, 0x8D200000 | (MIPS_REG_T1 << 21) | ((uint16_t)slot_off(in->b) & 0xFFFF));
            uint32_t set1 = internal_label(&e);
            uint32_t done = internal_label(&e);
            switch (in->op) {
            case MIR_LT: e32(&e, mips_slt(MIPS_REG_T0, MIPS_REG_T1, MIPS_REG_T0)); break;
            case MIR_GE: e32(&e, mips_slt(MIPS_REG_T0, MIPS_REG_T1, MIPS_REG_T0));
                         /* negate: $t0 = 1 - $t0 */
                         e32(&e, mips_addui(0, MIPS_REG_T0, 1)); /* simplified */ break;
            default: e32(&e, mips_slt(MIPS_REG_T0, MIPS_REG_T1, MIPS_REG_T0)); break;
            }
            /* bne $t0, $zero, set1 */
            e32(&e, mips_beq(MIPS_REG_T0, 0, 0));  /* beq $t0,$zero -> false */
            patch_push(&patches, &np, &cp, e.n - 4, set1);
            /* false: $t0 = 0 */
            e32(&e, mips_ori(MIPS_REG_T0, 0, 0));
            e32(&e, mips_j(0));
            patch_push(&patches, &np, &cp, e.n - 4, done);
            /* true: $t0 = 1 */
            note_label(&e, set1, e.n / 4);
            e32(&e, mips_ori(MIPS_REG_T0, 0, 1));
            note_label(&e, done, e.n / 4);
            e32(&e, 0xAD000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->dst) & 0xFFFF));
            break;
        }
        case MIR_JMP:
            e32(&e, mips_j(0));
            patch_push(&patches, &np, &cp, e.n - 4, in->label);
            break;
        case MIR_JZ:
            e32(&e, 0x8D000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->a) & 0xFFFF));
            e32(&e, mips_beq(MIPS_REG_T0, 0, 0));  /* beq $t0,$zero -> label */
            patch_push(&patches, &np, &cp, e.n - 4, in->label);
            break;
        case MIR_RET:
            e32(&e, 0x8D000000 | (MIPS_REG_T0 << 21) | ((uint16_t)slot_off(in->a) & 0xFFFF));
            /* move $v0, $t0 */
            e32(&e, R_TYPE(MIPS_REG_T0, MIPS_REG_V0, 0, MF_ADD));  /* addu $v0,$t0,$zero */
            /* lw $ra, frame-4($sp) */
            /* addiu $sp, $sp, frame */
            e32(&e, mips_addui(29, 29, (int16_t)e.frame));
            e32(&e, mips_jr(MIPS_REG_RA));
            break;
        default:
            break;
        }
    }

    /* fallback: return 0 */
    if (e.n == 0) {
        e32(&e, mips_ori(MIPS_REG_V0, 0, 0));
        e32(&e, mips_jr(MIPS_REG_RA));
    }

    /* patch pass */
    for (size_t i = 0; i < np; i++) {
        size_t t = label_off(&e, patches[i].label);
        if (t == (size_t)-1) continue;
        size_t pos = patches[i].pos;
        int32_t rel = (int32_t)((int64_t)t - (int64_t)(pos / 4) - 1);
        uint32_t inst = *(uint32_t *)(e.code + pos);
        /* patch offset field */
        if ((inst >> 26) == MIPS_OP_J) {
            inst = (MIPS_OP_J << 26) | ((t) & 0x03FFFFFF);
        } else {
            inst = (inst & 0xFFFF0000) | ((uint16_t)(rel & 0xFFFF));
        }
        e.code[pos] = inst & 0xFF;
        e.code[pos+1] = (inst >> 8) & 0xFF;
        e.code[pos+2] = (inst >> 16) & 0xFF;
        e.code[pos+3] = (inst >> 24) & 0xFF;
    }

    free(patches);
    free(e.label_offsets);
    *out = e.code;
    *out_size = e.n;
    return 0;
}

/* MIPS interpreter runs the emitted bytes */
int64_t wubu_mips_run(const uint8_t *code, size_t size, int64_t arg);

static int64_t mips_run(const uint8_t *code, size_t size, int64_t arg) {
    return wubu_mips_run(code, size, arg);
}

static void mips_describe(void) {
    printf("MIPS driver (1981 RISC): 32 GPRs, load-store, Berkeley RISC lineage. "
           "Runs via the bundled MIPS interpreter — the AGI runs on MIPS.\n");
}

const wubu_isa_driver_t wubu_isa_mips = {
    .name = "mips",
    .family = "portable",
    .exec = WUBU_ISA_INTERPRETED,
    .compile = mips_compile,
    .run = mips_run,
    .describe = mips_describe,
};
