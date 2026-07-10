/* jit_minic_token.c -- Mini-C tokenizer (self-contained lexer).
 *
 * minic_lex_next/init/cur/advance/expect/is_type. Types MinicLexer/
 * MinicToken/MinicTokType + fn decls in jit_internal.h. Minimal includes.
 */

#include "jit_internal.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

void minic_lex_next(MinicLexer *l) {
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

void minic_lex_init(MinicLexer *l, const char *src) {
    memset(l, 0, sizeof(*l));
    l->src = src;
    l->pos = 0;
    minic_lex_next(l);
    minic_lex_next(l);
}

MinicToken *minic_cur(MinicLexer *l) { return &l->cur; }

void minic_advance(MinicLexer *l) { minic_lex_next(l); }

int minic_expect(MinicLexer *l, MinicTokType t) {
    if (minic_cur(l)->type == t) { minic_advance(l); return 0; }
    return -1;
}

int minic_is_type(MinicTokType t) {
    return t == TOK_INT || t == TOK_LONG || t == TOK_I64 || t == TOK_U8 || t == TOK_VOID;
}
