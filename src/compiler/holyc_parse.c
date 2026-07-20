/*
 * holyc_parse.c  --  My Seed HolyC Parser + AST Utilities
 *
 * Recursive descent parser: tokens → AST.
 * Ported from ZealOS/src/Compiler/ParseExp.ZC + ParseStatement.ZC
 *
 * Grammar (simplified):
 *   program     → decl*
 *   decl        → func_decl | var_decl | struct_decl
 *   func_decl   → type ident '(' params ')' block
 *   var_decl    → type ident ['=' expr] ';'
 *   block       → '{' stmt* '}'
 *   stmt        → if_stmt | while_stmt | for_stmt | return_stmt
 *               | expr_stmt | block | var_decl
 *   expr        → assign
 *   assign      → ternary ['=' assign]
 *   ternary     → logic_or ['?' expr ':' ternary]
 *   logic_or    → logic_and ('||' logic_and)*
 *   logic_and   → equality ('&&' equality)*
 *   equality    → comparison (('=='|'!=') comparison)*
 *   comparison  → addition (('<'|'>'|'<='|'>=') addition)*
 *   addition    → multiplication (('+'|'-') multiplication)*
 *   multiplication → unary (('*'|'/'|'%') unary)*
 *   unary       → ('-'|'!'|'~'|'*'|'&') unary | postfix
 *   postfix     → primary ('++'|'--'|'[' expr ']'|'.' ident|'(' args ')')*
 *   primary     → INT_LIT | FLOAT_LIT | STRING_LIT | IDENT | '(' expr ')'
 */

#include "holyc.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -- AST Utilities ------------------------------------------------ */


/* -- AST Print (debug) -------------------------------------------- */


/* -- Type Size ---------------------------------------------------- */


/* -- Parser State ------------------------------------------------- */

static void parse_error(HCParser *p, const char *msg) {
    if (p->n_errors < HC_MAX_ERRORS) {
        snprintf(p->errors[p->n_errors], 256, "line %d: %s",
                 p->lex->line, msg);
        p->n_errors++;
    }
    p->has_error = true;
}

static HCTokenType peek(HCParser *p) {
    return p->lex->tok.type;
}

static HCTokenType advance(HCParser *p) {
    HCTokenType t = p->lex->tok.type;
    hc_lex_next(p->lex);
    return t;
}

static bool match(HCParser *p, HCTokenType type) {
    if (peek(p) == type) { advance(p); return true; }
    return false;
}

static void expect(HCParser *p, HCTokenType type) {
    if (peek(p) == type) { advance(p); return; }
    char msg[128];
    snprintf(msg, sizeof(msg), "expected token %d, got %d", type, peek(p));
    parse_error(p, msg);
}

/* -- Forward Declarations ----------------------------------------- */

static HCASTNode *parse_expr(HCParser *p);
static HCASTNode *parse_stmt(HCParser *p);
static HCASTNode *parse_decl(HCParser *p);

/* -- Parse Type --------------------------------------------------- */

