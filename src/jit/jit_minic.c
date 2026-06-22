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
#include "wubu_x86.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* -- Tokenizer ---------------------------------------------------- */

typedef enum {
    TOK_EOF = 0,
    TOK_INT, TOK_LONG, TOK_I64, TOK_U8, TOK_VOID,
    TOK_RETURN, TOK_IF, TOK_ELSE, TOK_WHILE,
    TOK_IDENT, TOK_NUMBER,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH,
    TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT, TOK_LEQ, TOK_GEQ,
    TOK_ASSIGN, TOK_NOT,
    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE,
    TOK_SEMI, TOK_COMMA,
} MinicTokType;

typedef struct {
    MinicTokType  type;
    char          text[256];
    int64_t       ival;
} MinicToken;

typedef struct {
    const char   *src;
    int           pos;
    MinicToken    cur;
    MinicToken    peek;
} MinicLexer;

static void minic_lex_next(MinicLexer *l) {
    while (l->src[l->pos] == ' ' || l->src[l->pos] == '\t' ||
           l->src[l->pos] == '\n' || l->src[l->pos] == '\r')
        l->pos++;

    l->cur = l->peek;
    memset(&l->peek, 0, sizeof(l->peek));

    char c = l->src[l->pos];
    if (c == '\0') { l->peek.type = TOK_EOF; return; }

    if (isdigit((unsigned char)c)) {
        int start = l->pos;
        while (isdigit((unsigned char)l->src[l->pos])) {
            l->peek.text[l->pos - start] = l->src[l->pos];
            l->pos++;
        }
        l->peek.text[l->pos - start] = '\0';
        l->peek.type = TOK_NUMBER;
        l->peek.ival = strtoll(l->peek.text, NULL, 10);
        return;
    }

    if (isalpha((unsigned char)c) || c == '_') {
        int start = l->pos;
        while (isalnum((unsigned char)l->src[l->pos]) || l->src[l->pos] == '_') {
            l->peek.text[l->pos - start] = l->src[l->pos];
            l->pos++;
        }
        l->peek.text[l->pos - start] = '\0';
        if (strcmp(l->peek.text, "int") == 0)   l->peek.type = TOK_INT;
        else if (strcmp(l->peek.text, "long") == 0)  l->peek.type = TOK_LONG;
        else if (strcmp(l->peek.text, "I64") == 0)   l->peek.type = TOK_I64;
        else if (strcmp(l->peek.text, "U8") == 0)    l->peek.type = TOK_U8;
        else if (strcmp(l->peek.text, "void") == 0)  l->peek.type = TOK_VOID;
        else if (strcmp(l->peek.text, "return") == 0) l->peek.type = TOK_RETURN;
        else if (strcmp(l->peek.text, "if") == 0)     l->peek.type = TOK_IF;
        else if (strcmp(l->peek.text, "else") == 0)   l->peek.type = TOK_ELSE;
        else if (strcmp(l->peek.text, "while") == 0)  l->peek.type = TOK_WHILE;
        else l->peek.type = TOK_IDENT;
        return;
    }

    if (c == '=' && l->src[l->pos+1] == '=') { l->peek.type = TOK_EQ; l->pos += 2; return; }
    if (c == '!' && l->src[l->pos+1] == '=') { l->peek.type = TOK_NEQ; l->pos += 2; return; }
    if (c == '<' && l->src[l->pos+1] == '=') { l->peek.type = TOK_LEQ; l->pos += 2; return; }
    if (c == '>' && l->src[l->pos+1] == '=') { l->peek.type = TOK_GEQ; l->pos += 2; return; }

    l->pos++;
    switch (c) {
        case '+': l->peek.type = TOK_PLUS;    l->peek.text[0] = '+'; break;
        case '-': l->peek.type = TOK_MINUS;   l->peek.text[0] = '-'; break;
        case '*': l->peek.type = TOK_STAR;    l->peek.text[0] = '*'; break;
        case '/': l->peek.type = TOK_SLASH;   l->peek.text[0] = '/'; break;
        case '<': l->peek.type = TOK_LT;      l->peek.text[0] = '<'; break;
        case '>': l->peek.type = TOK_GT;      l->peek.text[0] = '>'; break;
        case '=': l->peek.type = TOK_ASSIGN;  l->peek.text[0] = '='; break;
        case '!': l->peek.type = TOK_NOT;     l->peek.text[0] = '!'; break;
        case '(': l->peek.type = TOK_LPAREN;  l->peek.text[0] = '('; break;
        case ')': l->peek.type = TOK_RPAREN;  l->peek.text[0] = ')'; break;
        case '{': l->peek.type = TOK_LBRACE;  l->peek.text[0] = '{'; break;
        case '}': l->peek.type = TOK_RBRACE;  l->peek.text[0] = '}'; break;
        case ';': l->peek.type = TOK_SEMI;    l->peek.text[0] = ';'; break;
        case ',': l->peek.type = TOK_COMMA;   l->peek.text[0] = ','; break;
        default:  l->peek.type = TOK_EOF; break;
    }
}

