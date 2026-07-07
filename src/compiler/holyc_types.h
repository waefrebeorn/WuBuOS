/*
 * holyc_types.h  --  HolyC Compiler Core Types
 * Opaque struct forward declarations and shared type definitions.
 * C11, no external dependencies.
 */
#ifndef MYSEED_HOLYC_TYPES_H
#define MYSEED_HOLYC_TYPES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* -- Limits ------------------------------------------------------- */

#define HC_MAX_TOKEN_LEN    256
#define HC_MAX_IDENT_LEN    64
#define HC_MAX_AST_DEPTH    128
#define HC_MAX_LOCALS       64
#define HC_MAX_PARAMS       16
#define HC_MAX_STRING_LEN   4096
#define HC_MAX_ERRORS       32
#define HC_MAX_FUNCTIONS    64

/* -- Token Types -------------------------------------------------- */

typedef enum {
    HC_TOK_EOF = 0,

    /* Literals */
    HC_TOK_INT,
    HC_TOK_FLOAT,
    HC_TOK_CHAR,
    HC_TOK_STRING,

    HC_TOK_IDENT,

    /* Keywords */
    HC_KW_IF,
    HC_KW_ELSE,
    HC_KW_WHILE,
    HC_KW_FOR,
    HC_KW_DO,
    HC_KW_SWITCH,
    HC_KW_CASE,
    HC_KW_DEFAULT,
    HC_KW_BREAK,
    HC_KW_CONTINUE,
    HC_KW_RETURN,
    HC_KW_GOTO,

    HC_KW_I0, HC_KW_I8, HC_KW_I16, HC_KW_I32, HC_KW_I64,
    HC_KW_U0, HC_KW_U8, HC_KW_U16, HC_KW_U32, HC_KW_U64,
    HC_KW_F64,
    HC_KW_BOOL,
    /* Struct/class */
    HC_KW_CLASS,
    HC_KW_STRUCT,
    HC_KW_UNION,
    HC_KW_TYPEDEF,
    HC_KW_ENUM,
    HC_KW_STATIC,
    HC_KW_EXTERN,
    HC_KW_PUBLIC,
    HC_KW_CONST,
    HC_KW_VOLATILE,
    HC_KW_INLINE,
    HC_KW_UNUSED,  /* dummy to keep enum open */

    /* Operators */
    HC_TOK_PLUS,
    HC_TOK_MINUS,
    HC_TOK_STAR,
    HC_TOK_SLASH,
    HC_TOK_PERCENT,
    HC_TOK_AMP,
    HC_TOK_PIPE,
    HC_TOK_CARET,
    HC_TOK_TILDE,
    HC_TOK_BANG,
    HC_TOK_ASSIGN,
    HC_TOK_LT,
    HC_TOK_GT,

    HC_TOK_LE,
    HC_TOK_GE,
    HC_TOK_EQ,
    HC_TOK_NE,
    HC_TOK_AND,
    HC_TOK_OR,
    HC_TOK_LSHIFT,
    HC_TOK_RSHIFT,
    HC_TOK_SHL,
    HC_TOK_SHR,
    HC_TOK_INC,
    HC_TOK_DEC,
    HC_TOK_ARROW,
    HC_TOK_DOT,
    HC_TOK_LPAREN,
    HC_TOK_RPAREN,
    HC_TOK_LBRACE,
    HC_TOK_RBRACE,
    HC_TOK_LBRACKET,
    HC_TOK_RBRACKET,
    HC_TOK_COMMA,
    HC_TOK_SEMI,
    HC_TOK_COLON,
    HC_TOK_QUESTION,
    HC_TOK_ARROW_RET,
    HC_TOK_PLUS_PLUS,
    HC_TOK_MINUS_MINUS,

    HC_TOK_PLUS_ASSIGN,
    HC_TOK_MINUS_ASSIGN,
    HC_TOK_STAR_ASSIGN,
    HC_TOK_SLASH_ASSIGN,
    HC_TOK_PERCENT_ASSIGN,
    HC_TOK_AMP_ASSIGN,
    HC_TOK_PIPE_ASSIGN,
    HC_TOK_CARET_ASSIGN,
    HC_TOK_LSHIFT_ASSIGN,
    HC_TOK_RSHIFT_ASSIGN,
    HC_TOK_SHL_ASSIGN,
    HC_TOK_SHR_ASSIGN,

    HC_TOK_ELLIPSIS,
} HCTokenType;

/* -- Token -------------------------------------------------------- */

typedef struct {
    HCTokenType type;
    char text[HC_MAX_TOKEN_LEN];
    int64_t int_val;
    double float_val;
    char str_val[HC_MAX_STRING_LEN];
    int line;
    int col;
} HCToken;

/* -- Type System -------------------------------------------------- */

typedef enum {
    HC_TYPE_VOID = 0,
    HC_TYPE_I8,
    HC_TYPE_I16,
    HC_TYPE_I32,
    HC_TYPE_I64,
    HC_TYPE_U8,
    HC_TYPE_U16,
    HC_TYPE_U32,
    HC_TYPE_U64,
    HC_TYPE_F64,
    HC_TYPE_BOOL,
    HC_TYPE_PTR,
    HC_TYPE_ARRAY,
    HC_TYPE_STRUCT,
    HC_TYPE_UNION,
    HC_TYPE_ENUM,
    HC_TYPE_FUNC,
} HCTypeKind;

typedef struct HCType HCType;

