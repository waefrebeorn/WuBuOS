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
#include "jit_internal.h"
#include "wubu_x86.h"
#include "x86_regalloc.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>

/* -- Tokenizer ---------------------------------------------------- */






/* -- Variable / Scope -------------------------------------------- */

#define MINIC_MAX_VARS 64

typedef struct {
    char    name[64];
    int     slot;      /* Stack offset from RBP (negative), or arg reg index (0-5) */
    int     is_arg;    /* 1 if function argument (in register), 0 if local stack */
} MinicVar;

typedef struct {
    MinicVar    vars[MINIC_MAX_VARS];
    int         var_count;
    int         stack_offset;
} MinicScope;

static void scope_init(MinicScope *s) {
    memset(s, 0, sizeof(*s));
    s->stack_offset = -8;
}

static MinicVar *scope_find(MinicScope *s, const char *name) {
    for (int i = 0; i < s->var_count; i++) {
        if (strcmp(s->vars[i].name, name) == 0) return &s->vars[i];
    }
    return NULL;
}

static MinicVar *scope_add_local(MinicScope *s, const char *name) {
    if (s->var_count >= MINIC_MAX_VARS) return NULL;
    MinicVar *v = &s->vars[s->var_count++];
    snprintf(v->name, sizeof(v->name), "%s", name);
    v->is_arg = 0;
    v->slot = s->stack_offset;
    s->stack_offset -= 8;
    return v;
}

static MinicVar *scope_add_arg(MinicScope *s, const char *name, int arg_idx) {
    if (s->var_count >= MINIC_MAX_VARS) return NULL;
    MinicVar *v = &s->vars[s->var_count++];
    snprintf(v->name, sizeof(v->name), "%s", name);
    v->is_arg = 1;
    v->slot = arg_idx;
    return v;
}

/* Arg register mapping: arg 0-5 → RDI,RSI,RDX,RCX,R8,R9 */
static Wx86Reg arg_reg(int idx) {
    static const Wx86Reg regs[] = {
        WREG_RDI, WREG_RSI, WREG_RDX, WREG_RCX, WREG_R8, WREG_R9
    };
    if (idx >= 0 && idx < 6) return regs[idx];
    return WREG_RAX;
}

/* -- Forward declarations ---------------------------------------- */

typedef struct MinicCompiler MinicCompiler;
static void compile_expr(MinicCompiler *mc);
static void compile_stmt(MinicCompiler *mc);
static int mc_try_builtin(MinicCompiler *mc, const char *name, Wx86Reg aregs[6]);

/* -- Compiler State ---------------------------------------------- */

struct MinicCompiler {
    MinicLexer    lex;
    MinicScope    scope;
    Wx86Enc       enc;       /* Uses Wx86Enc from wubu_x86.h */
    int           n_args;
    int           in_func;
    int           error;
    char          error_msg[256];
    /* Linear-scan register allocator state */
    XRARegAlloc    ra;
    int            next_vreg;
    bool           use_xra;  /* true: use x86_regalloc instead of fixed rax+push/pop */
    /* Constant tracking: true when the current RAX holds a known constant.
     * Lets the allocator rematerialize instead of spilling to memory. */
    bool           rax_is_const;
    int64_t        rax_const_val;
    /* Compare-flag fusion (#2/#4): set when the last compiled expression was
     * a comparison (cmp reg,reg; setcc). The flags from the cmp are still live
     * until the next flag-writing instruction, so if/while can branch on the
     * compare's own cc instead of re-deriving via setcc+test. */
    bool           last_was_compare;
    Wx86CC         last_compare_cc;   /* cc of the comparison result */
    bool           last_compare_const0; /* RHS was constant 0 (used test) */
    size_t         cmp_after_pos;     /* encoder pos right after the cmp/test */
};

static void mc_error(MinicCompiler *mc, const char *msg) {
    mc->error = 1;
    snprintf(mc->error_msg, sizeof(mc->error_msg), "minic: %s (near '%s')",
             msg, mc->lex.cur.text);
}

/* -- Emit helper macros using Wx86Enc --------------------------- */

#define MC_EMIT(mc, call) do { if (!(mc)->error) { call; } } while(0)

/* -- Register-allocator helpers ----------------------------------- */

