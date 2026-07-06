/*
 * holyc_ast.h  --  HolyC AST Node Definitions and Utilities
 * Self-contained, C11, minimal includes.
 */
#ifndef MYSEED_HOLYC_AST_H
#define MYSEED_HOLYC_AST_H

#include "holyc_types.h"

/* AST node definition (opaque in types, concrete here) */
struct HCASTNode {
    HCASTKind kind;
    HCASTNode *child;          /* For unary/ternary/return/expr_stmt */
    HCASTNode *left;           /* For binary */
    HCASTNode *right;          /* For binary */
    HCASTNode *cond;           /* For if/while/for/ternary */
    HCASTNode *then_branch;    /* For if */
    HCASTNode *else_branch;    /* For if */
    HCASTNode *init_expr;      /* For for */
    HCASTNode *update;         /* For for */
    HCASTNode *body;           /* For while/for/do_while/func_decl/block */
    HCASTNode **stmts;         /* For block */
    int n_stmts;
    int stmts_cap;
    int args_cap;

    /* For var_decl / func_decl */
    char ident[HC_MAX_IDENT_LEN];
    HCType *ast_type;
    HCASTNode *init;           /* For var_decl */
    HCASTNode **params;        /* For func_decl */
    char param_names[HC_MAX_PARAMS][HC_MAX_IDENT_LEN];
    HCType *param_types[HC_MAX_PARAMS];
    int n_params;

    /* For extern_decl */
    char extern_c_name[HC_MAX_IDENT_LEN];
    HCType *extern_ret_type;
    HCType *extern_param_types[HC_MAX_PARAMS];
    int extern_n_params;

    /* For call */
    HCASTNode **args;
    int n_args;

    /* For string/char literals */
    char str_val[HC_MAX_STRING_LEN];

    /* For int/float literals */
    int64_t int_val;
    double float_val;

    /* Type info for codegen */
    HCType *type;

    /* Function call target */
    void *func_ptr;
    HCASTNode *callee;
};

/* AST utilities */
HCASTNode *hc_ast_new(HCASTKind kind);
void hc_ast_free(HCASTNode *node);
void hc_ast_add_stmt(HCASTNode *block, HCASTNode *stmt);
void hc_ast_add_arg(HCASTNode *call, HCASTNode *arg);
void hc_ast_add_param(HCASTNode *func, HCASTNode *param, const char *name, HCType *type);
void hc_ast_print(const HCASTNode *node, int indent);

#endif /* MYSEED_HOLYC_AST_H */