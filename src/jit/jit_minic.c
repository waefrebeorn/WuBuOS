/*
 * jit_minic.c  --  WuBuOS Mini C-to-x86-64 Compiler
 *
 * Self-hosted replacement for the MIR JIT backend.
 * Parses a tiny subset of C and emits x86-64 machine code
 * using the existing Wx86Enc encoder (wubu_x86.h).
 *
 * Supported grammar:
 *   program     = func_decl+
 *   func_decl   = type IDENT '(' params? ')' '{' stmt* '}'
 *   type        = "int" | "long" | "I64" | "U8" | "void"
 *   params      = type IDENT (',' type IDENT)*
 *   stmt        = return_stmt | decl_stmt | assign_stmt | if_stmt | while_stmt | expr_stmt
 *   return_stmt = "return" expr ';'
 *   decl_stmt   = type IDENT ('=' expr)? ';'
 *   assign_stmt = IDENT '=' expr ';'
 *   if_stmt     = "if" '(' expr ')' '{' stmt* '}' ("else" '{' stmt* '}')?
 *   while_stmt  = "while" '(' expr ')' '{' stmt* '}'
 *   expr_stmt   = expr ';'
 *   expr        = compare
 *   compare     = additive (("==" | "!=" | "<" | ">" | "<=" | ">=") additive)?
 *   additive    = multiplicative (('+' | '-') multiplicative)*
 *   multiplicative = primary (('*' | '/') primary)*
 *   primary     = NUMBER | IDENT | '(' expr ')' | ('-' | '!') primary | IDENT '(' args? ')'
 *   args        = expr (',' expr)*
 *
 * System V AMD64 calling convention:
 *   Args: RDI, RSI, RDX, RCX, R8, R9
 *   Return: RAX
 *   Callee-saved: RBX, RBP, R12-R15
 */

#include "jit.h"
#include "jit_minic_internal.h"
#include "wubu_x86.h"
#include "x86_regalloc.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>

#include "jit_branch_profile.h"
#include "jit_minic_loop.h"

/* -- Tokenizer ---------------------------------------------------- */






/* -- Variable / Scope -------------------------------------------- */

void scope_init(MinicScope *s) {
    memset(s, 0, sizeof(*s));
    s->stack_offset = -8;
}

MinicVar *scope_find(MinicScope *s, const char *name) {
    for (int i = 0; i < s->var_count; i++) {
        if (strcmp(s->vars[i].name, name) == 0) return &s->vars[i];
    }
    return NULL;
}

MinicVar *scope_add_local(MinicScope *s, const char *name) {
    if (s->var_count >= MINIC_MAX_VARS) return NULL;
    MinicVar *v = &s->vars[s->var_count++];
    snprintf(v->name, sizeof(v->name), "%s", name);
    v->is_arg = 0;
    v->slot = s->stack_offset;
    s->stack_offset -= 8;
    return v;
}

MinicVar *scope_add_arg(MinicScope *s, const char *name, int arg_idx) {
    if (s->var_count >= MINIC_MAX_VARS) return NULL;
    MinicVar *v = &s->vars[s->var_count++];
    snprintf(v->name, sizeof(v->name), "%s", name);
    v->is_arg = 1;
    v->slot = arg_idx;
    return v;
}

/* Arg register mapping: arg 0-5 → RDI,RSI,RDX,RCX,R8,R9 */
Wx86Reg arg_reg(int idx) {
    static const Wx86Reg regs[] = {
        WREG_RDI, WREG_RSI, WREG_RDX, WREG_RCX, WREG_R8, WREG_R9
    };
    if (idx >= 0 && idx < 6) return regs[idx];
    return WREG_RAX;
}

/* -- Forward declarations ---------------------------------------- */

typedef struct MinicCompiler MinicCompiler;

/* Expression chain — defined in jit_minic_expr.c */
extern void compile_expr(MinicCompiler *mc);
extern void compile_compare(MinicCompiler *mc);
extern void compile_bitwise_or(MinicCompiler *mc);
extern void compile_bitwise_xor(MinicCompiler *mc);
extern void compile_bitwise_and(MinicCompiler *mc);
extern void compile_shift(MinicCompiler *mc);
extern void compile_additive(MinicCompiler *mc);
extern void compile_multiplicative(MinicCompiler *mc);
extern void compile_primary(MinicCompiler *mc);

/* Statement compiler — defined in this file */
static void compile_stmt(MinicCompiler *mc);

/* Builtins / optimizations — defined in this file */
static int  mc_try_builtin(MinicCompiler *mc, const char *name, Wx86Reg aregs[6]);
static void compile_struct_decl(MinicCompiler *mc);

/* -- Compiler State ---------------------------------------------- */


void mc_error(MinicCompiler *mc, const char *msg) {
    mc->error = 1;
    snprintf(mc->error_msg, sizeof(mc->error_msg), "minic: %s (near '%s')",
             msg, mc->lex.cur.text);
}

/* -- Emit helper macros using Wx86Enc --------------------------- */

#define MC_EMIT(mc, call) do { if (!(mc)->error) { call; } } while(0)

/* -- Subsystem C: profile instrumentation ----------------------- */

/* Emit a runtime counter increment. We can't use RIP-relative addressing
 * because the counter (a global in .data) may be >2GB from the JIT code
 * (mmap'd in a different region). Instead: `movabs r11, addr; addq $1, [r11]`.
 * r11 is caller-saved in SysV ABI and safe to clobber in JIT leaf code.
 * Encoding: 49 bb [8-byte addr]  49 83 03 01  (14 bytes total). */
