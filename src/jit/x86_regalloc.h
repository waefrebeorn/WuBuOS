/*
 * x86_regalloc.h  --  WuBuOS x86-64 Mini Register Allocator
 *
 * Linear-scan register allocator for JIT compilation.
 * Manages SysV AMD64 ABI register conventions:
 *   Caller-saved (scratch): rax, rcx, rdx, rsi, rdi, r8, r9, r10, r11
 *   Callee-saved (preserved): rbx, rbp, r12-r15
 *   Special: rsp (stack), rbp (frame)
 *
 * Allocates from scratch pool first, spills to callee-saved when needed.
 */

#ifndef X86_REGALLOC_H
#define X86_REGALLOC_H

#include "wubu_x86.h"
#include <stdbool.h>
#include <stdint.h>

/* Maximum virtual registers tracked */
#define XRA_MAX_VREGS 32

typedef enum {
    XRA_FREE = 0,
    XRA_ALLOCED,   /* Currently allocated to a vreg */
    XRA_SPILLED,   /* Spilled to stack */
    XRA_RESERVED   /* Reserved (rsp, rbp, return, etc.) */
} XRARegState;

typedef struct {
    Wx86Reg  hw_reg;        /* Physical register */
    int      vreg;          /* Virtual register mapped here, or -1 */
    XRARegState state;      /* Current state */
    bool     callee_saved;  /* True if we must save/restore */
    int      spill_slot;    /* Stack offset for spill, or -1 */
} XRAPhysReg;

typedef struct {
    XRAPhysReg regs[16];   /* Physical register states */
    int        next_spill; /* Next available spill slot (8-byte slots) */
    int        n_callee_saved; /* Count of callee-saved regs used */
    int        frame_size;  /* Total stack frame size in bytes */
    int        n_args;      /* Number of function arguments */
    Wx86Reg    arg_vregs[6]; /* Which hw reg holds each arg */
} XRARegAlloc;

/* Initialize the register allocator */
void xra_init(XRARegAlloc *ra, int n_args);

/* Allocate a physical register for a virtual register.
 * Returns the physical register, or WREG_NONE if all are in use. */
Wx86Reg xra_alloc(XRARegAlloc *ra, int vreg);

/* Free a physical register */
void xra_free_reg(XRARegAlloc *ra, Wx86Reg hw);

/* Get the physical register for a virtual register, or WREG_NONE */
Wx86Reg xra_get_reg(const XRARegAlloc *ra, int vreg);

/* Mark a register as reserved (cannot be allocated) */
void xra_reserve(XRARegAlloc *ra, Wx86Reg hw);

/* Spill all allocated registers and compute final frame layout.
 * Returns total frame size in bytes (multiple of 16 for ABI alignment). */
int xra_finalize(XRARegAlloc *ra);

/* Get the set of callee-saved registers we need to push/pull.
 * Fills 'out' array with registers, returns count. */
int xra_callee_saved_list(const XRARegAlloc *ra, Wx86Reg *out, int max);

/* Prologue: push callee-saved regs, sub rsp */
void xra_emit_prologue(XRARegAlloc *ra, Wx86Enc *e);

/* Epilogue: add rsp, pop callee-saved regs, ret */
void xra_emit_epilogue(XRARegAlloc *ra, Wx86Enc *e);

/* Load function arguments from ABI registers into allocated vregs */
void xra_emit_load_args(XRARegAlloc *ra, Wx86Enc *e);

/* Store return value (rax) and restore callee-saved */
void xra_emit_return(XRARegAlloc *ra, Wx86Enc *e);

#endif /* X86_REGALLOC_H */
