/*
 * x86_regalloc.c  --  WuBuOS x86-64 Mini Register Allocator
 *
 * Linear-scan allocator for JIT-compiled functions.
 * Caller-saved first, callee-saved with spill when exhausted.
 */

#include "x86_regalloc.h"
#include <string.h>

/* Scratch pool (caller-saved, safe to clobber):
 * rax = return, rcx, rdx = arg3, rsi = arg2, rdi = arg1,
 * r8 = arg4, r9 = arg5, r10 = temp, r11 = temp
 * Callee-saved: rbx, r12, r13, r14, r15 (rbp left for frame) */

static const Wx86Reg scratch_pool[] = {
    WREG_R10, WREG_R11,           /* pure scratch — always free */
    WREG_RCX,  WREG_R8, WREG_R9, /* becomes free after args loaded */
    WREG_RSI,  WREG_RDI, WREG_RDX  /* free after args, but rdx used by idiv */
};
#define SCRATCH_COUNT 7

static const Wx86Reg callee_pool[] = {
    WREG_RBX, WREG_R12, WREG_R13, WREG_R14, WREG_R15
};
#define CALLEE_COUNT 5

void xra_init(XRARegAlloc *ra, int n_args) {
    memset(ra, 0, sizeof(XRARegAlloc));
    for (int i = 0; i < 16; i++) {
        ra->regs[i].hw_reg = (Wx86Reg)i;
        ra->regs[i].vreg = -1;
        ra->regs[i].state = XRA_FREE;
        ra->regs[i].callee_saved = false;
        ra->regs[i].spill_slot = -1;
    }
    /* Reserve special registers */
    ra->regs[WREG_RSP].state = XRA_RESERVED;
    ra->regs[WREG_RBP].state = XRA_RESERVED;
    ra->regs[WREG_RAX].state = XRA_RESERVED; /* return value */

    /* Mark callee-saved pool */
    for (int i = 0; i < CALLEE_COUNT; i++)
        ra->regs[callee_pool[i]].callee_saved = true;

    ra->n_args = n_args > 6 ? 6 : n_args;
    ra->next_spill = 0;
    ra->n_callee_saved = 0;
    ra->frame_size = 0;

    /* Pre-assign argument registers */
    Wx86ABI abi = wx86_sysv_abi();
    for (int i = 0; i < ra->n_args; i++) {
        Wx86Reg argreg = abi.arg_regs[i];
        ra->arg_vregs[i] = argreg;
        /* Mark as reserved initially — will be freed after load_args */
    }
}

Wx86Reg xra_alloc(XRARegAlloc *ra, int vreg) {
    if (vreg < 0 || vreg >= XRA_MAX_VREGS) return WREG_NONE;

    /* First: try scratch pool */
    for (int i = 0; i < SCRATCH_COUNT; i++) {
        Wx86Reg hw = scratch_pool[i];
        if (ra->regs[hw].state == XRA_FREE) {
            ra->regs[hw].state = XRA_ALLOCED;
            ra->regs[hw].vreg = vreg;
            return hw;
        }
    }

    /* Second: try callee-saved pool (we'll emit push/pop in prologue/epilogue) */
    for (int i = 0; i < CALLEE_COUNT; i++) {
        Wx86Reg hw = callee_pool[i];
        if (ra->regs[hw].state == XRA_FREE) {
            ra->regs[hw].state = XRA_ALLOCED;
            ra->regs[hw].vreg = vreg;
            ra->n_callee_saved++;
            return hw;
        }
    }

    /* Third: try argument registers that are now free */
    Wx86ABI abi = wx86_sysv_abi();
    for (int i = 0; i < ra->n_args; i++) {
        Wx86Reg hw = abi.arg_regs[i];
        if (ra->regs[hw].state == XRA_FREE) {
            ra->regs[hw].state = XRA_ALLOCED;
            ra->regs[hw].vreg = vreg;
            return hw;
        }
    }

    /* All out — mark as spilled */
    ra->next_spill++;
    return WREG_NONE;
}