/* Materialize the current RAX result into a fresh virtual register, emitting
 * a store if the allocator must spill. Returns the assigned hardware reg, or
 * WREG_NONE if spilled (caller must reload before use). */
static int mc_vreg_of_rax(MinicCompiler *mc) {
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
        if (removed) continue;
        e->buf[w++] = e->buf[i++];
    }
    e->pos = w;
}


/* -- Expression Compiler (result in RAX) ------------------------ */

/* SETcc doesn't exist in wubu_x86.h yet — we'll emit it manually */
static void wx86_setcc_r8(Wx86Enc *e, Wx86CC cc, Wx86Reg dst) {
    (void)dst; /* We only support setting AL (RAX low byte) for now */
    /* 0F 90+cc /0 with ModRM=0xC0 (mod=3, reg=0, rm=0 = RAX) */
    wx86_emit_byte(e, 0x0F);
    wx86_emit_byte(e, 0x90 + (uint8_t)cc);
    wx86_emit_byte(e, 0xC0);  /* modrm(3, 0, 0) */
}

static void compile_primary(MinicCompiler *mc) {
    MinicToken *tok = minic_cur(&mc->lex);

    if (tok->type == TOK_NUMBER) {
        MC_EMIT(mc, wx86_mov_reg_imm64(&mc->enc, WREG_RAX, tok->ival));
        mc->rax_is_const = true;
        mc->rax_const_val = tok->ival;
        minic_advance(&mc->lex);
        return;
    }

    if (tok->type == TOK_IDENT) {
        char name[64];
        snprintf(name, sizeof(name), "%s", tok->text);
        mc->rax_is_const = false;  /* args are not compile-time constants */

        minic_advance(&mc->lex);
        if (minic_cur(&mc->lex)->type == TOK_LPAREN) {
            /* Function call */
            minic_advance(&mc->lex);

            Wx86Reg aregs[] = { WREG_RDI, WREG_RSI, WREG_RDX, WREG_RCX, WREG_R8, WREG_R9 };
            /* Builtin branchless intrinsics: abs/fabs/min/max. */
            if (mc_try_builtin(mc, name, aregs)) return;
            int nargs = 0;

            if (minic_cur(&mc->lex)->type != TOK_RPAREN) {
                compile_expr(mc);
                MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, aregs[0], WREG_RAX));
                nargs = 1;
                while (minic_cur(&mc->lex)->type == TOK_COMMA && nargs < 6) {
                    minic_advance(&mc->lex);
                    compile_expr(mc);
                    MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, aregs[nargs], WREG_RAX));
                    nargs++;
                }
            }
            minic_expect(&mc->lex, TOK_RPAREN);

            /* Placeholder call: mov rax, 0; call rax */
            MC_EMIT(mc, wx86_mov_reg_imm64(&mc->enc, WREG_RAX, 0));
            MC_EMIT(mc, wx86_call_reg(&mc->enc, WREG_RAX));
            return;
        }

        /* Variable reference */
        MinicVar *v = scope_find(&mc->scope, name);
        if (!v) {
            mc_error(mc, "undefined variable");
            MC_EMIT(mc, wx86_mov_reg_imm64(&mc->enc, WREG_RAX, 0));
            return;
        }
        if (v->is_arg) {
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, arg_reg(v->slot)));
        } else {
            MC_EMIT(mc, wx86_mov_reg_mem(&mc->enc, WREG_RAX, WREG_RBP, v->slot));
        }
        return;
    }

    if (tok->type == TOK_LPAREN) {
        minic_advance(&mc->lex);
        compile_expr(mc);
        minic_expect(&mc->lex, TOK_RPAREN);
        return;
    }

    if (tok->type == TOK_MINUS) {
        minic_advance(&mc->lex);
        compile_primary(mc);
        MC_EMIT(mc, wx86_neg_reg(&mc->enc, WREG_RAX));
        if (mc->rax_is_const) mc->rax_const_val = -mc->rax_const_val;
        return;
    }
    if (tok->type == TOK_NOT) {
        minic_advance(&mc->lex);
        compile_primary(mc);
        MC_EMIT(mc, wx86_cmp_reg_imm32(&mc->enc, WREG_RAX, 0));
        /* set al = (rax==0); movzx rax, al */
        MC_EMIT(mc, wx86_setcc_r8(&mc->enc, WCC_E, WREG_RAX));
        /* movzx: 0F B6 C0 */
        wx86_emit_byte(&mc->enc, 0x0F);
        wx86_emit_byte(&mc->enc, 0xB6);
        wx86_emit_byte(&mc->enc, 0xC0);
        return;
    }
}

