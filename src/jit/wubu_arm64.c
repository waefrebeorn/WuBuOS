/*
 * wubu_arm64.c — ARM64/A64 instruction encoder.
 *
 * Encodes ARM64 instructions into a byte buffer for later execution.
 * Follows the A64 instruction set architecture (ARMv8-A).
 *
 * Key encoding formats:
 *   Data-processing (immediate): sf|op|S|100010|shift|imm12|Rn|Rd
 *   Data-processing (register):  sf|0|0|0|1011|shift|0|Rm|imm6|Rn|Rd (add/sub)
 *   Load/Store:                  size|V|0|0|0|0|0|opc|imm12|Rn|Rt
 *   Branch (unconditional):      000101|imm26
 *   Branch (conditional):        01010100|imm19|0|cond
 *   Move wide:                   sf|10|100101|hw|imm16|Rd
 */
#include "wubu_arm64.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -- Buffer management -------------------------------------------- */

static void ensure_cap(WArm64Enc *e, size_t need) {
    if (e->pos + need <= e->cap) return;
    if (!e->owns_buf) return;
    size_t newcap = e->cap ? e->cap * 2 : 256;
    while (newcap < e->pos + need) newcap *= 2;
    uint8_t *nb = (uint8_t *)realloc(e->buf, newcap);
    if (nb) { e->buf = nb; e->cap = newcap; }
}

void warm64_enc_init(WArm64Enc *e, uint8_t *buf, size_t cap) {
    e->buf = buf; e->pos = 0; e->cap = cap; e->owns_buf = 0;
}

void warm64_enc_init_dynamic(WArm64Enc *e, size_t initial_cap) {
    if (initial_cap == 0) initial_cap = 256;
    e->buf = (uint8_t *)malloc(initial_cap);
    e->cap = e->buf ? initial_cap : 0;
    e->pos = 0;
    e->owns_buf = 1;
}

void warm64_enc_free(WArm64Enc *e) {
    if (e && e->owns_buf && e->buf) { free(e->buf); e->buf = NULL; }
}

void warm64_emit_byte(WArm64Enc *e, uint8_t b) {
    ensure_cap(e, 1);
    if (e->pos < e->cap) e->buf[e->pos++] = b;
}

void warm64_emit_word(WArm64Enc *e, uint32_t w) {
    ensure_cap(e, 4);
    if (e->pos + 4 <= e->cap) {
        /* ARM64 is little-endian */
        e->buf[e->pos++] = (uint8_t)(w & 0xFF);
        e->buf[e->pos++] = (uint8_t)((w >> 8) & 0xFF);
        e->buf[e->pos++] = (uint8_t)((w >> 16) & 0xFF);
        e->buf[e->pos++] = (uint8_t)((w >> 24) & 0xFF);
    }
}

void warm64_emit_dword(WArm64Enc *e, uint32_t d) { warm64_emit_word(e, d); }

void warm64_emit_qword(WArm64Enc *e, uint64_t q) {
    ensure_cap(e, 8);
    if (e->pos + 8 <= e->cap) {
        for (int i = 0; i < 8; i++)
            e->buf[e->pos++] = (uint8_t)((q >> (i*8)) & 0xFF);
    }
}

size_t warm64_enc_pos(WArm64Enc *e) { return e->pos; }

/* -- Data processing (immediate) ----------------------------------- */
/* ADD (immediate): sf|0|0|100010|0|imm12|Rn|Rd */
void warm64_add_imm(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, uint16_t imm12, int sf) {
    uint32_t ins = 0x91000000;  /* ADD 64-bit, no shift */
    ins |= (sf & 1) << 31;
    ins |= (imm12 & 0xFFF) << 10;
    ins |= (rn & 0x1F) << 5;
    ins |= (rd & 0x1F);
    warm64_emit_word(e, ins);
}

/* SUB (immediate): sf|1|0|100010|0|imm12|Rn|Rd */
void warm64_sub_imm(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, uint16_t imm12, int sf) {
    uint32_t ins = 0xD1000000;  /* SUB 64-bit, no shift */
    ins |= (sf & 1) << 31;
    ins |= (imm12 & 0xFFF) << 10;
    ins |= (rn & 0x1F) << 5;
    ins |= (rd & 0x1F);
    warm64_emit_word(e, ins);
}