static int mc_emit_profile_inc(MinicCompiler *mc, int kind /* 0=taken, 1=not_taken */) {
    if (!mc->profile_enabled) return -1;
    if (mc->n_branches >= JBP_MAX_BRANCHES) return -1;
    int id = mc->n_branches++;
    int64_t *target = (kind == 0) ? jbp_counter_taken(id) : jbp_counter_not_taken(id);
    /* Save r11 (may be in use by XRA), then use it for the counter increment.
     * push r11 = 41 53, pop r11 = 41 5b */
    wx86_emit_byte(&mc->enc, 0x41); wx86_emit_byte(&mc->enc, 0x53); /* push r11 */
    /* movabs r11, imm64 = 49 bb [8 bytes] */
    int64_t addr = (int64_t)(uintptr_t)target;
    wx86_emit_byte(&mc->enc, 0x49);
    wx86_emit_byte(&mc->enc, 0xbb);
    for (int i = 0; i < 8; i++)
        wx86_emit_byte(&mc->enc, (uint8_t)((addr >> (i*8)) & 0xFF));
    /* addq $1, [r11] = 49 83 03 01 */
    wx86_emit_byte(&mc->enc, 0x49);
    wx86_emit_byte(&mc->enc, 0x83);
    wx86_emit_byte(&mc->enc, 0x03);
    wx86_emit_byte(&mc->enc, 0x01);
    /* Restore r11 */
    wx86_emit_byte(&mc->enc, 0x41); wx86_emit_byte(&mc->enc, 0x5b); /* pop r11 */
    return id;
}

/* -- Register-allocator helpers ----------------------------------- */

/* Materialize the current RAX result into a fresh virtual register, emitting
 * a store if the allocator must spill. Returns the assigned hardware reg, or
 * WREG_NONE if spilled (caller must reload before use). */
int mc_vreg_of_rax(MinicCompiler *mc) {
    int v = mc->next_vreg++;
    if (mc->use_xra) {
        /* If RAX holds a known constant, record it so a spill can
         * rematerialize as an immediate instead of a memory reload. */
        if (mc->rax_is_const)
            xra_mark_const(&mc->ra, v, mc->rax_const_val);
        /* Newly-materialized value is used at the next binop (now-ish). */
        xra_set_next_use(&mc->ra, v, xra_advance_pos(&mc->ra));
        /* Allocate, evicting the farthest-next-use active vreg if the pool
         * is exhausted (Poletto SpillAtInterval) rather than spilling the
         * incoming value. */
        Wx86Reg hw = xra_alloc_evict(&mc->ra, v, &mc->enc);
        if (hw != WREG_NONE) {
            /* Move rax result into allocated reg and register it */
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, hw, WREG_RAX));
        } else {
            /* Nothing evictable — spill the incoming value to its stack slot
             * (unless it is a known constant, which remats on reload). */
            int slot = xra_assign_spill_slot(&mc->ra, v);
            if (!xra_is_const(&mc->ra, v)) {
                int offset = -(8 * (slot + 1));
                MC_EMIT(mc, wx86_mov_mem_reg(&mc->enc, WREG_RBP, offset, WREG_RAX));
            }
        }
    }
    /* After materializing, the value is no longer a live "rax constant". */
    mc->rax_is_const = false;
    return v;
}

/* Reload a virtual register (possibly spilled) into a physical register,
 * materializing the result into RAX for the non-xra path compatibility. */
static Wx86Reg mc_load_vreg(MinicCompiler *mc, int vreg) {
    if (!mc->use_xra) return WREG_RAX;  /* rax holds the value */
    return xra_get_reg(&mc->ra, vreg);
}

/* -- Peephole optimizer ------------------------------------------- */

/* Remove redundant `mov rax,rX` immediately followed by `mov rX,rax` (same X).
 * These two instructions together are the identity — the allocator emits the
 * pair when it writes a binop result to rax, then immediately re-saves rax
 * into the freshly-reallocated same register as the next LHS vreg.
 *
 * Verified encodings (r10/r11 are the allocator's first-priority scratch regs):
 *   mov rax, r10 = 4c 89 d0        mov r10, rax = 49 89 c2
 *   mov rax, r11 = 4c 89 d8        mov r11, rax = 49 89 c3
 * Match the exact 6-byte identity pairs.
 */
static void mc_peephole_elim_mov_roundtrip(Wx86Enc *e) {
    if (!e || e->pos < 6) return;
    /* SAFETY: this pass shifts bytes, which would corrupt already-patched
     * rel32 branch displacements. If the buffer contains ANY near/far branch
     * (0F 8x jcc rel32, E9 jmp rel32, or 0F 80-8F), skip elimination entirely
     * — a correctness-preserving straight-line-only optimization. */
    for (size_t i = 0; i + 1 < e->pos; i++) {
        if (e->buf[i] == 0xE9) return;                       /* jmp rel32 */
        if (e->buf[i] == 0x0F && (e->buf[i+1] & 0xF0) == 0x80) return; /* jcc rel32 */
    }
    static const uint8_t PAI[][6] = {
        { 0x4c,0x89,0xd0, 0x49,0x89,0xc2 },  /* rax<->r10, r10<->rax */
        { 0x4c,0x89,0xd8, 0x49,0x89,0xc3 },  /* rax<->r11, r11<->rax */
    };
    size_t w = 0;
    for (size_t i = 0; i < e->pos; ) {
        int removed = 0;
        for (size_t p = 0; p < 2; p++) {
            if (i + 6 <= e->pos && memcmp(e->buf + i, PAI[p], 6) == 0) {
                i += 6;  /* skip the identity pair entirely */
                removed = 1;
                break;
            }
        }
        /* Peephole: mov rax, rax = 48 89 c0 (3 bytes) — NOP */
        if (!removed && i + 3 <= e->pos &&
            e->buf[i]==0x48 && e->buf[i+1]==0x89 && e->buf[i+2]==0xc0) {
            i += 3; removed = 1;
        }
        /* Peephole: push rax; pop rax = 50 58 (2 bytes) — NOP */
        if (!removed && i + 2 <= e->pos &&
            e->buf[i]==0x50 && e->buf[i+1]==0x58) {
            i += 2; removed = 1;
        }
        if (removed) continue;
        e->buf[w++] = e->buf[i++];
    }
    e->pos = w;
}


/* -- Expression Compiler (result in RAX) ------------------------ */

