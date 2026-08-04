/*
 * wubu_mir_lower.c -- AST -> MIR lowering (the hourglass neck).
 *
 * Takes a HolyC AST expression and emits MIR. Everything below this
 * point is ISA-agnostic: the SAME MIR feeds every driver in the
 * driver space (x86-64, RISC-V, PTX, ...). Adding a new ISA = adding
 * a driver, never touching the frontend.
 *
 * C11, self-contained. Uses the holyc AST.
 */
#include "holyc_ast.h"
#include "wubu_mir.h"
#include <stdint.h>

/* lower an AST expression into `p`; returns the vr holding its value */
wubu_vr_t wubu_mir_lower_expr(wubu_mir_prog_t *p, const HCASTNode *n)
{
    if (!n) return 0;

    switch (n->kind) {
    case HC_AST_INT_LIT:
        return wubu_mir_const(p, n->int_val);
    case HC_AST_CHAR_LIT:
        return wubu_mir_const(p, n->int_val);
    case HC_AST_BOOL_LIT:
        return wubu_mir_const(p, n->int_val);
    case HC_AST_FLOAT_LIT:
        /* the driver space: floats are a later wave (x87/SSE/FPU);
         * for now truncate like (I64) casts in the current codegen */
        return wubu_mir_const(p, (int64_t)n->float_val);

    case HC_AST_ADD: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        return wubu_mir_binop(p, MIR_ADD, a, b);
    }
    case HC_AST_SUB: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        return wubu_mir_binop(p, MIR_SUB, a, b);
    }
    case HC_AST_MUL: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        return wubu_mir_binop(p, MIR_MUL, a, b);
    }
    case HC_AST_DIV: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        return wubu_mir_binop(p, MIR_DIV, a, b);
    }
    case HC_AST_MOD: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        return wubu_mir_binop(p, MIR_MOD, a, b);
    }
    case HC_AST_BITAND: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        return wubu_mir_binop(p, MIR_AND, a, b);
    }
    case HC_AST_BITOR: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        return wubu_mir_binop(p, MIR_OR, a, b);
    }
    case HC_AST_BITXOR: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        return wubu_mir_binop(p, MIR_XOR, a, b);
    }
    case HC_AST_SHL: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        return wubu_mir_binop(p, MIR_SHL, a, b);
    }
    case HC_AST_SHR: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        return wubu_mir_binop(p, MIR_SHR, a, b);
    }
    case HC_AST_EQ: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        return wubu_mir_binop(p, MIR_EQ, a, b);
    }
    case HC_AST_NE: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        return wubu_mir_binop(p, MIR_NE, a, b);
    }
    case HC_AST_LT: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        return wubu_mir_binop(p, MIR_LT, a, b);
    }
    case HC_AST_LE: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        return wubu_mir_binop(p, MIR_LE, a, b);
    }
    case HC_AST_GT: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        return wubu_mir_binop(p, MIR_GT, a, b);
    }
    case HC_AST_GE: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        return wubu_mir_binop(p, MIR_GE, a, b);
    }

    /* logical && / || : lower to (a!=0) & (b!=0) / (a!=0) | (b!=0),
     * so the drivers never need short-circuit machinery. */
    case HC_AST_AND: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        wubu_vr_t za = wubu_mir_binop(p, MIR_NE, a, wubu_mir_const(p, 0));
        wubu_vr_t zb = wubu_mir_binop(p, MIR_NE, b, wubu_mir_const(p, 0));
        return wubu_mir_binop(p, MIR_AND, za, zb);
    }
    case HC_AST_OR: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->left);
        wubu_vr_t b = wubu_mir_lower_expr(p, n->right);
        wubu_vr_t za = wubu_mir_binop(p, MIR_NE, a, wubu_mir_const(p, 0));
        wubu_vr_t zb = wubu_mir_binop(p, MIR_NE, b, wubu_mir_const(p, 0));
        return wubu_mir_binop(p, MIR_OR, za, zb);
    }

    /* ternary: v = cond ? t : f  ->  branch on (cond != 0) */
    case HC_AST_TERNARY: {
        wubu_vr_t c = wubu_mir_lower_expr(p, n->cond);
        wubu_vr_t cz = wubu_mir_binop(p, MIR_NE, c, wubu_mir_const(p, 0));
        uint32_t l_false = wubu_mir_new_label(p);
        uint32_t l_end = wubu_mir_new_label(p);
        wubu_mir_jz(p, cz, l_false);
        /* then-branch: write the MERGE vr */
        wubu_vr_t merge = wubu_mir_const(p, 0);  /* reserve the merge vr */
        wubu_vr_t t = wubu_mir_lower_expr(p, n->then_branch);
        wubu_mir_mov_to(p, merge, t);
        wubu_mir_jmp(p, l_end);
        wubu_mir_place_label(p, l_false);
        /* else-branch: write the SAME merge vr */
        wubu_vr_t f = wubu_mir_lower_expr(p, n->else_branch);
        wubu_mir_mov_to(p, merge, f);
        wubu_mir_place_label(p, l_end);
        return merge;  /* only one arm executed; both wrote merge */
    }

    /* unary: the parser emits NEG / NOT / BITNOT with the operand as
     * child (never HC_AST_UNARY — that enum value is legacy). */
    case HC_AST_NEG: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->child);
        return wubu_mir_unop(p, MIR_NEG, a);
    }
    case HC_AST_NOT: {   /* logical ! : (a == 0) ? 1 : 0 */
        wubu_vr_t a = wubu_mir_lower_expr(p, n->child);
        return wubu_mir_binop(p, MIR_EQ, a, wubu_mir_const(p, 0));
    }
    case HC_AST_BITNOT: {
        wubu_vr_t a = wubu_mir_lower_expr(p, n->child);
        return wubu_mir_unop(p, MIR_NOT, a);
    }
    case HC_AST_UNARY: {  /* legacy fallback (should not occur) */
        wubu_vr_t a = wubu_mir_lower_expr(p, n->child);
        return wubu_mir_unop(p, MIR_NEG, a);
    }

    default:
        /* unsupported for the driver space yet — constant 0 (honest:
         * the lowering covers the expression battery; statements,
         * calls, globals are the next wave) */
        return wubu_mir_const(p, 0);
    }
}
