/*
 * wubu_isa_arm64.c -- the ARM64 (AArch64) ISA driver.
 *
 * Hop 5 of the ISA ladder (1985→2011): Acorn's Berkeley-RISC-inspired
 * design, now 230B+ chips. 31×64-bit GPRs, fixed-width 32-bit instrs.
 *
 * Strategy: the MIR register allocator assigns each virtual register to
 * an ARM64 register (X8-X27, 20 available) or a stack slot. Register-
 * resident vrs live in their assigned ARM64 register for their entire
 * lifetime. Spilled vrs use [SP - offset]. X0 = return, X9-X10 = scratch.
 *
 * C11, self-contained.
 */
#include "wubu_isa_driver.h"
#include "wubu_arm64.h"
#include "wubu_mir_regalloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <stdint.h>

/* ---- emitter ---- */
typedef struct {
    WArm64Enc enc;
    size_t frame;
    size_t *label_offsets;
    size_t n_labels;
    size_t internal_seq;
} arm64_emitter_t;

/* Map allocator physical register index to ARM64 register.
 * Physical 0 → X8, 1 → X9, ..., 19 → X27.
 * We skip X0-X7 (return/args/scratch) and X28-X31 (special). */
static WArm64Reg phys_to_arm64(int phys) {
    if (phys >= 0 && phys <= 19) return (WArm64Reg)(8 + phys);
    return WREG_X8; /* fallback */
}

static int32_t spill_off(int slot) {
    return -(int32_t)((slot + 1) * 8);
}

/* ---- patch system ---- */
typedef struct { size_t pos; uint32_t label; } arm64_patch_t;

static void note_label(arm64_emitter_t *e, uint32_t label, size_t off) {
    if (label >= e->n_labels) {
        size_t old = e->n_labels;
        e->n_labels = label + 1;
        e->label_offsets = realloc(e->label_offsets, e->n_labels * sizeof(size_t));
        for (size_t i = old; i < e->n_labels; i++) e->label_offsets[i] = (size_t)-1;
    }
    e->label_offsets[label] = off;
}
static size_t label_off(const arm64_emitter_t *e, uint32_t label) {
    return (label < e->n_labels) ? e->label_offsets[label] : (size_t)-1;
}
static uint32_t internal_label(arm64_emitter_t *e) {
    return (uint32_t)(e->n_labels + e->internal_seq++);
}
static void patch_push(arm64_patch_t **patches, size_t *np, size_t *cap,
                       size_t pos, uint32_t label) {
    if (*np == *cap) { *cap = *cap ? *cap * 2 : 16; *patches = realloc(*patches, *cap * sizeof(arm64_patch_t)); }
    (*patches)[*np].pos = pos;
    (*patches)[*np].label = label;
    (*np)++;
}
static WArm64CC mir_to_arm64_cc(wubu_mir_op_t op) {
    switch (op) {
    case MIR_EQ: return W64CC_EQ;
    case MIR_NE: return W64CC_NE;
    case MIR_LT: return W64CC_LT;
    case MIR_LE: return W64CC_LE;
    case MIR_GT: return W64CC_GT;
    case MIR_GE: return W64CC_GE;
    default: return W64CC_AL;
    }
}