/* -- Machine-level arithmetic helpers ----------------------------- */

/* Branchless abs: rax = |rax|.  mov rdx,rax; sar rdx,63 (mask=sign);
 * xor rax,rdx; sub rax,rdx  → (x ^ (x>>63)) - (x>>63). No branch, no cmov. */
static void mc_emit_abs_branchless(MinicCompiler *mc) {
    MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RDX, WREG_RAX));
    MC_EMIT(mc, wx86_sar_reg_imm8(&mc->enc, WREG_RDX, 63));
    MC_EMIT(mc, wx86_xor_reg_reg(&mc->enc, WREG_RAX, WREG_RDX));
    MC_EMIT(mc, wx86_sub_reg_reg(&mc->enc, WREG_RAX, WREG_RDX));
}

/* Branchless min/max via cmov: result already in rax, other operand in src.
 * For min(a,b): cmp a,b; cmovg rax,src  (rax = min). For max: cmovl. */
static void mc_emit_minmax_cmov(MinicCompiler *mc, Wx86Reg src, bool is_max) {
    MC_EMIT(mc, wx86_cmp_reg_reg(&mc->enc, WREG_RAX, src));
    MC_EMIT(mc, wx86_cmovcc_reg_reg(&mc->enc, is_max ? WCC_L : WCC_G, WREG_RAX, src));
}

/* Try to recognize a builtin function call: abs, min, max, fabs.
 * Compiles them BRANCHLESSLY (no call, no branch). Returns 1 if handled.
 * On entry the '(' has been consumed; on return the ')' is consumed. */
static int mc_try_builtin(MinicCompiler *mc, const char *name, Wx86Reg aregs[6]) {
    /* abs(x) / fabs(x) */
    if (strcmp(name, "abs") == 0 || strcmp(name, "fabs") == 0) {
        if (minic_cur(&mc->lex)->type != TOK_RPAREN) {
            compile_expr(mc);
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, aregs[0], WREG_RAX));
        }
        minic_expect(&mc->lex, TOK_RPAREN);
        /* compute abs on aregs[0] and move result to rax */
        MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, aregs[0]));
        mc_emit_abs_branchless(mc);
        return 1;
    }
    /* min(a,b) / max(a,b): two operands, branchless cmov select. */
    if (strcmp(name, "min") == 0 || strcmp(name, "max") == 0) {
        bool is_max = (strcmp(name, "max") == 0);
        if (minic_cur(&mc->lex)->type != TOK_RPAREN) {
            compile_expr(mc);
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, aregs[0], WREG_RAX));  /* a */
            if (minic_cur(&mc->lex)->type == TOK_COMMA) {
                minic_advance(&mc->lex);
                compile_expr(mc);
                MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, aregs[1], WREG_RAX));  /* b */
            }
        }
        minic_expect(&mc->lex, TOK_RPAREN);
        /* rax = aregs[0] (a); aregs[1] holds b. */
        MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, aregs[0]));
        mc_emit_minmax_cmov(mc, aregs[1], is_max);
        return 1;
    }
    return 0;
}

/* Granlund-Montgomery magic multiplier for SIGNED 64-bit division by a
 * constant d (|d| not a power of two, d != ±1, d != INT64_MIN).
 * Fills *magic (unsigned) and *shift; returns 0 on success, -1 if unsupported.
 * The division x/d is then: q = (int64_t)(((uint64_t)x*m)>>(64+shift)) with a
 * sign-correction added by the caller. (Hacker's Delight "magic" algorithm.) */
