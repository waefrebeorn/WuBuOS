/*
 * jit_minic_cg.c — Full multi-target Mini-C compiler using abstract codegen.
 *
 * Supports the full Mini-C grammar:
 *   - Functions with up to 6 arguments (a-f)
 *   - Local variable declarations (long x = expr;)
 *   - Assignment (x = expr;)
 *   - if/else statements
 *   - while loops
 *   - return statements
 *   - Expressions: +,-,*,/,%,&,|,^,<<,>>, comparison, ==,!=,<,>,<=,>=
 *   - Hex literals (0xFF), decimal, negative
 *   - Parenthesized expressions
 *
 * Targets: x86-64 and ARM64 via CodeGen abstraction.
 */
#include "jit_codegen.h"
#include "jit.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <stdint.h>

/* -- Tokenizer ---------------------------------------------------- */
typedef enum {
    TOK_EOF = 0, TOK_IDENT, TOK_NUMBER, TOK_HEX,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH, TOK_PERCENT,
    TOK_AMP, TOK_PIPE, TOK_CARET, TOK_TILDE,
    TOK_SHL, TOK_SHR,
    TOK_LPAREN, TOK_RPAREN, TOK_LBRACE, TOK_RBRACE,
    TOK_SEMI, TOK_COMMA,
    TOK_ASSIGN, TOK_EQ, TOK_NE, TOK_LT, TOK_GT, TOK_LE, TOK_GE,
    TOK_RETURN, TOK_IF, TOK_ELSE, TOK_WHILE, TOK_LONG,
} CGTokType;

typedef struct {
    CGTokType type;
    char text[64];
    int64_t ival;
} CGToken;

typedef struct {
    const char *src;
    int pos;
    CGToken cur;
    int error;
} CGLexer;

static void cg_lex_init(CGLexer *l, const char *src) {
    l->src = src;
    l->pos = 0;
    l->error = 0;
    /* Advance to first token */
    /* (simplified — we parse on the fly) */
}

static char cg_peek(CGLexer *l) { return l->src[l->pos]; }
static char cg_adv(CGLexer *l) { return l->src[l->pos++]; }
static void cg_skip_ws(CGLexer *l) {
    while (l->src[l->pos] == ' ' || l->src[l->pos] == '\t' || l->src[l->pos] == '\n' || l->src[l->pos] == '\r')
        l->pos++;
}

static CGTokType cg_next_token(CGLexer *l, CGToken *tok) {
    cg_skip_ws(l);
    char c = cg_peek(l);
    if (c == '\0') { tok->type = TOK_EOF; return TOK_EOF; }

    /* Identifiers and keywords */
    if (isalpha(c) || c == '_') {
        int i = 0;
        while (i < 63 && (isalnum(cg_peek(l)) || cg_peek(l) == '_')) {
            tok->text[i++] = cg_adv(l);
        }
        tok->text[i] = '\0';
        tok->type = TOK_IDENT;
        if (strcmp(tok->text, "return") == 0) tok->type = TOK_RETURN;
        else if (strcmp(tok->text, "if") == 0) tok->type = TOK_IF;
        else if (strcmp(tok->text, "else") == 0) tok->type = TOK_ELSE;
        else if (strcmp(tok->text, "while") == 0) tok->type = TOK_WHILE;
        else if (strcmp(tok->text, "long") == 0) tok->type = TOK_LONG;
        return tok->type;
    }

    /* Numbers */
    if (isdigit(c)) {
        int i = 0;
        if (c == '0' && (l->src[l->pos + 1] == 'x' || l->src[l->pos + 1] == 'X')) {
            /* Hex */
            tok->text[i++] = cg_adv(l); /* 0 */
            tok->text[i++] = cg_adv(l); /* x */
            while (i < 62 && isxdigit(cg_peek(l))) {
                tok->text[i++] = cg_adv(l);
            }
            tok->text[i] = '\0';
            tok->ival = (int64_t)strtoull(tok->text, NULL, 16);
            tok->type = TOK_HEX;
            return TOK_HEX;
        }
        int64_t val = 0;
        while (isdigit(cg_peek(l))) {
            val = val * 10 + (cg_adv(l) - '0');
        }
        tok->ival = val;
        tok->type = TOK_NUMBER;
        return TOK_NUMBER;
    }

    /* Operators */
    cg_adv(l);
    tok->type = TOK_EOF;  /* default */
    switch (c) {
        case '+': tok->type = TOK_PLUS; break;
        case '-': tok->type = TOK_MINUS; break;
        case '*': tok->type = TOK_STAR; break;
        case '/': tok->type = TOK_SLASH; break;
        case '%': tok->type = TOK_PERCENT; break;
        case '&':
            if (cg_peek(l) == '&') { cg_adv(l); tok->type = TOK_AMP; }  /* && → & for now */
            else tok->type = TOK_AMP;
            break;
        case '|': tok->type = TOK_PIPE; break;
        case '^': tok->type = TOK_CARET; break;
        case '~': tok->type = TOK_TILDE; break;
        case '<':
            if (cg_peek(l) == '<') { cg_adv(l); tok->type = TOK_SHL; }
            else if (cg_peek(l) == '=') { cg_adv(l); tok->type = TOK_LE; }
            else tok->type = TOK_LT;
            break;
        case '>':
            if (cg_peek(l) == '>') { cg_adv(l); tok->type = TOK_SHR; }
            else if (cg_peek(l) == '=') { cg_adv(l); tok->type = TOK_GE; }
            else tok->type = TOK_GT;
            break;
        case '(': tok->type = TOK_LPAREN; break;
        case ')': tok->type = TOK_RPAREN; break;
        case '{': tok->type = TOK_LBRACE; break;
        case '}': tok->type = TOK_RBRACE; break;
        case ';': tok->type = TOK_SEMI; break;
        case ',': tok->type = TOK_COMMA; break;
        case '=':
            if (cg_peek(l) == '=') { cg_adv(l); tok->type = TOK_EQ; }
            else tok->type = TOK_ASSIGN;
            break;
        case '!':
            if (cg_peek(l) == '=') { cg_adv(l); tok->type = TOK_NE; }
            break;
        default: break;
    }
    return tok->type;
}