static int arm64_compile(const wubu_mir_prog_t *p, uint8_t **out, size_t *out_size) {
    /* Step 1: register allocation — 20 physical regs (X8-X27) */
    size_t assign_count = 0;
    wubu_reg_assign_t *assign = wubu_mir_alloc_regs(p, 20, &assign_count);
    if (!assign) return -1;

    /* Count spilled vrs */
    size_t n_spilled = 0;
    for (size_t i = 0; i < assign_count; i++) {
        if (assign[i].reg < 0) {
            int slot = -assign[i].reg - 1;
            if ((size_t)slot >= n_spilled) n_spilled = slot + 1;
        }
    }

    arm64_emitter_t e;
    memset(&e, 0, sizeof(e));
    warm64_enc_init_dynamic(&e.enc, 512);
    e.frame = n_spilled * 8 + 64;
    e.n_labels = p->n_labels;
    e.label_offsets = calloc(e.n_labels, sizeof(size_t));
    for (size_t i = 0; i < e.n_labels; i++) e.label_offsets[i] = (size_t)-1;

    arm64_patch_t *patches = NULL;
    size_t np = 0, cp = 0;

    /* prologue */
    warm64_stp_pre(&e.enc, WREG_X29, WREG_X30, WREG_SP, -2);
    warm64_mov_reg(&e.enc, WREG_X29, WREG_SP);
    uint32_t frame_imm = (uint32_t)(e.frame);
    uint16_t lo = frame_imm & 0xFFF;
    warm64_sub_imm(&e.enc, WREG_SP, WREG_SP, lo, 1);
    if (frame_imm > 0xFFF) {
        uint16_t hi = (frame_imm >> 12) & 0xFFF;
        warm64_sub_imm(&e.enc, WREG_SP, WREG_SP, hi, 1);
    }

    /* Helper macros */
    #define VR_REG(vr) ((vr) < (wubu_vr_t)assign_count && assign[(vr)].reg >= 0 ? phys_to_arm64(assign[(vr)].reg) : WREG_XZR)
    #define VR_SPILL(vr) ((vr) < (wubu_vr_t)assign_count && assign[(vr)].reg < 0 ? spill_off(-assign[(vr)].reg - 1) : 0)
    #define VR_HAS_REG(vr) ((vr) < (wubu_vr_t)assign_count && assign[(vr)].reg >= 0)

    #define LOAD_VR(vr, dst) do { \
        if (VR_HAS_REG(vr)) { \
            WArm64Reg __src = VR_REG(vr); \
            if ((dst) != __src) warm64_mov_reg(&e.enc, dst, __src); \
        } else { \
            int32_t __off = VR_SPILL(vr); \
            warm64_ldr_imm(&e.enc, dst, WREG_SP, (int32_t)(__off / 8), 1); \
        } \
    } while(0)

    #define STORE_VR(src, vr) do { \
        if (VR_HAS_REG(vr)) { \
            WArm64Reg __dst = VR_REG(vr); \
            if ((src) != __dst) warm64_mov_reg(&e.enc, __dst, src); \
        } else { \
            int32_t __off = VR_SPILL(vr); \
            warm64_str_imm(&e.enc, src, WREG_SP, (int32_t)(__off / 8), 1); \
        } \
    } while(0)

    #define NEXT_IS_RET(vr) (i + 1 < p->n && p->ins[i+1].op == MIR_RET && p->ins[i+1].a == (wubu_vr_t)(vr))

    int result_in_x0 = 0;  /* set when last op skipped store to keep result in X0 */

    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (in->op == MIR_LABEL) { note_label(&e, in->label, e.enc.pos); result_in_x0 = 0; continue; }
        if (in->op != MIR_RET) result_in_x0 = 0;

        switch (in->op) {
        case MIR_CONST: {
            int64_t imm = in->imm;
            uint32_t uimm = (uint32_t)(imm & 0xFFFFFFFF);
            uint16_t hw0 = uimm & 0xFFFF;
            warm64_movz_imm(&e.enc, WREG_X0, hw0, 0, 1);
            if (uimm > 0xFFFF) {
                uint16_t hw1 = (uimm >> 16) & 0xFFFF;
                warm64_movz_imm(&e.enc, WREG_X0, hw1, 1, 1);
            }
            if (imm > 0xFFFFFFFFLL || imm < 0) {
                uint16_t hw2 = ((uint64_t)imm >> 32) & 0xFFFF;
                uint16_t hw3 = ((uint64_t)imm >> 48) & 0xFFFF;
                warm64_movz_imm(&e.enc, WREG_X0, hw2, 2, 1);
                warm64_movz_imm(&e.enc, WREG_X0, hw3, 3, 1);
            }
            if (NEXT_IS_RET(in->dst)) { result_in_x0 = 1; }
            else { STORE_VR(WREG_X0, in->dst); result_in_x0=0; }
            break;
        }
        case MIR_MOV:
            LOAD_VR(in->a, WREG_X0);
            if (NEXT_IS_RET(in->dst)) { result_in_x0 = 1; }
            else { STORE_VR(WREG_X0, in->dst); result_in_x0=0; }
            break;
        case MIR_ADD:
            LOAD_VR(in->a, WREG_X9);
            LOAD_VR(in->b, WREG_X10);
            warm64_add_reg(&e.enc, WREG_X0, WREG_X9, WREG_X10, 1);
            if (NEXT_IS_RET(in->dst)) { result_in_x0 = 1; }
            else { STORE_VR(WREG_X0, in->dst); result_in_x0 = 0; }
            break;
        case MIR_SUB:
            LOAD_VR(in->a, WREG_X9);
            LOAD_VR(in->b, WREG_X10);
            warm64_sub_reg(&e.enc, WREG_X0, WREG_X9, WREG_X10, 1);
            if (NEXT_IS_RET(in->dst)) { result_in_x0 = 1; }
            else { STORE_VR(WREG_X0, in->dst); result_in_x0 = 0; }
            break;
        case MIR_MUL:
            LOAD_VR(in->a, WREG_X9);
            LOAD_VR(in->b, WREG_X10);
            warm64_mul_reg(&e.enc, WREG_X0, WREG_X9, WREG_X10);
            if (NEXT_IS_RET(in->dst)) { result_in_x0 = 1; }
            else { STORE_VR(WREG_X0, in->dst); result_in_x0 = 0; }
            break;
        case MIR_AND:
            LOAD_VR(in->a, WREG_X9);
            LOAD_VR(in->b, WREG_X10);
            warm64_and_reg(&e.enc, WREG_X0, WREG_X9, WREG_X10, 1);
            if (NEXT_IS_RET(in->dst)) { result_in_x0 = 1; }
            else { STORE_VR(WREG_X0, in->dst); result_in_x0 = 0; }
            break;
        case MIR_OR:
            LOAD_VR(in->a, WREG_X9);
            LOAD_VR(in->b, WREG_X10);
            warm64_orr_reg(&e.enc, WREG_X0, WREG_X9, WREG_X10, 1);
            if (NEXT_IS_RET(in->dst)) { result_in_x0 = 1; }
            else { STORE_VR(WREG_X0, in->dst); result_in_x0 = 0; }
            break;
        case MIR_XOR:
            LOAD_VR(in->a, WREG_X9);
            LOAD_VR(in->b, WREG_X10);
            warm64_eor_reg(&e.enc, WREG_X0, WREG_X9, WREG_X10, 1);
            if (NEXT_IS_RET(in->dst)) { result_in_x0 = 1; }
            else { STORE_VR(WREG_X0, in->dst); result_in_x0 = 0; }
            break;
        case MIR_NEG:
            LOAD_VR(in->a, WREG_X9);
            warm64_sub_reg(&e.enc, WREG_X0, WREG_XZR, WREG_X9, 1);
            if (NEXT_IS_RET(in->dst)) { result_in_x0 = 1; }
            else { STORE_VR(WREG_X0, in->dst); result_in_x0=0; }
            break;
        case MIR_DIV:
            LOAD_VR(in->a, WREG_X9);
            LOAD_VR(in->b, WREG_X10);
            warm64_sdiv_reg(&e.enc, WREG_X0, WREG_X9, WREG_X10);
            STORE_VR(WREG_X0, in->dst);
            break;
        case MIR_MOD: {
            LOAD_VR(in->a, WREG_X9);
            LOAD_VR(in->b, WREG_X10);
            warm64_sdiv_reg(&e.enc, WREG_X0, WREG_X9, WREG_X10);
            warm64_mul_reg(&e.enc, WREG_X0, WREG_X0, WREG_X10);
            warm64_sub_reg(&e.enc, WREG_X0, WREG_X9, WREG_X0, 1);
            STORE_VR(WREG_X0, in->dst);
            break;
        }
        case MIR_SHL: {
            LOAD_VR(in->a, WREG_X9);
            LOAD_VR(in->b, WREG_X10);
            /* ARM64 LSLV: 0x1AC02000 | (Rm<<16) | (Rn<<5) | Rd */
            /* Emit raw encoding for LSLV X0, X9, X10 */
            uint32_t lslv = 0x1AC02000 | (WREG_X10 << 16) | (WREG_X9 << 5) | WREG_X0;
            warm64_emit_word(&e.enc, lslv);
            STORE_VR(WREG_X0, in->dst);
            break;
        }
        case MIR_SHR: {
            LOAD_VR(in->a, WREG_X9);
            LOAD_VR(in->b, WREG_X10);
            /* ARM64 LSRV: 0x1AC02400 | (Rm<<16) | (Rn<<5) | Rd */
            uint32_t lsrv = 0x1AC02400 | (WREG_X10 << 16) | (WREG_X9 << 5) | WREG_X0;
            warm64_emit_word(&e.enc, lsrv);
            STORE_VR(WREG_X0, in->dst);
            break;
        }
        case MIR_NOT: {
            LOAD_VR(in->a, WREG_X9);
            /* MVN: ORN X0, XZR, X9 */
            uint32_t orn = 0x00000000; /* placeholder */
            (void)orn;
            warm64_eor_reg(&e.enc, WREG_X0, WREG_X9, WREG_XZR, 1);
            /* XOR with -1 to get NOT */
            STORE_VR(WREG_X0, in->dst);
            break;
        }
        case MIR_EQ: case MIR_NE: case MIR_LT: case MIR_LE: case MIR_GT: case MIR_GE: {
            LOAD_VR(in->a, WREG_X9);
            LOAD_VR(in->b, WREG_X10);
            warm64_cmp_reg(&e.enc, WREG_X9, WREG_X10, 1);
            uint32_t set1 = internal_label(&e);
            uint32_t done = internal_label(&e);
            warm64_b_cond(&e.enc, 0, mir_to_arm64_cc(in->op));
            patch_push(&patches, &np, &cp, e.enc.pos - 4, set1);
            warm64_movz_imm(&e.enc, WREG_X0, 0, 0, 1);
            warm64_b_uncond(&e.enc, 0);
            patch_push(&patches, &np, &cp, e.enc.pos - 4, done);
            note_label(&e, set1, e.enc.pos);
            warm64_movz_imm(&e.enc, WREG_X0, 1, 0, 1);
            note_label(&e, done, e.enc.pos);
            STORE_VR(WREG_X0, in->dst);
            break;
        }
        case MIR_JMP:
            warm64_b_uncond(&e.enc, 0);
            patch_push(&patches, &np, &cp, e.enc.pos - 4, in->label);
            break;
        case MIR_JZ: {
            LOAD_VR(in->a, WREG_X0);
            warm64_cmp_imm(&e.enc, WREG_X0, 0, 1);
            warm64_b_cond(&e.enc, 0, W64CC_EQ);
            patch_push(&patches, &np, &cp, e.enc.pos - 4, in->label);
            break;
        }
        case MIR_RET:
            /* If result already in X0 (lookahead skip), don't reload */
            if (!result_in_x0) {
                LOAD_VR(in->a, WREG_X0);
            }
            result_in_x0 = 0;
            warm64_mov_reg(&e.enc, WREG_SP, WREG_X29);
            warm64_ldp_post(&e.enc, WREG_X29, WREG_X30, WREG_SP, 2);
            warm64_ret(&e.enc, WREG_X30);
            break;
        default:
            break;
        }
    }

    /* fallback ret */
    if (e.enc.pos == 0) {
        warm64_movz_imm(&e.enc, WREG_X0, 0, 0, 1);
        warm64_mov_reg(&e.enc, WREG_SP, WREG_X29);
        warm64_ldp_post(&e.enc, WREG_X29, WREG_X30, WREG_SP, 2);
        warm64_ret(&e.enc, WREG_X30);
    }

    /* patch pass */
    for (size_t i = 0; i < np; i++) {
        size_t t = label_off(&e, patches[i].label);
        if (t == (size_t)-1) continue;
        int32_t rel = (int32_t)((ssize_t)t - (ssize_t)patches[i].pos);
        uint32_t inst;
        if (rel >= -1048576 && rel < 1048576) {
            int32_t imm19 = rel / 4;
            inst = 0x54000000 | ((imm19 & 0x7FFFF) << 5);
        } else {
            int32_t imm26 = rel / 4;
            inst = 0x14000000 | (imm26 & 0x3FFFFFF);
        }
        e.enc.buf[patches[i].pos] = (inst >> 0) & 0xFF;
        e.enc.buf[patches[i].pos+1] = (inst >> 8) & 0xFF;
        e.enc.buf[patches[i].pos+2] = (inst >> 16) & 0xFF;
        e.enc.buf[patches[i].pos+3] = (inst >> 24) & 0xFF;
    }

    free(patches);
    free(e.label_offsets);
    wubu_mir_free_alloc(assign);

    *out = e.enc.buf;
    *out_size = e.enc.pos;
    return 0;
}

static int64_t arm64_run(const uint8_t *code, size_t size, int64_t arg) {
    (void)arg;
    void *mem = mmap(NULL, size + 4096, PROT_READ | PROT_WRITE | PROT_EXEC,
                     MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (mem == MAP_FAILED) return -999;
    memcpy(mem, code, size);
    __builtin___clear_cache((char *)mem, (char *)mem + size);
    int64_t (*fn)(void) = (int64_t (*)(void))mem;
    int64_t result = fn();
    munmap(mem, size + 4096);
    return result;
}

static void arm64_describe(void) {
    printf("ARM64 (AArch64) driver (2011 64-bit): 31 GPRs, MIR register "
           "allocator (20 regs), 230B+ chips. Runs via mmap+JIT.\n");
}

const wubu_isa_driver_t wubu_isa_arm64 = {
    .name = "arm64",
    .family = "native",
    .exec = WUBU_ISA_NATIVE,
    .compile = arm64_compile,
    .run = arm64_run,
    .describe = arm64_describe,
};