static HCType *parse_type(HCParser *p) {
    HCType *t = (HCType *)calloc(1, sizeof(HCType));
    t->kind = HC_TYPE_I64; /* HolyC default */

    switch (peek(p)) {
        case HC_KW_U0:   t->kind = HC_TYPE_VOID; advance(p); break;
        case HC_KW_I8:   t->kind = HC_TYPE_I8;   advance(p); break;
        case HC_KW_I16:  t->kind = HC_TYPE_I16;  advance(p); break;
        case HC_KW_I32:  t->kind = HC_TYPE_I32;  advance(p); break;
        case HC_KW_I64:  t->kind = HC_TYPE_I64;  advance(p); break;
        case HC_KW_U8:   t->kind = HC_TYPE_U8;   advance(p); break;
        case HC_KW_U16:  t->kind = HC_TYPE_U16;  advance(p); break;
        case HC_KW_U32:  t->kind = HC_TYPE_U32;  advance(p); break;
        case HC_KW_U64:  t->kind = HC_TYPE_U64;  advance(p); break;
        case HC_KW_F64:  t->kind = HC_TYPE_F64;  advance(p); break;
        case HC_KW_BOOL:  t->kind = HC_TYPE_BOOL; advance(p); break;
        case HC_KW_STRUCT: {
            /* Struct type: struct Name { ... } or struct Name */
            advance(p); /* struct */
            if (peek(p) == HC_TOK_IDENT) {
                strncpy(t->name, p->lex->tok.text, HC_MAX_IDENT_LEN - 1);
                advance(p);
            }
            if (peek(p) == HC_TOK_LBRACE) {
                /* Struct definition with members */
                advance(p); /* { */
                while (peek(p) != HC_TOK_RBRACE && peek(p) != HC_TOK_EOF) {
                    HCType *member_type = parse_type(p);
                    if (peek(p) != HC_TOK_IDENT) {
                        parse_error(p, "expected member name");
                        break;
                    }
                    if (t->n_members < HC_MAX_PARAMS) {
                        strncpy(t->members[t->n_members].name, p->lex->tok.text, HC_MAX_IDENT_LEN - 1);
                        advance(p); /* consume member name */
                        t->members[t->n_members].type = member_type;
                        t->members[t->n_members].offset = t->size;
                        t->size += hc_type_size(member_type);
                        /* Align to member alignment */
                        int align = hc_type_size(member_type);
                        if (align > t->align) t->align = align;
                        if ((t->size % align) != 0) {
                            t->size += align - (t->size % align);
                        }
                        t->n_members++;
                    }
                    expect(p, HC_TOK_SEMI);
                }
                expect(p, HC_TOK_RBRACE);
            }
            t->kind = HC_TYPE_STRUCT;
            break;
        }
        default: break; /* Keep default I64 */
    }

    /* Pointer types: type * */
    while (peek(p) == HC_TOK_STAR) {
        advance(p);
        HCType *ptr = (HCType *)calloc(1, sizeof(HCType));
        ptr->kind = HC_TYPE_PTR;
        ptr->base = t;
        t = ptr;
    }

    return t;
}

/* -- Parse Primary ------------------------------------------------ */

static HCASTNode *parse_primary(HCParser *p) {
    switch (peek(p)) {
        case HC_TOK_INT: {
            HCASTNode *n = hc_ast_new(HC_AST_INT_LIT);
            n->int_val = p->lex->tok.int_val;
            advance(p);
            return n;
        }
        case HC_TOK_FLOAT: {
            HCASTNode *n = hc_ast_new(HC_AST_FLOAT_LIT);
            n->float_val = p->lex->tok.float_val;
            advance(p);
            return n;
        }
        case HC_TOK_STRING: {
            HCASTNode *n = hc_ast_new(HC_AST_STRING_LIT);
            strncpy(n->str_val, p->lex->tok.text, HC_MAX_STRING_LEN - 1);
            advance(p);
            return n;
        }
        case HC_TOK_CHAR: {
            HCASTNode *n = hc_ast_new(HC_AST_CHAR_LIT);
            n->int_val = p->lex->tok.int_val;
            advance(p);
            return n;
        }
        case HC_TOK_IDENT: {
            HCASTNode *n = hc_ast_new(HC_AST_IDENT);
            strncpy(n->ident, p->lex->tok.text, HC_MAX_IDENT_LEN - 1);
            advance(p);
            return n;
        }
        case HC_TOK_LPAREN: {
            advance(p); /* ( */
            HCASTNode *expr = parse_expr(p);
            expect(p, HC_TOK_RPAREN);
            return expr;
        }
        default:
            parse_error(p, "expected expression");
            return NULL;
    }
}

/* -- Parse Postfix ------------------------------------------------ */

