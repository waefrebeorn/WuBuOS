/*
 * wubu_x86.c  --  WuBuOS x86-64 Machine Code Encoder
 *
 * Pure C, zero-dependency x86-64 instruction emitter.
 * Full REX.W + ModRM + SIB encoding for SysV AMD64 ABI.
 */

#include "wubu_x86.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

/* -- Encoder Buffer ----------------------------------------------- */

void wx86_enc_init(Wx86Enc *e, uint8_t *buf, size_t cap) {
    e->buf = buf;
    e->cap = cap;
    e->pos = 0;
    e->owns_buf = false;
}

void wx86_enc_init_dynamic(Wx86Enc *e, size_t initial_cap) {
    if (initial_cap == 0) initial_cap = 256;
    e->buf = (uint8_t *)malloc(initial_cap);
    e->cap = e->buf ? initial_cap : 0;
    e->pos = 0;
    e->owns_buf = true;
}

void wx86_enc_free(Wx86Enc *e) {
    if (e && e->owns_buf) {
        free(e->buf);
        e->buf = NULL;
        e->cap = 0;
        e->pos = 0;
        e->owns_buf = false;
    }
}

void wx86_enc_reset(Wx86Enc *e) {
    e->pos = 0;
}

/* -- Low-Level Emit ----------------------------------------------- */

static void ensure_cap(Wx86Enc *e, size_t need) {
    if (e->pos + need <= e->cap) return;
    if (!e->owns_buf) return;  /* caller-owned: silent truncation */
    size_t newcap = e->cap ? e->cap * 2 : 256;
    while (newcap < e->pos + need) newcap *= 2;
    uint8_t *nb = (uint8_t *)realloc(e->buf, newcap);
    if (nb) { e->buf = nb; e->cap = newcap; }
}

void wx86_emit_byte(Wx86Enc *e, uint8_t b) {
    ensure_cap(e, 1);
    if (e->pos < e->cap) e->buf[e->pos++] = b;
}

void wx86_emit_word(Wx86Enc *e, uint16_t w) {
    ensure_cap(e, 2);
    if (e->pos + 2 <= e->cap) {
        e->buf[e->pos++] = (uint8_t)(w & 0xFF);
        e->buf[e->pos++] = (uint8_t)((w >> 8) & 0xFF);
    }
}

void wx86_emit_dword(Wx86Enc *e, uint32_t d) {
    ensure_cap(e, 4);
    if (e->pos + 4 <= e->cap) {
        e->buf[e->pos++] = (uint8_t)(d & 0xFF);
        e->buf[e->pos++] = (uint8_t)((d >> 8) & 0xFF);
        e->buf[e->pos++] = (uint8_t)((d >> 16) & 0xFF);
        e->buf[e->pos++] = (uint8_t)((d >> 24) & 0xFF);
    }
}

void wx86_emit_qword(Wx86Enc *e, uint64_t q) {
    wx86_emit_dword(e, (uint32_t)(q & 0xFFFFFFFF));
    wx86_emit_dword(e, (uint32_t)((q >> 32) & 0xFFFFFFFF));
}

void wx86_patch_rel32(Wx86Enc *e, size_t patch_pos, size_t target_pos) {
    if (patch_pos + 4 > e->cap) return;
    int32_t rel = (int32_t)((int64_t)target_pos - (int64_t)(patch_pos + 4));
    e->buf[patch_pos + 0] = (uint8_t)(rel & 0xFF);
    e->buf[patch_pos + 1] = (uint8_t)((rel >> 8) & 0xFF);
    e->buf[patch_pos + 2] = (uint8_t)((rel >> 16) & 0xFF);
    e->buf[patch_pos + 3] = (uint8_t)((rel >> 24) & 0xFF);
}

void wx86_patch_rel8(Wx86Enc *e, size_t patch_pos, size_t target_pos) {
    if (patch_pos + 1 > e->cap) return;
    int8_t rel = (int8_t)((int64_t)target_pos - (int64_t)(patch_pos + 1));
    e->buf[patch_pos] = (uint8_t)rel;
}

