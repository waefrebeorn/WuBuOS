/*
 * holyc_test.c  --  My Seed HolyC Compiler Test Suite
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
    printf("=== HolyC Compiler Test Suite ===\n\n");

    /* -- Lexer Tests -- */
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

    /* -- Parser Tests -- */
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

    /* -- Eval Tests (basic arithmetic + comparison) -- */
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

    /* -- Cell 310: Ternary -- */
    printf("\n[Cell 310: Ternary]\n");
    T("1 ? 42 : 0", 42);
    T("0 ? 42 : 99", 99);
    T("3 < 5 ? 10 : 20", 10);
    T("5 < 3 ? 10 : 20", 20);
    T("1 ? 1 ? 2 : 3 : 4", 2);

    /* -- Cell 310: Logical AND -- */
    printf("\n[Cell 310: Logical AND]\n");
    T("1 && 1", 1);
    T("1 && 0", 0);
    T("0 && 1", 0);
    T("0 && 0", 0);
    T("3 && 5", 1);
    T("3 < 5 && 7 == 7", 1);
    T("3 > 5 && 7 == 7", 0);

    /* -- Cell 310: Logical OR -- */
    printf("\n[Cell 310: Logical OR]\n");
    T("0 || 1", 1);
    T("0 || 0", 0);
    T("1 || 0", 1);
    T("1 || 1", 1);
    T("3 > 5 || 7 == 7", 1);
    T("3 > 5 || 7 != 7", 0);

    /* -- Cell 310: Combined logical -- */
    printf("\n[Cell 310: Combined Logical]\n");
    T("1 && 1 || 0", 1);
    T("0 || 1 && 1", 1);
    T("0 || 0 && 1", 0);

    /* -- Cell 310: IF statement -- */
    printf("\n[Cell 310: IF Statement]\n");
    T("if (1) 42; else 0;", 42);
    T("if (0) 42; else 99;", 99);
    T("if (3 < 5) 10; else 20;", 10);

    /* -- Cell 310: WHILE loop -- */
    printf("\n[Cell 310: WHILE Statement]\n");
    T("while (0) 42;", 0);

    /* -- Cell 310: FOR loop -- */
    printf("\n[Cell 310: FOR Statement]\n");
    T("for (0; 0; 0) 42;", 0);

    /* -- Bitwise ops -- */
    printf("\n[Bitwise]\n");
    T("3 & 5", 1);
    T("3 | 5", 7);
    T("3 ^ 5", 6);
    T("~0", -1);

    /* -- Increment/Decrement -- */
    printf("\n[Inc/Dec]\n");
    T("{ I64 x = 5; ++x; }", 6);
    T("{ I64 x = 5; --x; }", 4);
    T("{ I64 x = 5; x++; }", 5);
    T("{ I64 x = 5; x--; }", 5);

    /* -- Deref/Address -- */
    printf("\n[Deref/Addr]\n");
    T("{ I64 x = 42; I64 *p = &x; *p; }", 42);
    T("{ I64 x = 10; &x; x; }", 10);  // address is non-zero stack address, then x returns 10

    /* -- Cast -- */
    printf("\n[Cast]\n");
    T("(I64)42", 42);
    T("(I64)3.14", 4614253070214989087LL);  // Double bit pattern for 3.14

    /* -- String Literals (Cell 311) -- */
    printf("\n[Cell 311: String Literals]\n");
    T("{ \"hello\"; 0; }", 0);  /* Just test it compiles and runs */

    /* -- Struct Definition (Cell 311) -- */
    printf("\n[Cell 311: Struct Definition]\n");
    T("{ struct A { I64 x; }; 0; }", 0);

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    printf("\n[Cell 311: Function Calls]\n");
    T("{ I64 foo() { return 42; } foo(); }", 42);
    T("{ I64 add(I64 a, I64 b) { return a + b; } add(3, 4); }", 7);
    T("{ I64 mul(I64 a, I64 b) { return a * b; } mul(5, 6); }", 30);

    printf("\n=== Results: %d/%d passed ===\n", g_pass, g_run);
    return (g_pass == g_run) ? 0 : 1;
}
