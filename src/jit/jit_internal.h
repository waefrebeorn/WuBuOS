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
    TOK_INT, TOK_LONG, TOK_I64, TOK_U8, TOK_VOID, TOK_STRUCT, TOK_SIZEOF,
    TOK_RETURN, TOK_IF, TOK_ELSE, TOK_WHILE,
    TOK_IDENT, TOK_NUMBER, TOK_STRING,
    TOK_PLUS, TOK_MINUS, TOK_STAR, TOK_SLASH,
    TOK_EQ, TOK_NEQ, TOK_LT, TOK_GT, TOK_LEQ, TOK_GEQ,
    TOK_ASSIGN, TOK_NOT,
    TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE,
    TOK_LBRACKET, TOK_RBRACKET,
    TOK_DOT, TOK_ARROW, TOK_AMP,
    TOK_SEMI, TOK_COMMA,
} MinicTokType;

typedef struct {
    MinicTokType  type;
    char          text[256];
    int64_t       ival;
} MinicToken;

typedef struct {
    const char    *src;
    int            pos;
    MinicToken    cur;
    MinicToken    peek;
} MinicLexer;

/* -- Tokenizer API (jit_minic_token.c) --------------------------- */
void   minic_lex_next(MinicLexer *l);
void   minic_lex_init(MinicLexer *l, const char *src);
MinicToken *minic_cur(MinicLexer *l);
MinicToken *minic_peek(MinicLexer *l);     /* 1-token lookahead */
MinicToken  minic_peek2(MinicLexer *l);    /* 2-token lookahead (no consume) */
void   minic_advance(MinicLexer *l);
int    minic_expect(MinicLexer *l, MinicTokType t);
int    minic_is_type(MinicTokType t);

/* -- Mini-C type system (jit_minic_type.c) -----------------------
 * Subsystem A: struct/array/pointer types with a layout pass that delivers
 * #19 (struct field reordering — members sorted by descending alignment to
 * minimize padding). The compiler owns the layout, so reordering is
 * transparent to generated code. */
typedef enum {
    MTY_I64 = 0,     /* 8-byte integer (long/int/I64) */
    MTY_U8,          /* 1-byte integer */
    MTY_STRUCT,      /* named aggregate with ordered members */
    MTY_ARRAY,       /* elem type + count */
    MTY_PTR,         /* pointer to elem type */
} MtyKind;

#define MINIC_MAX_TYPES 64
#define MINIC_MAX_MEMBERS 32

typedef struct MinicMember {
    char      name[64];
    int       mty;         /* index into registry, or -1 */
    int32_t   offset;      /* byte offset within the struct */
    int64_t   size;        /* cached sizeof */
    int       align;       /* cached alignment */
} MinicMember;

typedef struct MinicType {
    char       name[64];   /* struct tag name, or "" for anonymous */
    MtyKind    kind;
    int64_t    size;
    int        align;
    int        elem;       /* array/ptr element type index, or -1 */
    int64_t    count;      /* array count */
    MinicMember members[MINIC_MAX_MEMBERS];
    int        n_members;
    int        defined;    /* struct fully parsed (layout computed) */
} MinicType;

typedef struct {
    MinicType types[MINIC_MAX_TYPES];
    int n_types;
    int next_anon;
} MinicTypeRegistry;

void     minic_type_registry_init(MinicTypeRegistry *r);
int      minic_type_new(MinicTypeRegistry *r);               /* alloc slot */
MinicType *minic_type_find(MinicTypeRegistry *r, const char *name);
int      minic_type_index(MinicTypeRegistry *r, MinicType *t);
void     minic_type_layout(MinicTypeRegistry *r, MinicType *s); /* run #19 reorder */
int      minic_type_member_offset(MinicTypeRegistry *r, int type_idx, const char *member);
int      minic_type_member_size(MinicTypeRegistry *r, int type_idx, const char *member);
int      minic_type_size(MinicTypeRegistry *r, int type_idx);
int      minic_type_align(MinicTypeRegistry *r, int type_idx);
int      minic_type_is_struct(MinicTypeRegistry *r, int type_idx);
int      minic_type_is_ptr(MinicTypeRegistry *r, int type_idx);
int      minic_type_is_array(MinicTypeRegistry *r, int type_idx);
int      minic_type_elem(MinicTypeRegistry *r, int type_idx);


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
