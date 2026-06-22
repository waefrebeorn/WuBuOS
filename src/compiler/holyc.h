/*
 * holyc.h  --  My Seed HolyC Compiler
 *
 * A minimal, self-contained HolyC compiler in pure C11.
 * Ported design from ZealOS Compiler/*.ZC and holyc-lang.
 *
 * Pipeline: source → lexer → tokens → parser → AST → codegen → x86-64
 *
 * HolyC is a C-like language with:
 *   - Default I64 (64-bit int) type for untyped expressions
 *   - Opaque types via U0
 *   - Class-based OOP (member functions, inheritance)
 *   - First-class function pointers
 *   - Assembly inline blocks
 *   - JIT-oriented: compile and call immediately
 *
 * This compiler targets our JIT mmap backend for code emission.
 */

#ifndef MYSEED_HOLYC_H
#define MYSEED_HOLYC_H

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

/* -- Token Types -------------------------------------------------- */

typedef enum {
    /* End of input */
    HC_TOK_EOF = 0,

    /* Literals */
    HC_TOK_INT,         /* 42, 0xFF, 0b1010 */
    HC_TOK_FLOAT,       /* 3.14, 1.0e10 */
    HC_TOK_CHAR,        /* 'a' */
    HC_TOK_STRING,      /* "hello" */

    /* Identifiers and keywords */
    HC_TOK_IDENT,       /* variable/type names */

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

    /* Type keywords */
    HC_KW_I0, HC_KW_I8, HC_KW_I16, HC_KW_I32, HC_KW_I64,
    HC_KW_U0, HC_KW_U8, HC_KW_U16, HC_KW_U32, HC_KW_U64,
    HC_KW_F64,
    HC_KWBool,
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
    HC_TOK_PLUS,        /* + */
    HC_TOK_MINUS,       /* - */
    HC_TOK_STAR,        /* * */
    HC_TOK_SLASH,       /* / */
    HC_TOK_PERCENT,     /* % */
    HC_TOK_AMP,         /* & */
    HC_TOK_PIPE,        /* | */
    HC_TOK_CARET,       /* ^ */
    HC_TOK_TILDE,       /* ~ */
    HC_TOK_BANG,        /* ! */
    HC_TOK_ASSIGN,      /* = */
    HC_TOK_LT,          /* < */
    HC_TOK_GT,          /* > */

    /* Compound operators */
    HC_TOK_EQ,          /* == */
    HC_TOK_NE,          /* != */
    HC_TOK_LE,          /* <= */
    HC_TOK_GE,          /* >= */
    HC_TOK_AND,         /* && */
    HC_TOK_OR,          /* || */
    HC_TOK_SHL,         /* << */
    HC_TOK_SHR,         /* >> */
    HC_TOK_PLUS_EQ,     /* += */
    HC_TOK_MINUS_EQ,    /* -= */
    HC_TOK_STAR_EQ,     /* *= */
    HC_TOK_SLASH_EQ,    /* /= */
    HC_TOK_AMP_EQ,      /* &= */
    HC_TOK_PIPE_EQ,     /* |= */
    HC_TOK_CARET_EQ,    /* ^= */
    HC_TOK_SHL_EQ,      /* <<= */
    HC_TOK_SHR_EQ,      /* >>= */
    HC_TOK_PERCENT_EQ,  /* %= */
    HC_TOK_PLUS_PLUS,   /* ++ */
    HC_TOK_MINUS_MINUS, /* -- */
    HC_TOK_ARROW,       /* -> */
    HC_TOK_DOT,         /* . */

    /* Delimiters */
    HC_TOK_LPAREN,      /* ( */
    HC_TOK_RPAREN,      /* ) */
    HC_TOK_LBRACE,      /* { */
    HC_TOK_RBRACE,      /* } */
    HC_TOK_LBRACKET,    /* [ */
    HC_TOK_RBRACKET,    /* ] */
    HC_TOK_COMMA,       /* , */
    HC_TOK_SEMI,        /* ; */
    HC_TOK_COLON,       /* : */
    HC_TOK_QUESTION,    /* ? */

    /* Special */
    HC_TOK_ASM,         /* asm { ... } */
    HC_TOK_INCLUDE,     /* #include */
    HC_TOK_DEFINE,      /* #define */
} HCTokenType;

