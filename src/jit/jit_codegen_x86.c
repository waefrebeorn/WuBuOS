/*
 * jit_codegen_x86.c — x86-64 backend for the abstract code generator.
 *
 * Wraps Wx86Enc + wubu_x86.h functions behind the CodeGenVTable.
 */
#include "jit_codegen.h"
#include "wubu_x86.h"
#include <stdlib.h>
#include <string.h>

/* -- Register mapping: CGReg → Wx86Reg ---------------------------- */
static Wx86Reg cg_to_x86(CGReg r) {
    /* Neutral regs map to x86 registers.
     * CG_REG_0 = return value = RAX
     * CG_REG_1..6 = args = RDI,RSI,RDX,RCX,R8,R9
     * CG_REG_7..15 = scratch = R10-R15, RBX, RBP */
    static const Wx86Reg map[] = {
        WREG_RAX,                                /* 0: return */
        WREG_RDI, WREG_RSI, WREG_RDX, WREG_RCX,  /* 1-4: args */
        WREG_R8,  WREG_R9,                        /* 5-6: args */
        WREG_R10, WREG_R11, WREG_R12, WREG_R13,  /* 7-10: scratch */
        WREG_R14, WREG_R15,                       /* 11-12: scratch */
        WREG_RBX, WREG_RBP, WREG_RSP,             /* 13-15: special */
        WREG_RSP, WREG_RBP, WREG_R10, WREG_RAX,  /* aliases: SP,FP,LR,XZR */
    };
    if (r < 16) return map[r];
    if (r == CG_REG_SP) return WREG_RSP;
    if (r == CG_REG_FP) return WREG_RBP;
    if (r == CG_REG_XZR) return WREG_RAX;
    return WREG_RAX;
}

/* -- Condition mapping: CGCC → Wx86CC ----------------------------- */
static Wx86CC cg_to_x86cc(CGCC cc) {
    /* Map neutral CC to x86 CC. Note: x86 uses different encoding for
     * unsigned (B/AE/BE/A) vs signed (L/GE/LE/G) comparisons. We map
     * the neutral codes to signed x86 codes by default. */
    switch (cc) {
        case CG_CC_EQ:  return WCC_E;
        case CG_CC_NE:  return WCC_NE;
        case CG_CC_CS:  return WCC_B;  /* carry set = below */
        case CG_CC_CC:  return WCC_AE; /* carry clear = above or equal */
        case CG_CC_MI:  return WCC_S;  /* minus = sign set */
        case CG_CC_PL:  return WCC_NS; /* plus = sign clear */
        case CG_CC_VS:  return WCC_O;  /* overflow */
        case CG_CC_VC:  return WCC_NO; /* no overflow */
        case CG_CC_HI:  return WCC_A;  /* unsigned higher */
        case CG_CC_LS:  return WCC_BE; /* unsigned lower or same */
        case CG_CC_GE:  return WCC_GE; /* signed >= */
        case CG_CC_LT:  return WCC_L;  /* signed < */
        case CG_CC_GT:  return WCC_G;  /* signed > */
        case CG_CC_LE:  return WCC_LE; /* signed <= */
        case CG_CC_AL:  return WCC_O;  /* always (use O as placeholder, shouldn't branch) */
        default:        return WCC_E;
    }
}

/* -- Encoder wrapper ------------------------------------------------ */
typedef struct {
    Wx86Enc enc;
} X86Encoder;

static X86Encoder *x86_enc(CGEncoder *e) { return (X86Encoder *)e; }

/* -- VTable implementations ---------------------------------------- */

static void x86_emit_byte(CGEncoder *e, uint8_t b) {
    wx86_emit_byte(&x86_enc(e)->enc, b);
}

static void x86_emit_word32(CGEncoder *e, uint32_t w) {
    wx86_emit_dword(&x86_enc(e)->enc, w);
}

static void x86_emit_word64(CGEncoder *e, uint64_t q) {
    for (int i = 0; i < 8; i++)
        wx86_emit_byte(&x86_enc(e)->enc, (uint8_t)((q >> (i*8)) & 0xFF));
}

static size_t x86_pos(const CGEncoder *e) {
    return x86_enc((CGEncoder *)e)->enc.pos;
}

