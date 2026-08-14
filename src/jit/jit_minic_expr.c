/*
 * jit_minic_expr.c — Expression compiler chain for Mini-C JIT.
 *
 * Handles: primary → multiplicative → additive → shift → bitwise_and →
 *          bitwise_xor → bitwise_or → compare → expr
 *
 * Module-local; include jit_minic_internal.h for shared state.
 */
#include "jit_minic_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>

/* Forward declarations of helpers defined in jit_minic.c */
extern int  mc_try_builtin(MinicCompiler *mc, const char *name, Wx86Reg aregs[6]);
extern void mc_emit_abs_branchless(MinicCompiler *mc);
extern void mc_emit_minmax_cmov(MinicCompiler *mc, Wx86Reg src, bool is_max);
extern int  mc_magic_sdiv(int64_t d, uint64_t *magic, int *shift);
extern void mc_emit_div_const(MinicCompiler *mc, Wx86Reg src, int64_t d);
extern int  mc_emit_mul_const(MinicCompiler *mc, Wx86Reg dst, Wx86Reg src, int64_t c);
extern void wx86_setcc_r8(Wx86Enc *e, Wx86CC cc, Wx86Reg dst);
extern Wx86Reg arg_reg(int idx);
extern int  mc_vreg_of_rax(MinicCompiler *mc);

void compile_primary(MinicCompiler *mc) {
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
            MC_EMIT(mc, wx86_zero_reg(&mc->enc, WREG_RAX));
            MC_EMIT(mc, wx86_call_reg(&mc->enc, WREG_RAX));
            return;
        }

        /* Variable reference */
        MinicVar *v = scope_find(&mc->scope, name);
        if (!v) {
            mc_error(mc, "undefined variable");
            MC_EMIT(mc, wx86_zero_reg(&mc->enc, WREG_RAX));
            return;
        }
        if (v->is_arg) {
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, arg_reg(v->slot)));
        } else {            MC_EMIT(mc, wx86_mov_reg_mem(&mc->enc, WREG_RAX, WREG_RBP, v->slot));
        }
        /* Subsystem A: `p->member` — p is a pointer to a struct (v->mty holds
         * the struct type index). Load [p + offsetof(member)] using the
         * reordered offset from the type registry. */
        if (v->mty > 0 && minic_cur(&mc->lex)->type == TOK_ARROW) {
            minic_advance(&mc->lex);  /* skip -> */
            if (minic_cur(&mc->lex)->type == TOK_IDENT) {
                int off = minic_type_member_offset(&mc->types, v->mty, minic_cur(&mc->lex)->text);
                int msz = minic_type_member_size(&mc->types, v->mty, minic_cur(&mc->lex)->text);
                if (off >= 0) {
                    if (msz == 1)
                        MC_EMIT(mc, wx86_movzx_byte_reg_mem(&mc->enc, WREG_RAX, WREG_RAX, off));
                    else
                        MC_EMIT(mc, wx86_mov_reg_mem(&mc->enc, WREG_RAX, WREG_RAX, off));
                } else {
                    mc_error(mc, "unknown struct member");
                    MC_EMIT(mc, wx86_zero_reg(&mc->enc, WREG_RAX));
                }
                minic_advance(&mc->lex);
            }
        }
        return;
    }

    if (tok->type == TOK_SIZEOF) {
        minic_advance(&mc->lex);
        minic_expect(&mc->lex, TOK_LPAREN);
        int64_t sz = 0;
        if (minic_cur(&mc->lex)->type == TOK_STRUCT) {
            minic_advance(&mc->lex);
            if (minic_cur(&mc->lex)->type == TOK_IDENT) {
                MinicType *t = minic_type_find(&mc->types, minic_cur(&mc->lex)->text);
                if (t) sz = t->size;  /* reordered size */
                minic_advance(&mc->lex);
            }
        } else if (minic_cur(&mc->lex)->type == TOK_LONG ||
                   minic_cur(&mc->lex)->type == TOK_INT ||
                   minic_cur(&mc->lex)->type == TOK_I64) {
            sz = 8; minic_advance(&mc->lex);
        } else if (minic_cur(&mc->lex)->type == TOK_U8) {
            sz = 1; minic_advance(&mc->lex);
        }
        minic_expect(&mc->lex, TOK_RPAREN);
        MC_EMIT(mc, wx86_mov_reg_imm64(&mc->enc, WREG_RAX, sz));
        mc->rax_is_const = true;
        mc->rax_const_val = sz;
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
    if (tok->type == TOK_TILDE) {
        /* Bitwise NOT: ~x */
        minic_advance(&mc->lex);
        compile_primary(mc);
        MC_EMIT(mc, wx86_not_reg(&mc->enc, WREG_RAX));
        if (mc->rax_is_const) mc->rax_const_val = ~mc->rax_const_val;
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
    /* Unknown token: report error and advance to prevent infinite loop */
    mc_error(mc, "unexpected token in expression");
    MC_EMIT(mc, wx86_zero_reg(&mc->enc, WREG_RAX));
    minic_advance(&mc->lex);
}

/* Branchless abs: rax = |rax|.  mov rdx,rax; sar rdx,63 (mask=sign);
 * xor rax,rdx; sub rax,rdx  → (x ^ (x>>63)) - (x>>63). No branch, no cmov. */
void mc_emit_abs_branchless(MinicCompiler *mc) {
    MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RDX, WREG_RAX));
    MC_EMIT(mc, wx86_sar_reg_imm8(&mc->enc, WREG_RDX, 63));
    MC_EMIT(mc, wx86_xor_reg_reg(&mc->enc, WREG_RAX, WREG_RDX));
    MC_EMIT(mc, wx86_sub_reg_reg(&mc->enc, WREG_RAX, WREG_RDX));
}

