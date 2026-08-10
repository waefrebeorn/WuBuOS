/*
 * wubu_isa_z80.c -- the Zilog Z80 ISA driver.
 *
 * Hop 2 of the ISA ladder (1976): Faggin left Intel after the 8080,
 * built the Z80 as a MORE capable chip that stayed 8080-compatible.
 * 8-bit data, 16-bit address, dual register banks (AF/BC/DE/HL +
 * shadow AF'/BC'/DE'/HL'), index registers IX/IY, and the famous
 * variable-length block I/O (LDIR/CPIR). CP/M + ZX Spectrum/TRS-80/MSX.
 *
 * Strategy: SAME MIR as every other driver. Virtual registers live in
 * 16-bit memory slots (the Z80 has a real 16-bit address space); the
 * accumulator A does 8-bit ALU work, HL does 16-bit moves. The
 * differential battery (33 expressions) runs every driver against gcc.
 *
 * Encodings VERIFIED against GNU objdump (z80). Executed by the
 * bundled interpreter (wubu_z80_interp.c).
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
    size_t internal_seq;   /* fresh internal labels (>= n_labels) */
} z80_emitter_t;

/* an internal label: labels >= e->n_labels are free of MIR labels
 * (MIR labels are 0..n_labels-1). Note: the OTHER drivers may have
 * grown p->n_labels via wubu_mir_new_label, so e->n_labels is the
 * SNAPSHOT taken at this driver's compile start — internal labels
 * starting there can never collide with this program's real labels. */
static uint32_t internal_label(z80_emitter_t *e)
{
    return (uint32_t)(e->n_labels + e->internal_seq++);
}

static void e8(z80_emitter_t *e, uint8_t b)
{
    if (e->n + 1 > e->cap) {
        e->cap = e->cap ? e->cap * 2 : 256;
        e->code = realloc(e->code, e->cap);
    }
    e->code[e->n++] = b;
}
static void e16(z80_emitter_t *e, uint16_t w)
{
    e8(e, (uint8_t)(w & 0xFF));
    e8(e, (uint8_t)((w >> 8) & 0xFF));
}

/* memory slot for a virtual register: 2 bytes little-endian (16-bit) */
static uint16_t slot_addr(wubu_vr_t vr) { return (uint16_t)(vr * 2); }

/* ---- Z80 opcodes (VERIFIED against GNU objdump z80) ---- */
/* LD r, n      : 00rrr110 n            (A=111, B=000, C=001, D=010, E=011, H=100, L=101)
 * LD A, (nn)   : 00111010 nn
 * LD (nn), A   : 00110010 nn
 * LD HL, (nn)  : 00101010 nn
 * LD (nn), HL  : 00100010 nn
 * ADD A, r     : 10000rrr
 * SUB r        : 10010rrr
 * AND r        : 10100rrr
 * OR r         : 10110rrr
 * XOR r        : 10101rrr
 * CP r         : 10111rrr
 * INC r        : 00rrr100
 * DEC r        : 00rrr101
 * JP nn        : 11000011 nn
 * JP cc, nn    : 11cc010 nn
 * JR d         : 00011000 d
 * JR cc, d     : 001cc000 d
 * CALL nn      : 11001101 nn
 * RET          : 11001001
 * NOP          : 00000000
 * HALT         : 01110110
 * PUSH/POP     : 11x10101 / 11x10001
 * EX DE, HL    : 11101011
 * LD SP, HL    : 11111001
 */