/* -- Data processing (register) ------------------------------------ */
/* ADD (shifted register): sf|0|0|0|1011|00|0|Rm|000000|Rn|Rd */
void warm64_add_reg(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, WArm64Reg rm, int sf) {
    uint32_t ins = 0x8B000000;  /* ADD 64-bit, no shift */
    ins |= (sf & 1) << 31;
    ins |= (rm & 0x1F) << 16;
    ins |= (0 << 10);  /* imm6 = 0 (no shift) */
    ins |= (rn & 0x1F) << 5;
    ins |= (rd & 0x1F);
    warm64_emit_word(e, ins);
}

/* SUB (shifted register): sf|1|0|0|1011|00|0|Rm|000000|Rn|Rd */
void warm64_sub_reg(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, WArm64Reg rm, int sf) {
    uint32_t ins = 0xCB000000;  /* SUB 64-bit, no shift */
    ins |= (sf & 1) << 31;
    ins |= (rm & 0x1F) << 16;
    ins |= (rn & 0x1F) << 5;
    ins |= (rd & 0x1F);
    warm64_emit_word(e, ins);
}

/* MADD: sf|00|11011|000|Rm|0|Rn|Rd  (rd = rn + rm*ra, ra=XZR for mul) */
void warm64_mul_reg(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, WArm64Reg rm) {
    uint32_t ins = 0x9B007C00;  /* MADD 64-bit, Ra=XZR */
    ins |= (rm & 0x1F) << 16;
    ins |= (rn & 0x1F) << 5;
    ins |= (rd & 0x1F);
    warm64_emit_word(e, ins);
}

/* SDIV: sf|00|11010|110|Rm|00001|Rn|Rd */
void warm64_sdiv_reg(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, WArm64Reg rm) {
    uint32_t ins = 0x9AC00C00;  /* SDIV 64-bit */
    ins |= (rm & 0x1F) << 16;
    ins |= (rn & 0x1F) << 5;
    ins |= (rd & 0x1F);
    warm64_emit_word(e, ins);
}

/* -- Move -------------------------------------------------------- */
/* MOV (wide immediate): MOVZ sf|10|100101|hw|imm16|Rd */
void warm64_movz_imm(WArm64Enc *e, WArm64Reg rd, uint16_t imm16, int hw, int sf) {
    uint32_t ins = 0xD2800000;  /* MOVZ 64-bit */
    ins |= (sf & 1) << 31;
    ins |= (hw & 3) << 21;
    ins |= (imm16 & 0xFFFF) << 5;
    ins |= (rd & 0x1F);
    warm64_emit_word(e, ins);
}

/* MOV (wide immediate) — alias for MOVZ with hw=0 */
void warm64_mov_imm(WArm64Enc *e, WArm64Reg rd, uint16_t imm16) {
    warm64_movz_imm(e, rd, imm16, 0, 1);
}

/* MOV (register): ORR shifted register with XZR */
/* ORR: sf|0|1|0|1010|00|0|XZR|000000|Rn|Rd = MOV */
void warm64_mov_reg(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn) {
    uint32_t ins = 0xAA0003E0;  /* ORR Xd, XZR, Xn = MOV */
    ins |= (rn & 0x1F) << 16;
    ins |= (rd & 0x1F);
    warm64_emit_word(e, ins);
}

/* MOV (to/from SP): ADD (immediate) with imm=0, shift=0 */
void warm64_mov_sp(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn) {
    warm64_add_imm(e, rd, rn, 0, 1);
}

/* -- Bitwise (register) ------------------------------------------- */
/* AND (shifted register): sf|0|0|0|1010|00|0|Rm|000000|Rn|Rd */
void warm64_and_reg(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, WArm64Reg rm, int sf) {
    uint32_t ins = 0x8A000000;  /* AND 64-bit */
    ins |= (sf & 1) << 31;
    ins |= (rm & 0x1F) << 16;
    ins |= (rn & 0x1F) << 5;
    ins |= (rd & 0x1F);
    warm64_emit_word(e, ins);
}