/* -- REX / ModRM / SIB Helpers ------------------------------------ */

static inline uint8_t reg_lo(Wx86Reg r)  { return (uint8_t)(r & 0x7); }
static inline bool    reg_hi(Wx86Reg r)   { return (r >= 8 && r <= 15); }

uint8_t wx86_rex(Wx86Reg reg, Wx86Reg rm, bool w) {
    uint8_t rex = 0x40;
    if (w)       rex |= 0x08;  /* REX.W */
    if (reg_hi(reg)) rex |= 0x04;  /* REX.R */
    if (reg_hi(rm))  rex |= 0x01;  /* REX.B */
    return rex;
}

void wx86_emit_modrm(Wx86Enc *e, uint8_t mod, Wx86Reg reg, Wx86Reg rm) {
    uint8_t b = (uint8_t)((mod & 3) << 6) | (reg_lo(reg) << 3) | reg_lo(rm);
    wx86_emit_byte(e, b);
}

void wx86_emit_sib(Wx86Enc *e, uint8_t scale, Wx86Reg index, Wx86Reg base) {
    uint8_t sib = 0;
    switch (scale) {
        case 1: sib = 0x00; break;
        case 2: sib = 0x40; break;
        case 4: sib = 0x80; break;
        case 8: sib = 0xC0; break;
        default: sib = 0x00; break;
    }
    sib |= (reg_lo(index) << 3) | reg_lo(base);
    wx86_emit_byte(e, sib);
}

/* -- Internal: emit REX + opcode + ModRM for reg-reg ops ---------- */

static void emit_rex_modrm_reg_reg(Wx86Enc *e, uint8_t opcode,
                                     Wx86Reg dst, Wx86Reg src,
                                     bool src_is_reg_field) {
    /* For ops like ADD dst, src:  opcode has /r where src is reg field
     * For ops like IMUL dst, src: dst is reg field, src is rm field
     * src_is_reg_field: true if src goes in reg field (opcode /r convention)
     *                    false if dst goes in reg field (like IMUL, LEA) */
    Wx86Reg reg_field = src_is_reg_field ? src : dst;
    Wx86Reg rm_field   = src_is_reg_field ? dst : src;

    /* REX.W is always needed for 64-bit ops */
    uint8_t rex = wx86_rex(reg_field, rm_field, true);
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, opcode);
    wx86_emit_modrm(e, 3, reg_field, rm_field);  /* mod=11: register-register */
}

/* -- Instruction Encoding ----------------------------------------- */

int wx86_mov_reg_imm64(Wx86Enc *e, Wx86Reg dst, int64_t imm) {
    /* REX.W + B8+rd + imm64 */
    size_t start = e->pos;
    uint8_t rex = 0x48 | (reg_hi(dst) ? 0x01 : 0);
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, (uint8_t)(0xB8 + reg_lo(dst)));
    wx86_emit_qword(e, (uint64_t)imm);
    return (int)(e->pos - start);
}

int wx86_mov_reg_imm32(Wx86Enc *e, Wx86Reg dst, int32_t imm) {
    /* REX.W + C7 /0 + imm32 (sign-extended to 64) */
    size_t start = e->pos;
    uint8_t rex = wx86_rex(WREG_RAX, dst, true);  /* reg=0 for /0 */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0xC7);
    wx86_emit_modrm(e, 3, WREG_RAX, dst);  /* /0 = reg field = 0 */
    wx86_emit_dword(e, (uint32_t)imm);
    return (int)(e->pos - start);
}

int wx86_mov_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src) {
    /* REX.W + 89 + ModRM (src is reg field, dst is rm field) */
    size_t start = e->pos;
    emit_rex_modrm_reg_reg(e, 0x89, dst, src, true);
    return (int)(e->pos - start);
}