#define Z80_LD_A_N    0x3E
#define Z80_LD_B_N    0x06
#define Z80_LD_C_N    0x0E
#define Z80_LD_D_N    0x16
#define Z80_LD_E_N    0x1E
#define Z80_LD_H_N    0x26
#define Z80_LD_L_N    0x2E
#define Z80_LD_A_NN   0x3A
#define Z80_LD_NN_A   0x32
#define Z80_LD_HL_NN  0x2A
#define Z80_LD_NN_HL  0x22
#define Z80_ADD_A_B   0x80
#define Z80_ADD_A_C   0x81
#define Z80_ADD_A_D   0x82
#define Z80_ADD_A_E   0x83
#define Z80_ADD_A_H   0x84
#define Z80_ADD_A_L   0x85
#define Z80_SUB_B     0x90
#define Z80_SUB_C     0x91
#define Z80_SUB_D     0x92
#define Z80_SUB_E     0x93
#define Z80_AND_B     0xA0
#define Z80_OR_B      0xB0
#define Z80_XOR_B     0xA8
#define Z80_CP_B      0xB8
#define Z80_CP_C      0xB9
#define Z80_CP_D      0xBA
#define Z80_CP_E      0xBB
#define Z80_CP_H      0xBC
#define Z80_CP_L      0xBD
#define Z80_CP_A      0xBF
#define Z80_INC_A     0x3C
#define Z80_INC_B     0x04
#define Z80_INC_C     0x0C
#define Z80_INC_D     0x14
#define Z80_INC_E     0x1C
#define Z80_INC_H     0x24
#define Z80_INC_L     0x2C
#define Z80_DEC_A     0x3D
#define Z80_DEC_B     0x05
#define Z80_DEC_C     0x0D
#define Z80_DEC_D     0x15
#define Z80_DEC_E     0x1D
#define Z80_DEC_H     0x25
#define Z80_DEC_L     0x2D
#define Z80_JP_NN     0xC3
#define Z80_JP_NZ_NN  0xC2
#define Z80_JP_Z_NN   0xCA
#define Z80_JP_NC_NN  0xD2
#define Z80_JP_C_NN   0xDA
#define Z80_JP_P_NN   0xF2
#define Z80_JP_M_NN   0xFA
#define Z80_JR_D      0x18
#define Z80_JR_NZ_D   0x20
#define Z80_JR_Z_D    0x28
#define Z80_JR_NC_D   0x30
#define Z80_JR_C_D    0x38
#define Z80_CALL_NN   0xCD
#define Z80_RET       0xC9
#define Z80_NOP       0x00
#define Z80_HALT      0x76
#define Z80_EX_DE_HL  0xEB
#define Z80_LD_SP_HL  0xF9
#define Z80_ADD_HL_BC 0x09
#define Z80_ADD_HL_DE 0x19
#define Z80_ADD_HL_HL 0x29
#define Z80_ADD_HL_SP 0x39

/* the MUL/DIV/SHL/SHR/NEG helpers (all verified against objdump) */
#define Z80_XOR_A     0xAF   /* A = 0 */
#define Z80_ADD_A_A   0x87   /* A += A (doubles) */
#define Z80_LD_C_A    0x4F
#define Z80_LD_D_A    0x57
#define Z80_LD_C_N    0x0E
#define Z80_INC_D     0x14
#define Z80_OR_A      0xB7   /* OR A, A (sets flags from A) */
#define Z80_CB_PFX    0xCB
#define Z80_SRL_A     0x3F   /* CB 3F: A >>= 1 (logical) */

/* ---- patch system ---- */
typedef struct {
    size_t pos;
    size_t patch_size;   /* 1 (JR rel8) or 2 (JP abs16) */
    uint32_t label;
} z80_patch_t;

static void note_label(z80_emitter_t *e, uint32_t label, size_t off)
{
    if (label >= e->n_labels) {
        size_t old = e->n_labels;
        e->n_labels = label + 1;
        e->label_offsets = realloc(e->label_offsets, e->n_labels * sizeof(size_t));
        for (size_t i = old; i < e->n_labels; i++) e->label_offsets[i] = (size_t)-1;
    }
    e->label_offsets[label] = off;
}
static size_t label_off(const z80_emitter_t *e, uint32_t label)
{
    return (label < e->n_labels) ? e->label_offsets[label] : (size_t)-1;
}

static void patch_push(z80_emitter_t *e, z80_patch_t **patches,
                       size_t *np, size_t *cap, size_t pos,
                       size_t psize, uint32_t label)
{
    if (*np == *cap) {
        *cap = *cap ? *cap * 2 : 16;
        *patches = realloc(*patches, *cap * sizeof(z80_patch_t));
    }
    (*patches)[*np].pos = pos;
    (*patches)[*np].patch_size = psize;
    (*patches)[*np].label = label;
    (*np)++;
}