/* Branchless min/max via cmov: result already in rax, other operand in src.
 * For min(a,b): cmp a,b; cmovg rax,src  (rax = min). For max: cmovl. */
void mc_emit_minmax_cmov(MinicCompiler *mc, Wx86Reg src, bool is_max) {
    MC_EMIT(mc, wx86_cmp_reg_reg(&mc->enc, WREG_RAX, src));
    MC_EMIT(mc, wx86_cmovcc_reg_reg(&mc->enc, is_max ? WCC_L : WCC_G, WREG_RAX, src));
}

/* Try to recognize a builtin function call: abs, min, max, fabs.
 * Compiles them BRANCHLESSLY (no call, no branch). Returns 1 if handled.
 * On entry the '(' has been consumed; on return the ')' is consumed. */
int mc_try_builtin(MinicCompiler *mc, const char *name, Wx86Reg aregs[6]) {
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
int mc_magic_sdiv(int64_t d, uint64_t *magic, int *shift) {
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
void mc_emit_div_const(MinicCompiler *mc, Wx86Reg src, int64_t d) {
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
int mc_emit_mul_const(MinicCompiler *mc, Wx86Reg dst, Wx86Reg src, int64_t c) {
    if (c == 0) { MC_EMIT(mc, wx86_mov_reg_imm64(&mc->enc, dst, 0)); return 1; }
    if (c == 1) { MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, dst, src)); return 1; }
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


void compile_multiplicative(MinicCompiler *mc) {
    compile_primary(mc);

    while (minic_cur(&mc->lex)->type == TOK_STAR ||
           minic_cur(&mc->lex)->type == TOK_SLASH ||
           minic_cur(&mc->lex)->type == TOK_PERCENT) {
        MinicTokType op = minic_cur(&mc->lex)->type;
        minic_advance(&mc->lex);

        if (mc->use_xra) {
            /* Save LHS into a vreg (the allocator may spill it if the
             * scratch pool is exhausted by the RHS). */
            int lhs = mc_vreg_of_rax(mc);
            Wx86Reg lhs_hw = xra_get_reg(&mc->ra, lhs);
            if (lhs_hw == WREG_NONE)
                lhs_hw = xra_spill_load(&mc->ra, lhs, &mc->enc);
            /* Move rax (LHS value) into lhs_hw if not already there */
            if (lhs_hw != WREG_RAX) {
                MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, lhs_hw, WREG_RAX));
            }
            compile_primary(mc);
            /* RHS now in rax; LHS in lhs_hw. */
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
            } else if (op == TOK_PERCENT) {
                /* x % y: same as div but result is in rdx */
                MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RCX, WREG_RAX));  /* rcx = RHS */
                MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, lhs_hw));    /* rax = LHS */
                MC_EMIT(mc, wx86_cqo(&mc->enc));
                MC_EMIT(mc, wx86_idiv_reg(&mc->enc, WREG_RCX));
                MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, WREG_RDX));  /* remainder */
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
        } else {
            /* Non-XRA path: save LHS to stack, compile RHS, restore.
             * Using push/pop for correct nested expression handling. */
            MC_EMIT(mc, wx86_push_reg(&mc->enc, WREG_RAX));
            compile_primary(mc);

            /* Constant folding for the non-XRA path */
            if (mc->rax_is_const && op == TOK_STAR) {
                int64_t c = mc->rax_const_val;
                if (c == 0) { wx86_emit_byte(&mc->enc, 0x58); MC_EMIT(mc, wx86_zero_reg(&mc->enc, WREG_RAX)); goto mul_done_noxra; }
                if (c == 1) { wx86_emit_byte(&mc->enc, 0x58); goto mul_done_noxra; } /* pop rax = LHS */
            }
            if (mc->rax_is_const && op == TOK_SLASH && mc->rax_const_val == 1) {
                wx86_emit_byte(&mc->enc, 0x58); goto mul_done_noxra; /* pop rax = LHS */
            }

            if (op == TOK_STAR) {
                MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RCX, WREG_RAX));
                MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RAX));
                MC_EMIT(mc, wx86_imul_reg_reg(&mc->enc, WREG_RAX, WREG_RCX));
            } else if (op == TOK_PERCENT) {
                MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RCX, WREG_RAX));
                MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RAX));
                MC_EMIT(mc, wx86_cqo(&mc->enc));
                MC_EMIT(mc, wx86_idiv_reg(&mc->enc, WREG_RCX));
                MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, WREG_RDX)); /* remainder */
            } else {
                /* DIV */
                MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RCX, WREG_RAX));
                MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RAX));
                MC_EMIT(mc, wx86_cqo(&mc->enc));
                MC_EMIT(mc, wx86_idiv_reg(&mc->enc, WREG_RCX));
            }
            mul_done_noxra:
            mc->rax_is_const = false;
        }
    }
}