int wx86_mov_reg_mem(Wx86Enc *e, Wx86Reg dst, Wx86Reg base, int32_t disp) {
    /* REX.W + 8B + ModRM [+ SIB] [+ disp] */
    size_t start = e->pos;
    uint8_t rex = wx86_rex(dst, base, true);    wx86_emit_byte(e, rex);    wx86_emit_byte(e, 0x8B);

    /* Determine mod from displacement */
    if (disp == 0 && reg_lo(base) != 5) {  /* RBP/R13 always needs mod=01 */
        wx86_emit_modrm(e, 0, dst, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)  /* RSP/R12 needs SIB */
            wx86_emit_sib(e, 1, WREG_RSP, base);
    } else if (disp >= -128 && disp <= 127) {
        wx86_emit_modrm(e, 1, dst, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
        wx86_emit_byte(e, (uint8_t)(disp & 0xFF));
    } else {
        wx86_emit_modrm(e, 2, dst, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
        wx86_emit_dword(e, (uint32_t)disp);
    }
    return (int)(e->pos - start);
}

int wx86_movzx_byte_reg_mem(Wx86Enc *e, Wx86Reg dst, Wx86Reg base, int32_t disp) {
    /* MOVZX r64, byte[base+disp] = REX.W + 0F B6 + modrm [+ sib] [+ disp].
     * Same addressing as mov_reg_mem but opcode 0F B6 (zero-extend byte). */
    size_t start = e->pos;
    uint8_t rex = wx86_rex(dst, base, true);
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x0F);
    wx86_emit_byte(e, 0xB6);
    if (disp == 0 && reg_lo(base) != 5) {
        wx86_emit_modrm(e, 0, dst, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
    } else if (disp >= -128 && disp <= 127) {
        wx86_emit_modrm(e, 1, dst, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
        wx86_emit_byte(e, (uint8_t)(disp & 0xFF));
    } else {
        wx86_emit_modrm(e, 2, dst, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
        wx86_emit_dword(e, (uint32_t)disp);
    }
    return (int)(e->pos - start);
}

int wx86_mov_mem_reg(Wx86Enc *e, Wx86Reg base, int32_t disp, Wx86Reg src) {
    /* REX.W + 89 + ModRM (src is reg field) */
    size_t start = e->pos;
    uint8_t rex = wx86_rex(src, base, true);
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x89);

    if (disp == 0 && reg_lo(base) != 5) {
        wx86_emit_modrm(e, 0, src, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
    } else if (disp >= -128 && disp <= 127) {
        wx86_emit_modrm(e, 1, src, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
        wx86_emit_byte(e, (uint8_t)(disp & 0xFF));
    } else {
        wx86_emit_modrm(e, 2, src, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
        wx86_emit_dword(e, (uint32_t)disp);
    }
    return (int)(e->pos - start);
}

int wx86_movnti_mem_reg(Wx86Enc *e, Wx86Reg base, int32_t disp, Wx86Reg src) {
    /* MOVNTI m64, r64 = REX.W + 0F C3 /r. src is reg field, base is rm.
     * Same modrm/sib encoding as mov_mem_reg but with opcode 0F C3. */
    size_t start = e->pos;
    uint8_t rex = wx86_rex(src, base, true);
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x0F);
    wx86_emit_byte(e, 0xC3);
    if (disp == 0 && reg_lo(base) != 5) {
        wx86_emit_modrm(e, 0, src, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
    } else if (disp >= -128 && disp <= 127) {
        wx86_emit_modrm(e, 1, src, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
        wx86_emit_byte(e, (uint8_t)(disp & 0xFF));
    } else {
        wx86_emit_modrm(e, 2, src, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
        wx86_emit_dword(e, (uint32_t)disp);
    }
    return (int)(e->pos - start);
}

/* -- ALU reg, reg operations -------------------------------------- */

int wx86_add_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src) {
    size_t start = e->pos;
    emit_rex_modrm_reg_reg(e, 0x01, dst, src, true);
    return (int)(e->pos - start);
}

int wx86_add_rax_mem(Wx86Enc *e, Wx86Reg base, int32_t disp) {
    /* ADD rax, [base+disp] = REX.W + 03 + modrm (reg=000=rax). */
    size_t start = e->pos;
    uint8_t rex = wx86_rex(WREG_RAX, base, true);
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x03);
    if (disp == 0 && reg_lo(base) != 5) {
        wx86_emit_modrm(e, 0, WREG_RAX, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
    } else if (disp >= -128 && disp <= 127) {
        wx86_emit_modrm(e, 1, WREG_RAX, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
        wx86_emit_byte(e, (uint8_t)(disp & 0xFF));
    } else {
        wx86_emit_modrm(e, 2, WREG_RAX, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
        wx86_emit_dword(e, (uint32_t)disp);
    }
    return (int)(e->pos - start);
}

int wx86_adc_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src) {
    /* ADC r64, r/m64 = REX.W + 11 /r. dst += src + CF. */
    size_t start = e->pos;
    emit_rex_modrm_reg_reg(e, 0x11, dst, src, true);
    return (int)(e->pos - start);
}

int wx86_sbb_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src) {
    /* SBB r64, r/m64 = REX.W + 19 /r. dst -= src - CF. */
    size_t start = e->pos;
    emit_rex_modrm_reg_reg(e, 0x19, dst, src, true);
    return (int)(e->pos - start);
}

int wx86_clc(Wx86Enc *e) { wx86_emit_byte(e, 0xF8); return 1; }
int wx86_stc(Wx86Enc *e) { wx86_emit_byte(e, 0xF9); return 1; }

int wx86_add_reg_imm32(Wx86Enc *e, Wx86Reg dst, int32_t imm) {
    size_t start = e->pos;
    uint8_t rex = wx86_rex(WREG_RAX, dst, true);
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x81);
    wx86_emit_modrm(e, 3, WREG_RAX, dst);  /* /0 = add */
    wx86_emit_dword(e, (uint32_t)imm);
    return (int)(e->pos - start);
}

int wx86_sub_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src) {
    size_t start = e->pos;
    emit_rex_modrm_reg_reg(e, 0x29, dst, src, true);
    return (int)(e->pos - start);
}

int wx86_sub_reg_imm32(Wx86Enc *e, Wx86Reg dst, int32_t imm) {
    size_t start = e->pos;
    uint8_t rex = wx86_rex(WREG_RBX, dst, true);  /* REX.W */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x81);
    /* 0x81 /5 = SUB r/m64, imm32. The reg field is the opcode EXTENSION (5),
     * not a register — pass the literal extension value. */
    wx86_emit_byte(e, (uint8_t)(0xC0 | (5 << 3) | reg_lo(dst)));  /* mod=11, /5, rm=dst */
    wx86_emit_dword(e, (uint32_t)imm);
    return (int)(e->pos - start);
}

int wx86_and_reg_imm32(Wx86Enc *e, Wx86Reg dst, int32_t imm) {
    /* 0x81 /4 = AND r/m64, imm32 (opcode extension 4). */
    size_t start = e->pos;
    uint8_t rex = wx86_rex(WREG_RBX, dst, true);  /* REX.W */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x81);
    wx86_emit_byte(e, (uint8_t)(0xC0 | (4 << 3) | reg_lo(dst)));  /* mod=11, /4, rm=dst */
    wx86_emit_dword(e, (uint32_t)imm);
    return (int)(e->pos - start);
}

int wx86_and_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src) {
    size_t start = e->pos;
    emit_rex_modrm_reg_reg(e, 0x21, dst, src, true);
    return (int)(e->pos - start);
}

int wx86_or_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src) {
    size_t start = e->pos;
    emit_rex_modrm_reg_reg(e, 0x09, dst, src, true);
    return (int)(e->pos - start);
}

int wx86_or_reg_imm32(Wx86Enc *e, Wx86Reg dst, int32_t imm) {
    size_t start = e->pos;
    uint8_t rex = wx86_rex(WREG_RBX, dst, true);
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x81);
    wx86_emit_byte(e, (uint8_t)(0xC0 | (1 << 3) | reg_lo(dst)));  /* mod=11, /1, rm=dst */
    wx86_emit_dword(e, (uint32_t)imm);
    return (int)(e->pos - start);
}