/* -- Compiler state ------------------------------------------------ */
#define CG_MAX_VARS 32
#define CG_MAX_ARGS 6

typedef struct {
    char name[32];
    CGReg reg;       /* register where this var lives */
    int is_arg;      /* 1 if function argument */
} CGVar;

typedef struct {
    CodeGen *cg;
    CGLexer lex;
    CGToken cur_tok;
    CGVar vars[CG_MAX_VARS];
    int n_vars;
    CGReg arg_regs[CG_MAX_ARGS];
    int n_args;
    int error;
    int stack_slots;  /* number of stack slots for locals */
    CGReg result_reg; /* register where result should go */
} CGCompiler;

/* -- Variable lookup ---------------------------------------------- */
static CGReg cg_find_var(CGCompiler *cc, const char *name) {
    for (int i = 0; i < cc->n_vars; i++) {
        if (strcmp(cc->vars[i].name, name) == 0)
            return cc->vars[i].reg;
    }
    return (CGReg)-1;
}

static CGReg cg_add_var(CGCompiler *cc, const char *name, int is_arg) {
    CGReg r;
    if (is_arg) {
        r = cc->arg_regs[cc->n_args];
        cc->n_args++;
    } else {
        /* Assign register: args first, then locals */
        r = (CGReg)(cc->n_args + cc->stack_slots + 3);  /* +3 to avoid R0-R2 (result/scratch) */
        if (r >= 16) r = (CGReg)(15);  /* clamp */
        cc->stack_slots++;
    }
    strncpy(cc->vars[cc->n_vars].name, name, 31);
    cc->vars[cc->n_vars].reg = r;
    cc->vars[cc->n_vars].is_arg = is_arg;
    cc->n_vars++;
    return r;
}

/* -- Token helpers ------------------------------------------------ */
static int cg_cur(CGCompiler *cc, CGTokType t) { return cc->cur_tok.type == t; }
static int cg_consume(CGCompiler *cc, CGTokType t) {
    if (cc->cur_tok.type == t) {
        cg_next_token(&cc->lex, &cc->cur_tok);
        return 1;
    }
    return 0;
}

/* Forward declarations */
static void cg_compile_expr(CGCompiler *cg, CGReg dst);
static void cg_compile_stmt(CGCompiler *cc);
static void cg_compile_block(CGCompiler *cc);

/* -- Constant folding --------------------------------------------- */
/* Attempt to evaluate a compile-time constant expression.
 * Returns 1 and stores result in *out on success, 0 on failure.
 * On failure, lexer state is restored. */