static HCASTNode *parse_postfix(HCParser *p) {
    HCASTNode *expr = parse_primary(p);

    while (true) {
        if (peek(p) == HC_TOK_LPAREN) {
            /* Function call */
            advance(p); /* ( */
            HCASTNode *call = hc_ast_new(HC_AST_FUNC_CALL);
            call->callee = expr;
            if (peek(p) != HC_TOK_RPAREN) {
                hc_ast_add_arg(call, parse_expr(p));
                while (match(p, HC_TOK_COMMA))
                    hc_ast_add_arg(call, parse_expr(p));
            }
            expect(p, HC_TOK_RPAREN);
            expr = call;
        } else if (peek(p) == HC_TOK_LBRACKET) {
            /* Array index */
            advance(p); /* [ */
            HCASTNode *idx = hc_ast_new(HC_AST_INDEX);
            idx->left = expr;
            idx->right = parse_expr(p);
            expect(p, HC_TOK_RBRACKET);
            expr = idx;
        } else if (peek(p) == HC_TOK_DOT) {
            /* Member access */
            advance(p); /* . */
            HCASTNode *mem = hc_ast_new(HC_AST_MEMBER);
            mem->left = expr;
            if (peek(p) == HC_TOK_IDENT) {
                strncpy(mem->ident, p->lex->tok.text, HC_MAX_IDENT_LEN - 1);
                advance(p);
            }
            expr = mem;
        } else if (peek(p) == HC_TOK_ARROW) {
            /* Arrow access */
            advance(p); /* -> */
            HCASTNode *mem = hc_ast_new(HC_AST_ARROW);
            mem->left = expr;
            if (peek(p) == HC_TOK_IDENT) {
                strncpy(mem->ident, p->lex->tok.text, HC_MAX_IDENT_LEN - 1);
                advance(p);
            }
            expr = mem;
        } else if (peek(p) == HC_TOK_PLUS_PLUS) {
            advance(p);
            HCASTNode *inc = hc_ast_new(HC_AST_POST_INC);
            inc->child = expr;
            expr = inc;
        } else if (peek(p) == HC_TOK_MINUS_MINUS) {
            advance(p);
            HCASTNode *dec = hc_ast_new(HC_AST_POST_DEC);
            dec->child = expr;
            expr = dec;
        } else {
            break;
        }
    }

    return expr;
}

/* -- Parse Unary -------------------------------------------------- */

static HCASTNode *parse_unary(HCParser *p) {
    if (peek(p) == HC_TOK_MINUS) {
        advance(p);
        HCASTNode *n = hc_ast_new(HC_AST_NEG);
        n->child = parse_unary(p);
        return n;
    }
    if (peek(p) == HC_TOK_BANG) {
        advance(p);
        HCASTNode *n = hc_ast_new(HC_AST_NOT);
        n->child = parse_unary(p);
        return n;
    }
    if (peek(p) == HC_TOK_TILDE) {
        advance(p);
        HCASTNode *n = hc_ast_new(HC_AST_BITNOT);
        n->child = parse_unary(p);
        return n;
    }
    if (peek(p) == HC_TOK_STAR) {
        advance(p);
        HCASTNode *n = hc_ast_new(HC_AST_DEREF);
        n->child = parse_unary(p);
        return n;
    }
    if (peek(p) == HC_TOK_AMP) {
        advance(p);
        HCASTNode *n = hc_ast_new(HC_AST_ADDR);
        n->child = parse_unary(p);
        return n;
    }
    if (peek(p) == HC_TOK_PLUS_PLUS) {
        advance(p);
        HCASTNode *n = hc_ast_new(HC_AST_PRE_INC);
        n->child = parse_unary(p);
        return n;
    }
    if (peek(p) == HC_TOK_MINUS_MINUS) {
        advance(p);
        HCASTNode *n = hc_ast_new(HC_AST_PRE_DEC);
        n->child = parse_unary(p);
        return n;
    }
    return parse_postfix(p);
}

/* -- Parse Binary (precedence climbing) --------------------------- */

typedef struct { HCTokenType tok; HCASTKind ast; } BinOp;