static void minic_lex_init(MinicLexer *l, const char *src) {
    memset(l, 0, sizeof(*l));
    l->src = src;
    l->pos = 0;
    minic_lex_next(l);
    minic_lex_next(l);
}

static MinicToken *minic_cur(MinicLexer *l) { return &l->cur; }
static void minic_advance(MinicLexer *l) { minic_lex_next(l); }

static int minic_expect(MinicLexer *l, MinicTokType t) {
    if (minic_cur(l)->type == t) { minic_advance(l); return 0; }
    return -1;
}

static int minic_is_type(MinicTokType t) {
    return t == TOK_INT || t == TOK_LONG || t == TOK_I64 || t == TOK_U8 || t == TOK_VOID;
}

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

/* -- Compiler State ---------------------------------------------- */

struct MinicCompiler {
    MinicLexer    lex;
    MinicScope    scope;
    Wx86Enc       enc;       /* Uses Wx86Enc from wubu_x86.h */
    int           n_args;
    int           in_func;
    int           error;
    char          error_msg[256];
};

static void mc_error(MinicCompiler *mc, const char *msg) {
    mc->error = 1;
    snprintf(mc->error_msg, sizeof(mc->error_msg), "minic: %s (near '%s')",
             msg, mc->lex.cur.text);
}

/* -- Emit helper macros using Wx86Enc --------------------------- */

#define MC_EMIT(mc, call) do { if (!(mc)->error) { call; } } while(0)

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
        minic_advance(&mc->lex);
        return;
    }

    if (tok->type == TOK_IDENT) {
        char name[64];
        snprintf(name, sizeof(name), "%s", tok->text);

        minic_advance(&mc->lex);
        if (minic_cur(&mc->lex)->type == TOK_LPAREN) {
            /* Function call */
            minic_advance(&mc->lex);

            Wx86Reg aregs[] = { WREG_RDI, WREG_RSI, WREG_RDX, WREG_RCX, WREG_R8, WREG_R9 };
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

static void compile_multiplicative(MinicCompiler *mc) {
    compile_primary(mc);

    while (minic_cur(&mc->lex)->type == TOK_STAR ||
           minic_cur(&mc->lex)->type == TOK_SLASH) {
        MinicTokType op = minic_cur(&mc->lex)->type;
        minic_advance(&mc->lex);

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

static void compile_additive(MinicCompiler *mc) {
    compile_multiplicative(mc);

    while (minic_cur(&mc->lex)->type == TOK_PLUS ||
           minic_cur(&mc->lex)->type == TOK_MINUS) {
        MinicTokType op = minic_cur(&mc->lex)->type;
        minic_advance(&mc->lex);

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

        MC_EMIT(mc, wx86_cmp_reg_reg(&mc->enc, WREG_RAX, WREG_RCX));

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
        MC_EMIT(mc, wx86_setcc_r8(&mc->enc, cc, WREG_RAX));
        /* movzx rax, al */
        wx86_emit_byte(&mc->enc, 0x0F);
        wx86_emit_byte(&mc->enc, 0xB6);
        wx86_emit_byte(&mc->enc, 0xC0);
    }
}

static void compile_expr(MinicCompiler *mc) {
    compile_compare(mc);
}

/* -- Statement Compiler ------------------------------------------ */

static void compile_if_stmt(MinicCompiler *mc) {
    minic_advance(&mc->lex);
    minic_expect(&mc->lex, TOK_LPAREN);
    compile_expr(mc);
    minic_expect(&mc->lex, TOK_RPAREN);

    MC_EMIT(mc, wx86_test_reg_reg(&mc->enc, WREG_RAX, WREG_RAX));
    MC_EMIT(mc, wx86_jcc_rel32(&mc->enc, WCC_E));
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
    size_t loop_top = mc->enc.pos;

    minic_expect(&mc->lex, TOK_LPAREN);
    compile_expr(mc);
    minic_expect(&mc->lex, TOK_RPAREN);

    MC_EMIT(mc, wx86_test_reg_reg(&mc->enc, WREG_RAX, WREG_RAX));
    MC_EMIT(mc, wx86_jcc_rel32(&mc->enc, WCC_E));
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
            v->slot = -(8 * mc->n_args) + 8 * i;  /* arg0 at -8*n, arg1 at -8*(n-1), etc. */
            v->is_arg = 0;  /* Now on stack, accessed via RBP */
        }
    }
    /* Adjust stack_offset to account for args on stack */
    mc->scope.stack_offset = -(8 + args_size);

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

    /* Epilogue (default return 0) */
    MC_EMIT(mc, wx86_mov_reg_imm64(&mc->enc, WREG_RAX, 0));
    MC_EMIT(mc, wx86_mov_reg_reg(&mc->enc, WREG_RSP, WREG_RBP));
    MC_EMIT(mc, wx86_pop_reg(&mc->enc, WREG_RBP));
    MC_EMIT(mc, wx86_ret(&mc->enc));

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
        size_t wrap_len = 80 + src_len + strlen(fname);
        wrapped = (char *)malloc(wrap_len);
        if (!wrapped) return JIT_ERR_ALLOC;
        snprintf(wrapped, wrap_len,
                 "long %s(long a, long b) { return (%s); }",
                 fname, source);
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