void compile_shift(MinicCompiler *mc) {
    compile_additive(mc);
    while (minic_cur(&mc->lex)->type == TOK_SHL || minic_cur(&mc->lex)->type == TOK_SHR) {
        MinicTokType op = minic_cur(&mc->lex)->type;
        minic_advance(&mc->lex);
        /* Shift: LHS in rax, RHS must be in rcx (x86 shift encoding) */
        MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_R11, WREG_RAX));
        compile_additive(mc);  /* RHS can be additive: a << b+c */
        MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RCX, WREG_RAX)); /* rcx = shift amount */
        MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, WREG_R11)); /* rax = value (r11 safe: mult/additive done) */
        if (op == TOK_SHL)
            MC_EMIT(mc, wx86_shl_reg_imm8(&mc->enc, WREG_RAX, 0)); /* placeholder — uses cl */
        else
            MC_EMIT(mc, wx86_shr_reg_imm8(&mc->enc, WREG_RAX, 0)); /* placeholder — uses cl */
        /* Fix: x86 shift-by-cl needs the opcode with /4 or /5, not imm8 */
        /* Actually shl rax,cl = 48 D3 E0, shr rax,cl = 48 D3 E8 */
        /* The imm8 version is shl rax,imm = 48 C1 E0 imm — wrong */
        /* Re-emit correctly: overwrite the last 4 bytes */
        mc->enc.pos -= 4; /* undo the shl_reg_imm8 */
        if (op == TOK_SHL) {
            wx86_emit_byte(&mc->enc, 0x48); /* REX.W */
            wx86_emit_byte(&mc->enc, 0xD3); /* /4 = shl r/m64, cl */
            wx86_emit_byte(&mc->enc, 0xE0); /* modrm(3,4,0) = rax */
        } else {
            wx86_emit_byte(&mc->enc, 0x48); /* REX.W */
            wx86_emit_byte(&mc->enc, 0xD3); /* /7 = sar r/m64, cl (arithmetic for signed) */
            wx86_emit_byte(&mc->enc, 0xF8); /* modrm(3,7,0) = rax */
        }
        mc->rax_is_const = false;
    }
}