/* ORR (shifted register): sf|0|1|0|1010|00|0|Rm|000000|Rn|Rd */
void warm64_orr_reg(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, WArm64Reg rm, int sf) {
    uint32_t ins = 0xAA000000;  /* ORR 64-bit */
    ins |= (sf & 1) << 31;
    ins |= (rm & 0x1F) << 16;
    ins |= (rn & 0x1F) << 5;
    ins |= (rd & 0x1F);
    warm64_emit_word(e, ins);
}

/* EOR (shifted register): sf|1|0|0|1010|00|0|Rm|000000|Rn|Rd */
void warm64_eor_reg(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, WArm64Reg rm, int sf) {
    uint32_t ins = 0xCA000000;  /* EOR 64-bit */
    ins |= (sf & 1) << 31;
    ins |= (rm & 0x1F) << 16;
    ins |= (rn & 0x1F) << 5;
    ins |= (rd & 0x1F);
    warm64_emit_word(e, ins);
}

/* -- Shift -------------------------------------------------------- */
/* LSL (immediate): UBFM sf|10|100110|N|immr|imms|Rn|Rd */
/* For LSL by (64-sf*32 - shift): immr = -shift mod 64, imms = 63-shift (64-bit) */
void warm64_lsl_imm(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, uint8_t shift, int sf) {
    int bits = sf ? 64 : 32;
    int immr = (bits - shift) % bits;
    int imms = bits - 1 - shift;
    uint32_t ins = 0xD3400000;  /* UBFM 64-bit */
    ins |= (sf & 1) << 31;
    ins |= (sf & 1) << 22;  /* N bit */
    ins |= (immr & 0x3F) << 16;
    ins |= (imms & 0x3F) << 10;
    ins |= (rn & 0x1F) << 5;
    ins |= (rd & 0x1F);
    warm64_emit_word(e, ins);
}

/* LSR (immediate): UBFM sf|10|100110|N|immr|imms|Rn|Rd */
void warm64_lsr_imm(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, uint8_t shift, int sf) {
    int bits = sf ? 64 : 32;
    int immr = shift;
    int imms = bits - 1;
    uint32_t ins = 0xD3400000;  /* UBFM 64-bit */
    ins |= (sf & 1) << 31;
    ins |= (sf & 1) << 22;
    ins |= (immr & 0x3F) << 16;
    ins |= (imms & 0x3F) << 10;
    ins |= (rn & 0x1F) << 5;
    ins |= (rd & 0x1F);
    warm64_emit_word(e, ins);
}

/* ASR (immediate): SBFM sf|00|100110|N|immr|imms|Rn|Rd */
void warm64_asr_imm(WArm64Enc *e, WArm64Reg rd, WArm64Reg rn, uint8_t shift, int sf) {
    int immr = shift;
    int imms = sf ? 63 : 31;
    uint32_t ins = 0x93400000;  /* SBFM 64-bit */
    ins |= (sf & 1) << 31;
    ins |= (sf & 1) << 22;
    ins |= (immr & 0x3F) << 16;
    ins |= (imms & 0x3F) << 10;
    ins |= (rn & 0x1F) << 5;
    ins |= (rd & 0x1F);
    warm64_emit_word(e, ins);
}

/* -- Load / Store ------------------------------------------------- */
/* LDR (unsigned offset): size|111|0|00|01|imm12|Rn|Rt */
/* size=11 for 64-bit, size=10 for 32-bit */
void warm64_ldr_imm(WArm64Enc *e, WArm64Reg rt, WArm64Reg rn, int32_t imm12, int sf) {
    uint32_t ins = 0xF9400000;  /* LDR 64-bit unsigned offset */
    ins |= (sf ? 3 : 2) << 30;  /* size */
    /* imm12 is in units of 8 for 64-bit, 4 for 32-bit */
    uint32_t scale = sf ? 3 : 2;
    ins |= ((imm12 >> scale) & 0xFFF) << 10;
    ins |= (rn & 0x1F) << 5;
    ins |= (rt & 0x1F);
    warm64_emit_word(e, ins);
}