static const uint8_t *x86_buffer(const CGEncoder *e) {
    return x86_enc((CGEncoder *)e)->enc.buf;
}

static void x86_add_imm(CGEncoder *e, CGReg rd, CGReg rn, uint32_t imm) {
    /* Use LEA for add-immediate: LEA rd, [rn + imm] */
    if (rd == rn) {
        /* add rd, imm */
        if (imm <= 127) {
            wx86_emit_byte(&x86_enc(e)->enc, 0x48);
            wx86_emit_byte(&x86_enc(e)->enc, 0x83);
            wx86_emit_byte(&x86_enc(e)->enc, 0xC0 | (cg_to_x86(rd) & 7));
            wx86_emit_byte(&x86_enc(e)->enc, (uint8_t)imm);
        } else {
            wx86_emit_byte(&x86_enc(e)->enc, 0x48);
            wx86_emit_byte(&x86_enc(e)->enc, 0x81);
            wx86_emit_byte(&x86_enc(e)->enc, 0xC0 | (cg_to_x86(rd) & 7));
            wx86_emit_dword(&x86_enc(e)->enc, imm);
        }
    } else {
        /* lea rd, [rn + imm] */
        Wx86Reg dr = cg_to_x86(rd), sr = cg_to_x86(rn);
        if (imm == 0) {
            wx86_mov_reg_reg(&x86_enc(e)->enc, dr, sr);
        } else if (dr == WREG_RAX) {
            /* lea rax, [sr + imm] */
            wx86_emit_byte(&x86_enc(e)->enc, 0x48);
            wx86_emit_byte(&x86_enc(e)->enc, 0x8D);
            wx86_emit_byte(&x86_enc(e)->enc, 0x80 | (sr & 7));
            wx86_emit_dword(&x86_enc(e)->enc, imm);
        } else {
            /* General case: mov + add */
            wx86_mov_reg_reg(&x86_enc(e)->enc, dr, sr);
            if (imm > 0) {
                wx86_emit_byte(&x86_enc(e)->enc, 0x48);
                wx86_emit_byte(&x86_enc(e)->enc, 0x83);
                wx86_emit_byte(&x86_enc(e)->enc, 0xC0 | (dr & 7));
                wx86_emit_byte(&x86_enc(e)->enc, (uint8_t)(imm & 0xFF));
            }
        }
    }
}

static void x86_sub_imm(CGEncoder *e, CGReg rd, CGReg rn, uint32_t imm) {
    Wx86Reg dr = cg_to_x86(rd);
    if (imm <= 128) {
        wx86_emit_byte(&x86_enc(e)->enc, 0x48);
        wx86_emit_byte(&x86_enc(e)->enc, 0x83);
        wx86_emit_byte(&x86_enc(e)->enc, 0xE8 | (dr & 7));
        wx86_emit_byte(&x86_enc(e)->enc, (uint8_t)imm);
    } else {
        wx86_emit_byte(&x86_enc(e)->enc, 0x48);
        wx86_emit_byte(&x86_enc(e)->enc, 0x81);
        wx86_emit_byte(&x86_enc(e)->enc, 0xE8 | (dr & 7));
        wx86_emit_dword(&x86_enc(e)->enc, imm);
    }
}

static void x86_add_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    Wx86Reg dr = cg_to_x86(rd), sr = cg_to_x86(rn), mr = cg_to_x86(rm);
    if (dr == sr) {
        wx86_add_reg_reg(&x86_enc(e)->enc, dr, mr);  /* add rd, rm */
    } else if (dr == mr) {
        wx86_add_reg_reg(&x86_enc(e)->enc, dr, sr);  /* add rd, rn */
    } else {
        wx86_mov_reg_reg(&x86_enc(e)->enc, dr, sr);  /* mov rd, rn */
        wx86_add_reg_reg(&x86_enc(e)->enc, dr, mr);  /* add rd, rm */
    }
}

static void x86_sub_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    Wx86Reg dr = cg_to_x86(rd), sr = cg_to_x86(rn), mr = cg_to_x86(rm);
    if (dr == sr) {
        wx86_sub_reg_reg(&x86_enc(e)->enc, dr, mr);  /* sub rd, rm */
    } else {
        wx86_mov_reg_reg(&x86_enc(e)->enc, dr, sr);  /* mov rd, rn */
        wx86_sub_reg_reg(&x86_enc(e)->enc, dr, mr);  /* sub rd, rm */
    }
}

