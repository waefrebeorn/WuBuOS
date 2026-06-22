/*
 * wubu_x86.c  --  WuBuOS x86-64 Machine Code Encoder
 *
 * Pure C, zero-dependency x86-64 instruction emitter.
 * Full REX.W + ModRM + SIB encoding for SysV AMD64 ABI.
 */

#include "wubu_x86.h"

#include <stdlib.h>
#include <string.h>

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
    uint8_t rex = 0x48 | (reg_hi(dst) ? 0x01 : 0);
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, (uint8_t)(0xB8 + reg_lo(dst)));
    wx86_emit_qword(e, (uint64_t)imm);
    return 0;
}

int wx86_mov_reg_imm32(Wx86Enc *e, Wx86Reg dst, int32_t imm) {
    /* REX.W + C7 /0 + imm32 (sign-extended to 64) */
    uint8_t rex = wx86_rex(WREG_RAX, dst, true);  /* reg=0 for /0 */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0xC7);
    wx86_emit_modrm(e, 3, WREG_RAX, dst);  /* /0 = reg field = 0 */
    wx86_emit_dword(e, (uint32_t)imm);
    return 0;
}

int wx86_mov_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src) {
    /* REX.W + 89 + ModRM (src is reg field, dst is rm field) */
    emit_rex_modrm_reg_reg(e, 0x89, dst, src, true);
    return 0;
}

int wx86_mov_reg_mem(Wx86Enc *e, Wx86Reg dst, Wx86Reg base, int32_t disp) {
    /* REX.W + 8B + ModRM [+ SIB] [+ disp] */
    uint8_t rex = wx86_rex(dst, base, true);
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x8B);

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
    return 0;
}

int wx86_mov_mem_reg(Wx86Enc *e, Wx86Reg base, int32_t disp, Wx86Reg src) {
    /* REX.W + 89 + ModRM (src is reg field) */
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
    return 0;
}

/* -- ALU reg, reg operations -------------------------------------- */

int wx86_add_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src) {
    emit_rex_modrm_reg_reg(e, 0x01, dst, src, true);
    return 0;
}

int wx86_add_reg_imm32(Wx86Enc *e, Wx86Reg dst, int32_t imm) {
    uint8_t rex = wx86_rex(WREG_RAX, dst, true);
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x81);
    wx86_emit_modrm(e, 3, WREG_RAX, dst);  /* /0 = add */
    wx86_emit_dword(e, (uint32_t)imm);
    return 0;
}

int wx86_sub_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src) {
    emit_rex_modrm_reg_reg(e, 0x29, dst, src, true);
    return 0;
}

int wx86_sub_reg_imm32(Wx86Enc *e, Wx86Reg dst, int32_t imm) {
    uint8_t rex = wx86_rex(WREG_RBX, dst, true);  /* /5 = sub, reg_lo=5=rbx */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x81);
    wx86_emit_modrm(e, 3, WREG_RBX, dst);  /* /5 = sub */
    wx86_emit_dword(e, (uint32_t)imm);
    return 0;
}

int wx86_imul_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src) {
    /* REX.W + 0F AF + ModRM (dst=reg field, src=rm field) */
    uint8_t rex = wx86_rex(dst, src, true);
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x0F);
    wx86_emit_byte(e, 0xAF);
    wx86_emit_modrm(e, 3, dst, src);
    return 0;
}

int wx86_xor_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src) {
    emit_rex_modrm_reg_reg(e, 0x31, dst, src, true);
    return 0;
}

int wx86_cmp_reg_reg(Wx86Enc *e, Wx86Reg a, Wx86Reg b) {
    emit_rex_modrm_reg_reg(e, 0x39, a, b, true);
    return 0;
}

int wx86_cmp_reg_imm32(Wx86Enc *e, Wx86Reg dst, int32_t imm) {
    uint8_t rex = wx86_rex(WREG_RDI, dst, true);  /* /7 = cmp, reg_lo=7=rdi */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0x81);
    wx86_emit_modrm(e, 3, WREG_RDI, dst);  /* /7 = cmp */
    wx86_emit_dword(e, (uint32_t)imm);
    return 0;
}

int wx86_test_reg_reg(Wx86Enc *e, Wx86Reg a, Wx86Reg b) {
    emit_rex_modrm_reg_reg(e, 0x85, a, b, true);
    return 0;
}

int wx86_lea_reg_mem(Wx86Enc *e, Wx86Reg dst, Wx86Reg base, int32_t disp) {
    /* REX.W + 8D + ModRM — same encoding as MOV from memory but opcode 8D */
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
    return 0;
}

/* -- Shift Operations --------------------------------------------- */