static const BinOp mul_ops[] = {
    {HC_TOK_STAR,  HC_AST_MUL}, {HC_TOK_SLASH, HC_AST_DIV}, {HC_TOK_PERCENT, HC_AST_MOD},
    {HC_TOK_EOF,   0},
};
static const BinOp add_ops[] = {
    {HC_TOK_PLUS,  HC_AST_ADD}, {HC_TOK_MINUS, HC_AST_SUB},
    {HC_TOK_EOF,   0},
};
static const BinOp shift_ops[] = {
    {HC_TOK_SHL,   HC_AST_SHL}, {HC_TOK_SHR, HC_AST_SHR},
    {HC_TOK_EOF,   0},
};
static const BinOp cmp_ops[] = {
    {HC_TOK_LT,    HC_AST_LT}, {HC_TOK_GT, HC_AST_GT},
    {HC_TOK_LE,    HC_AST_LE}, {HC_TOK_GE, HC_AST_GE},
    {HC_TOK_EOF,   0},
};
static const BinOp eq_ops[] = {
    {HC_TOK_EQ,    HC_AST_EQ}, {HC_TOK_NE, HC_AST_NE},
    {HC_TOK_EOF,   0},
};
static const BinOp bitand_ops[] = {{HC_TOK_AMP, HC_AST_BITAND}, {HC_TOK_EOF, 0}};
static const BinOp bitxor_ops[] = {{HC_TOK_CARET, HC_AST_XOR}, {HC_TOK_EOF, 0}};
static const BinOp bitor_ops[]  = {{HC_TOK_PIPE, HC_AST_BITOR}, {HC_TOK_EOF, 0}};

static HCASTNode *parse_binop(HCParser *p, HCASTNode *(*higher)(HCParser*), const BinOp *ops) {
    HCASTNode *left = higher(p);
    while (true) {
        bool found = false;
        for (int i = 0; ops[i].tok != HC_TOK_EOF; i++) {
            if (peek(p) == ops[i].tok) {
                advance(p);
                HCASTNode *n = hc_ast_new(ops[i].ast);
                n->left = left;
                n->right = higher(p);
                left = n;
                found = true;
                break;
            }
        }
        if (!found) break;
    }
    return left;
}

/* -- Parse Cast ----------------------------------------------------- */
static HCASTNode *parse_cast(HCParser *p);

static HCASTNode *parse_mul(HCParser *p)      { return parse_binop(p, parse_cast, mul_ops); }

/* -- Parse Cast ----------------------------------------------------- */
static HCASTNode *parse_cast(HCParser *p) {
    if (peek(p) == HC_TOK_LPAREN) {
        /* Look ahead to see if this is a cast: (type) expr */
        /* Save lexer state for backtracking */
        int saved_pos = p->lex->pos;
        int saved_line = p->lex->line;
        int saved_col = p->lex->col;
        advance(p); /* consume ( */
        
        /* Check if next token is a type keyword or identifier (typedef name) */
        bool is_type = false;
        HCTokenType tok = peek(p);
        if (tok == HC_KW_I8 || tok == HC_KW_I16 || tok == HC_KW_I32 ||
            tok == HC_KW_I64 || tok == HC_KW_U8 || tok == HC_KW_U16 ||
            tok == HC_KW_U32 || tok == HC_KW_U64 || tok == HC_KW_F64 ||
            tok == HC_KW_BOOL || tok == HC_TOK_IDENT) {
            is_type = true;
        }
        
        if (is_type) {
            /* This is a cast - parse the type */
            HCType *cast_type = parse_type(p);
            expect(p, HC_TOK_RPAREN);
            HCASTNode *expr = parse_cast(p);  /* right-associative for nested casts */
            HCASTNode *n = hc_ast_new(HC_AST_CAST);
            n->child = expr;
            n->type = cast_type;
            return n;
        }
        /* Not a cast, backtrack fully and parse as parenthesized expression */
        p->lex->pos = saved_pos;
        p->lex->line = saved_line;
        p->lex->col = saved_col;
        /* Need to re-lex the token at saved position */
        hc_lex_next(p->lex);
        HCASTNode *expr = parse_expr(p);
        expect(p, HC_TOK_RPAREN);
        return expr;
    }
    return parse_unary(p);
}
static HCASTNode *parse_add(HCParser *p)      { return parse_binop(p, parse_mul, add_ops); }
static HCASTNode *parse_shift(HCParser *p)    { return parse_binop(p, parse_add, shift_ops); }
static HCASTNode *parse_cmp(HCParser *p)      { return parse_binop(p, parse_shift, cmp_ops); }
static HCASTNode *parse_eq(HCParser *p)       { return parse_binop(p, parse_cmp, eq_ops); }
static HCASTNode *parse_bitand(HCParser *p)   { return parse_binop(p, parse_eq, bitand_ops); }
static HCASTNode *parse_bitxor(HCParser *p)   { return parse_binop(p, parse_bitand, bitxor_ops); }
static HCASTNode *parse_bitor(HCParser *p)    { return parse_binop(p, parse_bitxor, bitor_ops); }

