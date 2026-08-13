/*
 * holyc_codegen_internal.h  --  Internal header for HolyC codegen modules
 * Shared declarations for codegen submodules (NOT static - they're implemented in .c files).
 */

#ifndef HOLYC_CODEGEN_INTERNAL_H
#define HOLYC_CODEGEN_INTERNAL_H

#include "holyc.h"
#include "holyc_parser.h"
#include "../jit/jit.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>

/* JIT_CALL macro from jit.h  --  call function pointer with 0 args */
#ifndef JIT_CALL
#define JIT_CALL(fn) ((int64_t(*)(void))(fn))()
#endif

/* -- Code Emission Helpers ---------------------------------------- */

void emit_byte(HCGen *gen, uint8_t b);
void emit_data_byte(HCGen *gen, uint8_t b);
void emit_word(HCGen *gen, uint16_t w);
void emit_dword(HCGen *gen, uint32_t d);
void emit_data_dword(HCGen *gen, uint32_t d);
void emit_data_qword(HCGen *gen, uint64_t q);
void emit_qword(HCGen *gen, uint64_t q);

/* -- Patch Helpers ------------------------------------------------- */

void patch_rel32(HCGen *gen, size_t patch_pos, size_t target_pos);

/* -- x86-64 Instruction Patterns ---------------------------------- */

void emit_mov_rax_imm64(HCGen *gen, int64_t val);

/* -- Global (data-section) variable access ----------------------- */
/* Emits a RIP-relative mov rax, [rip+disp32] load of a data-section global
 * at `global_offset` and records a patch so the caller (hc_eval /
 * wubu_holyd_eval) can fix up disp32 to point at exec+code_size+offset. */
void emit_global_load_rax(HCGen *gen, size_t global_offset);
/* Emits a RIP-relative mov [rip+disp32], rax store of a data-section global. */
void emit_global_store_rax(HCGen *gen, size_t global_offset);
void emit_mov_rdi_imm64(HCGen *gen, int64_t val);
void emit_cvt_f64_to_i64(HCGen *gen);
void emit_cvt_i64_to_f64(HCGen *gen);
void emit_add_rax_rdi(HCGen *gen);
void emit_sub_rax_rdi(HCGen *gen);
void emit_mul_rax_rdi(HCGen *gen);
void emit_div_rax_rdi(HCGen *gen);
void emit_udiv_rax_rdi(HCGen *gen);
void emit_mod_rax_rdi(HCGen *gen);
void emit_shl_rax_rdi(HCGen *gen);
void emit_shr_rax_rdi(HCGen *gen);
void emit_and_rax_rdi(HCGen *gen);
void emit_or_rax_rdi(HCGen *gen);
void emit_xor_rax_rdi(HCGen *gen);
void emit_xchg_rax_rdi(HCGen *gen);
void emit_neg_rax(HCGen *gen);
void emit_not_rax(HCGen *gen);
void emit_cmp_rax_rdi(HCGen *gen);
void emit_test_rax_rax(HCGen *gen);
void emit_mov_rdi_rax(HCGen *gen);
void emit_xor_rax_rax(HCGen *gen);
void emit_mov_rax_1(HCGen *gen);
void emit_ret(HCGen *gen);
void emit_prologue(HCGen *gen);

/* prefetchnta [rdi] — array-element loads (INDEX path holds base in rdi) */
void emit_prefetch_rdi(HCGen *gen);
/* prefetchnta [rax] — plain pointer dereference loads */
void emit_prefetch_rax(HCGen *gen);
void emit_prefetch_rax_off(HCGen *gen, int32_t off);   /* [rax+disp32] */
void emit_prefetch_rbp(HCGen *gen, int32_t off);       /* [rbp - off]  */
void emit_prefetch_rip(HCGen *gen, size_t global_offset); /* [rip+disp32] */
void emit_epilogue(HCGen *gen);

/* -- Conditional Set Patterns ------------------------------------- */

void emit_setcc(HCGen *gen, uint8_t set_op);

/* -- Jump Emission (5-byte, always patchable) --------------------- */

size_t emit_jmp_placeholder(HCGen *gen);
size_t emit_jcc_placeholder(HCGen *gen, uint8_t cc);

/* switch dispatch helpers */
void emit_push_rax(HCGen *gen);
void emit_pop_rax(HCGen *gen);
void emit_cmp_rax_mem_rsp(HCGen *gen);
void emit_mov_rax_mem_rsp(HCGen *gen);
void emit_add_rsp_8(HCGen *gen);

/* Condition codes for Jcc */
#define CC_O  0
#define CC_NO 1
#define CC_B  2   /* below (unsigned <) */
#define CC_NB 3   /* not below */
#define CC_E  4   /* equal / zero */
#define CC_NE 5   /* not equal / not zero */
#define CC_BE 6   /* below or equal */
#define CC_NBE 7  /* not below or equal (above) */
#define CC_S  8   /* sign */
#define CC_NS 9   /* not sign */
#define CC_L  12  /* less (signed <) */
#define CC_NL 13  /* not less */
#define CC_LE 14  /* less or equal */
#define CC_NLE 15 /* not less or equal (greater) */

/* -- Code Gen Init ------------------------------------------------ */

void hc_gen_init(HCGen *gen);

/* -- Expression Generation ---------------------------------------- */

int gen_expr(HCGen *gen, const HCASTNode *node);

/* -- Statement Generation ----------------------------------------- */

int gen_stmt(HCGen *gen, const HCASTNode *node);

/* -- Global patch info for runtime fixup of global variable access */
typedef struct {
    size_t code_patch_pos;
    size_t global_offset;
} HCGenGlobalPatch;

/* -- External function table for extern C functions */
typedef struct {
    char c_name[64];
    void *func_addr;
} HCGenExternFunc;

#endif /* HOLYC_CODEGEN_INTERNAL_H */