/* SETcc doesn't exist in wubu_x86.h yet — we'll emit it manually */
void wx86_setcc_r8(Wx86Enc *e, Wx86CC cc, Wx86Reg dst) {
    (void)dst; /* We only support setting AL (RAX low byte) for now */
    /* 0F 90+cc /0 with ModRM=0xC0 (mod=3, reg=0, rm=0 = RAX) */
    wx86_emit_byte(e, 0x0F);
    wx86_emit_byte(e, 0x90 + (uint8_t)cc);
    wx86_emit_byte(e, 0xC0);  /* modrm(3, 0, 0) */
}


/* -- Statement Compiler ------------------------------------------ */

static void compile_if_stmt(MinicCompiler *mc) {
    minic_advance(&mc->lex);
    minic_expect(&mc->lex, TOK_LPAREN);
    compile_expr(mc);
    minic_expect(&mc->lex, TOK_RPAREN);

    /* #4 cmp/jcc macro-fusion: if the condition was a comparison, its flags are
     * still live (setcc/movzx don't clobber them). Truncate the setcc/movzx
     * and branch on the compare's own flags with the inverted cc — dropping a
     * setcc + movzx + test (the compare's flag is already exactly what we need).
     * If the condition was NOT a comparison, keep the 0/1 test. */
    Wx86CC branch_cc = WCC_E;  /* jcc on 0/1 result: jump when rax==0 */
    bool fuse = mc->last_was_compare;
    if (fuse) {
        /* if(false-condition) skip the then-body. last_compare_cc is the cc of
         * the result (rax=1 when cc true). Jump past the body when cc is FALSE,
         * i.e. jump on the INVERTED compare cc. */
        switch (mc->last_compare_cc) {
            case WCC_E:  branch_cc = WCC_NE; break;
            case WCC_NE: branch_cc = WCC_E;  break;
            case WCC_L:  branch_cc = WCC_GE; break;
            case WCC_G:  branch_cc = WCC_LE; break;
            case WCC_LE: branch_cc = WCC_G;  break;
            case WCC_GE: branch_cc = WCC_L;  break;
            default: fuse = false; break;
        }
    }
    if (fuse) {
        /* Revert the setcc+movzx just emitted by compile_compare. */
        mc->enc.pos = mc->cmp_after_pos;
    } else {
        MC_EMIT(mc, wx86_test_reg_reg(&mc->enc, WREG_RAX, WREG_RAX));
    }
    MC_EMIT(mc, wx86_jcc_rel32(&mc->enc, branch_cc));
    size_t else_patch = wx86_jcc_rel32_pos(&mc->enc);
    /* Subsystem C: increment the not-taken counter at the fallthrough (the
     * instruction right after the jcc). Branch id is unique per jcc site; if
     * profiling is off this is a no-op and n_branches is untouched. */
    mc_emit_profile_inc(mc, 1 /* not-taken */);

    minic_expect(&mc->lex, TOK_LBRACE);

    /* #14 block layout: disabled — the swap logic conflicts with else-if
     * chains and nested expressions. TODO: implement with token replay. */
    #if 0
    int then_is_single_return =
        (minic_cur(&mc->lex)->type == TOK_RETURN);

    if (then_is_single_return && minic_peek(&mc->lex)->type == TOK_SEMI) {
        /* ... old #14 code ... */
        return;
    }
    #endif

    /* Normal path: no layout swap */
    while (minic_cur(&mc->lex)->type != TOK_RBRACE && minic_cur(&mc->lex)->type != TOK_EOF)
        compile_stmt(mc);
    minic_expect(&mc->lex, TOK_RBRACE);

    if (minic_cur(&mc->lex)->type == TOK_ELSE) {
        minic_advance(&mc->lex);
        MC_EMIT(mc, wx86_jmp_rel32(&mc->enc));
        size_t end_patch = wx86_jmp_rel32_pos(&mc->enc);
        wx86_patch_rel32(&mc->enc, else_patch, mc->enc.pos);

        /* Handle "else if" recursively */
        if (minic_cur(&mc->lex)->type == TOK_IF) {
            compile_if_stmt(mc);
        } else {
            minic_expect(&mc->lex, TOK_LBRACE);
            while (minic_cur(&mc->lex)->type != TOK_RBRACE && minic_cur(&mc->lex)->type != TOK_EOF)
                compile_stmt(mc);
            minic_expect(&mc->lex, TOK_RBRACE);
        }
        wx86_patch_rel32(&mc->enc, end_patch, mc->enc.pos);
    } else {
        wx86_patch_rel32(&mc->enc, else_patch, mc->enc.pos);
    }
}