static int mc_magic_sdiv(int64_t d, uint64_t *magic, int *shift) {
    if (d == 0 || d == 1 || d == -1) return -1;
    if (d == INT64_MIN) return -1;
    uint64_t ad = (uint64_t)(d < 0 ? -d : d);
    if ((ad & (ad - 1)) == 0) return -1;   /* power of two handled elsewhere */

    uint64_t two63 = UINT64_C(1) << 63;
    uint64_t anc = two63 - 1 - (two63 - 1) % ad;
    /* Hacker's Delight "magic" loop: keep doubling until q1 >= delta. */
    uint64_t q1 = two63 / anc, r1 = two63 % anc;
    uint64_t q2 = two63 / ad,  r2 = two63 % ad;
    int p = 63;
    do {
        p++;
        q1 *= 2; r1 *= 2;
        if (r1 >= anc) { q1++; r1 -= anc; }
        q2 *= 2; r2 *= 2;
        if (r2 >= ad)  { q2++; r2 -= ad; }
        uint64_t delta = ad - 1 - r2;
        if (!(q1 < delta || (q1 == delta && r1 == 0))) break;
    } while (1);
    uint64_t m = q2 + 1;
    int s = p - 64;
    if (m == 0) return -1;
    *magic = m; *shift = s;
    return 0;
}

/* Emit x/d for SIGNED x in src, constant divisor d (d != 0), result to RAX.
 * Uses magic-multiply for general d, arithmetic shift for |d| power of two,
 * and negate for d<0. Scratch registers are allocated through the allocator
 * so they never collide with a live vreg (incl. from enclosing expressions). */
static void mc_emit_div_const(MinicCompiler *mc, Wx86Reg src, int64_t d) {
    if (d == 1) {  /* x/1 = x */
        if (src != WREG_RAX) MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, src));
        return;
    }
    if (d == -1) {  /* x/-1 = -x */
        if (src != WREG_RAX) MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, src));
        MC_EMIT(mc, wx86_neg_reg(&mc->enc, WREG_RAX));
        return;
    }
    uint64_t ad = (uint64_t)(d < 0 ? -d : d);

    /* Allocate two scratch vregs for the sign mask / magic holder. */
    int st1 = mc->next_vreg++;
    int st2 = mc->next_vreg++;
    Wx86Reg r1 = xra_alloc_evict(&mc->ra, st1, &mc->enc);   /* sign mask */
    Wx86Reg r2 = xra_alloc_evict(&mc->ra, st2, &mc->enc);   /* magic / temp */
    /* If the allocator spilled one, fall back to the plain idiv path. */
    if (r1 == WREG_NONE || r2 == WREG_NONE) {
        if (r1 != WREG_NONE) xra_free_reg(&mc->ra, r1);
        if (r2 != WREG_NONE) xra_free_reg(&mc->ra, r2);
        MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, src));
        MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RCX, src));
        MC_EMIT(mc, wx86_cqo(&mc->enc));
        MC_EMIT(mc, wx86_idiv_reg(&mc->enc, WREG_RCX));
        return;
    }

    MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, src));      /* rax = x */
    MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, r1, src));            /* r1 = x (sign later) */

    if ((ad & (ad - 1)) == 0) {
        /* |d| = 2^k: q = (x + (x>>63 & (d-1))) >> k (round toward zero) */
        int k = 0; uint64_t t = ad; while (t > 1) { t >>= 1; k++; }
        MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, r2, src));
        MC_EMIT(mc, wx86_sar_reg_imm8(&mc->enc, r2, 63));        /* sign mask */
        MC_EMIT(mc, wx86_and_reg_imm32(&mc->enc, r2, (int32_t)(ad - 1)));
        MC_EMIT(mc, wx86_add_reg_reg(&mc->enc, WREG_RAX, r2));
        MC_EMIT(mc, wx86_sar_reg_imm8(&mc->enc, WREG_RAX, (uint8_t)k));
    } else {
        uint64_t m; int s;
        if (mc_magic_sdiv(d, &m, &s) == 0) {
            int64_t M = (int64_t)m;
            /* gcc-verified: rax=x; r2=magic; imul r2 [rdx=hi];
             * if(M<0) hi+=x; hi>>=s; q=hi; q -= sign(x). */
            MC_EMIT(mc, wx86_mov_reg_imm64(&mc->enc, r2, m));    /* r2 = magic */
            MC_EMIT(mc, wx86_imul_rax_rm(&mc->enc, r2));         /* rdx:rax = x*m, hi->rdx */
            if (M < 0) {
                /* compensate negative magic: rdx += x (x still in r1) */
                MC_EMIT(mc, wx86_add_reg_reg(&mc->enc, WREG_RDX, r1));
            }
            if (s > 0) MC_EMIT(mc, wx86_sar_reg_imm8(&mc->enc, WREG_RDX, (uint8_t)s));
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, WREG_RDX)); /* q = hi>>s */
            /* q -= sign(x): r1 already holds x; r1 = x>>63 */
            MC_EMIT(mc, wx86_sar_reg_imm8(&mc->enc, r1, 63));
            MC_EMIT(mc, wx86_sub_reg_reg(&mc->enc, WREG_RAX, r1));
        } else {
            /* fallback: real idiv (e.g. INT64_MIN divisor) */
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RCX, src));
            MC_EMIT(mc, wx86_cqo(&mc->enc));
            MC_EMIT(mc, wx86_idiv_reg(&mc->enc, WREG_RCX));
        }
    }
    if (d < 0) MC_EMIT(mc, wx86_neg_reg(&mc->enc, WREG_RAX));  /* x / -d */
    xra_free_reg(&mc->ra, r1);
    xra_free_reg(&mc->ra, r2);
}