static void x86_mul_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    Wx86Reg dr = cg_to_x86(rd), sr = cg_to_x86(rn), mr = cg_to_x86(rm);
    if (dr != sr) wx86_mov_reg_reg(&x86_enc(e)->enc, dr, sr);
    wx86_imul_reg_reg(&x86_enc(e)->enc, dr, mr);
}

static void x86_div_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    /* cqo; idiv rm — quotient in RAX, remainder in RDX */
    (void)rd; (void)rn;
    wx86_cqo(&x86_enc(e)->enc);
    wx86_idiv_reg(&x86_enc(e)->enc, cg_to_x86(rm));
}

static void x86_mod_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    /* cqo; idiv rm — remainder in RDX, move to rd */
    (void)rn;
    wx86_cqo(&x86_enc(e)->enc);
    wx86_idiv_reg(&x86_enc(e)->enc, cg_to_x86(rm));
    /* Remainder is in RDX. Move to rd if different. */
    /* RDX maps to CG_REG_3 in our neutral register model */
    if (rd != CG_REG_3) {
        wx86_mov_reg_reg(&x86_enc(e)->enc, cg_to_x86(rd), WREG_RDX);
    }
}

static void x86_and_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    Wx86Reg dr = cg_to_x86(rd), sr = cg_to_x86(rn), mr = cg_to_x86(rm);
    if (dr == sr) wx86_and_reg_reg(&x86_enc(e)->enc, dr, mr);
    else if (dr == mr) wx86_and_reg_reg(&x86_enc(e)->enc, dr, sr);
    else { wx86_mov_reg_reg(&x86_enc(e)->enc, dr, sr); wx86_and_reg_reg(&x86_enc(e)->enc, dr, mr); }
}

static void x86_orr_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    Wx86Reg dr = cg_to_x86(rd), sr = cg_to_x86(rn), mr = cg_to_x86(rm);
    if (dr == sr) wx86_or_reg_reg(&x86_enc(e)->enc, dr, mr);
    else if (dr == mr) wx86_or_reg_reg(&x86_enc(e)->enc, dr, sr);
    else { wx86_mov_reg_reg(&x86_enc(e)->enc, dr, sr); wx86_or_reg_reg(&x86_enc(e)->enc, dr, mr); }
}

static void x86_eor_reg(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm) {
    Wx86Reg dr = cg_to_x86(rd), sr = cg_to_x86(rn), mr = cg_to_x86(rm);
    if (dr == sr) wx86_xor_reg_reg(&x86_enc(e)->enc, dr, mr);
    else if (dr == mr) wx86_xor_reg_reg(&x86_enc(e)->enc, dr, sr);
    else { wx86_mov_reg_reg(&x86_enc(e)->enc, dr, sr); wx86_xor_reg_reg(&x86_enc(e)->enc, dr, mr); }
}

static void x86_lsl_imm(CGEncoder *e, CGReg rd, CGReg rn, uint8_t shift) {
    wx86_shl_reg_imm8(&x86_enc(e)->enc, cg_to_x86(rd), shift);
}

static void x86_lsr_imm(CGEncoder *e, CGReg rd, CGReg rn, uint8_t shift) {
    wx86_shr_reg_imm8(&x86_enc(e)->enc, cg_to_x86(rd), shift);
}

static void x86_asr_imm(CGEncoder *e, CGReg rd, CGReg rn, uint8_t shift) {
    wx86_sar_reg_imm8(&x86_enc(e)->enc, cg_to_x86(rd), shift);
}

static void x86_mov_imm(CGEncoder *e, CGReg rd, int64_t imm) {
    wx86_mov_reg_imm64(&x86_enc(e)->enc, cg_to_x86(rd), imm);
}

static void x86_mov_reg(CGEncoder *e, CGReg rd, CGReg rn) {
    wx86_mov_reg_reg(&x86_enc(e)->enc, cg_to_x86(rd), cg_to_x86(rn));
}

static void x86_load(CGEncoder *e, CGReg rt, CGReg base, int32_t offset) {
    wx86_mov_reg_mem(&x86_enc(e)->enc, cg_to_x86(rt), cg_to_x86(base), offset);
}