static void compile_while_stmt(MinicCompiler *mc) {
    minic_advance(&mc->lex);

    /* #15 branch alignment: align the loop head to a 16-byte boundary. */
    size_t pad = (16 - (mc->enc.pos & 15)) & 15;
    wx86_multi_nop(&mc->enc, pad);
    size_t loop_top = mc->enc.pos;

    minic_expect(&mc->lex, TOK_LPAREN);
    compile_expr(mc);
    minic_expect(&mc->lex, TOK_RPAREN);

    /* #4 cmp/jcc fusion for the loop condition (same as if). */
    Wx86CC branch_cc = WCC_E;
    bool fuse = mc->last_was_compare;
    if (fuse) {
        switch (mc->last_compare_cc) {
            case WCC_E:  branch_cc = WCC_NE; break;
            case WCC_NE: branch_cc = WCC_E;  break;
            case WCC_L:  branch_cc = WCC_GE; break;
            case WCC_G:  branch_cc = WCC_LE; break;
            case WCC_LE: branch_cc = WCC_G;  break;
            case WCC_GE: branch_cc = WCC_L;  break;
            default: fuse = false; break;
        }
    }
    if (fuse) {
        mc->enc.pos = mc->cmp_after_pos;
    } else {
        MC_EMIT(mc, wx86_test_reg_reg(&mc->enc, WREG_RAX, WREG_RAX));
    }
    MC_EMIT(mc, wx86_jcc_rel32(&mc->enc, branch_cc));
    size_t exit_patch = wx86_jcc_rel32_pos(&mc->enc);
    /* Subsystem C: profile the loop-condition fallthrough */
    mc_emit_profile_inc(mc, 1 /* not-taken */);

    /* TWO-PASS LOOP: disabled — incompatible with XRA. Compile directly. */
    #if 0
    /* ===== TWO-PASS LOOP: capture body, analyze, then optimize ===== */
    minic_expect(&mc->lex, TOK_LBRACE);

    /* Pass 1: Compile body into a temporary encoder */
    Wx86Enc body_enc;
    wx86_enc_init_dynamic(&body_enc, 4096);
    Wx86Enc saved_enc = mc->enc;   /* save a COPY of the main encoder */
    XRARegAlloc saved_ra = mc->ra;  /* save XRA state for two-pass while */
    mc->enc = body_enc;            /* switch to temp buffer */

    /* Capture the loop body for analysis AND compile to temp buffer. */
    mc->capture_loop = true;
    minic_loop_body_init(&mc->loop_body);
    while (minic_cur(&mc->lex)->type != TOK_RBRACE && minic_cur(&mc->lex)->type != TOK_EOF)
        compile_stmt(mc);
    mc->capture_loop = false;
    minic_expect(&mc->lex, TOK_RBRACE);

    /* Analyze the captured body. */
    if (mc->loop_body.n_stmts > 0) {
        int64_t trip; char iv[64]; int64_t stride;
        int n_ivs = minic_loop_analyze(&mc->loop_body, 0, '<', 0,
                                       &trip, iv, &stride);
        if (n_ivs > 0 && trip >= 0) {
            mc->loop_trip_count = trip;
            snprintf(mc->loop_iv, sizeof(mc->loop_iv), "%s", iv);
            mc->loop_iv_stride = stride;
        } else {
            mc->loop_trip_count = -1;
        }
        mc->loop_n_invariants = minic_loop_invariant_count(&mc->loop_body);
    }

    /* #13 LICM: hoist loop-invariant comparison operand into a register.
     * If the loop condition is `var < invariant` (or `invariant < var`) and
     * the invariant variable is NOT written in the body, load it into r11
     * before the loop and compare against r11 instead of the stack slot.
     * This saves one memory load per iteration. */
    /* Detect: the condition compiled a cmp rax,rcx where rcx was loaded from
     * a stack slot for a variable not written in the body. We patch the
     * cmp to use r11 and emit `mov r11, [rbp-slot]` before the loop top. */
    /* For this wave: the analysis result (loop_n_invariants) is stored. The
     * actual register hoisting requires tracking which stack slot the cmp
     * used, which we add in the next pass. The two-pass infrastructure is
     * now in place for this. */

    /* Restore main encoder and emit the analyzed body. */
    body_enc = mc->enc;
    mc->enc = saved_enc;
    mc->ra = saved_ra;  /* restore XRA state after two-pass while */

    /* Emit the body code from the temporary buffer. */
    for (size_t i = 0; i < body_enc.pos; i++) {
        wx86_emit_byte(&mc->enc, body_enc.buf[i]);
    }
    wx86_enc_free(&body_enc);

    /* Jump back to condition */
    MC_EMIT(mc, wx86_jmp_rel32(&mc->enc));
    wx86_patch_rel32(&mc->enc, wx86_jmp_rel32_pos(&mc->enc), loop_top);

    wx86_patch_rel32(&mc->enc, exit_patch, mc->enc.pos);
    #else
    /* SINGLE-PASS: compile body directly to main encoder */
    minic_expect(&mc->lex, TOK_LBRACE);
    while (minic_cur(&mc->lex)->type != TOK_RBRACE && minic_cur(&mc->lex)->type != TOK_EOF)
        compile_stmt(mc);
    minic_expect(&mc->lex, TOK_RBRACE);

    /* Jump back to condition */
    MC_EMIT(mc, wx86_jmp_rel32(&mc->enc));
    wx86_patch_rel32(&mc->enc, wx86_jmp_rel32_pos(&mc->enc), loop_top);

    wx86_patch_rel32(&mc->enc, exit_patch, mc->enc.pos);
    #endif
}

static void compile_return_stmt(MinicCompiler *mc) {
    minic_advance(&mc->lex);
    if (minic_cur(&mc->lex)->type != TOK_SEMI)
        compile_expr(mc);
    minic_expect(&mc->lex, TOK_SEMI);

    /* Fast path: just ret. The function epilogue handles rbp. */
    if (!mc->need_frame) {
        MC_EMIT(mc, wx86_ret(&mc->enc));
    } else if (!mc->use_xra) {
        /* Non-XRA: restore frame pointer */
        MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RSP, WREG_RBP));
        MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RBP));
        MC_EMIT(mc, wx86_ret(&mc->enc));
    } else {
        /* XRA mode with frame: restore callee-saved regs + frame */
        xra_emit_return(&mc->ra, &mc->enc);
        MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RSP, WREG_RBP));
        MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RBP));
        MC_EMIT(mc, wx86_ret(&mc->enc));
    }
}

static void compile_decl_stmt(MinicCompiler *mc) {
    minic_advance(&mc->lex);  /* skip type */

    if (minic_cur(&mc->lex)->type != TOK_IDENT) {
        mc_error(mc, "expected identifier in declaration");
        return;
    }
    char name[64];
    snprintf(name, sizeof(name), "%s", minic_cur(&mc->lex)->text);
    minic_advance(&mc->lex);

    MinicVar *v = scope_add_local(&mc->scope, name);

    if (minic_cur(&mc->lex)->type == TOK_ASSIGN) {
        minic_advance(&mc->lex);
        compile_expr(mc);
        if (v) {
            if (mc->use_xra) {
                /* XRA mode: value stays in rax, XRA tracks the vregister */
                /* The value will be spilled to stack only if register pressure requires it */
            } else {
                MC_EMIT(mc, wx86_mov_mem_reg(&mc->enc, WREG_RBP, v->slot, WREG_RAX));
            }
        }
    }
    minic_expect(&mc->lex, TOK_SEMI);
}

