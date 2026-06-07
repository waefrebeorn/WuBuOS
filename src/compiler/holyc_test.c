/*
 * holyc_test.c — My Seed HolyC Compiler Test Suite
 */

#include "holyc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_run = 0, g_pass = 0;

#define T(expr, expected) do { \
    int64_t r = hc_eval(expr); \
    g_run++; \
    if (r == (int64_t)(expected)) { g_pass++; printf("  ✅ eval %-30s = %lld\n", expr, (long long)r); } \
    else { printf("  ❌ eval %-30s = %lld (expected %lld)\n", expr, (long long)r, (long long)(int64_t)(expected)); } \
} while(0)

#define L(expected, actual, msg) do { \
    g_run++; \
    if ((actual) == (expected)) { g_pass++; printf("  ✅ %s\n", msg); } \
    else { printf("  ❌ %s (got %d)\n", msg, (int)(actual)); } \
} while(0)

int main(void) {
    printf("═══ HolyC Compiler Test Suite ═══\n\n");

    /* ── Lexer Tests ── */
    printf("[Lexer]\n");
    {
        HCLexer lex;
        hc_lex_init(&lex, "42");
        L(HC_TOK_INT, lex.tok.type, "int literal");
        L(42, (int)lex.tok.int_val, "int value 42");
    }
    {
        HCLexer lex;
        hc_lex_init(&lex, "0xFF");
        L(HC_TOK_INT, lex.tok.type, "hex literal");
        L(255, (int)lex.tok.int_val, "hex value 0xFF");
    }
    {
        HCLexer lex;
        hc_lex_init(&lex, "0b1010");
        L(HC_TOK_INT, lex.tok.type, "binary literal");
        L(10, (int)lex.tok.int_val, "binary value 0b1010");
    }
    {
        HCLexer lex;
        hc_lex_init(&lex, "3.14");
        L(HC_TOK_FLOAT, lex.tok.type, "float literal");
    }
    {
        HCLexer lex;
        hc_lex_init(&lex, "\"hello\"");
        L(HC_TOK_STRING, lex.tok.type, "string literal");
    }
    {
        HCLexer lex;
        hc_lex_init(&lex, "myVar");
        L(HC_TOK_IDENT, lex.tok.type, "identifier");
    }
    {
        HCLexer lex;
        hc_lex_init(&lex, "I64");
        L(HC_KW_I64, lex.tok.type, "keyword I64");
    }
    {
        HCLexer lex;
        hc_lex_init(&lex, "U0");
        L(HC_KW_U0, lex.tok.type, "keyword U0");
    }
    {
        HCLexer lex;
        hc_lex_init(&lex, "==");
        L(HC_TOK_EQ, lex.tok.type, "operator ==");
    }
    {
        HCLexer lex;
        hc_lex_init(&lex, "->");
        L(HC_TOK_ARROW, lex.tok.type, "operator ->");
    }
    {
        HCLexer lex;
        hc_lex_init(&lex, "++");
        L(HC_TOK_PLUS_PLUS, lex.tok.type, "operator ++");
    }

    /* ── Parser Tests ── */
    printf("\n[Parser]\n");
    {
        HCLexer lex; hc_lex_init(&lex, "42");
        HCParser p; hc_parse_init(&p, &lex);
        HCASTNode *ast = hc_parse_expr(&p);
        L(1, ast && ast->kind == HC_AST_INT_LIT && ast->int_val == 42, "parse int lit");
        hc_ast_free(ast);
    }
    {
        HCLexer lex; hc_lex_init(&lex, "3 + 4");
        HCParser p; hc_parse_init(&p, &lex);
        HCASTNode *ast = hc_parse_expr(&p);
        L(1, ast && ast->kind == HC_AST_ADD, "parse add");
        hc_ast_free(ast);
    }
    {
        HCLexer lex; hc_lex_init(&lex, "5 * 6");
        HCParser p; hc_parse_init(&p, &lex);
        HCASTNode *ast = hc_parse_expr(&p);
        L(1, ast && ast->kind == HC_AST_MUL, "parse mul");
        hc_ast_free(ast);
    }
    {
        HCLexer lex; hc_lex_init(&lex, "2 + 3 * 4");
        HCParser p; hc_parse_init(&p, &lex);
        HCASTNode *ast = hc_parse_expr(&p);
        L(1, ast && ast->kind == HC_AST_ADD && ast->right && ast->right->kind == HC_AST_MUL, "precedence: 2+(3*4)");
        hc_ast_free(ast);
    }
    {
        HCLexer lex; hc_lex_init(&lex, "-5");
        HCParser p; hc_parse_init(&p, &lex);
        HCASTNode *ast = hc_parse_expr(&p);
        L(1, ast && ast->kind == HC_AST_NEG, "parse neg");
        hc_ast_free(ast);
    }
    {
        HCLexer lex; hc_lex_init(&lex, "3 < 5");
        HCParser p; hc_parse_init(&p, &lex);
        HCASTNode *ast = hc_parse_expr(&p);
        L(1, ast && ast->kind == HC_AST_LT, "parse lt");
        hc_ast_free(ast);
    }
    {
        HCLexer lex; hc_lex_init(&lex, "(2 + 3) * 4");
        HCParser p; hc_parse_init(&p, &lex);
        HCASTNode *ast = hc_parse_expr(&p);
        L(1, ast && ast->kind == HC_AST_MUL && ast->left && ast->left->kind == HC_AST_ADD, "parens precedence");
        hc_ast_free(ast);
    }
    {
        HCLexer lex; hc_lex_init(&lex, "I64 add(I64 a, I64 b) { return a + b; }");
        HCParser p; hc_parse_init(&p, &lex);
        HCASTNode *ast = hc_parse_decl(&p);
        L(1, ast && ast->kind == HC_AST_FUNC_DECL && ast->n_params == 2, "parse func decl");
        hc_ast_free(ast);
    }

    /* ── Eval Tests ── */
    printf("\n[Codegen/Eval]\n");
    T("42", 42);
    T("3 + 4", 7);
    T("10 - 3", 7);
    T("5 * 5", 25);
    T("42 / 7", 6);
    T("17 % 5", 2);
    T("-5", -5);
    T("(3 + 4) * 5", 35);
    T("2 * 3 + 4", 10);
    T("100 / 10", 10);
    T("3 < 5", 1);
    T("5 > 3", 1);
    T("7 == 7", 1);
    T("7 != 8", 1);
    T("3 > 5", 0);
    T("7 != 7", 0);
    T("5 <= 5", 1);
    T("5 >= 5", 1);
    T("4 <= 3", 0);

    printf("\n═══ Results: %d/%d passed ═══\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