/* logic_and, logic_or */
static HCASTNode *parse_logic_and(HCParser *p) {
    HCASTNode *left = parse_bitor(p);
    while (peek(p) == HC_TOK_AND) {
        advance(p);
        HCASTNode *n = hc_ast_new(HC_AST_AND);
        n->left = left; n->right = parse_bitor(p);
        left = n;
    }
    return left;
}

static HCASTNode *parse_logic_or(HCParser *p) {
    HCASTNode *left = parse_logic_and(p);
    while (peek(p) == HC_TOK_OR) {
        advance(p);
        HCASTNode *n = hc_ast_new(HC_AST_OR);
        n->left = left; n->right = parse_logic_and(p);
        left = n;
    }
    return left;
}

/* -- Parse Ternary ------------------------------------------------ */

static HCASTNode *parse_ternary(HCParser *p) {
    HCASTNode *expr = parse_logic_or(p);
    if (peek(p) == HC_TOK_QUESTION) {
        advance(p);
        HCASTNode *n = hc_ast_new(HC_AST_TERNARY);
        n->cond = expr;
        n->then_branch = parse_expr(p);
        expect(p, HC_TOK_COLON);
        n->else_branch = parse_ternary(p);
        return n;
    }
    return expr;
}

/* -- Parse Assignment --------------------------------------------- */

static HCASTNode *parse_assign(HCParser *p) {
    HCASTNode *left = parse_ternary(p);

    HCASTKind assign_kind = 0;
    switch (peek(p)) {
        case HC_TOK_ASSIGN:       assign_kind = HC_AST_ASSIGN; break;
        case HC_TOK_PLUS_ASSIGN:  assign_kind = HC_AST_ADD_ASSIGN; break;
        case HC_TOK_MINUS_ASSIGN: assign_kind = HC_AST_SUB_ASSIGN; break;
        case HC_TOK_STAR_ASSIGN:  assign_kind = HC_AST_MUL_ASSIGN; break;
        case HC_TOK_SLASH_ASSIGN: assign_kind = HC_AST_DIV_ASSIGN; break;
        default: return left;
    }

    advance(p);
    HCASTNode *n = hc_ast_new(assign_kind);
    n->left = left;
    n->right = parse_assign(p);
    return n;
}

/* -- Parse Expression --------------------------------------------- */

static HCASTNode *parse_expr(HCParser *p) {
    return parse_assign(p);
}

/* -- Parse Block -------------------------------------------------- */

HCASTNode *parse_block(HCParser *p) {
    expect(p, HC_TOK_LBRACE);
    HCASTNode *block = hc_ast_new(HC_AST_BLOCK);
    if (!block) { p->has_error = true; return NULL; }
    while (peek(p) != HC_TOK_RBRACE && peek(p) != HC_TOK_EOF) {
        int start_pos = p->lex->pos;
        hc_ast_add_stmt(block, parse_stmt(p));
        /* Guard against a statement that fails to consume any input. An
         * unrecognized token (e.g. garbage source) would otherwise leave the
         * lexer position unchanged, spin the loop forever, exhaust memory,
         * and crash when hc_ast_new() returns NULL. Bail out cleanly as a
         * parse error so the caller reports it instead of segfaulting. */
        if (!p->has_error && p->lex->pos == start_pos) {
            parse_error(p, "unexpected token in block");
            break;
        }
        if (p->has_error) break;
    }
    expect(p, HC_TOK_RBRACE);
    return block;
}

HCASTNode *hc_parse_block(HCParser *p) {
    return parse_block(p);
}

/* -- Parse Statement ---------------------------------------------- */

