/* jit_internal.h -- Internal shared types/helpers for jit_minic sub-modules.
 * Mini-C lexer types (MinicTokType/MinicToken/MinicLexer) + tokenizer API.
 * Public API in jit.h. Tokenizer lives in jit_minic_token.c.
 */

#ifndef JIT_INTERNAL_H
#define JIT_INTERNAL_H

#include "jit.h"
#include <stdint.h>
#include <stdbool.h>
#include <string.h>


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

/* -- Tokenizer API (jit_minic_token.c) --------------------------- */
void   minic_lex_next(MinicLexer *l);
void   minic_lex_init(MinicLexer *l, const char *src);
MinicToken *minic_cur(MinicLexer *l);
void   minic_advance(MinicLexer *l);
int    minic_expect(MinicLexer *l, MinicTokType t);
int    minic_is_type(MinicTokType t);


/* -- x86-64 opcode encoders (jit_encode.c) ----------------------- */
int enc_mov_eax_imm32(unsigned char *buf, int32_t imm);
int enc_mov_rdi_imm64(unsigned char *buf, int64_t imm);
int enc_add_eax_edi(unsigned char *buf);
int enc_imul_eax_edi(unsigned char *buf);
int enc_sub_eax_esi(unsigned char *buf);
int enc_xor_eax_eax(unsigned char *buf);
int enc_ret(unsigned char *buf);
int enc_mov_eax_edi(unsigned char *buf);
int enc_add_eax_esi(unsigned char *buf);
int enc_mov_eax_esi(unsigned char *buf);
int enc_neg_eax(unsigned char *buf);

#endif /* JIT_INTERNAL_H */
