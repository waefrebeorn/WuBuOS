/* holyc_parse_ast.c -- HolyC AST construction/utility helpers (self-contained).
 *
 * hc_ast_new / hc_ast_free / hc_ast_add_stmt / hc_ast_add_arg. Uses HCASTNode
 * (holyc_parse.h). Minimal includes.
 */

#include "holyc_parse_internal.h"

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
