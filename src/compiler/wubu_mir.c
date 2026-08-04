/*
 * wubu_mir.c -- the WuBuOS mid-level IR: builder + dumper.
 *
 * The hourglass neck: AST -> MIR -> ISA drivers. Virtual registers
 * make the register-clobber bug class impossible by construction.
 *
 * C11, self-contained.
 */
#include "wubu_mir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void wubu_mir_init(wubu_mir_prog_t *p)
{
    memset(p, 0, sizeof(*p));
    p->n_labels = 1;
}

void wubu_mir_free(wubu_mir_prog_t *p)
{
    if (!p) return;
    free(p->ins);
    memset(p, 0, sizeof(*p));
}

static wubu_mir_instr_t *emit(wubu_mir_prog_t *p)
{
    if (p->n == p->cap) {
        size_t nc = p->cap ? p->cap * 2 : 64;
        wubu_mir_instr_t *ni =
            (wubu_mir_instr_t *)realloc(p->ins, nc * sizeof(*ni));
        if (!ni) return NULL;
        p->ins = ni;
        p->cap = nc;
    }
    memset(&p->ins[p->n], 0, sizeof(p->ins[p->n]));
    return &p->ins[p->n++];
}

wubu_vr_t wubu_mir_const(wubu_mir_prog_t *p, int64_t imm)
{
    wubu_mir_instr_t *i = emit(p);
    if (!i) return 0;
    i->op = MIR_CONST;
    i->dst = (wubu_vr_t)p->n;   /* fresh vr: reuse the instr index+1 */
    i->imm = imm;
    return i->dst;
}

wubu_vr_t wubu_mir_binop(wubu_mir_prog_t *p, wubu_mir_op_t op,
                         wubu_vr_t a, wubu_vr_t b)
{
    wubu_mir_instr_t *i = emit(p);
    if (!i) return 0;
    i->op = op;
    i->dst = (wubu_vr_t)p->n;
    i->a = a;
    i->b = b;
    return i->dst;
}

wubu_vr_t wubu_mir_unop(wubu_mir_prog_t *p, wubu_mir_op_t op, wubu_vr_t a)
{
    return wubu_mir_binop(p, op, a, 0);
}

wubu_vr_t wubu_mir_mov(wubu_mir_prog_t *p, wubu_vr_t a)
{
    return wubu_mir_binop(p, MIR_MOV, a, 0);
}

/* mov INTO a pre-chosen destination vr (for phi-merge: both arms of a
 * branch write the SAME merge vr, only one executes) */
wubu_vr_t wubu_mir_mov_to(wubu_mir_prog_t *p, wubu_vr_t dst, wubu_vr_t a)
{
    wubu_mir_instr_t *i = emit(p);
    if (!i) return 0;
    i->op = MIR_MOV;
    i->dst = dst;
    i->a = a;
    return dst;
}

uint32_t wubu_mir_new_label(wubu_mir_prog_t *p)
{
    return p->n_labels++;
}

void wubu_mir_jmp(wubu_mir_prog_t *p, uint32_t label)
{
    wubu_mir_instr_t *i = emit(p);
    if (!i) return;
    i->op = MIR_JMP;
    i->label = label;
}

void wubu_mir_jz(wubu_mir_prog_t *p, wubu_vr_t cond, uint32_t label)
{
    wubu_mir_instr_t *i = emit(p);
    if (!i) return;
    i->op = MIR_JZ;
    i->a = cond;
    i->label = label;
}

void wubu_mir_place_label(wubu_mir_prog_t *p, uint32_t label)
{
    wubu_mir_instr_t *i = emit(p);
    if (!i) return;
    i->op = MIR_LABEL;
    i->label = label;
}

void wubu_mir_ret(wubu_mir_prog_t *p, wubu_vr_t v)
{
    wubu_mir_instr_t *i = emit(p);
    if (!i) return;
    i->op = MIR_RET;
    i->a = v;
}

static const char *op_name(wubu_mir_op_t op)
{
    switch (op) {
    case MIR_CONST: return "const";
    case MIR_ADD:   return "add";
    case MIR_SUB:   return "sub";
    case MIR_MUL:   return "mul";
    case MIR_DIV:   return "div";
    case MIR_MOD:   return "mod";
    case MIR_AND:   return "and";
    case MIR_OR:    return "or";
    case MIR_XOR:   return "xor";
    case MIR_SHL:   return "shl";
    case MIR_SHR:   return "shr";
    case MIR_NEG:   return "neg";
    case MIR_NOT:   return "not";
    case MIR_EQ:    return "eq";
    case MIR_NE:    return "ne";
    case MIR_LT:    return "lt";
    case MIR_LE:    return "le";
    case MIR_GT:    return "gt";
    case MIR_GE:    return "ge";
    case MIR_MOV:   return "mov";
    case MIR_JMP:   return "jmp";
    case MIR_JZ:    return "jz";
    case MIR_LABEL: return "label";
    case MIR_RET:   return "ret";
    default:        return "?";
    }
}

void wubu_mir_dump(const wubu_mir_prog_t *p)
{
    if (!p) return;
    for (size_t i = 0; i < p->n; i++) {
        const wubu_mir_instr_t *in = &p->ins[i];
        if (in->op == MIR_LABEL) {
            printf("L%u:\n", in->label);
            continue;
        }
        if (in->op == MIR_JMP) {
            printf("  %-6s L%u\n", op_name(in->op), in->label);
            continue;
        }
        if (in->op == MIR_JZ) {
            printf("  %-6s v%u == 0 -> L%u\n", op_name(in->op), in->a, in->label);
            continue;
        }
        if (in->op == MIR_RET) {
            printf("  %-6s v%u\n", op_name(in->op), in->a);
            continue;
        }
        if (in->op == MIR_CONST) {
            printf("  %-6s v%u = %lld\n", op_name(in->op), in->dst,
                   (long long)in->imm);
            continue;
        }
        if (in->op == MIR_NEG || in->op == MIR_NOT || in->op == MIR_MOV) {
            printf("  %-6s v%u = v%u\n", op_name(in->op), in->dst, in->a);
            continue;
        }
        printf("  %-6s v%u = v%u, v%u\n", op_name(in->op), in->dst, in->a, in->b);
    }
}