/* ---- 8-bit ALU via A (load A from slot, op with B, store A to slot) ---- */
static void alu8(z80_emitter_t *e, uint8_t alu_op, uint16_t va, uint16_t vdst)
{
    /* LD A, (va) */
    e8(e, Z80_LD_A_NN);
    e16(e, va);
    /* ALU op with B (value loaded into B) */
    e8(e, Z80_LD_B_N);
    e8(e, 0x00); /* placeholder — patched below */
    /* LD (va), A first, then LD B, (vb) — no, do it properly:
     * We need: LD A, (va); LD B, (vb); ALU; LD (vdst), A */
}

/* Simplified: use memory-to-accumulator pattern with explicit B load */
static void load_b_from_slot(z80_emitter_t *e, uint16_t va)
{
    /* LD B, (HL) requires HL = va. Instead: LD HL, (nn); LD B, (HL) is
     * indirect. Simplest: LD A, (va); LD B, A */
    e8(e, Z80_LD_A_NN);
    e16(e, va);
    e8(e, 0x47); /* LD B, A */
}

static void store_a_to_slot(z80_emitter_t *e, uint16_t vd)
{
    e8(e, Z80_LD_NN_A);
    e16(e, vd);
}

/* ---- comparison: computes a <op> b into dst (0/1) ---- */
/* The Z80 CP B sets: C flag on borrow (a < b), Z flag on equality.
 * Layout: the conditional jump(s) lead to set1 (true) or fall
 * through to set0 (false). set0 stores 0 then JP done; set1 stores 1.
 * GT is the inverted case: equal/less jump to set0, the fall-through
 * IS set1 (a > b). */