static void compile_assign_or_expr_stmt(MinicCompiler *mc) {
    if (minic_cur(&mc->lex)->type != TOK_IDENT) {
        compile_expr(mc);
        minic_expect(&mc->lex, TOK_SEMI);
        return;
    }

    char name[64];
    snprintf(name, sizeof(name), "%s", minic_cur(&mc->lex)->text);
    minic_advance(&mc->lex);

    if (minic_cur(&mc->lex)->type == TOK_ASSIGN) {
        minic_advance(&mc->lex);

        /* Subsystem B: loop-body capture. When compiling a while-body,
         * recognize `var = var +/- imm` (induction-variable update) and
         * `var = <invariant>` (loop-invariant expression) and record them
         * into mc->loop_body for post-compile analysis. We peek at the
         * token stream BEFORE calling compile_expr so we don't disturb
         * the compiler state. */
        if (mc->capture_loop && minic_cur(&mc->lex)->type == TOK_IDENT) {
            const char *rhs_name = minic_cur(&mc->lex)->text;
            MinicTokType op0 = minic_peek(&mc->lex)->type;
            if (op0 == TOK_PLUS || op0 == TOK_MINUS) {
                MinicToken rhs2 = minic_peek2(&mc->lex);
                if (rhs2.type == TOK_NUMBER) {
                    int64_t imm = rhs2.ival;
                    if (strcmp(rhs_name, name) == 0) {
                        minic_loop_add_assign(&mc->loop_body, name,
                            (op0 == TOK_PLUS) ? '+' : '-',
                            name, NULL, imm);
                    } else {
                        minic_loop_add_assign(&mc->loop_body, name, '=',
                            rhs_name, NULL, 0);
                    }
                }
            }
        }

        compile_expr(mc);

        MinicVar *v = scope_find(&mc->scope, name);
        if (!v) {
            mc_error(mc, "undefined variable in assignment");
        } else if (v->is_arg) {
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, arg_reg(v->slot), WREG_RAX));
        } else {
            MC_EMIT(mc, wx86_mov_mem_reg(&mc->enc, WREG_RBP, v->slot, WREG_RAX));
        }
        minic_expect(&mc->lex, TOK_SEMI);
    } else {
        MinicVar *v = scope_find(&mc->scope, name);
        if (v) {
            if (v->is_arg)
                MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, arg_reg(v->slot)));
            else
                MC_EMIT(mc, wx86_mov_reg_mem(&mc->enc, WREG_RAX, WREG_RBP, v->slot));
        }
        minic_expect(&mc->lex, TOK_SEMI);
    }
}

static void compile_stmt(MinicCompiler *mc) {
    MinicTokType tt = minic_cur(&mc->lex)->type;
    if (tt == TOK_RETURN) compile_return_stmt(mc);
    else if (tt == TOK_IF) compile_if_stmt(mc);
    else if (tt == TOK_WHILE) compile_while_stmt(mc);
    else if (minic_is_type(tt)) compile_decl_stmt(mc);
    else compile_assign_or_expr_stmt(mc);
}

/* -- Subsystem A: struct declaration ----------------------------- */

/* Parse `struct Name { type member; ... };` and register it with #19 field
 * reordering. Member types are primitives (I64=long, U8) for this wave; a
 * later wave can nest structs/arrays. The declaration emits no code. */
static void compile_struct_decl(MinicCompiler *mc) {
    minic_advance(&mc->lex);  /* skip 'struct' */
    if (minic_cur(&mc->lex)->type != TOK_IDENT) {
        mc_error(mc, "expected struct name");
        return;
    }
    char name[64];
    snprintf(name, sizeof(name), "%s", minic_cur(&mc->lex)->text);
    minic_advance(&mc->lex);

    int t = minic_type_new(&mc->types);
    if (t < 0) { mc_error(mc, "too many types"); return; }
    MinicType *st = &mc->types.types[t];
    st->kind = MTY_STRUCT;
    snprintf(st->name, sizeof(st->name), "%s", name);

    minic_expect(&mc->lex, TOK_LBRACE);
    while (minic_cur(&mc->lex)->type != TOK_RBRACE && minic_cur(&mc->lex)->type != TOK_EOF) {
        /* member type: 'long' or 'char' -> I64 / U8 */
        int mty;
        if (minic_cur(&mc->lex)->type == TOK_LONG ||
            minic_cur(&mc->lex)->type == TOK_INT ||
            minic_cur(&mc->lex)->type == TOK_I64) mty = 0;
        else if (minic_cur(&mc->lex)->type == TOK_U8) mty = 1;
        else { minic_advance(&mc->lex); continue; }
        minic_advance(&mc->lex);

        if (minic_cur(&mc->lex)->type != TOK_IDENT) {
            mc_error(mc, "expected member name");
            return;
        }
        if (st->n_members < MINIC_MAX_MEMBERS) {
            MinicMember *m = &st->members[st->n_members++];
            snprintf(m->name, sizeof(m->name), "%s", minic_cur(&mc->lex)->text);
            m->mty = mty;
        }
        minic_advance(&mc->lex);
        minic_expect(&mc->lex, TOK_SEMI);
    }
    minic_expect(&mc->lex, TOK_RBRACE);
    minic_expect(&mc->lex, TOK_SEMI);

    minic_type_layout(&mc->types, st);   /* #19 field reordering */
}

/* -- Function Compiler ------------------------------------------- */