/* lea strength reduction: emit dst = src * const for small const via lea
 * (1 cyc, no flags) when possible; returns 1 if handled, 0 to fall back.
 *   *2,*4,*8 -> shl; *3,*5,*9,*15 -> lea [base + base*scale]. */
static int mc_emit_mul_const(MinicCompiler *mc, Wx86Reg dst, Wx86Reg src, int64_t c) {
    if (c == 2 || c == 4 || c == 8 || c == 16 || c == 32 || c == 64 || c == 128 || c == 256) {
        int k = 0; int64_t t = c; while (t > 1) { t >>= 1; k++; }
        MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, dst, src));
        MC_EMIT(mc, wx86_shl_reg_imm8(&mc->enc, dst, (uint8_t)k));
        return 1;
    }
    /* lea dst, [src + src*scale] gives src*(scale+1) for scale 1,2,4,8
     * (SIB scale field is 2 bits: 1,2,4,8 only). So c-1 must be 1,2,4,8. */
    static const int64_t lea_ok[] = { 3, 5, 9 };  /* 1+2, 1+4, 1+8 */
    for (int i = 0; i < 3; i++) {
        if (c == lea_ok[i]) {
            int scale = c - 1;  /* 2,4,8,16 → scale index 1,2,3,4 (2^s) */
            int s = 0; int64_t t = scale; while (t > 1) { t >>= 1; s++; }
            wx86_lea_scaled_index(&mc->enc, dst, src, s);
            return 1;
        }
    }
    return 0;  /* fall back to imul */
}


static void compile_multiplicative(MinicCompiler *mc) {
    compile_primary(mc);

    while (minic_cur(&mc->lex)->type == TOK_STAR ||
           minic_cur(&mc->lex)->type == TOK_SLASH) {
        MinicTokType op = minic_cur(&mc->lex)->type;
        minic_advance(&mc->lex);

        if (mc->use_xra) {
            /* Save LHS into a vreg (the allocator may spill it if the
             * scratch pool is exhausted by the RHS). */
            int lhs = mc_vreg_of_rax(mc);
            compile_primary(mc);
            /* RHS now in rax (possibly a known constant); LHS in vreg. */
            Wx86Reg lhs_hw = xra_get_reg(&mc->ra, lhs);
            if (lhs_hw == WREG_NONE)
                lhs_hw = xra_spill_load(&mc->ra, lhs, &mc->enc);
            if (op == TOK_STAR) {
                if (mc->rax_is_const) {
                    /* LHS * constant: lea/shl strength reduction or 3-op imul. */
                    int64_t c = mc->rax_const_val;
                    if (!mc_emit_mul_const(mc, WREG_RAX, lhs_hw, c))
                        MC_EMIT(mc, wx86_imul_reg_reg_imm32(&mc->enc, WREG_RAX, lhs_hw, (int32_t)c));
                } else {
                    MC_EMIT(mc, wx86_imul_reg_reg(&mc->enc, lhs_hw, WREG_RAX));  /* lhs *= rhs */
                    MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, lhs_hw));   /* result -> rax */
                }
            } else {
                if (mc->rax_is_const && mc->rax_const_val != 0) {
                    /* LHS / constant: magic-multiply or shift (no idiv). */
                    mc_emit_div_const(mc, lhs_hw, mc->rax_const_val);
                } else {
                    /* Result = LHS / RHS. Move LHS to rax, RHS to rcx, idiv. */
                    MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RCX, WREG_RAX));  /* rcx = RHS (divisor) */
                    MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, lhs_hw));    /* rax = LHS (dividend) */
                    MC_EMIT(mc, wx86_cqo(&mc->enc));                              /* sign-extend rax -> rdx:rax */
                    MC_EMIT(mc, wx86_idiv_reg(&mc->enc, WREG_RCX));               /* rax = LHS / RHS */
                }
            }
            mc->rax_is_const = false;  /* result of a runtime binop */
            xra_free_reg(&mc->ra, lhs_hw);
            xra_set_next_use(&mc->ra, lhs, -1);  /* LHS consumed; value dead */
        } else {
            MC_EMIT(mc, wx86_push_reg(&mc->enc, WREG_RAX));
            compile_primary(mc);

            if (op == TOK_STAR) {
                MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RCX, WREG_RAX));
                MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RAX));
                MC_EMIT(mc, wx86_imul_reg_reg(&mc->enc, WREG_RAX, WREG_RCX));
            } else {
                MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RCX, WREG_RAX));
                MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RAX));
                MC_EMIT(mc, wx86_cqo(&mc->enc));
                MC_EMIT(mc, wx86_idiv_reg(&mc->enc, WREG_RCX));
            }
        }
    }
}