void compile_bitwise_and(MinicCompiler *mc) {
    compile_shift(mc);
    while (minic_cur(&mc->lex)->type == TOK_AMP) {
        minic_advance(&mc->lex);
        if (mc->use_xra) {
            int lhs = mc_vreg_of_rax(mc);
            compile_shift(mc);
            Wx86Reg lhs_hw = xra_get_reg(&mc->ra, lhs);
            if (lhs_hw == WREG_NONE) lhs_hw = xra_spill_load(&mc->ra, lhs, &mc->enc);
            MC_EMIT(mc, wx86_and_reg_reg(&mc->enc, lhs_hw, WREG_RAX));
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, lhs_hw));
            xra_free_reg(&mc->ra, lhs_hw);
            xra_set_next_use(&mc->ra, lhs, -1);
        } else {
            MC_EMIT(mc, wx86_push_reg(&mc->enc, WREG_RAX));
            compile_shift(mc);
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_R10, WREG_RAX));
            MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RAX));
            MC_EMIT(mc, wx86_and_reg_reg(&mc->enc, WREG_RAX, WREG_R10));
        }
        mc->rax_is_const = false;
    }
}

void compile_bitwise_xor(MinicCompiler *mc) {
    compile_bitwise_and(mc);
    while (minic_cur(&mc->lex)->type == TOK_CARET) {
        minic_advance(&mc->lex);
        if (mc->use_xra) {
            int lhs = mc_vreg_of_rax(mc);
            compile_bitwise_and(mc);
            Wx86Reg lhs_hw = xra_get_reg(&mc->ra, lhs);
            if (lhs_hw == WREG_NONE) lhs_hw = xra_spill_load(&mc->ra, lhs, &mc->enc);
            MC_EMIT(mc, wx86_xor_reg_reg(&mc->enc, lhs_hw, WREG_RAX));
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, lhs_hw));
            xra_free_reg(&mc->ra, lhs_hw);
            xra_set_next_use(&mc->ra, lhs, -1);
        } else {
            MC_EMIT(mc, wx86_push_reg(&mc->enc, WREG_RAX));
            compile_bitwise_and(mc);
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_R10, WREG_RAX));
            MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RAX));
            MC_EMIT(mc, wx86_xor_reg_reg(&mc->enc, WREG_RAX, WREG_R10));
        }
        mc->rax_is_const = false;
    }
}

void compile_bitwise_or(MinicCompiler *mc) {
    compile_bitwise_xor(mc);
    while (minic_cur(&mc->lex)->type == TOK_PIPE) {
        minic_advance(&mc->lex);
        if (mc->use_xra) {
            int lhs = mc_vreg_of_rax(mc);
            compile_bitwise_xor(mc);
            Wx86Reg lhs_hw = xra_get_reg(&mc->ra, lhs);
            if (lhs_hw == WREG_NONE) lhs_hw = xra_spill_load(&mc->ra, lhs, &mc->enc);
            MC_EMIT(mc, wx86_or_reg_reg(&mc->enc, lhs_hw, WREG_RAX));
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, lhs_hw));
            xra_free_reg(&mc->ra, lhs_hw);
            xra_set_next_use(&mc->ra, lhs, -1);
        } else {
            MC_EMIT(mc, wx86_push_reg(&mc->enc, WREG_RAX));
            compile_bitwise_xor(mc);
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_R10, WREG_RAX));
            MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RAX));
            MC_EMIT(mc, wx86_or_reg_reg(&mc->enc, WREG_RAX, WREG_R10));
        }
        mc->rax_is_const = false;
    }
}

