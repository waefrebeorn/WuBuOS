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

HCASTNode *hc_ast_new(HCASTKind kind) {
    HCASTNode *n = (HCASTNode *)calloc(1, sizeof(HCASTNode));
    if (!n) return NULL;
    n->kind = kind;
    return n;
}

void hc_ast_free(HCASTNode *node) {
    if (!node) return;
    hc_ast_free(node->child);
    hc_ast_free(node->left);
    hc_ast_free(node->right);
    hc_ast_free(node->cond);
    hc_ast_free(node->then_branch);
    hc_ast_free(node->else_branch);
    hc_ast_free(node->init);
    hc_ast_free(node->callee);
    hc_ast_free(node->body);
    hc_ast_free(node->init_expr);
    hc_ast_free(node->update);

    if (node->stmts) {
        for (int i = 0; i < node->n_stmts; i++)
            hc_ast_free(node->stmts[i]);
        free(node->stmts);
    }
    if (node->args) {
        for (int i = 0; i < node->n_args; i++)
            hc_ast_free(node->args[i]);
        free(node->args);
    }
    free(node);
}

void hc_ast_add_stmt(HCASTNode *block, HCASTNode *stmt) {
    if (!block || !stmt) return;
    if (block->n_stmts >= block->stmts_cap) {
        block->stmts_cap = block->stmts_cap ? block->stmts_cap * 2 : 8;
        block->stmts = (HCASTNode **)realloc(block->stmts, block->stmts_cap * sizeof(HCASTNode *));
    }
    block->stmts[block->n_stmts++] = stmt;
}

void hc_ast_add_arg(HCASTNode *call, HCASTNode *arg) {
    if (!call || !arg) return;
    if (call->n_args >= call->args_cap) {
        call->args_cap = call->args_cap ? call->args_cap * 2 : 4;
        call->args = (HCASTNode **)realloc(call->args, call->args_cap * sizeof(HCASTNode *));
    }
    call->args[call->n_args++] = arg;
}

/* -- AST Print (debug) -------------------------------------------- */

static const char *ast_kind_name(HCASTKind k) {
    switch (k) {
        case HC_AST_INT_LIT:    return "INT";
        case HC_AST_FLOAT_LIT:  return "FLOAT";
        case HC_AST_STRING_LIT: return "STRING";
        case HC_AST_IDENT:      return "IDENT";
        case HC_AST_NEG:        return "NEG";
        case HC_AST_NOT:        return "NOT";
        case HC_AST_ADD:        return "ADD";
        case HC_AST_SUB:        return "SUB";
        case HC_AST_MUL:        return "MUL";
        case HC_AST_DIV:        return "DIV";
        case HC_AST_MOD:        return "MOD";
        case HC_AST_EQ:         return "EQ";
        case HC_AST_NE:         return "NE";
        case HC_AST_LT:         return "LT";
        case HC_AST_LE:         return "LE";
        case HC_AST_GT:         return "GT";
        case HC_AST_GE:         return "GE";
        case HC_AST_AND:        return "AND";
        case HC_AST_OR:         return "OR";
        case HC_AST_ASSIGN:     return "ASSIGN";
        case HC_AST_IF:         return "IF";
        case HC_AST_WHILE:      return "WHILE";
        case HC_AST_FOR:        return "FOR";
        case HC_AST_RETURN:     return "RETURN";
        case HC_AST_BLOCK:      return "BLOCK";
        case HC_AST_EXPR_STMT:  return "EXPR_STMT";
        case HC_AST_VAR_DECL:   return "VAR_DECL";
        case HC_AST_FUNC_DECL:  return "FUNC_DECL";
        case HC_AST_FUNC_CALL:  return "FUNC_CALL";
        default:                return "?";
    }
}

void hc_ast_print(const HCASTNode *node, int indent) {
    if (!node) return;
    for (int i = 0; i < indent; i++) printf("  ");
    printf("%s", ast_kind_name(node->kind));
    if (node->kind == HC_AST_INT_LIT)    printf(" val=%lld", (long long)node->int_val);
    if (node->kind == HC_AST_FLOAT_LIT)  printf(" val=%g", node->float_val);
    if (node->kind == HC_AST_IDENT)      printf(" name='%s'", node->ident);
    if (node->kind == HC_AST_STRING_LIT) printf(" str=\"%s\"", node->str_val);
    printf("\n");
    if (node->child) hc_ast_print(node->child, indent + 1);
    if (node->left)  hc_ast_print(node->left, indent + 1);
    if (node->right) hc_ast_print(node->right, indent + 1);
    if (node->cond)        hc_ast_print(node->cond, indent + 1);
    if (node->then_branch) hc_ast_print(node->then_branch, indent + 1);
    if (node->else_branch) hc_ast_print(node->else_branch, indent + 1);
    if (node->callee)      hc_ast_print(node->callee, indent + 1);
    if (node->body)        hc_ast_print(node->body, indent + 1);
    if (node->init)        hc_ast_print(node->init, indent + 1);
    if (node->init_expr)   hc_ast_print(node->init_expr, indent + 1);
    if (node->update)      hc_ast_print(node->update, indent + 1);
    for (int i = 0; i < node->n_stmts; i++) hc_ast_print(node->stmts[i], indent + 1);
    for (int i = 0; i < node->n_args; i++)  hc_ast_print(node->args[i], indent + 1);
}

/* -- Type Size ---------------------------------------------------- */

size_t hc_type_size(const HCType *t) {
    if (!t) return 8;
    switch (t->kind) {
        case HC_TYPE_VOID: return 0;
        case HC_TYPE_I8:   case HC_TYPE_U8:  return 1;
        case HC_TYPE_I16:  case HC_TYPE_U16: return 2;
        case HC_TYPE_I32:  case HC_TYPE_U32: case HC_TYPE_BOOL: return 4;
        case HC_TYPE_I64:  case HC_TYPE_U64: case HC_TYPE_F64:  return 8;
        case HC_TYPE_PTR:  return 8;
        case HC_TYPE_ARRAY: return t->base ? hc_type_size(t->base) * (size_t)t->array_size : 0;
        case HC_TYPE_STRUCT: return t->size > 0 ? t->size : 8;
        default: return 8;
    }
}

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
        case HC_KWBool:  t->kind = HC_TYPE_BOOL; advance(p); break;
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

static HCASTNode *parse_mul(HCParser *p)      { return parse_binop(p, parse_unary, mul_ops); }
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
        case HC_TOK_ASSIGN:     assign_kind = HC_AST_ASSIGN; break;
        case HC_TOK_PLUS_EQ:    assign_kind = HC_AST_ADD_ASSIGN; break;
        case HC_TOK_MINUS_EQ:   assign_kind = HC_AST_SUB_ASSIGN; break;
        case HC_TOK_STAR_EQ:    assign_kind = HC_AST_MUL_ASSIGN; break;
        case HC_TOK_SLASH_EQ:   assign_kind = HC_AST_DIV_ASSIGN; break;
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
    while (peek(p) != HC_TOK_RBRACE && peek(p) != HC_TOK_EOF) {
        hc_ast_add_stmt(block, parse_stmt(p));
    }
    expect(p, HC_TOK_RBRACE);
    return block;
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