static HCASTNode *parse_stmt(HCParser *p) {
    /* If statement */
    if (match(p, HC_KW_IF)) {
        HCASTNode *n = hc_ast_new(HC_AST_IF);
        expect(p, HC_TOK_LPAREN);
        n->cond = parse_expr(p);
        expect(p, HC_TOK_RPAREN);
        n->then_branch = parse_stmt(p);
        if (match(p, HC_KW_ELSE))
            n->else_branch = parse_stmt(p);
        return n;
    }

    /* While statement */
    if (match(p, HC_KW_WHILE)) {
        HCASTNode *n = hc_ast_new(HC_AST_WHILE);
        expect(p, HC_TOK_LPAREN);
        n->cond = parse_expr(p);
        expect(p, HC_TOK_RPAREN);
        n->body = parse_stmt(p);
        return n;
    }

    /* For statement */
    if (match(p, HC_KW_FOR)) {
        HCASTNode *n = hc_ast_new(HC_AST_FOR);
        expect(p, HC_TOK_LPAREN);
        n->init_expr = parse_expr(p);
        expect(p, HC_TOK_SEMI);
        n->cond = parse_expr(p);
        expect(p, HC_TOK_SEMI);
        n->update = parse_expr(p);
        expect(p, HC_TOK_RPAREN);
        n->body = parse_stmt(p);
        return n;
    }

    /* Return statement */
    if (match(p, HC_KW_RETURN)) {
        HCASTNode *n = hc_ast_new(HC_AST_RETURN);
        if (peek(p) != HC_TOK_SEMI)
            n->child = parse_expr(p);
        expect(p, HC_TOK_SEMI);
        return n;
    }

    /* Break */
    if (match(p, HC_KW_BREAK)) {
        expect(p, HC_TOK_SEMI);
        return hc_ast_new(HC_AST_BREAK);
    }

    /* Continue */
    if (match(p, HC_KW_CONTINUE)) {
        expect(p, HC_TOK_SEMI);
        return hc_ast_new(HC_AST_CONTINUE);
    }

    /* Block */
    if (peek(p) == HC_TOK_LBRACE) {
        return parse_block(p);
    }

    /* Variable declaration (type followed by ident) */
    if (peek(p) >= HC_KW_I0 && peek(p) <= HC_KW_VOLATILE) {
        return hc_parse_decl(p);
    }

    /* Expression statement */
    HCASTNode *expr = parse_expr(p);
    expect(p, HC_TOK_SEMI);
    HCASTNode *n = hc_ast_new(HC_AST_EXPR_STMT);
    n->child = expr;
    return n;
}

/* -- Parse Declaration -------------------------------------------- */