static void compile_additive(MinicCompiler *mc) {
    compile_multiplicative(mc);

    while (minic_cur(&mc->lex)->type == TOK_PLUS ||
           minic_cur(&mc->lex)->type == TOK_MINUS) {
        MinicTokType op = minic_cur(&mc->lex)->type;
        minic_advance(&mc->lex);

        if (mc->use_xra) {
            /* Save LHS into a vreg; compile RHS into rax. Then combine. */
            int lhs = mc_vreg_of_rax(mc);
            compile_multiplicative(mc);
            Wx86Reg lhs_hw = xra_get_reg(&mc->ra, lhs);
            if (lhs_hw == WREG_NONE) {
                /* #10 memory-operand fusion: if the LHS is spilled and the op
                 * is ADD (commutative), fold the stack slot into the ALU op
                 * (add rax,[rbp-slot]) instead of reloading into a register.
                 * Saves a mov and the register. SUB is non-commutative so it
                 * keeps the reload path. */
                if (op == TOK_PLUS) {
                    int slot = xra_assign_spill_slot(&mc->ra, lhs);
                    int offset = -(8 * (slot + 1));
                    MC_EMIT(mc, wx86_add_rax_mem(&mc->enc, WREG_RBP, offset));
                    mc->rax_is_const = false;
                    xra_free_reg(&mc->ra, lhs_hw);
                    xra_set_next_use(&mc->ra, lhs, -1);
                    continue;  /* result already in rax */
                }
                lhs_hw = xra_spill_load(&mc->ra, lhs, &mc->enc);
            }
            if (op == TOK_PLUS)
                MC_EMIT(mc, wx86_add_reg_reg(&mc->enc, lhs_hw, WREG_RAX));  /* lhs += rhs (rax) */
            else
                MC_EMIT(mc, wx86_sub_reg_reg(&mc->enc, lhs_hw, WREG_RAX));  /* lhs -= rhs (rax) */
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, lhs_hw));      /* result back in rax */
            mc->rax_is_const = false;  /* result of a runtime binop */
            xra_free_reg(&mc->ra, lhs_hw);
            xra_set_next_use(&mc->ra, lhs, -1);  /* LHS consumed; value dead */
        } else {
            MC_EMIT(mc, wx86_push_reg(&mc->enc, WREG_RAX));
            compile_multiplicative(mc);

            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RCX, WREG_RAX));
            MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RAX));

            if (op == TOK_PLUS)
                MC_EMIT(mc, wx86_add_reg_reg(&mc->enc, WREG_RAX, WREG_RCX));
            else
                MC_EMIT(mc, wx86_sub_reg_reg(&mc->enc, WREG_RAX, WREG_RCX));
        }
    }
}