/* -- Token -------------------------------------------------------- */

typedef struct {
    HCTokenType  type;
    char         text[HC_MAX_TOKEN_LEN];
    int64_t      int_val;    /* For HC_TOK_INT */
    double       float_val;  /* For HC_TOK_FLOAT */
    int          line;
    int          col;
} HCToken;

/* -- Type System -------------------------------------------------- */

typedef enum {
    HC_TYPE_VOID = 0,
    HC_TYPE_I8,  HC_TYPE_I16, HC_TYPE_I32, HC_TYPE_I64,
    HC_TYPE_U8,  HC_TYPE_U16, HC_TYPE_U32, HC_TYPE_U64,
    HC_TYPE_F64,
    HC_TYPE_BOOL,
    HC_TYPE_PTR,     /* Pointer to base_type */
    HC_TYPE_ARRAY,   /* array of base_type, size in array_size */
    HC_TYPE_FUNC,    /* Function: ret_type + param_types */
    HC_TYPE_STRUCT,  /* Struct/class */
    HC_TYPE_UNION,   /* Union */
    HC_TYPE_ENUM,
    HC_TYPE_TYPEDEF,
} HCTypeKind;

typedef struct HCType HCType;
struct HCType {
    HCTypeKind   kind;
    HCType      *base;       /* For ptr/array/func: the element type */
    int          array_size; /* For arrays */
    char         name[HC_MAX_IDENT_LEN]; /* For struct/enum/typedef */
    /* Struct members */
    struct {
        char   name[HC_MAX_IDENT_LEN];
        HCType *type;
        int    offset;
    } members[HC_MAX_PARAMS]; /* Reuse param limit for member count */
    int          n_members;
    int          size;       /* Total struct size in bytes */
    int          align;      /* Struct alignment requirement */
    unsigned     is_const : 1;
    unsigned     is_static : 1;
    unsigned     is_volatile : 1;
    unsigned     is_unsigned : 1;
};

/* Size of a type in bytes */
size_t hc_type_size(const HCType *t);

/* -- AST Node Types ----------------------------------------------- */

typedef enum {
    /* Literals */
    HC_AST_INT_LIT,
    HC_AST_FLOAT_LIT,
    HC_AST_STRING_LIT,
    HC_AST_CHAR_LIT,
    HC_AST_BOOL_LIT,

    /* Identifiers */
    HC_AST_IDENT,

    /* Unary ops */
    HC_AST_NEG,         /* -expr */
    HC_AST_NOT,         /* !expr */
    HC_AST_BITNOT,      /* ~expr */
    HC_AST_DEREF,       /* *expr */
    HC_AST_ADDR,        /* &expr */
    HC_AST_PRE_INC,     /* ++expr */
    HC_AST_PRE_DEC,     /* --expr */
    HC_AST_POST_INC,    /* expr++ */
    HC_AST_POST_DEC,    /* expr-- */
    HC_AST_CAST,        /* (type)expr */

    /* Binary ops */
    HC_AST_ADD, HC_AST_SUB, HC_AST_MUL, HC_AST_DIV, HC_AST_MOD,
    HC_AST_AND, HC_AST_OR, HC_AST_BITAND, HC_AST_BITOR, HC_AST_XOR,
    HC_AST_SHL, HC_AST_SHR,
    HC_AST_EQ, HC_AST_NE, HC_AST_LT, HC_AST_LE, HC_AST_GT, HC_AST_GE,

    /* Assignment */
    HC_AST_ASSIGN,
    HC_AST_ADD_ASSIGN, HC_AST_SUB_ASSIGN, HC_AST_MUL_ASSIGN, HC_AST_DIV_ASSIGN,

    /* Ternary */
    HC_AST_TERNARY,     /* cond ? then : else */

    /* Statements */
    HC_AST_EXPR_STMT,   /* expression; */
    HC_AST_BLOCK,       /* { stmts... } */
    HC_AST_IF,          /* if (cond) then [else else] */
    HC_AST_WHILE,       /* while (cond) body */
    HC_AST_DO_WHILE,    /* do body while (cond) */
    HC_AST_FOR,         /* for (init; cond; update) body */
    HC_AST_RETURN,      /* return expr */
    HC_AST_BREAK,
    HC_AST_CONTINUE,

    /* Declarations */
    HC_AST_VAR_DECL,    /* type name [= init] */
    HC_AST_FUNC_DECL,   /* type name(params) { body } */
    HC_AST_FUNC_CALL,   /* name(args) */
    HC_AST_STRUCT_DECL,
    HC_AST_EXTERN_DECL,      /* extern "C" func_name(params) -> ret_type; */

    /* Array/struct access */
    HC_AST_INDEX,       /* expr[index] */
    HC_AST_MEMBER,      /* expr.member */
    HC_AST_ARROW,       /* expr->member */
} HCASTKind;