int wx86_not_reg(Wx86Enc *e, Wx86Reg dst) {
    size_t start = e->pos;
    uint8_t rex = wx86_rex(WREG_RBX, dst, true);
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0xF7);
    wx86_emit_byte(e, (uint8_t)(0xC0 | (2 << 3) | reg_lo(dst)));  /* mod=11, /2, rm=dst */
    return (int)(e->pos - start);
}

int wx86_lea_scaled_index(Wx86Enc *e, Wx86Reg dst, Wx86Reg base, uint8_t scale) {
    /* lea r64, [base + base*2^scale]: REX.W(+R if dst hi, +B if base hi) + 8D,
     * modrm(mod=00, reg=dst, rm=100=RSP->SIB), SIB(scale, index=base, base=base). */
    size_t start = e->pos;
    uint8_t rex = 0x48
        | (reg_hi(dst)  ? 0x04 : 0)   /* REX.R for dst (reg field) */
        | (reg_hi(base) ? 0x02 : 0)   /* REX.X for base (SIB index) */
        | (reg_hi(base) ? 0x01 : 0);  /* REX.B for base (SIB base) */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x8D);
    /* mod=00, reg=dst (reg field), rm=100 (forces SIB). */
    wx86_emit_byte(e, (uint8_t)((reg_lo(dst) << 3) | 0x04));
    /* SIB: scale=scale (bits 7-6), index=base (bits 5-3), base=base (bits 2-0). */
    wx86_emit_byte(e, (uint8_t)((scale << 6) | (reg_lo(base) << 3) | reg_lo(base)));
    return (int)(e->pos - start);
}