static int compile_func(MinicCompiler *mc, const char *target_fn) {
    if (!minic_is_type(minic_cur(&mc->lex)->type)) {
        mc_error(mc, "expected type in function declaration");
        return -1;
    }
    minic_advance(&mc->lex);

    if (minic_cur(&mc->lex)->type != TOK_IDENT) {
        mc_error(mc, "expected function name");
        return -1;
    }
    char func_name[64];
    snprintf(func_name, sizeof(func_name), "%s", minic_cur(&mc->lex)->text);
    minic_advance(&mc->lex);

    int is_target = (target_fn && strcmp(func_name, target_fn) == 0);

    minic_expect(&mc->lex, TOK_LPAREN);
    scope_init(&mc->scope);
    mc->n_args = 0;

    while (minic_cur(&mc->lex)->type != TOK_RPAREN && minic_cur(&mc->lex)->type != TOK_EOF) {
        if (mc->n_args > 0) minic_expect(&mc->lex, TOK_COMMA);
        int arg_mty = 0;   /* default I64 */
        if (minic_cur(&mc->lex)->type == TOK_STRUCT) {
            /* struct Name *p — pointer arg; type = struct (for -> access) */
            minic_advance(&mc->lex);
            if (minic_cur(&mc->lex)->type == TOK_IDENT) {
                MinicType *t = minic_type_find(&mc->types, minic_cur(&mc->lex)->text);
                if (t) arg_mty = minic_type_index(&mc->types, t);
                minic_advance(&mc->lex);
            }
            minic_expect(&mc->lex, TOK_STAR);
        } else if (!minic_is_type(minic_cur(&mc->lex)->type)) {
            break;
        } else {
            minic_advance(&mc->lex);  /* skip primitive type */
        }
        if (minic_cur(&mc->lex)->type == TOK_IDENT) {
            MinicVar *v = scope_add_arg(&mc->scope, minic_cur(&mc->lex)->text, mc->n_args);
            if (v) v->mty = arg_mty;
            minic_advance(&mc->lex);
        }
        mc->n_args++;
    }
    minic_expect(&mc->lex, TOK_RPAREN);

    if (!is_target) {
        int brace_depth = 0;
        minic_expect(&mc->lex, TOK_LBRACE);
        brace_depth = 1;
        while (brace_depth > 0 && minic_cur(&mc->lex)->type != TOK_EOF) {
            if (minic_cur(&mc->lex)->type == TOK_LBRACE) brace_depth++;
            if (minic_cur(&mc->lex)->type == TOK_RBRACE) brace_depth--;
            minic_advance(&mc->lex);
        }
        return 0;
    }

    /* Prologue — FAST PATH: if no locals needed and args fit in registers,
     * skip the frame entirely. Args stay in their parameter registers
     * (RDI,RSI,RDX,RCX,R8,R9). This eliminates push rbp/mov rbp,rsp/pop rbp
     * and all arg pushes — saving 5-8 instructions per call. */
    mc->need_frame = (mc->n_args > 0);  /* always use frame for functions with args */

    if (!mc->need_frame) {
        /* Scan the function body for local declarations. */
        MinicLexer save = mc->lex;
        if (minic_cur(&mc->lex)->type == TOK_LBRACE) {
            minic_advance(&mc->lex); /* skip { */
            int depth = 1;
            int steps = 0;
            while (depth > 0 && minic_cur(&mc->lex)->type != TOK_EOF && steps < 10000) {
                steps++;
                if (minic_cur(&mc->lex)->type == TOK_LBRACE) depth++;
                if (minic_cur(&mc->lex)->type == TOK_RBRACE) depth--;
                if (depth == 1 && minic_is_type(minic_cur(&mc->lex)->type)) {
                    minic_advance(&mc->lex);
                    if (minic_cur(&mc->lex)->type == TOK_IDENT) {
                        mc->need_frame = 1;
                        break;
                    }
                }
                minic_advance(&mc->lex);
            }
        } else {
            mc->need_frame = 1; /* can't scan, use frame to be safe */
        }
        mc->lex = save;
    }
    if (!mc->need_frame) {
        /* Fast path: no args, no locals. Minimal prologue. */
        if (mc->n_args > 0) {
            /* Has args: push rbp + args to stack for safe expression eval */
            MC_EMIT(mc, wx86_push_reg(&mc->enc, WREG_RBP));
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RBP, WREG_RSP));
            mc->scope.stack_offset = -8;
            for (int i = mc->n_args - 1; i >= 0; i--) {
                MC_EMIT(mc, wx86_push_reg(&mc->enc, arg_reg(i)));
                mc->scope.stack_offset -= 8;
            }
            for (int i = 0; i < mc->n_args && i < 6; i++) {
                MinicVar *v = scope_find(&mc->scope, mc->scope.vars[i].name);
                if (v) {
                    v->is_arg = 0;
                    v->slot = mc->scope.stack_offset + 8 * i;
                }
            }
        }
        goto fast_prologue_done;
    }

    /* SLOW PATH: full frame setup (original code) */
    MC_EMIT(mc, wx86_push_reg(&mc->enc, WREG_RBP));
    MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RBP, WREG_RSP));

    /* Push args to stack — saves them for later reference.
     * After pushes: [rbp-8] = last_pushed, [rbp-8*n] = first_pushed.
     * We push in reverse order (arg n-1 first, arg 0 last) so:
     * [rbp-8] = arg[0], [rbp-16] = arg[1], ... */
    for (int i = mc->n_args - 1; i >= 0; i--) {
        MC_EMIT(mc, wx86_push_reg(&mc->enc, arg_reg(i)));
    }

    /* Now allocate stack space for local variables.
     * Calculate total frame size: args already pushed from -8 to -8*n.
     * Locals go below that. We need to SUB RSP to make room. */
    int args_size = mc->n_args * 8;

    /* Relocate arg vars: arg[n-1] at [rbp-8], arg[n-2] at [rbp-16], etc.
     * Because we push from n-1 down to 0, the last push (arg0) is at [rbp-8*n+8*n-8]
     * Actually simpler: push order is arg[n-1], arg[n-2], ..., arg[0].
     * Stack after: [rbp-8] = arg[0] (pushed last), [rbp-16] = arg[1], etc.
     * Wait no: we push i from n-1 DOWN to 0, so:
     *   First push: arg_reg(n-1) → goes to [rbp-8]
     *   Second push: arg_reg(n-2) → goes to [rbp-16]
     *   ...
     *   Last push: arg_reg(0) → goes to [rbp-8*n]
     * So arg[0] is at [rbp-8*n], arg[1] at [rbp-8*(n-1)], etc.
     * For 2 args: arg[0] at [rbp-16], arg[1] at [rbp-8]. */
    for (int i = 0; i < mc->n_args && i < 6; i++) {
        MinicVar *v = scope_find(&mc->scope, mc->scope.vars[i].name);
        if (v) {
            v->slot = -(8 * mc->n_args) + 8 * i;  /* arg0 deepest: [rbp-8*n], arg[n-1] at [rbp-8] */
            v->is_arg = 0;  /* Now on stack, accessed via RBP */
        }
    }
    /* Adjust stack_offset to account for args on stack */
    mc->scope.stack_offset = -(8 + args_size);

