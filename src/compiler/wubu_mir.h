/*
 * wubu_mir.h -- the WuBuOS mid-level IR (the hourglass neck).
 *
 * The compiler doctrine (compiler-doctrine.md): the frontend emits ONE
 * IR, and every ISA is a DRIVER that consumes it. This is the neck of
 * the hourglass: AST (any language) -> MIR -> x86-64 / RISC-V / ARM64
 * / PTX drivers. It is how the compiler unlocks ALL hardware: one
 * frontend, N backends, each a driver in the driver space.
 *
 * MIR is minimal 3-address code with VIRTUAL registers (u32 indices,
 * 0 = the return value register). Because operands are virtual, the
 * register-clobber bug family that plagued the direct x86 codegen
 * (the rdi-clobber binops) is IMPOSSIBLE here — each driver does its
 * own register assignment.
 *
 * Comparisons produce 0/1 in a virtual register (no flags registers in
 * the IR), and branches test a virtual register (MIR_JZ jumps when
 * the vr is 0). Labels are symbolic (MIR_LABEL); drivers resolve them.
 *
 * C11, self-contained.
 */
#ifndef WUBU_MIR_H
#define WUBU_MIR_H

#include <stddef.h>
#include <stdint.h>

/* a virtual register index. 0 is reserved as the "value" register the
 * program returns (MIR_RET reads it). */
typedef uint32_t wubu_vr_t;

typedef enum {
    MIR_CONST = 1,     /* dst = imm */
    MIR_ADD,           /* dst = a + b */
    MIR_SUB,           /* dst = a - b */
    MIR_MUL,           /* dst = a * b */
    MIR_DIV,           /* dst = a / b */
    MIR_MOD,           /* dst = a % b */
    MIR_AND,           /* dst = a & b (bitwise) */
    MIR_OR,            /* dst = a | b (bitwise) */
    MIR_XOR,           /* dst = a ^ b */
    MIR_SHL,           /* dst = a << b */
    MIR_SHR,           /* dst = a >> b */
    MIR_NEG,           /* dst = -a */
    MIR_NOT,           /* dst = ~a */
    MIR_EQ,            /* dst = (a == b) ? 1 : 0 */
    MIR_NE,            /* dst = (a != b) ? 1 : 0 */
    MIR_LT,            /* dst = (a < b) ? 1 : 0  (signed) */
    MIR_LE,            /* dst = (a <= b) ? 1 : 0 (signed) */
    MIR_GT,            /* dst = (a > b) ? 1 : 0  (signed) */
    MIR_GE,            /* dst = (a >= b) ? 1 : 0 (signed) */
    MIR_MOV,           /* dst = src */
    MIR_JMP,           /* pc = label */
    MIR_JZ,            /* if (src == 0) pc = label */
    MIR_LABEL,         /* a jump target (no code) */
    MIR_RET            /* return vr 0 */
} wubu_mir_op_t;

typedef struct {
    wubu_mir_op_t op;
    wubu_vr_t dst;
    wubu_vr_t a;
    wubu_vr_t b;
    int64_t imm;
    uint32_t label;              /* for JMP/JZ: the target label id */
} wubu_mir_instr_t;

typedef struct {
    wubu_mir_instr_t *ins;       /* dynamic array */
    size_t n, cap;
    uint32_t n_labels;           /* next label id */
    uint32_t n_args;             /* number of function arguments (v1..n_args) */
} wubu_mir_prog_t;

/* O1: init a program (zeroed = empty) */
void wubu_mir_init(wubu_mir_prog_t *p);
void wubu_mir_free(wubu_mir_prog_t *p);

/* O2: append instructions (returns the new vr for dst ops) */
wubu_vr_t wubu_mir_const(wubu_mir_prog_t *p, int64_t imm);
wubu_vr_t wubu_mir_binop(wubu_mir_prog_t *p, wubu_mir_op_t op,
                         wubu_vr_t a, wubu_vr_t b);
wubu_vr_t wubu_mir_unop(wubu_mir_prog_t *p, wubu_mir_op_t op, wubu_vr_t a);
wubu_vr_t wubu_mir_mov(wubu_mir_prog_t *p, wubu_vr_t a);
/* mov INTO a pre-chosen dst (phi-merge: both arms write the same vr) */
wubu_vr_t wubu_mir_mov_to(wubu_mir_prog_t *p, wubu_vr_t dst, wubu_vr_t a);
uint32_t wubu_mir_new_label(wubu_mir_prog_t *p);
void wubu_mir_jmp(wubu_mir_prog_t *p, uint32_t label);
void wubu_mir_jz(wubu_mir_prog_t *p, wubu_vr_t cond, uint32_t label);
void wubu_mir_place_label(wubu_mir_prog_t *p, uint32_t label);
void wubu_mir_ret(wubu_mir_prog_t *p, wubu_vr_t v);

/* Set the number of function arguments (v1..n_args get pre-assigned to arg regs) */
void wubu_mir_set_n_args(wubu_mir_prog_t *p, uint32_t n_args);

/* O3: dump the program (the hourglass neck, visible) */
void wubu_mir_dump(const wubu_mir_prog_t *p);

#endif /* WUBU_MIR_H */