int wx86_imul_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src) {
    /* REX.W + 0F AF + ModRM (dst=reg field, src=rm field) */
    size_t start = e->pos;
    uint8_t rex = wx86_rex(dst, src, true);
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x0F);
    wx86_emit_byte(e, 0xAF);
    wx86_emit_modrm(e, 3, dst, src);
    return (int)(e->pos - start);
}

int wx86_imul_reg_reg_imm32(Wx86Enc *e, Wx86Reg dst, Wx86Reg src, int32_t imm) {
    /* IMUL r64, r/m64, imm32 = REX.W + 69 /r + imm32.
     * dst is the reg field (mod=11), src is rm field. */
    size_t start = e->pos;
    uint8_t rex = wx86_rex(dst, src, true);
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x69);
    wx86_emit_modrm(e, 3, dst, src);
    wx86_emit_dword(e, (uint32_t)imm);
    return (int)(e->pos - start);
}

int wx86_imul_rax_rm(Wx86Enc *e, Wx86Reg rm) {
    /* IMUL r/m64 (one-operand): REX.W + F7 /5, modrm mod=11 rm=rm.
     * Computes rdx:rax = rax * rm (full 128-bit signed product). */
    size_t start = e->pos;
    uint8_t rex = wx86_rex(WREG_RAX, rm, true);  /* REX.W + REX.B if rm hi */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0xF7);
    wx86_emit_byte(e, (uint8_t)(0xC0 | (5 << 3) | reg_lo(rm)));  /* /5 = IMUL */
    return (int)(e->pos - start);
}

int wx86_xor_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src) {
    size_t start = e->pos;
    emit_rex_modrm_reg_reg(e, 0x31, dst, src, true);
    return (int)(e->pos - start);
}