static int cg_const_eval(CGCompiler *cc, int64_t *out);

static int cg_const_eval_primary(CGCompiler *cc, int64_t *out) {
    /* Save lexer state */
    CGToken saved_tok = cc->cur_tok;
    CGLexer saved_lex = cc->lex;

    if (cg_consume(cc, TOK_LPAREN)) {
        if (cg_const_eval(cc, out) && cg_consume(cc, TOK_RPAREN)) return 1;
        /* Restore and fail */
        cc->cur_tok = saved_tok;
        cc->lex = saved_lex;
        return 0;
    }
    if (cg_cur(cc, TOK_NUMBER)) {
        *out = cc->cur_tok.ival;
        cg_consume(cc, TOK_NUMBER);
        return 1;
    }
    if (cg_cur(cc, TOK_HEX)) {
        *out = cc->cur_tok.ival;
        cg_consume(cc, TOK_HEX);
        return 1;
    }
    if (cg_cur(cc, TOK_MINUS)) {
        cg_consume(cc, TOK_MINUS);
        int64_t v;
        if (cg_const_eval_primary(cc, &v)) {
            *out = -v;
            return 1;
        }
        cc->cur_tok = saved_tok;
        cc->lex = saved_lex;
        return 0;
    }
    if (cg_cur(cc, TOK_TILDE)) {
        cg_consume(cc, TOK_TILDE);
        int64_t v;
        if (cg_const_eval_primary(cc, &v)) {
            *out = ~v;
            return 1;
        }
        cc->cur_tok = saved_tok;
        cc->lex = saved_lex;
        return 0;
    }
    /* Not a constant */
    cc->cur_tok = saved_tok;
    cc->lex = saved_lex;
    return 0;
}

static int cg_const_eval_multiplicative(CGCompiler *cc, int64_t *out) {
    CGToken saved_start_tok = cc->cur_tok;
    CGLexer saved_start_lex = cc->lex;
    int64_t lhs;
    if (!cg_const_eval_primary(cc, &lhs)) { cc->cur_tok = saved_start_tok; cc->lex = saved_start_lex; return 0; }
    for (;;) {
        CGToken saved_tok = cc->cur_tok;
        CGLexer saved_lex = cc->lex;
        if (cg_consume(cc, TOK_STAR)) {
            int64_t rhs;
            if (!cg_const_eval_primary(cc, &rhs)) { cc->cur_tok = saved_tok; cc->lex = saved_lex; return 0; }
            lhs = lhs * rhs;
        } else if (cg_consume(cc, TOK_SLASH)) {
            int64_t rhs;
            if (!cg_const_eval_primary(cc, &rhs)) { cc->cur_tok = saved_tok; cc->lex = saved_lex; return 0; }
            if (rhs == 0) { *out = 0; return 1; }
            lhs = lhs / rhs;
        } else if (cg_consume(cc, TOK_PERCENT)) {
            int64_t rhs;
            if (!cg_const_eval_primary(cc, &rhs)) { cc->cur_tok = saved_tok; cc->lex = saved_lex; return 0; }
            if (rhs == 0) { *out = 0; return 1; }
            lhs = lhs % rhs;
        } else break;
    }
    *out = lhs;
    return 1;
}

static int cg_const_eval_additive(CGCompiler *cc, int64_t *out) {
    CGToken saved_start_tok = cc->cur_tok;
    CGLexer saved_start_lex = cc->lex;
    int64_t lhs;
    if (!cg_const_eval_multiplicative(cc, &lhs)) { cc->cur_tok = saved_start_tok; cc->lex = saved_start_lex; return 0; }
    for (;;) {
        CGToken saved_tok = cc->cur_tok;
        CGLexer saved_lex = cc->lex;
        if (cg_consume(cc, TOK_PLUS)) {
            int64_t rhs;
            if (!cg_const_eval_multiplicative(cc, &rhs)) { cc->cur_tok = saved_tok; cc->lex = saved_lex; return 0; }
            lhs = lhs + rhs;
        } else if (cg_consume(cc, TOK_MINUS)) {
            int64_t rhs;
            if (!cg_const_eval_multiplicative(cc, &rhs)) { cc->cur_tok = saved_tok; cc->lex = saved_lex; return 0; }
            lhs = lhs - rhs;
        } else break;
    }
    *out = lhs;
    return 1;
}