/* -- AST Node ----------------------------------------------------- */

typedef struct HCASTNode HCASTNode;
struct HCASTNode {
    HCASTKind   kind;
    HCType     *type;      /* Result type (inferred or declared) */

    /* For literals */
    int64_t     int_val;
    double      float_val;
    char        str_val[HC_MAX_STRING_LEN];

    /* For identifiers */
    char        ident[HC_MAX_IDENT_LEN];

    /* Unary: child */
    HCASTNode  *child;

    /* Binary: left, right */
    HCASTNode  *left;
    HCASTNode  *right;

    /* Ternary: cond, then, else */
    HCASTNode  *cond;
    HCASTNode  *then_branch;
    HCASTNode  *else_branch;

    /* Blocks / function body: list of statements */
    HCASTNode **stmts;
    int         n_stmts;
    int         stmts_cap;

    /* Function call: callee + args */
    HCASTNode  *callee;
    HCASTNode **args;
    int         n_args;
    int         args_cap;

    /* Variable declaration: init value */
    HCASTNode  *init;

    /* Function declaration: params + body */
    char        param_names[HC_MAX_PARAMS][HC_MAX_IDENT_LEN];
    HCType     *param_types[HC_MAX_PARAMS];
    int         n_params;
    HCASTNode  *body;

    /* Extern declaration: C function name + params + return type */
    char        extern_c_name[HC_MAX_IDENT_LEN];
    HCType     *extern_ret_type;
    HCType     *extern_param_types[HC_MAX_PARAMS];
    int         extern_n_params;

    /* For: init + cond + update + body */
    HCASTNode  *init_expr;
    HCASTNode  *update;

    /* Line/col for error reporting */
    int         line;
    int         col;
};

/* -- Lexer -------------------------------------------------------- */

typedef struct {
    const char *src;        /* Source text */
    int         pos;        /* Current position */
    int         line;       /* Current line */
    int         col;        /* Current column */
    HCToken     tok;        /* Current token */
    bool        has_error;
    char        error[256];
} HCLexer;

/* Initialize lexer with source text */
void hc_lex_init(HCLexer *lex, const char *src);

/* Advance to next token. Returns token type. */
HCTokenType hc_lex_next(HCLexer *lex);

/* Peek current token without advancing */
HCTokenType hc_lex_peek(HCLexer *lex);

/* Expect a specific token type, error if not */
int hc_lex_expect(HCLexer *lex, HCTokenType expected);

/* -- Parser ------------------------------------------------------- */

typedef struct {
    HCLexer    *lex;
    HCASTNode  *ast;       /* Root of parsed AST */
    bool        has_error;
    char        errors[HC_MAX_ERRORS][256];
    int         n_errors;
} HCParser;

/* Initialize parser */
void hc_parse_init(HCParser *p, HCLexer *lex);

/* Parse a full compilation unit (all top-level declarations) */
HCASTNode *hc_parse_compilation_unit(HCParser *p);

