/*
 * wubu_x86.h  --  WuBuOS x86-64 Machine Code Encoder
 *
 * Pure C, zero-dependency x86-64 instruction emitter.
 * Supports: MOV, ADD, SUB, IMUL, XOR, CMP, TEST, Jcc, JMP,
 *           CALL, PUSH, POP, RET, LEA, SHL, SHR, SAR, NEG, IDIV, CQO
 * Full REX.W prefix, ModRM, SIB encoding.
 * Register names map to System V AMD64 ABI convention.
 */

#ifndef WUBU_X86_H
#define WUBU_X86_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* -- x86-64 Register Encoding ------------------------------------ */

typedef enum {
    WREG_RAX = 0,  WREG_RCX = 1,  WREG_RDX = 2,  WREG_RBX = 3,
    WREG_RSP = 4,  WREG_RBP = 5,  WREG_RSI = 6,  WREG_RDI = 7,
    WREG_R8  = 8,  WREG_R9  = 9,  WREG_R10 = 10, WREG_R11 = 11,
    WREG_R12 = 12, WREG_R13 = 13, WREG_R14 = 14, WREG_R15 = 15,
    WREG_NONE = -1
} Wx86Reg;

/* Condition codes for Jcc / CMOVcc / SETcc */
typedef enum {
    WCC_O   = 0x0,  WCC_NO  = 0x1,
    WCC_B   = 0x2,  WCC_AE  = 0x3,
    WCC_E   = 0x4,  WCC_NE  = 0x5,
    WCC_BE  = 0x6,  WCC_A   = 0x7,
    WCC_S   = 0x8,  WCC_NS  = 0x9,
    WCC_L   = 0xC,  WCC_GE  = 0xD,
    WCC_LE  = 0xE,  WCC_G   = 0xF
} Wx86CC;

/* -- Encoder Buffer ----------------------------------------------- */

typedef struct {
    uint8_t *buf;       /* Output buffer (caller-allocated or internal) */
    size_t   cap;       /* Buffer capacity */
    size_t   pos;       /* Current write position */
    bool     owns_buf;  /* True if encoder allocated buf internally */
} Wx86Enc;

/* Initialize encoder with caller-owned buffer */
void wx86_enc_init(Wx86Enc *e, uint8_t *buf, size_t cap);

/* Initialize encoder with internal dynamic buffer (realloc) */
void wx86_enc_init_dynamic(Wx86Enc *e, size_t initial_cap);

/* Free internal buffer (only if owns_buf) */
void wx86_enc_free(Wx86Enc *e);

/* Reset write position to 0 */
void wx86_enc_reset(Wx86Enc *e);

/* -- Low-Level Emit ----------------------------------------------- */

void wx86_emit_byte(Wx86Enc *e, uint8_t b);
void wx86_emit_word(Wx86Enc *e, uint16_t w);
void wx86_emit_dword(Wx86Enc *e, uint32_t d);
void wx86_emit_qword(Wx86Enc *e, uint64_t q);

/* Patch 4-byte relative offset at position 'patch_pos'
 * Offset = target - (patch_pos + 4), i.e. relative to next instruction */
void wx86_patch_rel32(Wx86Enc *e, size_t patch_pos, size_t target_pos);

/* Patch 1-byte relative offset (short jumps) */
void wx86_patch_rel8(Wx86Enc *e, size_t patch_pos, size_t target_pos);

/* -- REX / ModRM / SIB helpers ------------------------------------ */

/* Compute REX prefix byte for given reg and rm (64-bit operand) */
uint8_t wx86_rex(Wx86Reg reg, Wx86Reg rm, bool w);

/* Emit ModRM byte: mod(2) | reg(3) | rm(3) */
void wx86_emit_modrm(Wx86Enc *e, uint8_t mod, Wx86Reg reg, Wx86Reg rm);

/* Emit SIB byte: scale(2) | index(3) | base(3) */
void wx86_emit_sib(Wx86Enc *e, uint8_t scale, Wx86Reg index, Wx86Reg base);

/* -- Instruction Encoding ----------------------------------------- */

/* MOV reg, imm64 (movabs — 10 bytes: REX.W + B8+rd + imm64) */
int wx86_mov_reg_imm64(Wx86Enc *e, Wx86Reg dst, int64_t imm);

/* MOV reg, imm32 (sign-extended to 64: REX.W + C7 + ModRM + imm32 — 7 bytes) */
int wx86_mov_reg_imm32(Wx86Enc *e, Wx86Reg dst, int32_t imm);

/* MOV reg, reg (REX.W + 89 + ModRM — 3 bytes) */
int wx86_mov_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src);

/* MOV reg, [base + disp] (load from memory) */
int wx86_mov_reg_mem(Wx86Enc *e, Wx86Reg dst, Wx86Reg base, int32_t disp);

/* MOV [base + disp], reg (store to memory) */
int wx86_mov_mem_reg(Wx86Enc *e, Wx86Reg base, int32_t disp, Wx86Reg src);

/* ADD reg, reg (REX.W + 01 + ModRM) */
int wx86_add_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src);