static int cg_const_eval_shift(CGCompiler *cc, int64_t *out) {
    CGToken saved_start_tok = cc->cur_tok;
    CGLexer saved_start_lex = cc->lex;
    int64_t lhs;
    if (!cg_const_eval_additive(cc, &lhs)) { cc->cur_tok = saved_start_tok; cc->lex = saved_start_lex; return 0; }
    for (;;) {
        CGToken saved_tok = cc->cur_tok;
        CGLexer saved_lex = cc->lex;
        if (cg_consume(cc, TOK_SHL)) {
            int64_t rhs;
            if (!cg_const_eval_additive(cc, &rhs)) { cc->cur_tok = saved_tok; cc->lex = saved_lex; return 0; }
            lhs = lhs << (rhs & 63);
        } else if (cg_consume(cc, TOK_SHR)) {
            int64_t rhs;
            if (!cg_const_eval_additive(cc, &rhs)) { cc->cur_tok = saved_tok; cc->lex = saved_lex; return 0; }
            lhs = lhs >> (rhs & 63);
        } else break;
    }
    *out = lhs;
    return 1;
}

static int cg_const_eval_bitwise(CGCompiler *cc, int64_t *out) {
    CGToken saved_start_tok = cc->cur_tok;
    CGLexer saved_start_lex = cc->lex;
    int64_t lhs;
    if (!cg_const_eval_shift(cc, &lhs)) { cc->cur_tok = saved_start_tok; cc->lex = saved_start_lex; return 0; }
    for (;;) {
        CGToken saved_tok = cc->cur_tok;
        CGLexer saved_lex = cc->lex;
        if (cg_consume(cc, TOK_AMP)) {
            int64_t rhs;
            if (!cg_const_eval_shift(cc, &rhs)) { cc->cur_tok = saved_tok; cc->lex = saved_lex; return 0; }
            lhs = lhs & rhs;
        } else if (cg_consume(cc, TOK_CARET)) {
            int64_t rhs;
            if (!cg_const_eval_shift(cc, &rhs)) { cc->cur_tok = saved_tok; cc->lex = saved_lex; return 0; }
            lhs = lhs ^ rhs;
        } else if (cg_consume(cc, TOK_PIPE)) {
            int64_t rhs;
            if (!cg_const_eval_shift(cc, &rhs)) { cc->cur_tok = saved_tok; cc->lex = saved_lex; return 0; }
            lhs = lhs | rhs;
        } else break;
    }
    *out = lhs;
    return 1;
}

static int cg_const_eval_compare(CGCompiler *cc, int64_t *out) {
    CGToken saved_start_tok = cc->cur_tok;
    CGLexer saved_start_lex = cc->lex;
    int64_t lhs;
    if (!cg_const_eval_bitwise(cc, &lhs)) { cc->cur_tok = saved_start_tok; cc->lex = saved_start_lex; return 0; }
    CGToken saved_tok = cc->cur_tok;
    CGLexer saved_lex = cc->lex;
    if (cg_consume(cc, TOK_EQ)) {
        int64_t rhs;
        if (!cg_const_eval_bitwise(cc, &rhs)) { cc->cur_tok = saved_tok; cc->lex = saved_lex; return 0; }
        *out = (lhs == rhs) ? 1 : 0;
    } else if (cg_consume(cc, TOK_NE)) {
        int64_t rhs;
        if (!cg_const_eval_bitwise(cc, &rhs)) { cc->cur_tok = saved_tok; cc->lex = saved_lex; return 0; }
        *out = (lhs != rhs) ? 1 : 0;
    } else if (cg_consume(cc, TOK_LT)) {
        int64_t rhs;
        if (!cg_const_eval_bitwise(cc, &rhs)) { cc->cur_tok = saved_tok; cc->lex = saved_lex; return 0; }
        *out = (lhs < rhs) ? 1 : 0;
    } else if (cg_consume(cc, TOK_GT)) {
        int64_t rhs;
        if (!cg_const_eval_bitwise(cc, &rhs)) { cc->cur_tok = saved_tok; cc->lex = saved_lex; return 0; }
        *out = (lhs > rhs) ? 1 : 0;
    } else if (cg_consume(cc, TOK_LE)) {
        int64_t rhs;
        if (!cg_const_eval_bitwise(cc, &rhs)) { cc->cur_tok = saved_tok; cc->lex = saved_lex; return 0; }
        *out = (lhs <= rhs) ? 1 : 0;
    } else if (cg_consume(cc, TOK_GE)) {
        int64_t rhs;
        if (!cg_const_eval_bitwise(cc, &rhs)) { cc->cur_tok = saved_tok; cc->lex = saved_lex; return 0; }
        *out = (lhs >= rhs) ? 1 : 0;
    } else {
        *out = lhs;
    }
    return 1;
}