HCASTNode *hc_parse_decl(HCParser *p) {
    /* Handle extern "C" func_name(params) -> ret_type; */
    if (match(p, HC_KW_EXTERN)) {
        /* Expect "C" string literal */
        if (!match(p, HC_TOK_STRING)) {
            parse_error(p, "expected extern string literal (e.g., \"C\")");
            return NULL;
        }
        /* Verify it's "C" */
        if (strcmp(p->lex->tok.text, "\"C\"") != 0 && strcmp(p->lex->tok.text, "C") != 0) {
            parse_error(p, "only extern \"C\" is supported");
            return NULL;
        }

        /* Parse return type */
        HCType *ret_type = parse_type(p);
        if (!ret_type) {
            parse_error(p, "expected return type after extern \"C\"");
            return NULL;
        }

        /* Expect function name */
        if (peek(p) != HC_TOK_IDENT) {
            parse_error(p, "expected function name");
            return NULL;
        }
        char func_name[HC_MAX_IDENT_LEN];
        strncpy(func_name, p->lex->tok.text, HC_MAX_IDENT_LEN - 1);
        advance(p);

        /* Expect ( for parameters */
        expect(p, HC_TOK_LPAREN);

        /* Create extern declaration AST node */
        HCASTNode *ext = hc_ast_new(HC_AST_EXTERN_DECL);
        ext->extern_ret_type = ret_type;
        strncpy(ext->extern_c_name, func_name, HC_MAX_IDENT_LEN - 1);
        ext->extern_n_params = 0;

        /* Parse parameters */
        int pi = 0;
        if (peek(p) != HC_TOK_RPAREN) {
            ext->extern_param_types[pi] = parse_type(p);
            if (peek(p) == HC_TOK_IDENT) {
                /* Skip parameter name */
                advance(p);
            }
            pi++;
            while (match(p, HC_TOK_COMMA) && pi < HC_MAX_PARAMS) {
                ext->extern_param_types[pi] = parse_type(p);
                if (peek(p) == HC_TOK_IDENT) {
                    advance(p);
                }
                pi++;
            }
        }
        ext->extern_n_params = pi;

        expect(p, HC_TOK_RPAREN);

        /* Optional -> ret_type for explicit return type */
        if (match(p, HC_TOK_ARROW)) {
            HCType *explicit_ret = parse_type(p);
            if (explicit_ret) ext->extern_ret_type = explicit_ret;
        }

        expect(p, HC_TOK_SEMI);
        return ext;
    }

    HCType *type = parse_type(p);

    /* Check if this is a struct/union/enum type definition without a variable name */
    if (type->kind == HC_TYPE_STRUCT || type->kind == HC_TYPE_UNION || type->kind == HC_TYPE_ENUM) {
        if (peek(p) == HC_TOK_SEMI) {
            /* Type definition like "struct Point { ... };" - just consume semicolon */
            advance(p);
            /* Create a no-op AST node for the type definition */
            HCASTNode *n = hc_ast_new(HC_AST_STRUCT_DECL);
            n->type = type;
            return n;
        }
    }

    if (peek(p) != HC_TOK_IDENT) {
        parse_error(p, "expected identifier");
        return NULL;
    }

    char name[HC_MAX_IDENT_LEN];
    strncpy(name, p->lex->tok.text, HC_MAX_IDENT_LEN - 1);
    advance(p);

    /* Function declaration: type name(params) { body } */
    if (peek(p) == HC_TOK_LPAREN) {
        advance(p); /* ( */
        HCASTNode *func = hc_ast_new(HC_AST_FUNC_DECL);
        strncpy(func->ident, name, HC_MAX_IDENT_LEN - 1);
        func->type = type;

        /* Parse parameters */
        int pi = 0;
        if (peek(p) != HC_TOK_RPAREN) {
            func->param_types[pi] = parse_type(p);
            if (peek(p) == HC_TOK_IDENT) {
                strncpy(func->param_names[pi], p->lex->tok.text, HC_MAX_IDENT_LEN - 1);
                advance(p);
            }
            pi++;
            while (match(p, HC_TOK_COMMA) && pi < HC_MAX_PARAMS) {
                func->param_types[pi] = parse_type(p);
                if (peek(p) == HC_TOK_IDENT) {
                    strncpy(func->param_names[pi], p->lex->tok.text, HC_MAX_IDENT_LEN - 1);
                    advance(p);
                }
                pi++;
            }
        }
        func->n_params = pi;
        expect(p, HC_TOK_RPAREN);

        /* Parse body */
        func->body = parse_block(p);
        return func;
    }

    /* Variable declaration: type name [= expr] ; */
    HCASTNode *var = hc_ast_new(HC_AST_VAR_DECL);
    strncpy(var->ident, name, HC_MAX_IDENT_LEN - 1);
    var->type = type;

    if (match(p, HC_TOK_ASSIGN)) {
        var->init = parse_expr(p);
    }
    expect(p, HC_TOK_SEMI);
    return var;
}

/* -- Parse Compilation Unit --------------------------------------- */

void hc_parse_init(HCParser *p, HCLexer *lex) {
    memset(p, 0, sizeof(*p));
    p->lex = lex;
}

HCASTNode *hc_parse_compilation_unit(HCParser *p) {
    HCASTNode *unit = hc_ast_new(HC_AST_BLOCK);

    while (peek(p) != HC_TOK_EOF && !p->has_error) {
        HCASTNode *decl = hc_parse_decl(p);
        if (decl) hc_ast_add_stmt(unit, decl);
        else break;
    }

    return unit;
}

HCASTNode *hc_parse_expr(HCParser *p) {
    return parse_expr(p);
}

HCASTNode *hc_parse_stmt(HCParser *p) {
    return parse_stmt(p);
}

HCTokenType hc_parse_peek(HCParser *p) {
    return peek(p);
}