/* ADD reg, imm32 (REX.W + 81 + /0 + imm32) */
int wx86_add_reg_imm32(Wx86Enc *e, Wx86Reg dst, int32_t imm);

/* SUB reg, reg (REX.W + 29 + ModRM) */
int wx86_sub_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src);

/* SUB reg, imm32 (REX.W + 81 + /5 + imm32) */
int wx86_sub_reg_imm32(Wx86Enc *e, Wx86Reg dst, int32_t imm);

/* IMUL reg, reg (REX.W + 0F AF + ModRM — 4 bytes) */
int wx86_imul_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src);

/* XOR reg, reg (REX.W + 31 + ModRM) — also used for reg=0 (zero register) */
int wx86_xor_reg_reg(Wx86Enc *e, Wx86Reg dst, Wx86Reg src);

/* CMP reg, reg (REX.W + 39 + ModRM) */
int wx86_cmp_reg_reg(Wx86Enc *e, Wx86Reg a, Wx86Reg b);

/* CMP reg, imm32 (REX.W + 81 + /7 + imm32) */
int wx86_cmp_reg_imm32(Wx86Enc *e, Wx86Reg dst, int32_t imm);

/* TEST reg, reg (REX.W + 85 + ModRM) */
int wx86_test_reg_reg(Wx86Enc *e, Wx86Reg a, Wx86Reg b);

/* LEA reg, [base + disp] (REX.W + 8D + ModRM) */
int wx86_lea_reg_mem(Wx86Enc *e, Wx86Reg dst, Wx86Reg base, int32_t disp);

/* SHL reg, imm8 (REX.W + C1 + /4 + imm8) */
int wx86_shl_reg_imm8(Wx86Enc *e, Wx86Reg dst, uint8_t count);

/* SHR reg, imm8 (REX.W + C1 + /5 + imm8) — logical */
int wx86_shr_reg_imm8(Wx86Enc *e, Wx86Reg dst, uint8_t count);

/* SAR reg, imm8 (REX.W + C1 + /7 + imm8) — arithmetic */
int wx86_sar_reg_imm8(Wx86Enc *e, Wx86Reg dst, uint8_t count);

/* NEG reg (REX.W + F7 + /3) */
int wx86_neg_reg(Wx86Enc *e, Wx86Reg dst);

/* CQO (sign-extend rax→rdx:rax) — REX.W + 99 */
int wx86_cqo(Wx86Enc *e);

/* IDIV reg (REX.W + F7 + /7) — divides rdx:rax by src */
int wx86_idiv_reg(Wx86Enc *e, Wx86Reg src);

/* -- Control Flow ------------------------------------------------- */

/* RET — C3 */
int wx86_ret(Wx86Enc *e);

/* JMP rel32 (E9 + rel32) — returns position for backpatching */
int wx86_jmp_rel32(Wx86Enc *e);
#define wx86_jmp_rel32_pos(e) ((e)->pos - 4) /* patch position */

/* Jcc rel32 (0F 8x + rel32) — returns position for backpatching */
int wx86_jcc_rel32(Wx86Enc *e, Wx86CC cc);
#define wx86_jcc_rel32_pos(e) ((e)->pos - 4) /* patch position */

/* CALL rel32 (E8 + rel32) */
int wx86_call_rel32(Wx86Enc *e);

/* CALL reg (FF + /2 + ModRM) — indirect call through register */
int wx86_call_reg(Wx86Enc *e, Wx86Reg reg);

/* PUSH reg (50+rd — no REX needed for rax-rdi, + REX.B for r8-r15) */
int wx86_push_reg(Wx86Enc *e, Wx86Reg src);

/* POP reg (58+rd) */
int wx86_pop_reg(Wx86Enc *e, Wx86Reg dst);

/* SUB rsp, imm8 (for stack frame allocation) — REX.W + 83 + /5 + imm8 */
int wx86_sub_rsp_imm8(Wx86Enc *e, uint8_t imm);

/* ADD rsp, imm8 (for stack frame deallocation) — REX.W + 83 + /0 + imm8 */
int wx86_add_rsp_imm8(Wx86Enc *e, uint8_t imm);

/* -- Stack Argument Loading (SysV ABI) ---------------------------- */
/* Push stack args right-to-left for >6 params
 * then load rdi/rsi/rdx/rcx/r8/r9 for first 6 */

/* Load SysV arg registers: 6 args in rdi, rsi, rdx, rcx, r8, r9 */
typedef struct {
    Wx86Reg arg_regs[6];  /* RDI, RSI, RDX, RCX, R8, R9 */
    Wx86Reg ret_reg;      /* RAX */
    Wx86Reg spill_reg;    /* R10 — caller-saved, safe as temp */
} Wx86ABI;

/* Get the standard SysV ABI layout */
Wx86ABI wx86_sysv_abi(void);

/* Get register name string (for disassembly) */
const char *wx86_reg_name(Wx86Reg r);

/* Get condition code name string */
const char *wx86_cc_name(Wx86CC cc);

#endif /* WUBU_X86_H */