static int cg_const_eval(CGCompiler *cc, int64_t *out) {
    return cg_const_eval_compare(cc, out);
}

static int is_power_of_2(int64_t v) {
    return v > 0 && (v & (v - 1)) == 0;
}

static int ilog2(int64_t v) {
    int shift = 0;
    while (v > 1) { v >>= 1; shift++; }
    return shift;
}

/* -- Expression compiler ------------------------------------------ */

static int cg_compile_primary(CGCompiler *cc, CGReg dst, int64_t *out_const) {
    if (out_const) *out_const = 0;
    if (cg_consume(cc, TOK_LPAREN)) {
        cg_compile_expr(cc, dst);
        cg_consume(cc, TOK_RPAREN);
        return 0;  /* not a simple constant */
    }
    if (cg_cur(cc, TOK_NUMBER)) {
        if (out_const) *out_const = cc->cur_tok.ival;
        cg_mov_imm(cc->cg, dst, cc->cur_tok.ival);
        cg_consume(cc, TOK_NUMBER);
        return 1;  /* constant */
    }
    if (cg_cur(cc, TOK_HEX)) {
        if (out_const) *out_const = cc->cur_tok.ival;
        cg_mov_imm(cc->cg, dst, cc->cur_tok.ival);
        cg_consume(cc, TOK_HEX);
        return 1;  /* constant */
    }
    if (cg_cur(cc, TOK_MINUS)) {
        cg_consume(cc, TOK_MINUS);
        int64_t v;
        if (cg_compile_primary(cc, dst, &v)) {
            /* Negate: dst = 0 - dst */
            cg_mov_imm(cc->cg, CG_REG_9, 0);
            cg_sub_reg(cc->cg, dst, CG_REG_9, dst);
            if (out_const) *out_const = -v;
            return 1;
        }
        cg_mov_imm(cc->cg, dst, 0);
        return 0;
    }
    if (cg_cur(cc, TOK_TILDE)) {
        cg_consume(cc, TOK_TILDE);
        int64_t v;
        if (cg_compile_primary(cc, dst, &v)) {
            cg_mov_imm(cc->cg, CG_REG_9, -1);
            cg_eor_reg(cc->cg, dst, dst, CG_REG_9);
            if (out_const) *out_const = ~v;
            return 1;
        }
        cg_mov_imm(cc->cg, dst, 0);
        return 0;
    }
    if (cg_cur(cc, TOK_IDENT)) {
        const char *name = cc->cur_tok.text;
        cg_consume(cc, TOK_IDENT);
        CGReg r = cg_find_var(cc, name);
        if ((int)r < 0) {
            /* Auto-detect function arguments (single letters a-f) */
            int len = strlen(name);
            if (len == 1 && name[0] >= 'a' && name[0] < 'g') {
                int idx = name[0] - 'a';
                if (idx < CG_MAX_ARGS) {
                    r = cg_add_var(cc, name, 1);
                }
            }
            if ((int)r < 0) {
                /* Unknown var — treat as 0 */
                cg_mov_imm(cc->cg, dst, 0);
            } else if (r != dst) {
                cg_mov_reg(cc->cg, dst, r);
            }
        } else if (r != dst) {
            cg_mov_reg(cc->cg, dst, r);
        }
        return 0;
    }
    /* Unknown token */
    cg_mov_imm(cc->cg, dst, 0);
    return 0;
}