/* Parse a single expression */
HCASTNode *hc_parse_expr(HCParser *p);

/* Parse a single statement */
HCASTNode *hc_parse_stmt(HCParser *p);

/* Parse a declaration */
HCASTNode *hc_parse_decl(HCParser *p);

/* Parse a block { ... } */
HCASTNode *parse_block(HCParser *p);

/* Peek current token (for eval dispatch) */
HCTokenType hc_parse_peek(HCParser *p);

/* -- Symbol Table ------------------------------------------------- */

typedef struct {
    char        name[HC_MAX_IDENT_LEN];
    HCType     *type;
    int         stack_offset; /* For locals */
    bool        is_global;
    bool        is_param;
} HCSymbol;

typedef struct HCFunction HCFunction;
struct HCFunction {
    char        name[HC_MAX_IDENT_LEN];
    void       *func_ptr;       /* Compiled function address */
    int         n_params;
};

#define HC_MAX_FUNCTIONS 64

typedef struct {
    HCSymbol    locals[HC_MAX_LOCALS];
    int         n_locals;
    int         stack_size;   /* Total stack frame size */
} HCSymTab;

/* -- Code Generator ----------------------------------------------- */

typedef struct {
    uint8_t    *code;        /* Output buffer for machine code */
    size_t      code_size;
    size_t      code_cap;
    /* Data section for string literals and globals */
    uint8_t    *data;
    size_t      data_size;
    size_t      data_cap;
    HCSymTab    symbols;
    int         label_count; /* For generating unique labels */
    int         loop_depth;  /* For break/continue */
    int         break_label;
    int         continue_label;
    /* Break/continue patch stacks */
    size_t      break_patches[10][16];  /* [loop_depth][nested breaks] */
    int         n_break_patches[10];
    size_t      continue_patches[10][16];
    int         n_continue_patches[10];
    /* Function table for function calls */
    HCFunction  functions[HC_MAX_FUNCTIONS];
    int         n_functions;

    /* Extern C function table for foreign function calls */
    struct {
        char   c_name[HC_MAX_IDENT_LEN];  /* C function name */
        void  *func_addr;                 /* Resolved at runtime via dlsym */
    } extern_funcs[32];
    int         n_extern_funcs;

    bool        has_error;
    char        error[256];
} HCGen;

/* Initialize code generator */
void hc_gen_init(HCGen *gen);

/* Generate code for an AST node */
int hc_gen_node(HCGen *gen, const HCASTNode *node);

/* Generate code for a full function */
int hc_gen_function(HCGen *gen, const HCASTNode *func);

/* Get the generated machine code */
const uint8_t *hc_gen_code(const HCGen *gen, size_t *out_size);

/* -- AST Utilities ------------------------------------------------ */

/* Allocate a new AST node */
HCASTNode *hc_ast_new(HCASTKind kind);

/* Free an AST tree */
void hc_ast_free(HCASTNode *node);

/* Add a statement to a block node */
void hc_ast_add_stmt(HCASTNode *block, HCASTNode *stmt);

/* Add an argument to a function call node */
void hc_ast_add_arg(HCASTNode *call, HCASTNode *arg);

/* Print AST for debugging */
void hc_ast_print(const HCASTNode *node, int indent);

/* -- Compiler (top-level) ----------------------------------------- */

typedef struct {
    HCLexer     lex;
    HCParser    parse;
    HCGen       gen;
    HCASTNode  *ast;
    bool        has_error;
    char        error[256];
} HCCompiler;

/*
 * Compile HolyC source code.
 * Returns pointer to executable machine code, or NULL on error.
 * out_size receives the size of the generated code.
 */
void *hc_compile(const char *source, size_t *out_size);

/*
 * Compile and execute HolyC source.
 * Returns the I64 return value, or 0 on error.
 */
int64_t hc_eval(const char *source);

/*
 * Compile HolyC function from source.
 * Returns a function pointer that can be called, or NULL on error.
 */
void *hc_compile_func(const char *source, const char *func_name);

#endif /* MYSEED_HOLYC_H */