/* Zero register: xor r32, r32 (2 bytes, zeroes full 64-bit reg). */
int wx86_zero_reg(Wx86Enc *e, Wx86Reg r) {
    size_t start = e->pos;
    /* 31 /r with mod=11, no REX.W (32-bit op zeroes upper 32 bits too) */
    if (reg_hi(r)) wx86_emit_byte(e, 0x48 | 0x04); /* REX.B */
    wx86_emit_byte(e, 0x31);
    wx86_emit_modrm(e, 3, reg_lo(r), reg_lo(r));
    return (int)(e->pos - start);
}

int wx86_cmp_reg_reg(Wx86Enc *e, Wx86Reg a, Wx86Reg b) {
    size_t start = e->pos;
    emit_rex_modrm_reg_reg(e, 0x39, a, b, true);
    return (int)(e->pos - start);
}

int wx86_test_reg_reg(Wx86Enc *e, Wx86Reg a, Wx86Reg b) {
    /* TEST r64, r/m64: REX.W + 85 /r. a is reg field, b is rm field. */
    size_t start = e->pos;
    emit_rex_modrm_reg_reg(e, 0x85, a, b, true);
    return (int)(e->pos - start);
}

int wx86_cmovcc_reg_reg(Wx86Enc *e, Wx86CC cc, Wx86Reg dst, Wx86Reg src) {
    /* CMOVcc r64, r/m64: REX.W + 0F 40+cc /r. DST is the reg field, SRC rm. */
    size_t start = e->pos;
    uint8_t rex = wx86_rex(dst, src, true);  /* reg_field=dst, rm_field=src */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x0F);
    wx86_emit_byte(e, (uint8_t)(0x40 + (uint8_t)cc));
    wx86_emit_modrm(e, 3, dst, src);
    return (int)(e->pos - start);
}

int wx86_cmp_reg_imm32(Wx86Enc *e, Wx86Reg dst, int32_t imm) {
    size_t start = e->pos;
    uint8_t rex = wx86_rex(WREG_RDI, dst, true);  /* /7 = cmp, reg_lo=7=rdi */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x81);
    wx86_emit_modrm(e, 3, WREG_RDI, dst);  /* /7 = cmp */
    wx86_emit_dword(e, (uint32_t)imm);
    return (int)(e->pos - start);
}

int wx86_lea_reg_mem(Wx86Enc *e, Wx86Reg dst, Wx86Reg base, int32_t disp) {
    /* REX.W + 8D + ModRM — same encoding as MOV from memory but opcode 8D */
    size_t start = e->pos;
    uint8_t rex = wx86_rex(dst, base, true);
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x8D);

    if (disp == 0 && reg_lo(base) != 5) {
        wx86_emit_modrm(e, 0, dst, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
    } else if (disp >= -128 && disp <= 127) {
        wx86_emit_modrm(e, 1, dst, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
        wx86_emit_byte(e, (uint8_t)(disp & 0xFF));
    } else {
        wx86_emit_modrm(e, 2, dst, base);
        if (reg_lo(base) == 4 || reg_lo(base) == 12)
            wx86_emit_sib(e, 1, WREG_RSP, base);
        wx86_emit_dword(e, (uint32_t)disp);
    }
    return (int)(e->pos - start);
}

/* -- Shift Operations --------------------------------------------- */

int wx86_shl_reg_imm8(Wx86Enc *e, Wx86Reg dst, uint8_t count) {
    size_t start = e->pos;
    uint8_t rex = wx86_rex(WREG_RSP, dst, true);  /* /4 = shl */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0xC1);
    wx86_emit_modrm(e, 3, WREG_RSP, dst);  /* /4 */
    wx86_emit_byte(e, count);
    return (int)(e->pos - start);
}

int wx86_shr_reg_imm8(Wx86Enc *e, Wx86Reg dst, uint8_t count) {
    size_t start = e->pos;
    uint8_t rex = wx86_rex(WREG_RBP, dst, true);  /* /5 = shr */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0xC1);
    wx86_emit_modrm(e, 3, WREG_RBP, dst);  /* /5 */
    wx86_emit_byte(e, count);
    return (int)(e->pos - start);
}