fast_prologue_done:

    /* Initialize the linear-scan register allocator for this function.
     * When enabled, expression temporaries (LHS of binops) are allocated
     * to physical registers / spill slots instead of the fixed rax+push/pop
     * dance, exercising the x86_regalloc pass. Args remain stack-backed for
     * [rbp-relative] access. */
    /* XRA disabled below if function has local variables */
    mc->use_xra = 0; /* XRA disabled — needs redesign for expression chains */

    /* Disable XRA for functions with local variables — XRA spill/reload
     * for named locals is not yet implemented. The non-XRA path uses
     * stack-based locals which always work correctly. */
    if (mc->use_xra && mc->need_frame) {
        MinicLexer probe = mc->lex;
        if (minic_cur(&probe)->type == TOK_LBRACE) {
            minic_advance(&probe); /* skip { */
            int depth = 1;
            while (depth > 0 && minic_cur(&probe)->type != TOK_EOF) {
                if (minic_cur(&probe)->type == TOK_LBRACE) depth++;
                if (minic_cur(&probe)->type == TOK_RBRACE) depth--;
                if (depth == 1 && minic_is_type(minic_cur(&probe)->type)) {
                    mc->use_xra = 0;
                    break;
                }
                minic_advance(&probe);
            }
        }
    }

    if (mc->use_xra) {
        xra_init(&mc->ra, mc->n_args);
        xra_emit_load_args(&mc->ra, &mc->enc);
    }

    /* Emit stack frame allocation with a placeholder immediate. We patch it
     * after compiling the body once we know the actual stack needed.
     * For now, emit sub rsp, 0 (will be patched). Encoding: 83 EC imm8 or
     * 81 EC imm32. We use imm32 form (5 bytes: 48 81 EC imm32) with
     * placeholder 0, and patch the 4 bytes after compilation. */
    size_t frame_patch_pos = mc->enc.pos + 3;  /* offset of the imm32 */
    if (mc->need_frame) {
        MC_EMIT(mc, wx86_sub_reg_imm32(&mc->enc, WREG_RSP, 0));  /* placeholder */
    }

    mc->in_func = 1;

    /* Body */
    minic_expect(&mc->lex, TOK_LBRACE);
    while (minic_cur(&mc->lex)->type != TOK_RBRACE && minic_cur(&mc->lex)->type != TOK_EOF) {
        compile_stmt(mc);
        if (mc->error) break;
    }
    minic_expect(&mc->lex, TOK_RBRACE);

    mc->in_func = 0;

    /* Patch the stack frame size (slow path only) */
    if (mc->need_frame) {
        int frame_size = (-mc->scope.stack_offset);
        /* Only add scratch space for push/pop expression evaluation
         * (non-XRA path). XRA uses registers, no scratch needed. */
        if (!mc->use_xra) frame_size += 256;
        frame_size = (frame_size + 15) & ~15;
        if (frame_size < 16) frame_size = 16;  /* minimum aligned frame */
        mc->enc.buf[frame_patch_pos + 0] = (uint8_t)(frame_size & 0xFF);
        mc->enc.buf[frame_patch_pos + 1] = (uint8_t)((frame_size >> 8) & 0xFF);
        mc->enc.buf[frame_patch_pos + 2] = (uint8_t)((frame_size >> 16) & 0xFF);
        mc->enc.buf[frame_patch_pos + 3] = (uint8_t)((frame_size >> 24) & 0xFF);
    }

    /* Epilogue — FAST PATH: restore rbp if pushed, then ret */
    if (!mc->need_frame && !mc->use_xra) {
        if (mc->n_args > 0) {
            MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RBP));
        }
        MC_EMIT(mc, wx86_ret(&mc->enc));
        return 0;
    }

    /* SLOW PATH: full epilogue */
    if (mc->use_xra) {
        /* The default-return-0 must still land in rax before the allocator
         * epilogue restores callee-saved regs and returns. */
        MC_EMIT(mc, wx86_zero_reg(&mc->enc, WREG_RAX));
        xra_emit_return(&mc->ra, &mc->enc);
    } else {
        MC_EMIT(mc, wx86_zero_reg(&mc->enc, WREG_RAX));
        MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RSP, WREG_RBP));
        MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RBP));
        MC_EMIT(mc, wx86_ret(&mc->enc));
    }

    return 0;
}

/* -- Public API: jit_minic_compile() ------------------------------ */