static void x86_store(CGEncoder *e, CGReg rt, CGReg base, int32_t offset) {
    wx86_mov_mem_reg(&x86_enc(e)->enc, cg_to_x86(base), offset, cg_to_x86(rt));
}

static void x86_cmp_imm(CGEncoder *e, CGReg rn, uint32_t imm) {
    Wx86Reg r = cg_to_x86(rn);
    if (imm <= 127) {
        wx86_emit_byte(&x86_enc(e)->enc, 0x48);
        wx86_emit_byte(&x86_enc(e)->enc, 0x83);
        wx86_emit_byte(&x86_enc(e)->enc, 0xF8 | (r & 7));
        wx86_emit_byte(&x86_enc(e)->enc, (uint8_t)imm);
    } else {
        wx86_emit_byte(&x86_enc(e)->enc, 0x48);
        wx86_emit_byte(&x86_enc(e)->enc, 0x81);
        wx86_emit_byte(&x86_enc(e)->enc, 0xF8 | (r & 7));
        wx86_emit_dword(&x86_enc(e)->enc, imm);
    }
}

static void x86_cmp_reg(CGEncoder *e, CGReg rn, CGReg rm) {
    wx86_cmp_reg_reg(&x86_enc(e)->enc, cg_to_x86(rn), cg_to_x86(rm));
}

static void x86_cset(CGEncoder *e, CGReg rd, CGCC cc) {
    Wx86Reg dr = cg_to_x86(rd);
    wx86_setcc_r8(&x86_enc(e)->enc, cg_to_x86cc(cc), dr);
    /* movzx r32, r8 — rex prefix depends on reg */
    if (dr >= 8) {
        wx86_emit_byte(&x86_enc(e)->enc, 0x41);
    }
    wx86_emit_byte(&x86_enc(e)->enc, 0x0F);
    wx86_emit_byte(&x86_enc(e)->enc, 0xB6);
    wx86_emit_byte(&x86_enc(e)->enc, 0xC0 | ((dr & 7) << 3) | (dr & 7));
}

static void x86_b_uncond(CGEncoder *e, int32_t offset) {
    wx86_jmp_rel32(&x86_enc(e)->enc);
}

static void x86_b_cond(CGEncoder *e, int32_t offset, CGCC cc) {
    wx86_jcc_rel32(&x86_enc(e)->enc, cg_to_x86cc(cc));
}

static void x86_b_reg(CGEncoder *e, CGReg rn) {
    wx86_jmp_reg(&x86_enc(e)->enc, cg_to_x86(rn));
}

static void x86_ret(CGEncoder *e) {
    wx86_ret(&x86_enc(e)->enc);
}

static size_t x86_branch_pos(CGEncoder *e) {
    return wx86_jcc_rel32_pos(&x86_enc(e)->enc);
}

static void x86_patch_branch(CGEncoder *e, size_t pos, size_t target) {
    wx86_patch_rel32(&x86_enc(e)->enc, pos, target);
}

static void x86_noop(CGEncoder *e) { (void)e; }
static void x86_do_block(CGEncoder *e) { (void)e; }
static void x86_do_block_i64(CGEncoder *e) { (void)e; }
static void x86_do_loop(CGEncoder *e) { (void)e; }
static void x86_do_if(CGEncoder *e) { (void)e; }
static void x86_do_else(CGEncoder *e) { (void)e; }
static void x86_do_end(CGEncoder *e) { (void)e; }
static void x86_br(CGEncoder *e, uint32_t l) { (void)e; (void)l; }
static void x86_br_if(CGEncoder *e, uint32_t l) { (void)e; (void)l; }
static void x86_push(CGEncoder *e, CGReg rt) {
    wx86_push_reg(&x86_enc(e)->enc, cg_to_x86(rt));
}

static void x86_pop(CGEncoder *e, CGReg rt) {
    wx86_pop_reg(&x86_enc(e)->enc, cg_to_x86(rt));
}