int wx86_sar_reg_imm8(Wx86Enc *e, Wx86Reg dst, uint8_t count) {
    size_t start = e->pos;
    uint8_t rex = wx86_rex(WREG_RDI, dst, true);  /* /7 = sar */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0xC1);
    wx86_emit_modrm(e, 3, WREG_RDI, dst);  /* /7 */
    wx86_emit_byte(e, count);
    return (int)(e->pos - start);
}

int wx86_neg_reg(Wx86Enc *e, Wx86Reg dst) {
    /* REX.W + F7 /3 */
    size_t start = e->pos;
    uint8_t rex = wx86_rex(WREG_RBX, dst, true);  /* /3 */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0xF7);
    wx86_emit_modrm(e, 3, WREG_RBX, dst);
    return (int)(e->pos - start);
}

int wx86_cqo(Wx86Enc *e) {
    size_t start = e->pos;
    wx86_emit_byte(e, 0x48);  /* REX.W */
    wx86_emit_byte(e, 0x99);  /* CQO */
    return (int)(e->pos - start);
}

int wx86_idiv_reg(Wx86Enc *e, Wx86Reg src) {
    /* REX.W + F7 /7 */
    size_t start = e->pos;
    uint8_t rex = wx86_rex(WREG_RDI, src, true);  /* /7 */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0xF7);
    wx86_emit_modrm(e, 3, WREG_RDI, src);
    return (int)(e->pos - start);
}

/* -- Control Flow ------------------------------------------------- */

int wx86_ret(Wx86Enc *e) {
    size_t start = e->pos;
    wx86_emit_byte(e, 0xC3);
    return (int)(e->pos - start);
}

int wx86_jmp_rel32(Wx86Enc *e) {
    size_t start = e->pos;
    wx86_emit_byte(e, 0xE9);
    wx86_emit_dword(e, 0);  /* placeholder — patch later */
    return (int)(e->pos - start);
}

int wx86_jcc_rel32(Wx86Enc *e, Wx86CC cc) {
    size_t start = e->pos;
    wx86_emit_byte(e, 0x0F);
    wx86_emit_byte(e, (uint8_t)(0x80 | (cc & 0xF)));
    wx86_emit_dword(e, 0);  /* placeholder — patch later */
    return (int)(e->pos - start);
}

int wx86_call_rel32(Wx86Enc *e) {
    size_t start = e->pos;
    wx86_emit_byte(e, 0xE8);
    wx86_emit_dword(e, 0);  /* placeholder — patch later */
    return (int)(e->pos - start);
}

int wx86_call_reg(Wx86Enc *e, Wx86Reg reg) {
    /* REX.W + FF /2 */
    size_t start = e->pos;
    uint8_t rex = wx86_rex(WREG_RDX, reg, true);  /* /2 = call */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0xFF);
    wx86_emit_modrm(e, 3, WREG_RDX, reg);  /* /2 */
    return (int)(e->pos - start);
}

int wx86_push_reg(Wx86Enc *e, Wx86Reg src) {
    size_t start = e->pos;
    if (reg_hi(src)) {
        wx86_emit_byte(e, 0x41);  /* REX.B */
    }
    wx86_emit_byte(e, (uint8_t)(0x50 + reg_lo(src)));
    return (int)(e->pos - start);
}

int wx86_pop_reg(Wx86Enc *e, Wx86Reg dst) {
    size_t start = e->pos;
    if (reg_hi(dst)) {
        wx86_emit_byte(e, 0x41);  /* REX.B */
    }
    wx86_emit_byte(e, (uint8_t)(0x58 + reg_lo(dst)));
    return (int)(e->pos - start);
}