static void compile_compare(MinicCompiler *mc) {
    compile_additive(mc);

    MinicTokType op = minic_cur(&mc->lex)->type;
    if (op == TOK_EQ || op == TOK_NEQ || op == TOK_LT ||
        op == TOK_GT || op == TOK_LEQ || op == TOK_GEQ) {
        minic_advance(&mc->lex);

        MC_EMIT(mc, wx86_push_reg(&mc->enc, WREG_RAX));
        compile_additive(mc);

        MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RCX, WREG_RAX));
        MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RAX));

        if (mc->rax_is_const && mc->rax_const_val == 0 &&
            (op == TOK_EQ || op == TOK_NEQ)) {
            /* test rax,rax is 1 byte shorter than cmp rax,0 and sets ZF. */
            MC_EMIT(mc, wx86_test_reg_reg(&mc->enc, WREG_RAX, WREG_RAX));
        } else {
            MC_EMIT(mc, wx86_cmp_reg_reg(&mc->enc, WREG_RAX, WREG_RCX));
        }

        Wx86CC cc;
        switch (op) {
            case TOK_EQ:  cc = WCC_E;  break;
            case TOK_NEQ: cc = WCC_NE; break;
            case TOK_LT:  cc = WCC_L;  break;
            case TOK_GT:  cc = WCC_G;  break;
            case TOK_LEQ: cc = WCC_LE; break;
            case TOK_GEQ: cc = WCC_GE; break;
            default: cc = WCC_E; break;
        }
        /* Record the compare for flag-fusion: the cmp/test just emitted sets
         * the flags; a following if/while can branch on cc directly. */
        mc->last_was_compare = true;
        mc->last_compare_cc = cc;
        mc->last_compare_const0 = (mc->rax_is_const && mc->rax_const_val == 0);
        /* Record the position AFTER the cmp/test (before setcc) so a following
         * if/while can truncate back to here and emit a fused jcc on these flags. */
        mc->cmp_after_pos = mc->enc.pos;
        MC_EMIT(mc, wx86_setcc_r8(&mc->enc, cc, WREG_RAX));
        /* movzx rax, al */
        wx86_emit_byte(&mc->enc, 0x0F);
        wx86_emit_byte(&mc->enc, 0xB6);
        wx86_emit_byte(&mc->enc, 0xC0);
    }
}

static void compile_expr(MinicCompiler *mc) {
    mc->last_was_compare = false;  /* reset; compile_compare re-sets if compare */
    compile_compare(mc);
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

    minic_expect(&mc->lex, TOK_LBRACE);
    while (minic_cur(&mc->lex)->type != TOK_RBRACE && minic_cur(&mc->lex)->type != TOK_EOF)
        compile_stmt(mc);
    minic_expect(&mc->lex, TOK_RBRACE);

    if (minic_cur(&mc->lex)->type == TOK_ELSE) {
        minic_advance(&mc->lex);
        MC_EMIT(mc, wx86_jmp_rel32(&mc->enc));
        size_t end_patch = wx86_jmp_rel32_pos(&mc->enc);
        wx86_patch_rel32(&mc->enc, else_patch, mc->enc.pos);

        minic_expect(&mc->lex, TOK_LBRACE);
        while (minic_cur(&mc->lex)->type != TOK_RBRACE && minic_cur(&mc->lex)->type != TOK_EOF)
            compile_stmt(mc);
        minic_expect(&mc->lex, TOK_RBRACE);

        wx86_patch_rel32(&mc->enc, end_patch, mc->enc.pos);
    } else {
        wx86_patch_rel32(&mc->enc, else_patch, mc->enc.pos);
    }
}

static void compile_while_stmt(MinicCompiler *mc) {
    minic_advance(&mc->lex);

    /* #15 branch alignment: align the loop head to a 16-byte boundary with
     * single-byte NOPs so the loop-top target is 16-byte aligned. x86 loop
     * bodies re-fetch the target each iteration; 16B alignment of the back-edge
     * target avoids a 2-cycle front-end penalty on modern cores. Only pad when
     * within the encoder's spare capacity (padding is harmless: NOPs are
     * skipped by the pipeline). */
    size_t pad = (16 - (mc->enc.pos & 15)) & 15;
    for (size_t i = 0; i < pad; i++) wx86_emit_byte(&mc->enc, 0x90);
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

    minic_expect(&mc->lex, TOK_LBRACE);
    while (minic_cur(&mc->lex)->type != TOK_RBRACE && minic_cur(&mc->lex)->type != TOK_EOF)
        compile_stmt(mc);
    minic_expect(&mc->lex, TOK_RBRACE);

    /* Jump back to condition */
    MC_EMIT(mc, wx86_jmp_rel32(&mc->enc));
    wx86_patch_rel32(&mc->enc, wx86_jmp_rel32_pos(&mc->enc), loop_top);

    wx86_patch_rel32(&mc->enc, exit_patch, mc->enc.pos);
}

