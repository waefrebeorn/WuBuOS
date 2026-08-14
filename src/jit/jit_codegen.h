/*
 * jit_codegen.h — Abstract code generation interface.
 *
 * Unifies x86-64 (Wx86Enc) and ARM64 (WArm64Enc) behind a common API
 * so the minic compiler can target either backend.
 *
 * Design: function pointer vtable + opaque context. The minic compiler
 * calls cg_* functions which dispatch to the active backend.
 */
#ifndef JIT_CODEGEN_H
#define JIT_CODEGEN_H

#include <stdint.h>
#include <stddef.h>
#include "wubu_x86.h"

/* Forward declarations of x86 helpers defined in jit_minic.c */
void wx86_setcc_r8(Wx86Enc *e, Wx86CC cc, Wx86Reg dst);

#ifdef __cplusplus
extern "C" {
#endif

/* -- Opaque types ------------------------------------------------- */
typedef struct CodeGen CodeGen;
typedef struct CGEncoder CGEncoder;

/* -- Register enumeration (backend-neutral) ------------------------ */
typedef enum {
    CG_REG_0 = 0, CG_REG_1, CG_REG_2, CG_REG_3,
    CG_REG_4, CG_REG_5, CG_REG_6, CG_REG_7,
    CG_REG_8, CG_REG_9, CG_REG_10, CG_REG_11,
    CG_REG_12, CG_REG_13, CG_REG_14, CG_REG_15,
    /* Aliases */
    CG_REG_SP = 16,
    CG_REG_FP = 17,
    CG_REG_LR = 18,
    CG_REG_XZR = 19,
} CGReg;

/* -- Condition codes (neutral) ------------------------------------ */
typedef enum {
    CG_CC_EQ = 0, CG_CC_NE, CG_CC_CS, CG_CC_CC,
    CG_CC_MI, CG_CC_PL, CG_CC_VS, CG_CC_VC,
    CG_CC_HI, CG_CC_LS, CG_CC_GE, CG_CC_LT,
    CG_CC_GT, CG_CC_LE, CG_CC_AL,
} CGCC;

/* -- Code generator vtable ---------------------------------------- */
typedef struct {
    /* Backend identity */
    const char *name;  /* "x86-64" or "arm64" */

    /* Buffer access */
    const uint8_t *(*buffer)(const CGEncoder *e);
    size_t (*pos)(const CGEncoder *e);

    /* Emit helpers (optional, for advanced use) */
    void  (*emit_byte)(CGEncoder *e, uint8_t b);
    void  (*emit_word32)(CGEncoder *e, uint32_t w);
    void  (*emit_word64)(CGEncoder *e, uint64_t q);

    /* Data processing */
    void (*add_imm)(CGEncoder *e, CGReg rd, CGReg rn, uint32_t imm);
    void (*sub_imm)(CGEncoder *e, CGReg rd, CGReg rn, uint32_t imm);
    void (*add_reg)(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm);
    void (*sub_reg)(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm);
    void (*mul_reg)(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm);
    void (*div_reg)(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm);  /* signed */

    /* Bitwise */
    void (*and_reg)(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm);
    void (*orr_reg)(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm);
    void (*eor_reg)(CGEncoder *e, CGReg rd, CGReg rn, CGReg rm);

    /* Shift */
    void (*lsl_imm)(CGEncoder *e, CGReg rd, CGReg rn, uint8_t shift);
    void (*lsr_imm)(CGEncoder *e, CGReg rd, CGReg rn, uint8_t shift);
    void (*asr_imm)(CGEncoder *e, CGReg rd, CGReg rn, uint8_t shift);

    /* Move */
    void (*mov_imm)(CGEncoder *e, CGReg rd, int64_t imm);  /* handles wide immediates */
    void (*mov_reg)(CGEncoder *e, CGReg rd, CGReg rn);

    /* Load / Store */
    void (*load)(CGEncoder *e, CGReg rt, CGReg base, int32_t offset);
    void (*store)(CGEncoder *e, CGReg rt, CGReg base, int32_t offset);

    /* Comparison */
    void (*cmp_imm)(CGEncoder *e, CGReg rn, uint32_t imm);
    void (*cmp_reg)(CGEncoder *e, CGReg rn, CGReg rm);
    void (*cset)(CGEncoder *e, CGReg rd, CGCC cc);

    /* Branch */
    void (*b_uncond)(CGEncoder *e, int32_t offset);  /* backend-specific patching */
    void (*b_cond)(CGEncoder *e, int32_t offset, CGCC cc);
    void (*b_reg)(CGEncoder *e, CGReg rn);  /* indirect jump */
    void (*ret)(CGEncoder *e);

    /* Branch fixups */
    size_t (*branch_pos)(CGEncoder *e);
    void   (*patch_branch)(CGEncoder *e, size_t pos, size_t target);

    /* Stack */
    void (*push)(CGEncoder *e, CGReg rt);
    void (*pop)(CGEncoder *e, CGReg rt);

    /* Frame setup / teardown */
    void (*prologue)(CGEncoder *e, int n_args, int stack_slots);
    void (*epilogue)(CGEncoder *e, int stack_slots);
} CodeGenVTable;

/* -- Code generator state ----------------------------------------- */
struct CodeGen {
    const CodeGenVTable *vt;
    CGEncoder *enc;
    /* Backend-neutral state */
    int n_args;
    int stack_depth;
    int backend;  /* 0 = x86-64, 1 = arm64 */
};

/* -- Factory functions -------------------------------------------- */
CodeGen *cg_create_x86(void);
CodeGen *cg_create_arm64(void);
void     cg_destroy(CodeGen *cg);

/* -- Dispatch macros (inline wrappers) ---------------------------- */
static inline void cg_add_imm(CodeGen *cg, CGReg rd, CGReg rn, uint32_t imm) {
    cg->vt->add_imm(cg->enc, rd, rn, imm);
}
static inline void cg_sub_imm(CodeGen *cg, CGReg rd, CGReg rn, uint32_t imm) {
    cg->vt->sub_imm(cg->enc, rd, rn, imm);
}
static inline void cg_add_reg(CodeGen *cg, CGReg rd, CGReg rn, CGReg rm) {
    cg->vt->add_reg(cg->enc, rd, rn, rm);
}
static inline void cg_sub_reg(CodeGen *cg, CGReg rd, CGReg rn, CGReg rm) {
    cg->vt->sub_reg(cg->enc, rd, rn, rm);
}
static inline void cg_mul_reg(CodeGen *cg, CGReg rd, CGReg rn, CGReg rm) {
    cg->vt->mul_reg(cg->enc, rd, rn, rm);
}
static inline void cg_div_reg(CodeGen *cg, CGReg rd, CGReg rn, CGReg rm) {
    cg->vt->div_reg(cg->enc, rd, rn, rm);
}
static inline void cg_and_reg(CodeGen *cg, CGReg rd, CGReg rn, CGReg rm) {
    cg->vt->and_reg(cg->enc, rd, rn, rm);
}
static inline void cg_orr_reg(CodeGen *cg, CGReg rd, CGReg rn, CGReg rm) {
    cg->vt->orr_reg(cg->enc, rd, rn, rm);
}
static inline void cg_eor_reg(CodeGen *cg, CGReg rd, CGReg rn, CGReg rm) {
    cg->vt->eor_reg(cg->enc, rd, rn, rm);
}
static inline void cg_lsl_imm(CodeGen *cg, CGReg rd, CGReg rn, uint8_t s) {
    cg->vt->lsl_imm(cg->enc, rd, rn, s);
}
static inline void cg_lsr_imm(CodeGen *cg, CGReg rd, CGReg rn, uint8_t s) {
    cg->vt->lsr_imm(cg->enc, rd, rn, s);
}
static inline void cg_asr_imm(CodeGen *cg, CGReg rd, CGReg rn, uint8_t s) {
    cg->vt->asr_imm(cg->enc, rd, rn, s);
}
static inline void cg_mov_imm(CodeGen *cg, CGReg rd, int64_t imm) {
    cg->vt->mov_imm(cg->enc, rd, imm);
}
static inline void cg_mov_reg(CodeGen *cg, CGReg rd, CGReg rn) {
    cg->vt->mov_reg(cg->enc, rd, rn);
}
static inline void cg_load(CodeGen *cg, CGReg rt, CGReg base, int32_t off) {
    cg->vt->load(cg->enc, rt, base, off);
}
static inline void cg_store(CodeGen *cg, CGReg rt, CGReg base, int32_t off) {
    cg->vt->store(cg->enc, rt, base, off);
}
static inline void cg_cmp_imm(CodeGen *cg, CGReg rn, uint32_t imm) {
    cg->vt->cmp_imm(cg->enc, rn, imm);
}
static inline void cg_cmp_reg(CodeGen *cg, CGReg rn, CGReg rm) {
    cg->vt->cmp_reg(cg->enc, rn, rm);
}
static inline void cg_cset(CodeGen *cg, CGReg rd, CGCC cc) {
    cg->vt->cset(cg->enc, rd, cc);
}
static inline void cg_b_uncond(CodeGen *cg, int32_t off) {
    cg->vt->b_uncond(cg->enc, off);
}
static inline void cg_b_cond(CodeGen *cg, int32_t off, CGCC cc) {
    cg->vt->b_cond(cg->enc, off, cc);
}
static inline void cg_b_reg(CodeGen *cg, CGReg rn) {
    cg->vt->b_reg(cg->enc, rn);
}
static inline void cg_ret(CodeGen *cg) {
    cg->vt->ret(cg->enc);
}
static inline size_t cg_branch_pos(CodeGen *cg) {
    return cg->vt->branch_pos(cg->enc);
}
static inline void cg_patch_branch(CodeGen *cg, size_t pos, size_t target) {
    cg->vt->patch_branch(cg->enc, pos, target);
}
static inline void cg_push(CodeGen *cg, CGReg rt) {
    cg->vt->push(cg->enc, rt);
}
static inline void cg_pop(CodeGen *cg, CGReg rt) {
    cg->vt->pop(cg->enc, rt);
}
static inline size_t cg_pos(CodeGen *cg) {
    return cg->vt->pos(cg->enc);
}
static inline const uint8_t *cg_buffer(CodeGen *cg) {
    return cg->vt->buffer(cg->enc);
}

#ifdef __cplusplus
}
#endif

#endif /* JIT_CODEGEN_H */