static void cg_compile_multiplicative(CGCompiler *cc, CGReg dst) {
    cg_compile_primary(cc, dst, NULL);
    for (;;) {
        if (cg_consume(cc, TOK_STAR)) {
            int64_t rhs_const;
            CGReg rhs = CG_REG_8;
            int is_const = cg_compile_primary(cc, rhs, &rhs_const);
            /* Strength reduction: x * (2^n) -> x << n */
            if (is_const && is_power_of_2(rhs_const)) {
                cg_lsl_imm(cc->cg, dst, dst, (uint8_t)ilog2(rhs_const));
            } else if (is_const && rhs_const == 0) {
                cg_mov_imm(cc->cg, dst, 0);
            } else if (is_const && rhs_const == 1) {
                /* x * 1 = x, nothing to do */
            } else {
                /* Ensure dst and rhs are different for mul */
                if (dst == rhs) {
                    cg_mov_reg(cc->cg, CG_REG_8, dst);
                    cg_mul_reg(cc->cg, dst, CG_REG_8, rhs);
                } else {
                    cg_mul_reg(cc->cg, dst, dst, rhs);
                }
            }
        } else if (cg_consume(cc, TOK_SLASH)) {
            int64_t rhs_const;
            CGReg rhs = CG_REG_8;
            int is_const = cg_compile_primary(cc, rhs, &rhs_const);
            /* Strength reduction: x / (2^n) -> x >> n */
            if (is_const && is_power_of_2(rhs_const)) {
                cg_asr_imm(cc->cg, dst, dst, (uint8_t)ilog2(rhs_const));
            } else if (is_const && rhs_const == 1) {
                /* x / 1 = x, nothing to do */
            } else {
                cg_div_reg(cc->cg, dst, dst, rhs);
            }
        } else if (cg_consume(cc, TOK_PERCENT)) {
            int64_t rhs_const;
            CGReg rhs = CG_REG_8;
            int is_const = cg_compile_primary(cc, rhs, &rhs_const);
            /* Strength reduction: x % (2^n) -> x & (2^n - 1) */
            if (is_const && is_power_of_2(rhs_const)) {
                cg_mov_imm(cc->cg, CG_REG_9, rhs_const - 1);
                cg_and_reg(cc->cg, dst, dst, CG_REG_9);
            } else {
                cg_mod_reg(cc->cg, dst, dst, rhs);
            }
        } else {
            break;
        }
    }
}

static void cg_compile_additive(CGCompiler *cc, CGReg dst) {
    cg_compile_multiplicative(cc, dst);
    for (;;) {
        if (cg_consume(cc, TOK_PLUS)) {
            CGReg rhs = CG_REG_7;
            cg_compile_multiplicative(cc, rhs);
            cg_add_reg(cc->cg, dst, dst, rhs);
        } else if (cg_consume(cc, TOK_MINUS)) {
            CGReg rhs = CG_REG_7;
            cg_compile_multiplicative(cc, rhs);
            cg_sub_reg(cc->cg, dst, dst, rhs);
        } else {
            break;
        }
    }
}

static void cg_compile_bitwise(CGCompiler *cc, CGReg dst) {
    cg_compile_additive(cc, dst);
    for (;;) {
        if (cg_consume(cc, TOK_AMP)) {
            CGReg rhs = CG_REG_7;
            cg_compile_additive(cc, rhs);
            cg_and_reg(cc->cg, dst, dst, rhs);
        } else if (cg_consume(cc, TOK_PIPE)) {
            CGReg rhs = CG_REG_7;
            cg_compile_additive(cc, rhs);
            cg_orr_reg(cc->cg, dst, dst, rhs);
        } else if (cg_consume(cc, TOK_CARET)) {
            CGReg rhs = CG_REG_7;
            cg_compile_additive(cc, rhs);
            cg_eor_reg(cc->cg, dst, dst, rhs);
        } else {
            break;
        }
    }
}

static void cg_compile_shift(CGCompiler *cc, CGReg dst) {
    cg_compile_bitwise(cc, dst);
    for (;;) {
        if (cg_consume(cc, TOK_SHL)) {
            /* Parse shift amount (constant only for now) */
            if (cg_cur(cc, TOK_NUMBER)) {
                uint8_t s = (uint8_t)(cc->cur_tok.ival & 63);
                cg_consume(cc, TOK_NUMBER);
                cg_lsl_imm(cc->cg, dst, dst, s);
            }
        } else if (cg_consume(cc, TOK_SHR)) {
            if (cg_cur(cc, TOK_NUMBER)) {
                uint8_t s = (uint8_t)(cc->cur_tok.ival & 63);
                cg_consume(cc, TOK_NUMBER);
                cg_asr_imm(cc->cg, dst, dst, s);
            }
        } else {
            break;
        }
    }
}