/* STR (unsigned offset): size|111|0|00|00|imm12|Rn|Rt */
void warm64_str_imm(WArm64Enc *e, WArm64Reg rt, WArm64Reg rn, int32_t imm12, int sf) {
    uint32_t ins = 0xF9000000;  /* STR 64-bit unsigned offset */
    ins |= (sf ? 3 : 2) << 30;
    uint32_t scale = sf ? 3 : 2;
    ins |= ((imm12 >> scale) & 0xFFF) << 10;
    ins |= (rn & 0x1F) << 5;
    ins |= (rt & 0x1F);
    warm64_emit_word(e, ins);
}

/* LDR (register offset): size|111|0|00|00|000|0|11|Rm|0110|00|Rn|Rt
 * Simplified: size|111|0|00|00|000|0|11|Rm|option|S|Rn|Rt
 * Using option=011 (UXTW), S=0 */
void warm64_ldr_reg(WArm64Enc *e, WArm64Reg rt, WArm64Reg rn, WArm64Reg rm, int sf) {
    uint32_t ins = 0xF8606800;  /* LDR 64-bit, register offset, UXTW */
    ins |= (sf ? 3 : 2) << 30;
    ins |= (rm & 0x1F) << 16;
    ins |= (rn & 0x1F) << 5;
    ins |= (rt & 0x1F);
    warm64_emit_word(e, ins);
}

/* STR (register offset) */
void warm64_str_reg(WArm64Enc *e, WArm64Reg rt, WArm64Reg rn, WArm64Reg rm, int sf) {
    uint32_t ins = 0xF8206800;  /* STR 64-bit, register offset, UXTW */
    ins |= (sf ? 3 : 2) << 30;
    ins |= (rm & 0x1F) << 16;
    ins |= (rn & 0x1F) << 5;
    ins |= (rt & 0x1F);
    warm64_emit_word(e, ins);
}

/* -- Branch ------------------------------------------------------- */
/* B (unconditional): 000101|imm26 */
void warm64_b_uncond(WArm64Enc *e, int32_t imm26) {
    uint32_t ins = 0x14000000;
    ins |= (imm26 & 0x3FFFFFF);
    warm64_emit_word(e, ins);
}

/* B.cond: 01010100|imm19|0|cond */
void warm64_b_cond(WArm64Enc *e, int32_t imm19, WArm64CC cc) {
    uint32_t ins = 0x54000000;
    ins |= (imm19 & 0x7FFFF) << 5;
    ins |= (cc & 0xF);
    warm64_emit_word(e, ins);
}

/* BR: 1101011|0000|11111|000000|Rn|00000 */
void warm64_b_reg(WArm64Enc *e, WArm64Reg rn) {
    uint32_t ins = 0xD61F0000;
    ins |= (rn & 0x1F) << 5;
    warm64_emit_word(e, ins);
}

/* BLR: 1101011|0001|11111|000000|Rn|00000 */
void warm64_bl_reg(WArm64Enc *e, WArm64Reg rn) {
    uint32_t ins = 0xD63F0000;
    ins |= (rn & 0x1F) << 5;
    warm64_emit_word(e, ins);
}

/* RET: 1101011|0010|11111|000000|Rn|00000 */
void warm64_ret(WArm64Enc *e, WArm64Reg rn) {
    uint32_t ins = 0xD65F0000;
    ins |= (rn & 0x1F) << 5;
    warm64_emit_word(e, ins);
}

/* -- Comparison --------------------------------------------------- */
/* CMP (immediate): SUBS (immediate) with Rd=XZR */
/* sf|1|1|100010|0|imm12|Xn|11111 */
void warm64_cmp_imm(WArm64Enc *e, WArm64Reg rn, uint16_t imm12, int sf) {
    uint32_t ins = 0xF100001F;  /* SUBS XZR, Xn, #imm */
    ins |= (sf & 1) << 31;
    ins |= (imm12 & 0xFFF) << 10;
    ins |= (rn & 0x1F) << 5;
    warm64_emit_word(e, ins);
}

/* CMP (register): SUBS (register) with Rd=XZR */
/* sf|1|1|0|1011|00|0|Rm|000000|Xn|11111 */
void warm64_cmp_reg(WArm64Enc *e, WArm64Reg rn, WArm64Reg rm, int sf) {
    uint32_t ins = 0xEB00001F;  /* SUBS XZR, Xn, Xm */
    ins |= (sf & 1) << 31;
    ins |= (rm & 0x1F) << 16;
    ins |= (rn & 0x1F) << 5;
    warm64_emit_word(e, ins);
}