void xra_free_reg(XRARegAlloc *ra, Wx86Reg hw) {
    if (hw >= 0 && hw <= 15) {
        ra->regs[hw].state = XRA_FREE;
        ra->regs[hw].vreg = -1;
    }
}

Wx86Reg xra_get_reg(const XRARegAlloc *ra, int vreg) {
    for (int i = 0; i < 16; i++) {
        if (ra->regs[i].vreg == vreg && ra->regs[i].state == XRA_ALLOCED)
            return (Wx86Reg)i;
    }
    return WREG_NONE;
}

void xra_reserve(XRARegAlloc *ra, Wx86Reg hw) {
    if (hw >= 0 && hw <= 15) {
        ra->regs[hw].state = XRA_RESERVED;
        ra->regs[hw].vreg = -1;
    }
}

int xra_finalize(XRARegAlloc *ra) {
    /* Frame layout: [spill slots] [callee-saved pushes]
     * Callee-saved are pushed by prologue, slots above that.
     * Each spill slot: 8 bytes. Total aligned to 16. */
    int callee_push_bytes = ra->n_callee_saved * 8;
    int spill_bytes = ra->next_spill * 8;
    int total = callee_push_bytes + spill_bytes;
    /* Align to 16 bytes for SysV ABI */
    ra->frame_size = ((total + 15) / 16) * 16;
    return ra->frame_size;
}

int xra_callee_saved_list(const XRARegAlloc *ra, Wx86Reg *out, int max) {
    int count = 0;
    /* Check which callee-saved regs are in use */
    for (int i = 0; i < CALLEE_COUNT && count < max; i++) {
        Wx86Reg hw = callee_pool[i];
        if (ra->regs[hw].state == XRA_ALLOCED ||
            ra->regs[hw].state == XRA_SPILLED) {
            out[count++] = hw;
        }
    }
    return count;
}

void xra_emit_prologue(XRARegAlloc *ra, Wx86Enc *e) {
    xra_finalize(ra);

    /* Push callee-saved registers */
    Wx86Reg saved[5];
    int n_saved = xra_callee_saved_list(ra, saved, 5);
    for (int i = 0; i < n_saved; i++)
        wx86_push_reg(e, saved[i]);

    /* Allocate stack frame */
    if (ra->frame_size > 0) {
        if (ra->frame_size <= 127) {
            wx86_sub_rsp_imm8(e, (uint8_t)ra->frame_size);
        } else {
            wx86_sub_reg_imm32(e, WREG_RSP, ra->frame_size);
        }
    }
}

void xra_emit_epilogue(XRARegAlloc *ra, Wx86Enc *e) {
    /* Deallocate stack frame */
    if (ra->frame_size > 0) {
        if (ra->frame_size <= 127) {
            wx86_add_rsp_imm8(e, (uint8_t)ra->frame_size);
        } else {
            wx86_add_reg_imm32(e, WREG_RSP, ra->frame_size);
        }
    }

    /* Pop callee-saved registers in reverse order */
    Wx86Reg saved[5];
    int n_saved = xra_callee_saved_list(ra, saved, 5);
    for (int i = n_saved - 1; i >= 0; i--)
        wx86_pop_reg(e, saved[n_saved - 1 - i]);
}

void xra_emit_load_args(XRARegAlloc *ra, Wx86Enc *e) {
    /* Arguments are already in rdi, rsi, rdx, rcx, r8, r9.
     * If we need them in other regs, move them.
     * For the simple case, we just mark them as alloced. */
    Wx86ABI abi = wx86_sysv_abi();
    for (int i = 0; i < ra->n_args; i++) {
        Wx86Reg argreg = abi.arg_regs[i];
        ra->regs[argreg].state = XRA_ALLOCED;
        ra->regs[argreg].vreg = i;  /* vreg = arg index */
    }
}

void xra_emit_return(XRARegAlloc *ra, Wx86Enc *e) {
    xra_emit_epilogue(ra, e);
    wx86_ret(e);
}