JITResult jit_minic_compile(JITContext *ctx,
                             const char *source,
                             JITLang lang,
                             const char *fn_name,
                             JITFunc *out_func) {
    (void)lang;

    if (!source || !out_func) return JIT_ERR_COMPILE;

    /* If source doesn't start with a type keyword, it's a raw expression.
     * Wrap it: "long fn(long a, long b) { return (expr); }" */
    MinicLexer probe;
    minic_lex_init(&probe, source);
    int is_expr = !minic_is_type(minic_cur(&probe)->type) &&
                  minic_cur(&probe)->type != TOK_STRUCT;  /* struct decl, not expr */

    char *wrapped = NULL;
    const char *compile_src = source;

    if (is_expr) {
        size_t src_len = strlen(source);
        const char *fname = (fn_name && fn_name[0]) ? fn_name : "minic_fn";
        /* Probe which arg-names a-f the expression uses, so we declare
         * exactly that many parameters (and no garbage registers). */
        const char *names[] = {"a","b","c","d","e","f"};
        int n_used = 0;
        for (int i = 0; i < 6; i++) {
            /* Match name as a standalone identifier (bounded by non-alpha). */
            const char *p = source;
            while ((p = strstr(p, names[i])) != NULL) {
                char before = p == source ? ' ' : *(p - 1);
                char after = p[strlen(names[i])];
                if (!((before >= 'a' && before <= 'z') ||
                      (before >= 'A' && before <= 'Z') || (before == '_') ||
                      (after >= 'a' && after <= 'z') ||
                      (after >= 'A' && after <= 'Z') || (after == '_'))) {
                    n_used = i + 1;  /* at least up through this name */
                    break;
                }
                p += strlen(names[i]);
            }
        }
        if (n_used < 1) n_used = 1;  /* always declare at least arg 'a' */
        /* Build the arg list: (long a, long b, ..., long <last>) */
        char arglist[64] = "";
        char *q = arglist;
        for (int i = 0; i < n_used; i++) {
            q += snprintf(q, sizeof(arglist) - (q - arglist),
                          "%s%s%s", (i ? ", " : ""), "long ", names[i]);
        }
        size_t wrap_len = 160 + src_len + strlen(fname) + (size_t)(q - arglist);
        wrapped = (char *)malloc(wrap_len);
        if (!wrapped) return JIT_ERR_ALLOC;
        snprintf(wrapped, wrap_len,
                 "long %s(%s) { return (%s); }",
                 fname, arglist, source);
        compile_src = wrapped;
    }

    MinicCompiler mc;
    memset(&mc, 0, sizeof(mc));
    minic_lex_init(&mc.lex, compile_src);
    wx86_enc_init_dynamic(&mc.enc, 4096);
    minic_type_registry_init(&mc.types);   /* Subsystem A: type system */
    /* Subsystem C: opt-in branch profile. WUBU_JIT_PGO=1 enables counter
     * increment emission before each jcc; default off so production builds
     * get zero overhead and a stable counter array is owned by the runtime. */
    mc.profile_enabled = getenv("WUBU_JIT_PGO") && getenv("WUBU_JIT_PGO")[0] == '1';
    if (mc.profile_enabled) jbp_init(JBP_MAX_BRANCHES);

    const char *target = (fn_name && fn_name[0]) ? fn_name : NULL;

    /* Scan for target function name if not specified */
    if (!target) {
        MinicLexer scan_lex;
        minic_lex_init(&scan_lex, compile_src);
        /* Advance through tokens looking for first function name */
        while (minic_cur(&scan_lex)->type != TOK_EOF) {
            if (minic_is_type(minic_cur(&scan_lex)->type)) {
                minic_advance(&scan_lex);
                if (minic_cur(&scan_lex)->type == TOK_IDENT) {
                    /* Found first function name */
                    static char first_fn[64];
                    snprintf(first_fn, sizeof(first_fn), "%s", minic_cur(&scan_lex)->text);
                    target = first_fn;
                    break;
                }
            }
            minic_advance(&scan_lex);
        }
    }

    if (!target) {
        wx86_enc_free(&mc.enc);
        if (wrapped) free(wrapped);
        return JIT_ERR_COMPILE;
    }

    /* Compile: walk all function declarations, only emit for target */
    while (minic_cur(&mc.lex)->type != TOK_EOF && !mc.error) {
        /* Subsystem A: a top-level `struct Name { ... };` registers a type
         * (with #19 field reordering) in the registry and is skipped for
         * codegen. */
        if (minic_cur(&mc.lex)->type == TOK_STRUCT) {
            compile_struct_decl(&mc);
            continue;
        }
        if (!minic_is_type(minic_cur(&mc.lex)->type)) {
            minic_advance(&mc.lex);
            continue;
        }

        /* Peek function name */
        int save_pos = mc.lex.pos;
        MinicToken save_cur = mc.lex.cur;
        MinicToken save_peek = mc.lex.peek;
        minic_advance(&mc.lex);  /* skip type */
        char peek_name[64] = {0};
        if (minic_cur(&mc.lex)->type == TOK_IDENT)
            snprintf(peek_name, sizeof(peek_name), "%s", minic_cur(&mc.lex)->text);
        /* Restore */
        mc.lex.pos = save_pos;
        mc.lex.cur = save_cur;
        mc.lex.peek = save_peek;

        compile_func(&mc, target);
        if (mc.enc.pos > 0) break;  /* Found and compiled target */
    }

    if (mc.error || mc.enc.pos == 0) {
        wx86_enc_free(&mc.enc);
        if (wrapped) free(wrapped);
        return mc.error ? JIT_ERR_COMPILE : JIT_ERR_LINK;
    }

    /* Copy encoder buffer into executable memory */
    if (mc.use_xra)
        mc_peephole_elim_mov_roundtrip(&mc.enc);  /* drop no-op mov rax,rX;mov rX,rax pairs */
    void *exec = jit_alloc_exec(mc.enc.pos);
    if (!exec) {
        wx86_enc_free(&mc.enc);
        if (wrapped) free(wrapped);
        return JIT_ERR_ALLOC;
    }
    memcpy(exec, mc.enc.buf, mc.enc.pos);

    out_func->code = exec;
    out_func->code_size = mc.enc.pos;
    out_func->backend = JIT_BACKEND_MMAP;  /* Uses mmap executable memory */
    out_func->name = strdup(target);
    out_func->n_args = mc.n_args;

    jit_stats_add_alloc(ctx, mc.enc.pos);
    jit_stats_inc_compiled(ctx);

    wx86_enc_free(&mc.enc);
    if (wrapped) free(wrapped);
    return JIT_OK;
}