static void emit_compare(z80_emitter_t *e, int mir_op, uint16_t va,
                         uint16_t vb, uint16_t vdst,
                         z80_patch_t **patches, size_t *np, size_t *cap)
{
    uint32_t set0 = internal_label(e);
    uint32_t set1 = internal_label(e);
    uint32_t done = internal_label(e);

    /* LD A, (vb); LD B, A; LD A, (va); CP B
     * -> A = va, B = vb; CP tests A-B = va-vb (sets C on va<vb,
     * Z on va==vb). NOTE: the operand order matters — A must be va. */
    e8(e, Z80_LD_A_NN);
    e16(e, vb);
    e8(e, 0x47); /* LD B, A — B = vb */
    e8(e, Z80_LD_A_NN);
    e16(e, va);
    e8(e, Z80_CP_B);

    /* Per-op: the conditional jumps + the two stores, each complete.
     * set0 stores 0, set1 stores 1; both then JP done. */
    switch (mir_op) {
    case MIR_EQ: /* JP Z set1; set0; JP done; set1 */
        e8(e, Z80_JP_Z_NN);
        e16(e, 0x0000);
        patch_push(e, patches, np, cap, e->n - 2, 2, set1);
        note_label(e, set0, e->n);
        e8(e, Z80_LD_A_N); e8(e, 0x00);
        store_a_to_slot(e, vdst);
        e8(e, Z80_JP_NN);
        e16(e, 0x0000);
        patch_push(e, patches, np, cap, e->n - 2, 2, done);
        note_label(e, set1, e->n);
        e8(e, Z80_LD_A_N); e8(e, 0x01);
        store_a_to_slot(e, vdst);
        break;
    case MIR_NE: /* JP NZ set1; set0; JP done; set1 */
        e8(e, Z80_JP_NZ_NN);
        e16(e, 0x0000);
        patch_push(e, patches, np, cap, e->n - 2, 2, set1);
        note_label(e, set0, e->n);
        e8(e, Z80_LD_A_N); e8(e, 0x00);
        store_a_to_slot(e, vdst);
        e8(e, Z80_JP_NN);
        e16(e, 0x0000);
        patch_push(e, patches, np, cap, e->n - 2, 2, done);
        note_label(e, set1, e->n);
        e8(e, Z80_LD_A_N); e8(e, 0x01);
        store_a_to_slot(e, vdst);
        break;
    case MIR_LT: /* JP C set1; set0; JP done; set1 */
        e8(e, Z80_JP_C_NN);
        e16(e, 0x0000);
        patch_push(e, patches, np, cap, e->n - 2, 2, set1);
        note_label(e, set0, e->n);
        e8(e, Z80_LD_A_N); e8(e, 0x00);
        store_a_to_slot(e, vdst);
        e8(e, Z80_JP_NN);
        e16(e, 0x0000);
        patch_push(e, patches, np, cap, e->n - 2, 2, done);
        note_label(e, set1, e->n);
        e8(e, Z80_LD_A_N); e8(e, 0x01);
        store_a_to_slot(e, vdst);
        break;
    case MIR_LE: /* JP C set1; JP Z set1; set0; JP done; set1 */
        e8(e, Z80_JP_C_NN);
        e16(e, 0x0000);
        patch_push(e, patches, np, cap, e->n - 2, 2, set1);
        e8(e, Z80_JP_Z_NN);
        e16(e, 0x0000);
        patch_push(e, patches, np, cap, e->n - 2, 2, set1);
        note_label(e, set0, e->n);
        e8(e, Z80_LD_A_N); e8(e, 0x00);
        store_a_to_slot(e, vdst);
        e8(e, Z80_JP_NN);
        e16(e, 0x0000);
        patch_push(e, patches, np, cap, e->n - 2, 2, done);
        note_label(e, set1, e->n);
        e8(e, Z80_LD_A_N); e8(e, 0x01);
        store_a_to_slot(e, vdst);
        break;
    case MIR_GT: /* JP Z set0; JP C set0; set1; JP done; set0 */
        e8(e, Z80_JP_Z_NN);
        e16(e, 0x0000);
        patch_push(e, patches, np, cap, e->n - 2, 2, set0);
        e8(e, Z80_JP_C_NN);
        e16(e, 0x0000);
        patch_push(e, patches, np, cap, e->n - 2, 2, set0);
        note_label(e, set1, e->n);
        e8(e, Z80_LD_A_N); e8(e, 0x01);
        store_a_to_slot(e, vdst);
        e8(e, Z80_JP_NN);
        e16(e, 0x0000);
        patch_push(e, patches, np, cap, e->n - 2, 2, done);
        note_label(e, set0, e->n);
        e8(e, Z80_LD_A_N); e8(e, 0x00);
        store_a_to_slot(e, vdst);
        break;
    case MIR_GE: /* JP NC set1; set0; JP done; set1 */
        e8(e, Z80_JP_NC_NN);
        e16(e, 0x0000);
        patch_push(e, patches, np, cap, e->n - 2, 2, set1);
        note_label(e, set0, e->n);
        e8(e, Z80_LD_A_N); e8(e, 0x00);
        store_a_to_slot(e, vdst);
        e8(e, Z80_JP_NN);
        e16(e, 0x0000);
        patch_push(e, patches, np, cap, e->n - 2, 2, done);
        note_label(e, set1, e->n);
        e8(e, Z80_LD_A_N); e8(e, 0x01);
        store_a_to_slot(e, vdst);
        break;
    default: /* JP Z set1; set0; JP done; set1 */
        e8(e, Z80_JP_Z_NN);
        e16(e, 0x0000);
        patch_push(e, patches, np, cap, e->n - 2, 2, set1);
        note_label(e, set0, e->n);
        e8(e, Z80_LD_A_N); e8(e, 0x00);
        store_a_to_slot(e, vdst);
        e8(e, Z80_JP_NN);
        e16(e, 0x0000);
        patch_push(e, patches, np, cap, e->n - 2, 2, done);
        note_label(e, set1, e->n);
        e8(e, Z80_LD_A_N); e8(e, 0x01);
        store_a_to_slot(e, vdst);
        break;
    }

    /* done: */
    note_label(e, done, e->n);
}