static void cg_compile_compare(CGCompiler *cc, CGReg dst) {
    cg_compile_shift(cc, dst);
    CGCC cc_type = CG_CC_AL;
    if (cg_consume(cc, TOK_EQ)) cc_type = CG_CC_EQ;
    else if (cg_consume(cc, TOK_NE)) cc_type = CG_CC_NE;
    else if (cg_consume(cc, TOK_LE)) cc_type = CG_CC_LE;
    else if (cg_consume(cc, TOK_GE)) cc_type = CG_CC_GE;
    else if (cg_consume(cc, TOK_LT)) cc_type = CG_CC_LT;
    else if (cg_consume(cc, TOK_GT)) cc_type = CG_CC_GT;
    else return;

    CGReg rhs = CG_REG_7;
    cg_compile_shift(cc, rhs);
    cg_cmp_reg(cc->cg, dst, rhs);
    cg_cset(cc->cg, dst, cc_type);
}

static void cg_compile_expr(CGCompiler *cc, CGReg dst) {
    /* Constant folding: if the entire expression is compile-time constant,
     * evaluate it now and emit just a load-immediate. */
    int64_t const_val;
    if (cg_const_eval(cc, &const_val)) {
        cg_mov_imm(cc->cg, dst, const_val);
        return;
    }
    cg_compile_compare(cc, dst);
}

/* -- Statement compiler ------------------------------------------- */

static void cg_compile_return_stmt(CGCompiler *cc) {
    cg_consume(cc, TOK_RETURN);
    cg_compile_expr(cc, CG_REG_0);
    /* Result is in R0 (which maps to RDI on x86, X0 on ARM64).
     * For x86-64, we need to move to RAX (return register).
     * For ARM64, X0 is already the return register.
     * The backend handles this: CG_REG_0 is the return register. */
}

static void cg_compile_decl_stmt(CGCompiler *cc) {
    /* Skip type keyword */
    while (cg_cur(cc, TOK_LONG)) cg_consume(cc, TOK_LONG);
    /* Variable name */
    if (!cg_cur(cc, TOK_IDENT)) return;
    char name[32];
    strncpy(name, cc->cur_tok.text, 31);
    cg_consume(cc, TOK_IDENT);
    CGReg r = cg_add_var(cc, name, 0);
    if (cg_consume(cc, TOK_ASSIGN)) {
        cg_compile_expr(cc, r);
    }
}

static void cg_compile_assign_stmt(CGCompiler *cc) {
    if (!cg_cur(cc, TOK_IDENT)) return;
    char name[32];
    strncpy(name, cc->cur_tok.text, 31);
    name[31] = '\0';
    cg_consume(cc, TOK_IDENT);
    cg_consume(cc, TOK_ASSIGN);
    CGReg r = cg_find_var(cc, name);
    if ((int)r < 0) r = CG_REG_9;  /* unknown var */
    cg_compile_expr(cc, r);
}

static void cg_compile_if_stmt(CGCompiler *cc) {
    cg_consume(cc, TOK_IF);
    cg_consume(cc, TOK_LPAREN);
    cg_compile_expr(cc, CG_REG_0);
    cg_consume(cc, TOK_RPAREN);

    /* Compare R0 with 0 */
    cg_cmp_imm(cc->cg, CG_REG_0, 0);
    cg_b_cond(cc->cg, 0, CG_CC_EQ);  /* jump to else if R0 == 0 */
    size_t else_patch = cg_branch_pos(cc->cg);

    /* Support both braced and unbraced then-block */
    if (cg_cur(cc, TOK_LBRACE)) {
        cg_compile_block(cc);
    } else {
        cg_compile_stmt(cc);
    }

    if (cg_consume(cc, TOK_ELSE)) {
        cg_b_uncond(cc->cg, 0);  /* jump over else */
        size_t end_patch = cg_branch_pos(cc->cg);
        cg_patch_branch(cc->cg, else_patch, cg_pos(cc->cg));
        /* Support both braced and unbraced else-block */
        if (cg_cur(cc, TOK_LBRACE)) {
            cg_compile_block(cc);
        } else {
            cg_compile_stmt(cc);
        }
        cg_patch_branch(cc->cg, end_patch, cg_pos(cc->cg));
    } else {
        cg_patch_branch(cc->cg, else_patch, cg_pos(cc->cg));
    }
}