int wx86_shl_reg_imm8(Wx86Enc *e, Wx86Reg dst, uint8_t count) {
    uint8_t rex = wx86_rex(WREG_RSP, dst, true);  /* /4 = shl */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0xC1);
    wx86_emit_modrm(e, 3, WREG_RSP, dst);  /* /4 */
    wx86_emit_byte(e, count);
    return 0;
}

int wx86_shr_reg_imm8(Wx86Enc *e, Wx86Reg dst, uint8_t count) {
    uint8_t rex = wx86_rex(WREG_RBP, dst, true);  /* /5 = shr */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0xC1);
    wx86_emit_modrm(e, 3, WREG_RBP, dst);  /* /5 */
    wx86_emit_byte(e, count);
    return 0;
}

int wx86_sar_reg_imm8(Wx86Enc *e, Wx86Reg dst, uint8_t count) {
    uint8_t rex = wx86_rex(WREG_RDI, dst, true);  /* /7 = sar */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0xC1);
    wx86_emit_modrm(e, 3, WREG_RDI, dst);  /* /7 */
    wx86_emit_byte(e, count);
    return 0;
}

int wx86_neg_reg(Wx86Enc *e, Wx86Reg dst) {
    /* REX.W + F7 /3 */
    uint8_t rex = wx86_rex(WREG_RBX, dst, true);  /* /3 */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0xF7);
    wx86_emit_modrm(e, 3, WREG_RBX, dst);
    return 0;
}

int wx86_cqo(Wx86Enc *e) {
    wx86_emit_byte(e, 0x48);  /* REX.W */
    wx86_emit_byte(e, 0x99);  /* CQO */
    return 0;
}

int wx86_idiv_reg(Wx86Enc *e, Wx86Reg src) {
    /* REX.W + F7 /7 */
    uint8_t rex = wx86_rex(WREG_RDI, src, true);  /* /7 */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0xF7);
    wx86_emit_modrm(e, 3, WREG_RDI, src);
    return 0;
}

/* -- Control Flow ------------------------------------------------- */

int wx86_ret(Wx86Enc *e) {
    wx86_emit_byte(e, 0xC3);
    return 0;
}

int wx86_jmp_rel32(Wx86Enc *e) {
    wx86_emit_byte(e, 0xE9);
    wx86_emit_dword(e, 0);  /* placeholder — patch later */
    return 0;
}

int wx86_jcc_rel32(Wx86Enc *e, Wx86CC cc) {
    wx86_emit_byte(e, 0x0F);
    wx86_emit_byte(e, (uint8_t)(0x80 | (cc & 0xF)));
    wx86_emit_dword(e, 0);  /* placeholder — patch later */
    return 0;
}

int wx86_call_rel32(Wx86Enc *e) {
    wx86_emit_byte(e, 0xE8);
    wx86_emit_dword(e, 0);  /* placeholder — patch later */
    return 0;
}

int wx86_call_reg(Wx86Enc *e, Wx86Reg reg) {
    /* REX.W + FF /2 */
    uint8_t rex = wx86_rex(WREG_RDX, reg, true);  /* /2 = call */
    wx86_emit_byte(e, rex);
    wx86_emit_byte(e, 0xFF);
    wx86_emit_modrm(e, 3, WREG_RDX, reg);  /* /2 */
    return 0;
}

int wx86_push_reg(Wx86Enc *e, Wx86Reg src) {
    if (reg_hi(src)) {
        wx86_emit_byte(e, 0x41);  /* REX.B */
    }
    wx86_emit_byte(e, (uint8_t)(0x50 + reg_lo(src)));
    return 0;
}

int wx86_pop_reg(Wx86Enc *e, Wx86Reg dst) {
    if (reg_hi(dst)) {
        wx86_emit_byte(e, 0x41);  /* REX.B */
    }
    wx86_emit_byte(e, (uint8_t)(0x58 + reg_lo(dst)));
    return 0;
}

int wx86_sub_rsp_imm8(Wx86Enc *e, uint8_t imm) {
    wx86_emit_byte(e, 0x48);  /* REX.W */
    wx86_emit_byte(e, 0x83);
    wx86_emit_byte(e, 0xEC);  /* ModRM: 11 101 100 = sub rsp, imm8 */
    wx86_emit_byte(e, imm);
    return 0;
}

int wx86_add_rsp_imm8(Wx86Enc *e, uint8_t imm) {
    wx86_emit_byte(e, 0x48);  /* REX.W */
    wx86_emit_byte(e, 0x83);
    wx86_emit_byte(e, 0xC4);  /* ModRM: 11 000 100 = add rsp, imm8 */
    wx86_emit_byte(e, imm);
    return 0;
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