static int z80_compile(const wubu_mir_prog_t *p, uint8_t **out, size_t *out_size)
{
    size_t max_vr = 0;
    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (in->dst > max_vr) max_vr = in->dst;
        if (in->a > max_vr) max_vr = in->a;
        if (in->b > max_vr) max_vr = in->b;
    }

    z80_emitter_t e;
    memset(&e, 0, sizeof(e));
    e.frame = (max_vr + 1) * 2;
    e.n_labels = p->n_labels;
    e.label_offsets = calloc(e.n_labels, sizeof(size_t));
    for (size_t i = 0; i < e.n_labels; i++) e.label_offsets[i] = (size_t)-1;

    z80_patch_t *patches = NULL;
    size_t np = 0, cp = 0;

    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (in->op == MIR_LABEL) {
            note_label(&e, in->label, e.n);
            continue;
        }
        uint16_t va = slot_addr(in->a);
        uint16_t vb = slot_addr(in->b);
        uint16_t vd = slot_addr(in->dst);

        switch (in->op) {
        case MIR_CONST:
            /* LD A, imm; LD (vd), A */
            e8(&e, Z80_LD_A_N);
            e8(&e, (uint8_t)(in->imm & 0xFF));
            store_a_to_slot(&e, vd);
            break;

        case MIR_MOV:
            e8(&e, Z80_LD_A_NN);
            e16(&e, va);
            store_a_to_slot(&e, vd);
            break;

        case MIR_ADD:
            /* LD A, (va); LD B, A; LD A, (vb); ADD A, B; LD (vd), A */
            e8(&e, Z80_LD_A_NN);
            e16(&e, va);
            e8(&e, 0x47); /* LD B, A */
            e8(&e, Z80_LD_A_NN);
            e16(&e, vb);
            e8(&e, Z80_ADD_A_B);
            store_a_to_slot(&e, vd);
            break;

        case MIR_SUB:
            /* A = va - vb: LD A,(vb); LD B,A; LD A,(va); SUB B.
             * The operand order matters (the first load must be the
             * SUBTRAHEND into B, the second the minuend into A). */
            e8(&e, Z80_LD_A_NN);
            e16(&e, vb);
            e8(&e, 0x47); /* LD B, A */
            e8(&e, Z80_LD_A_NN);
            e16(&e, va);
            e8(&e, Z80_SUB_B);
            store_a_to_slot(&e, vd);
            break;

        case MIR_AND:
            e8(&e, Z80_LD_A_NN);
            e16(&e, va);
            e8(&e, 0x47);
            e8(&e, Z80_LD_A_NN);
            e16(&e, vb);
            e8(&e, Z80_AND_B);
            store_a_to_slot(&e, vd);
            break;

        case MIR_OR:
            e8(&e, Z80_LD_A_NN);
            e16(&e, va);
            e8(&e, 0x47);
            e8(&e, Z80_LD_A_NN);
            e16(&e, vb);
            e8(&e, Z80_OR_B);
            store_a_to_slot(&e, vd);
            break;

        case MIR_XOR:
            e8(&e, Z80_LD_A_NN);
            e16(&e, va);
            e8(&e, 0x47);
            e8(&e, Z80_LD_A_NN);
            e16(&e, vb);
            e8(&e, Z80_XOR_B);
            store_a_to_slot(&e, vd);
            break;

        case MIR_SHL: {
            /* vd = va << vb: C = va, B = vb (count), A = 0
             * loop: ADD A,A (double); DEC B; JR NZ loop */
            uint32_t loop = internal_label(&e);
            e8(&e, Z80_LD_A_NN); e16(&e, va);
            e8(&e, Z80_LD_C_A);            /* C = va */
            e8(&e, Z80_LD_A_NN); e16(&e, vb);
            e8(&e, 0x47);                  /* LD B, A — the count */
            e8(&e, 0x79);                  /* LD A, C — the value */
            note_label(&e, loop, e.n);
            e8(&e, Z80_ADD_A_A);           /* A += A (double) */
            e8(&e, Z80_DEC_B);
            size_t jr = e.n;
            e8(&e, Z80_JR_NZ_D);
            size_t jr_disp = e.n;
            e8(&e, 0x00);
            patch_push(&e, &patches, &np, &cp, jr_disp, 1, loop);
            store_a_to_slot(&e, vd);
            break;
        }
        case MIR_SHR: {
            /* vd = va >> vb: C = va, B = vb, A = 0
             * loop: SRL A is not enough — the value must be shifted.
             * Use: LD A, (vd); SRL A (CB 3F); LD (vd), A; DEC B; JR NZ */
            uint32_t loop = internal_label(&e);
            /* vd = va */
            e8(&e, Z80_LD_A_NN); e16(&e, va);
            store_a_to_slot(&e, vd);
            /* B = vb */
            e8(&e, Z80_LD_A_NN); e16(&e, vb);
            e8(&e, 0x47);                  /* LD B, A — the count */
            note_label(&e, loop, e.n);
            e8(&e, Z80_LD_A_NN); e16(&e, vd);
            e8(&e, Z80_CB_PFX); e8(&e, Z80_SRL_A);   /* A >>= 1 */
            store_a_to_slot(&e, vd);
            e8(&e, Z80_DEC_B);
            size_t jr2 = e.n;
            e8(&e, Z80_JR_NZ_D);
            size_t jr2_disp = e.n;
            e8(&e, 0x00);
            patch_push(&e, &patches, &np, &cp, jr2_disp, 1, loop);
            break;
        }
        case MIR_MUL: {
            /* vd = va * vb (8-bit, shift-add): C = va, B = vb,
             * A = 0; loop: ADD A,C; DEC B; JR NZ */
            uint32_t loop = internal_label(&e);
            e8(&e, Z80_LD_A_NN); e16(&e, va);
            e8(&e, Z80_LD_C_A);            /* C = va */
            e8(&e, Z80_LD_A_NN); e16(&e, vb);
            e8(&e, 0x47);                  /* LD B, A — the multiplier */
            e8(&e, Z80_XOR_A);             /* A = 0 (the product) */
            note_label(&e, loop, e.n);
            e8(&e, Z80_ADD_A_C);           /* A += C */
            e8(&e, Z80_DEC_B);
            size_t jr3 = e.n;
            e8(&e, Z80_JR_NZ_D);
            size_t jr3_disp = e.n;
            e8(&e, 0x00);
            patch_push(&e, &patches, &np, &cp, jr3_disp, 1, loop);
            store_a_to_slot(&e, vd);
            break;
        }
        case MIR_DIV: case MIR_MOD: {
            /* 8-bit SIGNED division: C = |dividend|, B = |divisor|,
             * D = quotient, E = dividend sign (0/1), H = divisor
             * sign (0/1). Negate the operands first (via JP P), run
             * the unsigned subtract loop, then re-apply the sign:
             *   DIV: result negated iff E XOR H
             *   MOD: result negated iff E (the dividend's sign) */
            uint32_t loop = internal_label(&e);
            uint32_t done = internal_label(&e);
            uint32_t pos1 = internal_label(&e);
            uint32_t pos2 = internal_label(&e);
            uint32_t pos3 = internal_label(&e);

            /* C = va; E = 0 */
            e8(&e, Z80_LD_A_NN); e16(&e, va);
            e8(&e, Z80_LD_C_A);
            e8(&e, Z80_LD_E_N); e8(&e, 0x00);
            /* test the dividend sign: LD A,C; OR A; JP P, pos1 */
            e8(&e, 0x79);                  /* LD A, C */
            e8(&e, Z80_OR_A);
            e8(&e, Z80_JP_P_NN);
            e16(&e, 0x0000);
            patch_push(&e, &patches, &np, &cp, e.n - 2, 2, pos1);
            /* negative: C = 0 - C; E = 1 */
            e8(&e, 0x79);                  /* LD A, C */
            e8(&e, 0x47);                  /* LD B, A */
            e8(&e, Z80_XOR_A);
            e8(&e, Z80_SUB_B);             /* A = 0 - C */
            e8(&e, Z80_LD_C_A);
            e8(&e, Z80_LD_E_N); e8(&e, 0x01);
            note_label(&e, pos1, e.n);

            /* B = vb; test the divisor sign */
            e8(&e, Z80_LD_A_NN); e16(&e, vb);
            e8(&e, 0x47);                  /* LD B, A */
            e8(&e, Z80_OR_A);              /* OR A sets flags from B's copy */
            e8(&e, Z80_JP_P_NN);
            e16(&e, 0x0000);
            patch_push(&e, &patches, &np, &cp, e.n - 2, 2, pos2);
            /* negative divisor: B = 0 - B; H = 1 */
            e8(&e, Z80_LD_A_N); e8(&e, 0x00);
            e8(&e, Z80_SUB_B);             /* A = 0 - B */
            e8(&e, 0x47);                  /* LD B, A */
            e8(&e, Z80_LD_H_N); e8(&e, 0x01);
            e8(&e, Z80_JP_NN);
            e16(&e, 0x0000);
            patch_push(&e, &patches, &np, &cp, e.n - 2, 2, pos2);
            /* positive divisor: H = 0 */
            note_label(&e, pos2, e.n);
            e8(&e, Z80_LD_H_N); e8(&e, 0x00);

            /* the unsigned subtract loop: D = quotient */
            e8(&e, Z80_XOR_A);
            e8(&e, Z80_LD_D_A);
            note_label(&e, loop, e.n);
            e8(&e, 0x79);                  /* LD A, C */
            e8(&e, Z80_CP_B);              /* A - B: C flag = borrow */
            size_t jrc = e.n;
            e8(&e, Z80_JR_C_D);            /* if A < B: done */
            size_t jrc_disp = e.n;
            e8(&e, 0x00);
            e8(&e, 0x79);                  /* LD A, C */
            e8(&e, Z80_SUB_B);             /* A = C - B */
            e8(&e, Z80_LD_C_A);            /* C = new dividend */
            e8(&e, Z80_INC_D);
            size_t jr4 = e.n;
            e8(&e, Z80_JR_D);
            size_t jr4_disp = e.n;
            e8(&e, 0x00);
            patch_push(&e, &patches, &np, &cp, jr4_disp, 1, loop);

            /* done: pick the result, then apply the sign */
            note_label(&e, done, e.n);
            if (in->op == MIR_MOD) {
                /* MOD: the remainder is in C; negate iff E == 1.
                 *   LD A,E; OR A; JP Z mod_pos
                 *   (E==1) A = 0 - C; JP mod_done
                 *   mod_pos: A = C
                 *   mod_done: store */
                uint32_t mod_pos = internal_label(&e);
                uint32_t mod_done = internal_label(&e);
                e8(&e, 0x7B);              /* LD A, E */
                e8(&e, Z80_OR_A);
                e8(&e, Z80_JP_Z_NN);
                e16(&e, 0x0000);
                patch_push(&e, &patches, &np, &cp, e.n - 2, 2, mod_pos);
                e8(&e, 0x79);              /* LD A, C */
                e8(&e, 0x47);              /* LD B, A */
                e8(&e, Z80_XOR_A);
                e8(&e, Z80_SUB_B);         /* A = 0 - C */
                e8(&e, Z80_JP_NN);
                e16(&e, 0x0000);
                patch_push(&e, &patches, &np, &cp, e.n - 2, 2, mod_done);
                note_label(&e, mod_pos, e.n);
                e8(&e, 0x79);              /* LD A, C */
                note_label(&e, mod_done, e.n);
                store_a_to_slot(&e, vd);
            } else {
                /* DIV: the quotient is in D; negate iff E XOR H. */
                uint32_t div_pos = internal_label(&e);
                uint32_t div_done = internal_label(&e);
                e8(&e, 0x7B);              /* LD A, E */
                e8(&e, 0xAC);              /* XOR H -> A = E^H */
                e8(&e, Z80_OR_A);
                e8(&e, Z80_JP_Z_NN);
                e16(&e, 0x0000);
                patch_push(&e, &patches, &np, &cp, e.n - 2, 2, div_pos);
                e8(&e, 0x7A);              /* LD A, D */
                e8(&e, 0x47);              /* LD B, A */
                e8(&e, Z80_XOR_A);
                e8(&e, Z80_SUB_B);         /* A = 0 - D */
                e8(&e, Z80_JP_NN);
                e16(&e, 0x0000);
                patch_push(&e, &patches, &np, &cp, e.n - 2, 2, div_done);
                note_label(&e, div_pos, e.n);
                e8(&e, 0x7A);              /* LD A, D */
                note_label(&e, div_done, e.n);
                store_a_to_slot(&e, vd);
            }
            /* patch the JR C done target directly */
            if (jrc_disp > 0 && e.label_offsets[done] != (size_t)-1) {
                int32_t rel = (int32_t)(e.label_offsets[done] - (jrc_disp + 1));
                if (rel < -128) rel = -128;
                if (rel > 127) rel = 127;
                e.code[jrc_disp] = (uint8_t)(rel & 0xFF);
            }
            break;
        }
        case MIR_NEG:
            /* A = 0 - a: LD A,(va); LD B,A; XOR A; SUB B */
            e8(&e, Z80_LD_A_NN);
            e16(&e, va);
            e8(&e, 0x47); /* LD B, A */
            e8(&e, Z80_XOR_A); /* A = 0 */
            e8(&e, Z80_SUB_B); /* A = 0 - a */
            store_a_to_slot(&e, vd);
            break;

        case MIR_NOT:
            /* A = ~a = a ^ 0xFF */
            e8(&e, Z80_LD_A_NN);
            e16(&e, va);
            e8(&e, 0xEE); /* XOR A, 0xFF (immediate) — 0xEE is XOR n */
            e8(&e, 0xFF);
            store_a_to_slot(&e, vd);
            break;

        case MIR_EQ: case MIR_NE: case MIR_LT: case MIR_LE:
        case MIR_GT: case MIR_GE:
            emit_compare(&e, in->op, va, vb, vd, &patches, &np, &cp);
            break;

        case MIR_JMP:
            e8(&e, Z80_JP_NN);
            size_t jp = e.n;
            e16(&e, 0x0000);
            patch_push(&e, &patches, &np, &cp, jp, 2, in->label);
            break;

        case MIR_JZ:
            /* LD A, (va); OR A (sets Z); JR Z, label */
            e8(&e, Z80_LD_A_NN);
            e16(&e, va);
            e8(&e, 0xB7); /* OR A (0xB7) sets flags from A */
            size_t jr = e.n;
            e8(&e, Z80_JR_Z_D);
            size_t jr_disp = e.n;
            e8(&e, 0x00);
            patch_push(&e, &patches, &np, &cp, jr_disp, 1, in->label);
            break;

        case MIR_RET:
            /* LD A, (va); HALT (the interpreter reads A) */
            e8(&e, Z80_LD_A_NN);
            e16(&e, va);
            e8(&e, Z80_HALT);
            break;

        default:
            break;
        }
    }

    if (e.n == 0 || e.code[e.n - 1] != Z80_HALT) {
        e8(&e, Z80_HALT);
    }

    /* patch pass */
    for (size_t i = 0; i < np; i++) {
        size_t t = label_off(&e, patches[i].label);
        if (t == (size_t)-1) continue;
        if (patches[i].patch_size == 1) {
            /* JR rel8: displacement from the JR's second byte */
            int32_t rel = (int32_t)(t - (patches[i].pos + 1));
            if (rel < -128) rel = -128;
            if (rel > 127) rel = 127;
            e.code[patches[i].pos] = (uint8_t)(rel & 0xFF);
        } else {
            /* JP abs16 */
            e.code[patches[i].pos] = (uint8_t)(t & 0xFF);
            e.code[patches[i].pos + 1] = (uint8_t)((t >> 8) & 0xFF);
        }
    }

    free(patches);
    free(e.label_offsets);
    *out = e.code;
    *out_size = e.n;
    return 0;
}

int64_t wubu_z80_run(const uint8_t *code, size_t size, int64_t arg);

static int64_t z80_run(const uint8_t *code, size_t size, int64_t arg)
{
    return wubu_z80_run(code, size, arg);
}

static void z80_describe(void)
{
    printf("Zilog Z80 driver (1976 8-bit): A/B/C/D/E/H/L + F, dual reg banks, "
           "IX/IY, CP/M + ZX Spectrum heritage. Encodings verified against "
           "GNU objdump; runs via the bundled Z80 interpreter.\n");
}

const wubu_isa_driver_t wubu_isa_z80 = {
    .name = "z80",
    .family = "portable",
    .exec = WUBU_ISA_INTERPRETED,
    .compile = z80_compile,
    .run = z80_run,
    .describe = z80_describe,
};