/* CSET: CSINC sf|00|11010100|Rm|0011111|Rd where Rm=cond^1 */
/* Actually: CSET Rd, cond = CSINC Rd, XZR, XZR, invert(cond) */
void warm64_cset(WArm64Enc *e, WArm64Reg rd, WArm64CC cc) {
    uint32_t ins = 0x9A9F07E0;  /* CSINC Xd, XZR, XZR, cond^1 */
    ins |= ((cc ^ 1) & 0xF) << 12;
    ins |= (rd & 0x1F);
    warm64_emit_word(e, ins);
}

/* -- Branch fixups ------------------------------------------------ */
size_t warm64_branch_pos(WArm64Enc *e) {
    /* Return position of the start of the last branch instruction */
    return e->pos - 4;
}

void warm64_patch_branch(WArm64Enc *e, size_t branch_pos, size_t target) {
    if (branch_pos + 4 > e->pos) return;
    intptr_t offset = (intptr_t)target - (intptr_t)branch_pos;
    uint32_t *p = (uint32_t *)(e->buf + branch_pos);
    uint32_t ins = *p;

    /* Check if this is B.cond (0x54......) or B (100101......) */
    if ((ins >> 24) == 0x54) {
        /* B.cond: imm19 at bits [23:5], encoded as offset/4 */
        int32_t imm19 = (int32_t)(offset / 4);
        *p = (ins & 0xFF00001F) | ((imm19 & 0x7FFFF) << 5);
    } else if ((ins >> 26) == 0x05) {
        /* B (unconditional): imm26 at bits [25:0], encoded as offset/4 */
        int32_t imm26 = (int32_t)(offset / 4);
        *p = (ins & ~0x3FFFFFF) | (imm26 & 0x3FFFFFF);
    }
    /* else: unknown instruction, don't patch */
}

/* -- Stack frame -------------------------------------------------- */
/* STP (pre-index): 1010100100|imm7|Rt2|Rn|Rt1 */
void warm64_stp_pre(WArm64Enc *e, WArm64Reg rt1, WArm64Reg rt2, WArm64Reg rn, int32_t imm7) {
    uint32_t ins = 0xA9800000;  /* STP 64-bit, pre-index */
    ins |= ((imm7 / 8) & 0x7F) << 15;
    ins |= (rt2 & 0x1F) << 10;
    ins |= (rn & 0x1F) << 5;
    ins |= (rt1 & 0x1F);
    warm64_emit_word(e, ins);
}

/* LDP (post-index): 1010100011|imm7|Rt2|Rn|Rt1 */
void warm64_ldp_post(WArm64Enc *e, WArm64Reg rt1, WArm64Reg rt2, WArm64Reg rn, int32_t imm7) {
    uint32_t ins = 0xA8C00000;  /* LDP 64-bit, post-index */
    ins |= ((imm7 / 8) & 0x7F) << 15;
    ins |= (rt2 & 0x1F) << 10;
    ins |= (rn & 0x1F) << 5;
    ins |= (rt1 & 0x1F);
    warm64_emit_word(e, ins);
}

/* PUSH: STP pre-index with SP, imm=-16 */
void warm64_push(WArm64Enc *e, WArm64Reg rt) {
    /* Push rt to stack: SP -= 16, store rt at SP */
    /* Use STP rt, XZR, [SP, #-16]! */
    uint32_t ins = 0xA9BF03E0;  /* STP Xt, XZR, [SP, #-16]! */
    ins |= (rt & 0x1F) << 10;
    warm64_emit_word(e, ins);
}

/* POP: LDP post-index with SP, imm=16 */
void warm64_pop(WArm64Enc *e, WArm64Reg rt) {
    /* Pop rt from stack: load rt from SP, SP += 16 */
    /* Use LDP Xt, XZR, [SP], #16 */
    uint32_t ins = 0xA8C003E0;  /* LDP Xt, XZR, [SP], #16 */
    ins |= (rt & 0x1F);
    warm64_emit_word(e, ins);
}