static void x86_prologue(CGEncoder *e, int n_args, int stack_slots) {
    /* push rbp; mov rbp, rsp; sub rsp, stack_slots*8 */
    wx86_push_reg(&x86_enc(e)->enc, WREG_RBP);
    wx86_mov_reg_reg(&x86_enc(e)->enc, WREG_RBP, WREG_RSP);
    if (stack_slots > 0) {
        uint32_t sz = stack_slots * 8;
        if (sz <= 128) {
            wx86_emit_byte(&x86_enc(e)->enc, 0x48);
            wx86_emit_byte(&x86_enc(e)->enc, 0x83);
            wx86_emit_byte(&x86_enc(e)->enc, 0xEC);
            wx86_emit_byte(&x86_enc(e)->enc, (uint8_t)sz);
        } else {
            wx86_emit_byte(&x86_enc(e)->enc, 0x48);
            wx86_emit_byte(&x86_enc(e)->enc, 0x81);
            wx86_emit_byte(&x86_enc(e)->enc, 0xEC);
            wx86_emit_dword(&x86_enc(e)->enc, sz);
        }
    }
}

static void x86_epilogue(CGEncoder *e, int stack_slots) {
    /* mov rsp, rbp; pop rbp; ret */
    wx86_mov_reg_reg(&x86_enc(e)->enc, WREG_RSP, WREG_RBP);
    wx86_pop_reg(&x86_enc(e)->enc, WREG_RBP);
    wx86_ret(&x86_enc(e)->enc);
}

/* -- VTable ------------------------------------------------------- */
static void x86_cmp_reg_cc(CGEncoder *e, CGReg rn, CGReg rm, CGCC cc);

static const CodeGenVTable x86_vtable = {
    .name = "x86-64",
    .buffer = x86_buffer,
    .emit_byte = x86_emit_byte,
    .emit_word32 = x86_emit_word32,
    .emit_word64 = x86_emit_word64,
    .pos = x86_pos,
    .add_imm = x86_add_imm,
    .sub_imm = x86_sub_imm,
    .add_reg = x86_add_reg,
    .sub_reg = x86_sub_reg,
    .mul_reg = x86_mul_reg,
    .div_reg = x86_div_reg,
    .mod_reg = x86_mod_reg,
    .and_reg = x86_and_reg,
    .orr_reg = x86_orr_reg,
    .eor_reg = x86_eor_reg,
    .lsl_imm = x86_lsl_imm,
    .lsr_imm = x86_lsr_imm,
    .asr_imm = x86_asr_imm,
    .mov_imm = x86_mov_imm,
    .mov_reg = x86_mov_reg,
    .load = x86_load,
    .store = x86_store,
    .cmp_imm = x86_cmp_imm,
    .cmp_reg = x86_cmp_reg,
    .cmp_reg_cc = x86_cmp_reg_cc,
    .cset = x86_cset,
    .b_uncond = x86_b_uncond,
    .b_cond = x86_b_cond,
    .b_reg = x86_b_reg,
    .ret = x86_ret,
    .branch_pos = x86_branch_pos,
    .patch_branch = x86_patch_branch,
    .push = x86_push,
    .pop = x86_pop,
    .drop = x86_noop,
    .do_block = x86_do_block,
    .do_block_i64 = x86_do_block_i64,
    .do_loop = x86_do_loop,
    .do_if = x86_do_if,
    .do_else = x86_do_else,
    .do_end = x86_do_end,
    .br = x86_br,
    .br_if = x86_br_if,
    .prologue = x86_prologue,
    .epilogue = x86_epilogue,
};

/* -- Factory ------------------------------------------------------ */
CodeGen *cg_create_x86(void) {
    CodeGen *cg = (CodeGen *)calloc(1, sizeof(CodeGen));
    X86Encoder *enc = (X86Encoder *)calloc(1, sizeof(X86Encoder));
    if (!cg || !enc) { free(cg); free(enc); return NULL; }
    cg->vt = &x86_vtable;
    cg->enc = (CGEncoder *)enc;
    cg->backend = 0;
    wx86_enc_init_dynamic(&enc->enc, 4096);
    return cg;
}

void cg_destroy(CodeGen *cg) {
    if (!cg) return;
    if (cg->enc && cg->backend == 0) {
        X86Encoder *enc = (X86Encoder *)cg->enc;
        wx86_enc_free(&enc->enc);
    }
    free(cg->enc);
    free(cg);
}


static void x86_cmp_reg_cc(CGEncoder *e, CGReg rn, CGReg rm, CGCC cc) {
    x86_cmp_reg(e, rn, rm);
    x86_cset(e, rn, cc);
}