struct HCType {
    HCTypeKind kind;
    HCType *base;              /* for pointers/arrays */
    int64_t size;              /* size in bytes */
    int64_t align;             /* alignment requirement */
    int n_members;             /* for structs/unions */
    char name[HC_MAX_IDENT_LEN]; /* for struct/enum/typedef name */
    struct {
        char name[HC_MAX_IDENT_LEN];
        HCType *type;
        int64_t offset;
    } members[32];
    int array_size;            /* for arrays */
    /* for function types */
    HCType *ret_type;
    HCType **param_types;
    int n_params;
};

/* -- AST Node Kinds ----------------------------------------------- */

typedef enum {
    HC_AST_NONE = 0,
    HC_AST_INT_LIT,
    HC_AST_FLOAT_LIT,
    HC_AST_CHAR_LIT,
    HC_AST_STRING_LIT,
    HC_AST_BOOL_LIT,
    HC_AST_IDENT,
    HC_AST_ADD,
    HC_AST_SUB,
    HC_AST_MUL,
    HC_AST_DIV,
    HC_AST_MOD,
    HC_AST_AND,
    HC_AST_OR,
    HC_AST_BITAND,
    HC_AST_BITOR,
    HC_AST_BITXOR,
    HC_AST_SHL,
    HC_AST_SHR,
    HC_AST_EQ,
    HC_AST_NE,
    HC_AST_LT,
    HC_AST_LE,
    HC_AST_GT,
    HC_AST_GE,
    HC_AST_UNARY,
    HC_AST_CAST,
    HC_AST_CALL,
    HC_AST_FUNC_CALL,
    HC_AST_INDEX,
    HC_AST_DOT,
    HC_AST_MEMBER,
    HC_AST_ARROW,
    HC_AST_TERNARY,
    HC_AST_ASSIGN,
    HC_AST_COMPOUND_ASSIGN,
    HC_AST_PRE_INC,
    HC_AST_PRE_DEC,
    HC_AST_POST_INC,
    HC_AST_POST_DEC,
    HC_AST_EXPR_STMT,
    HC_AST_RETURN,
    HC_AST_IF,
    HC_AST_WHILE,
    HC_AST_FOR,
    HC_AST_DO_WHILE,
    HC_AST_BLOCK,
    HC_AST_VAR_DECL,
    HC_AST_FUNC_DECL,
    HC_AST_EXTERN_DECL,
    HC_AST_STRUCT_DECL,
    HC_AST_BREAK,
    HC_AST_CONTINUE,
    HC_AST_NEG,
    HC_AST_NOT,
    HC_AST_BITNOT,
    HC_AST_DEREF,
    HC_AST_ADDR,
    HC_AST_XOR,
    HC_AST_MOD_ASSIGN,
    HC_AST_SHL_ASSIGN,
    HC_AST_SHR_ASSIGN,
    HC_AST_AMP_ASSIGN,
    HC_AST_PIPE_ASSIGN,
    HC_AST_CARET_ASSIGN,
    HC_AST_ADD_ASSIGN,
    HC_AST_SUB_ASSIGN,
    HC_AST_MUL_ASSIGN,
    HC_AST_DIV_ASSIGN,
} HCASTKind;

/* -- Forward declarations for opaque structs ---------------------------------- */

typedef struct HCLexer HCLexer;
typedef struct HCParser HCParser;
typedef struct HCGen HCGen;
typedef struct HCCompiler HCCompiler;
typedef struct HCASTNode HCASTNode;
typedef struct HCSymbol HCSymbol;
typedef struct HCSymTab HCSymTab;
typedef struct HCFunction HCFunction;

/* -- Symbol Table ------------------------------------------------------------- */

struct HCSymbol {
    char name[HC_MAX_IDENT_LEN];
    HCType *type;
    int stack_offset;
    bool is_global;
    bool is_param;
};

struct HCSymTab {
    HCSymbol locals[HC_MAX_LOCALS];
    int n_locals;
    int stack_size;
};

/* -- Function Table ----------------------------------------------------------- */

struct HCFunction {
    char name[HC_MAX_IDENT_LEN];
    void *func_ptr;
    int n_params;
};

/* -- Lexer struct (full definition needed by lexer.c) ------------------------ */

struct HCLexer {
    const char *src;
    int pos;
    int line;
    int col;
    HCToken tok;
    bool has_error;
    char error[256];
};

/* -- Parser struct (full definition needed by parser.c) ---------------------- */

struct HCParser {
    HCLexer *lex;
    HCASTNode *ast;
    bool has_error;
    char errors[HC_MAX_ERRORS][256];
    int n_errors;
};

/* -- Code Generator struct (full definition needed by codegen.c) ------------- */

struct HCGen {
    uint8_t *code;
    size_t code_size;
    size_t code_cap;
    uint8_t *data;
    size_t data_size;
    size_t data_cap;
    HCSymTab symbols;
    int label_count;
    int loop_depth;
    int break_label;
    int continue_label;
    size_t break_patches[10][16];
    int n_break_patches[10];
    size_t continue_patches[10][16];
    int n_continue_patches[10];
    HCFunction functions[HC_MAX_FUNCTIONS];
    int n_functions;
    struct {
        char c_name[HC_MAX_IDENT_LEN];
        void *func_addr;
    } extern_funcs[32];
    int n_extern_funcs;
    struct {
        size_t code_patch_pos;
        size_t global_offset;
    } global_patches[32];
    int n_global_patches;
    bool has_error;
    char error[256];
    bool has_prologue;   /* set once emit_prologue() has built a stack frame */
    bool in_function;    /* true while emitting a function body (vs module-level) */
};

/* -- Compiler struct (full definition) --------------------------------------- */

struct HCCompiler {
    HCLexer lex;
    HCParser parse;
    HCGen gen;
    HCASTNode *ast;
    bool has_error;
    char error[256];
};

#endif /* MYSEED_HOLYC_TYPES_H */