static void compile_return_stmt(MinicCompiler *mc) {
    minic_advance(&mc->lex);
    if (minic_cur(&mc->lex)->type != TOK_SEMI)
        compile_expr(mc);
    minic_expect(&mc->lex, TOK_SEMI);

    MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RSP, WREG_RBP));
    MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RBP));
    MC_EMIT(mc, wx86_ret(&mc->enc));
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
        if (v)
            MC_EMIT(mc, wx86_mov_mem_reg(&mc->enc, WREG_RBP, v->slot, WREG_RAX));
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
        if (!minic_is_type(minic_cur(&mc->lex)->type)) break;
        minic_advance(&mc->lex);  /* skip type */
        if (minic_cur(&mc->lex)->type == TOK_IDENT) {
            scope_add_arg(&mc->scope, minic_cur(&mc->lex)->text, mc->n_args);
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

    /* Prologue */
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

    /* Initialize the linear-scan register allocator for this function.
     * When enabled, expression temporaries (LHS of binops) are allocated
     * to physical registers / spill slots instead of the fixed rax+push/pop
     * dance, exercising the x86_regalloc pass. Args remain stack-backed for
     * [rbp-relative] access. */
    mc->next_vreg = mc->n_args;  /* vregs start after arg indices */
    mc->use_xra = (getenv("WUBU_JIT_XRA") != NULL);
    if (mc->use_xra) {
        xra_init(&mc->ra, mc->n_args);
        xra_emit_load_args(&mc->ra, &mc->enc);
    }

    mc->in_func = 1;

    /* Compile the body first into a temporary encoder to count locals.
     * (Actually, locals are declared as they appear, so the stack slots
     * are allocated during compilation. But we need to pre-allocate stack
     * space for the maximum depth of push operations in expressions.)
     * 
     * Simple approach: after compiling the body, we know the max stack_offset.
     * But we need to SUB RSP before the body. So we use a conservative
     * estimate: count the body's declarations, then emit RSP adjustment.
     * 
     * For now: we know locals go from stack_offset downward.
     * We need RSP to be at or below the lowest local.
     * After pushing args, RSP = RBP - args_size.
     * Locals start at RBP - (args_size + 8), going down.
     * We need to SUB RSP, (total_locals * 8) to make room.
     *
     * Actually, we can solve this differently: instead of mov [rbp+X], rax
     * for locals, we could use RSP-relative addressing. But that requires
     * tracking RSP changes, which is hard with push/pop.
     *
     * Easiest fix: move RSP down by a generous amount to avoid push
     * overwriting locals. We track the max stack depth used by push. */

    /* We'll use a two-pass approach:
     * 1. Reset encoder, compile function body to count locals
     * 2. Discard, re-compile with proper stack allocation
     * 
     * But that's complex. Instead, let's pre-allocate 256 bytes of stack
     * which is more than enough for most functions, and adjust later. */
    MC_EMIT(mc, wx86_sub_reg_imm32(&mc->enc, WREG_RSP, 256));
    /* After sub rsp, 256: rsp = rbp - 8*n - 256 */

    mc->in_func = 1;

    /* Body */
    minic_expect(&mc->lex, TOK_LBRACE);
    while (minic_cur(&mc->lex)->type != TOK_RBRACE && minic_cur(&mc->lex)->type != TOK_EOF) {
        compile_stmt(mc);
        if (mc->error) break;
    }
    minic_expect(&mc->lex, TOK_RBRACE);

    mc->in_func = 0;

    /* Epilogue */
    if (mc->use_xra) {
        /* The default-return-0 must still land in rax before the allocator
         * epilogue restores callee-saved regs and returns. */
        MC_EMIT(mc, wx86_mov_reg_imm64(&mc->enc, WREG_RAX, 0));
        xra_emit_return(&mc->ra, &mc->enc);
    } else {
        MC_EMIT(mc, wx86_mov_reg_imm64(&mc->enc, WREG_RAX, 0));
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
    int is_expr = !minic_is_type(minic_cur(&probe)->type);

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

