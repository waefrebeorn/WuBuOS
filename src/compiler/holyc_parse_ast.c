/* holyc_parse_ast.c -- HolyC AST construction/utility helpers (self-contained).
 *
 * hc_ast_new / hc_ast_free / hc_ast_add_stmt / hc_ast_add_arg. Uses HCASTNode
 * (holyc_parse.h). Minimal includes.
 */

#include "holyc_parse_internal.h"
#include <stdio.h>

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

/* -- AST print + type-size (moved from holyc_parse.c to consolidate AST utils) -- */

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