static void cg_compile_while_stmt(CGCompiler *cc) {
    cg_consume(cc, TOK_WHILE);
    cg_consume(cc, TOK_LPAREN);
    size_t loop_top = cg_pos(cc->cg);
    cg_compile_expr(cc, CG_REG_0);
    cg_consume(cc, TOK_RPAREN);

    cg_cmp_imm(cc->cg, CG_REG_0, 0);
    cg_b_cond(cc->cg, 0, CG_CC_EQ);  /* exit if R0 == 0 */
    size_t exit_patch = cg_branch_pos(cc->cg);

    /* Support both braced and unbraced while body */
    if (cg_cur(cc, TOK_LBRACE)) {
        cg_compile_block(cc);
    } else {
        cg_compile_stmt(cc);
    }
    /* Jump back to loop top */
    cg_b_uncond(cc->cg, 0);  /* emit placeholder */
    size_t back_patch = cg_branch_pos(cc->cg);
    cg_patch_branch(cc->cg, back_patch, loop_top);  /* patch to jump to loop_top */
    cg_patch_branch(cc->cg, exit_patch, cg_pos(cc->cg));
}

static void cg_compile_block(CGCompiler *cc) {
    cg_consume(cc, TOK_LBRACE);
    while (!cg_cur(cc, TOK_RBRACE) && !cg_cur(cc, TOK_EOF)) {
        cg_compile_stmt(cc);
    }
    cg_consume(cc, TOK_RBRACE);
}

static void cg_compile_stmt(CGCompiler *cc) {
    if (cg_cur(cc, TOK_RETURN)) {
        cg_compile_return_stmt(cc);
        cg_consume(cc, TOK_SEMI);
    } else if (cg_cur(cc, TOK_IF)) {
        cg_compile_if_stmt(cc);
    } else if (cg_cur(cc, TOK_WHILE)) {
        cg_compile_while_stmt(cc);
    } else if (cg_cur(cc, TOK_LONG)) {
        cg_compile_decl_stmt(cc);
        cg_consume(cc, TOK_SEMI);
    } else if (cg_cur(cc, TOK_IDENT)) {
        /* Could be assignment or expression */
        /* Peek ahead for '=' by saving full state */
        CGToken save_tok = cc->cur_tok;
        CGLexer save_lex = cc->lex;
        cg_consume(cc, TOK_IDENT);
        int is_assign = cg_cur(cc, TOK_ASSIGN);
        /* Restore full state */
        cc->cur_tok = save_tok;
        cc->lex = save_lex;
        if (is_assign) {
            cg_compile_assign_stmt(cc);
        } else {
            /* Expression statement */
            cg_compile_expr(cc, CG_REG_9);
        }
        cg_consume(cc, TOK_SEMI);
    } else {
        cg_compile_expr(cc, CG_REG_9);
        cg_consume(cc, TOK_SEMI);
    }
}

/* -- Public API --------------------------------------------------- */

/*
 * Compile a C expression or function body using the abstract codegen.
 * Supports: expressions, if/while/return, declarations, assignments.
 */
int jit_minic_compile_cg(CodeGen *cg, const char *src) {
    if (!cg || !src) return -1;

    CGCompiler cc = {0};
    cc.cg = cg;
    cg_lex_init(&cc.lex, src);

    /* Setup argument registers: a→RDI(1), b→RSI(2), c→RDX(3), etc. */
    for (int i = 0; i < 6; i++) cc.arg_regs[i] = (CGReg)(i + 1);

    /* Get first token */
    cg_next_token(&cc.lex, &cc.cur_tok);

    /* Prologue */
    cg->vt->prologue(cg->enc, 0, 8);

    /* Compile statements */
    while (!cg_cur(&cc, TOK_EOF)) {
        cg_compile_stmt(&cc);
    }

    /* Epilogue */
    cg->vt->epilogue(cg->enc, 8);

    return cc.error;
}

/*
 * Legacy: compile simple expression (kept for backward compat).
 */
int jit_minic_compile_expr(CodeGen *cg, const char *src) {
    return jit_minic_compile_cg(cg, src);
}

/*
 * Get the compiled code buffer and size.
 */
const uint8_t *jit_minic_get_code(CodeGen *cg, size_t *size) {
    if (!cg || !size) return NULL;
    *size = cg_pos(cg);
    return cg_buffer(cg);
}