void compile_additive(MinicCompiler *mc) {
    compile_multiplicative(mc);

    while (minic_cur(&mc->lex)->type == TOK_PLUS ||
           minic_cur(&mc->lex)->type == TOK_MINUS) {
        MinicTokType op = minic_cur(&mc->lex)->type;
        minic_advance(&mc->lex);

        if (mc->use_xra && mc->need_frame) {
            /* XRA path for additive: only when frame is set up (has locals).
             * For leaf functions, the non-XRA path uses r10/r11 which works correctly. */
            int lhs = mc_vreg_of_rax(mc);
            compile_multiplicative(mc);
            Wx86Reg lhs_hw = xra_get_reg(&mc->ra, lhs);
            if (lhs_hw == WREG_NONE) {
                /* #10 memory-operand fusion */
                if (op == TOK_PLUS) {
                    int slot = xra_assign_spill_slot(&mc->ra, lhs);
                    int offset = -(8 * (slot + 1));
                    MC_EMIT(mc, wx86_add_rax_mem(&mc->enc, WREG_RBP, offset));
                    mc->rax_is_const = false;
                    xra_free_reg(&mc->ra, lhs_hw);
                    xra_set_next_use(&mc->ra, lhs, -1);
                    continue;
                }
                lhs_hw = xra_spill_load(&mc->ra, lhs, &mc->enc);
            }
            /* XRA constant folding: x+0 = x, x-0 = x */
            if (mc->rax_is_const && mc->rax_const_val == 0) {
                MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, lhs_hw));
                xra_free_reg(&mc->ra, lhs_hw);
                xra_set_next_use(&mc->ra, lhs, -1);
                mc->rax_is_const = false;
                continue;
            }
            if (op == TOK_PLUS)
                MC_EMIT(mc, wx86_add_reg_reg(&mc->enc, lhs_hw, WREG_RAX));
            else
                MC_EMIT(mc, wx86_sub_reg_reg(&mc->enc, lhs_hw, WREG_RAX));
            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RAX, lhs_hw));
            mc->rax_is_const = false;
            xra_free_reg(&mc->ra, lhs_hw);
            xra_set_next_use(&mc->ra, lhs, -1);
        } else {
            /* Non-XRA path: save LHS to stack, compile RHS, restore.
             * Push/pop correctly handles nested expressions. */
            MC_EMIT(mc, wx86_push_reg(&mc->enc, WREG_RAX));
            compile_multiplicative(mc);

            /* Non-XRA constant folding: x+0 = x, x-0 = x */
            if (mc->rax_is_const && mc->rax_const_val == 0) {
                /* Stack has LHS (x), rax has 0. Pop LHS into rax, discard 0. */
                wx86_emit_byte(&mc->enc, 0x58); /* pop rax = x */
                mc->rax_is_const = false;
                continue;
            }

            MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RCX, WREG_RAX));
            MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RAX));

            if (op == TOK_PLUS)
                MC_EMIT(mc, wx86_add_reg_reg(&mc->enc, WREG_RAX, WREG_RCX));
            else
                MC_EMIT(mc, wx86_sub_reg_reg(&mc->enc, WREG_RAX, WREG_RCX));
        }
    }
}

void compile_compare(MinicCompiler *mc) {
    compile_bitwise_or(mc);

    MinicTokType op = minic_cur(&mc->lex)->type;
    if (op == TOK_EQ || op == TOK_NEQ || op == TOK_LT ||
        op == TOK_GT || op == TOK_LEQ || op == TOK_GEQ) {
        minic_advance(&mc->lex);

        MC_EMIT(mc, wx86_push_reg(&mc->enc, WREG_RAX));
        compile_bitwise_or(mc);

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

void compile_expr(MinicCompiler *mc) {
    mc->last_was_compare = false;  /* reset; compile_compare re-sets if compare */
    compile_compare(mc);
}