int wx86_sub_rsp_imm8(Wx86Enc *e, uint8_t imm) {
    size_t start = e->pos;
    wx86_emit_byte(e, 0x48);  /* REX.W */
    wx86_emit_byte(e, 0x83);
    wx86_emit_byte(e, 0xEC);  /* ModRM: 11 101 100 = sub rsp, imm8 */
    wx86_emit_byte(e, imm);
    return (int)(e->pos - start);
}

int wx86_add_rsp_imm8(Wx86Enc *e, uint8_t imm) {
    size_t start = e->pos;
    wx86_emit_byte(e, 0x48);  /* REX.W */
    wx86_emit_byte(e, 0x83);
    wx86_emit_byte(e, 0xC4);  /* ModRM: 11 000 100 = add rsp, imm8 */
    wx86_emit_byte(e, imm);
    return (int)(e->pos - start);
}

/* -- ABI ---------------------------------------------------------- */

Wx86ABI wx86_sysv_abi(void) {
    Wx86ABI abi;
    abi.arg_regs[0] = WREG_RDI;
    abi.arg_regs[1] = WREG_RSI;
    abi.arg_regs[2] = WREG_RDX;
    abi.arg_regs[3] = WREG_RCX;
    abi.arg_regs[4] = WREG_R8;
    abi.arg_regs[5] = WREG_R9;
    abi.ret_reg = WREG_RAX;
    abi.spill_reg = WREG_R10;
    return abi;
}

/* -- Register / Condition Names ----------------------------------- */

static const char *reg_names[] = {
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi",
    "r8",  "r9",  "r10", "r11", "r12", "r13", "r14", "r15"
};

const char *wx86_reg_name(Wx86Reg r) {
    if (r >= 0 && r <= 15) return reg_names[r];
    return "?";
}

static const char *cc_names[] = {
    "o",  "no", "b",  "ae",
    "e",  "ne", "be", "a",
    "s",  "ns", NULL, NULL,
    "l",  "ge", "le", "g"
};

const char *wx86_cc_name(Wx86CC cc) {
    if (cc >= 0 && cc <= 15 && cc_names[cc]) return cc_names[cc];
    return "?";
}

/* Multi-byte NOP — aligns code. Uses recommended AMD64 multi-byte NOP encodings.
 * Much faster than N single-byte NOPs (1 decode slot vs N). */
int wx86_multi_nop(Wx86Enc *e, int bytes) {
    if (bytes <= 0) return 0;
    static const uint8_t n2[]  = { 0x66, 0x90 };
    static const uint8_t n3[]  = { 0x0F, 0x1F, 0x00 };
    static const uint8_t n4[]  = { 0x0F, 0x1F, 0x40, 0x00 };
    static const uint8_t n5[]  = { 0x0F, 0x1F, 0x44, 0x00, 0x00 };
    static const uint8_t n6[]  = { 0x66, 0x0F, 0x1F, 0x44, 0x00, 0x00 };
    static const uint8_t n7[]  = { 0x0F, 0x1F, 0x80, 0x00, 0x00, 0x00, 0x00 };
    static const uint8_t n8[]  = { 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 };
    static const uint8_t n9[]  = { 0x66, 0x0F, 0x1F, 0x84, 0x00, 0x00, 0x00, 0x00, 0x00 };
    while (bytes > 0) {
        const uint8_t *seq; int len;
        if (bytes >= 9) { seq = n9; len = 9; }
        else if (bytes == 8) { seq = n8; len = 8; }
        else if (bytes == 7) { seq = n7; len = 7; }
        else if (bytes == 6) { seq = n6; len = 6; }
        else if (bytes == 5) { seq = n5; len = 5; }
        else if (bytes == 4) { seq = n4; len = 4; }
        else if (bytes == 3) { seq = n3; len = 3; }
        else if (bytes == 2) { seq = n2; len = 2; }
        else { wx86_emit_byte(e, 0x90); len = 1; }
        for (int i = 0; i < len; i++) wx86_emit_byte(e, seq[i]);
        bytes -= len;
    }
    return 0;
}
