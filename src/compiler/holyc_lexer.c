/*
 * holyc_lexer.c — My Seed HolyC Lexer
 *
 * Tokenizes HolyC source text into a stream of tokens.
 * Ported from ZealOS/src/Compiler/Lex.ZC and holyc-lang/src/lexer.c
 *
 * HolyC lexer differences from C:
 *   - I64/U0/F64 are type keywords, not identifiers
 *   - Identifiers starting with '_' can be system symbols
 *   - Numeric literals: 0x (hex), 0b (binary), default I64
 */

#include "holyc.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* ── Keyword Table ──────────────────────────────────────────────── */

typedef struct {
    const char  *name;
    HCTokenType  type;
} HCKeyword;

static const HCKeyword hc_keywords[] = {
    /* Control flow */
    {"if",       HC_KW_IF},
    {"else",     HC_KW_ELSE},
    {"while",    HC_KW_WHILE},
    {"for",      HC_KW_FOR},
    {"do",       HC_KW_DO},
    {"switch",   HC_KW_SWITCH},
    {"case",     HC_KW_CASE},
    {"default",  HC_KW_DEFAULT},
    {"break",    HC_KW_BREAK},
    {"continue", HC_KW_CONTINUE},
    {"return",   HC_KW_RETURN},
    {"goto",     HC_KW_GOTO},

    /* HolyC type keywords */
    {"I0",       HC_KW_I0},
    {"I8",       HC_KW_I8},
    {"I16",      HC_KW_I16},
    {"I32",      HC_KW_I32},
    {"I64",      HC_KW_I64},
    {"U0",       HC_KW_U0},
    {"U8",       HC_KW_U8},
    {"U16",      HC_KW_U16},
    {"U32",      HC_KW_U32},
    {"U64",      HC_KW_U64},
    {"F64",      HC_KW_F64},
    {"Bool",     HC_KWBool},

    /* C-compatible type keywords (aliases) */
    {"void",     HC_KW_U0},
    {"char",     HC_KW_I8},
    {"short",    HC_KW_I16},
    {"int",      HC_KW_I32},
    {"long",     HC_KW_I64},
    {"float",    HC_KW_F64},
    {"double",   HC_KW_F64},
    {"bool",     HC_KWBool},

    /* Struct/class */
    {"class",    HC_KW_CLASS},
    {"struct",   HC_KW_STRUCT},
    {"union",    HC_KW_UNION},
    {"typedef",  HC_KW_TYPEDEF},
    {"enum",     HC_KW_ENUM},

    /* Storage class */
    {"static",   HC_KW_STATIC},
    {"extern",   HC_KW_EXTERN},
    {"public",   HC_KW_PUBLIC},
    {"const",    HC_KW_CONST},
    {"volatile", HC_KW_VOLATILE},
    {"inline",   HC_KW_INLINE},
};

#define N_KEYWORDS (sizeof(hc_keywords) / sizeof(hc_keywords[0]))

/* ── Lexer Helpers ──────────────────────────────────────────────── */

static char hc_peek(const HCLexer *lex) {
    return lex->src[lex->pos];
}

static char hc_advance(HCLexer *lex) {
    char c = lex->src[lex->pos];
    lex->pos++;
    if (c == '\n') {
        lex->line++;
        lex->col = 0;
    } else {
        lex->col++;
    }
    return c;
}

static bool hc_is_at_end(const HCLexer *lex) {
    return lex->src[lex->pos] == '\0';
}

static void hc_lex_error(HCLexer *lex, const char *msg) {
    lex->has_error = true;
    snprintf(lex->error, sizeof(lex->error), "line %d: %s", lex->line, msg);
}

/* ── Lexer Initialization ───────────────────────────────────────── */

void hc_lex_init(HCLexer *lex, const char *src) {
    memset(lex, 0, sizeof(*lex));
    lex->src = src;
    lex->line = 1;
    lex->col = 1;
    /* Prime the first token */
    hc_lex_next(lex);
}

HCTokenType hc_lex_peek(HCLexer *lex) {
    return lex->tok.type;
}

/* ── Skip Whitespace and Comments ───────────────────────────────── */

static void hc_skip_whitespace(HCLexer *lex) {
    while (!hc_is_at_end(lex)) {
        char c = hc_peek(lex);

        /* Whitespace */
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            hc_advance(lex);
            continue;
        }

        /* Line comment: // */
        if (c == '/' && lex->src[lex->pos + 1] == '/') {
            while (!hc_is_at_end(lex) && hc_peek(lex) != '\n')
                hc_advance(lex);
            continue;
        }

        /* Block comment: /* ... */
        if (c == '/' && lex->src[lex->pos + 1] == '*') {
            hc_advance(lex); /* / */
            hc_advance(lex); /* * */
            while (!hc_is_at_end(lex)) {
                if (hc_peek(lex) == '*' && lex->src[lex->pos + 1] == '/') {
                    hc_advance(lex); /* * */
                    hc_advance(lex); /* / */
                    break;
                }
                hc_advance(lex);
            }
            continue;
        }

        break;
    }
}

/* ── Lex Number ─────────────────────────────────────────────────── */

static HCTokenType hc_lex_number(HCLexer *lex) {
    int start_line = lex->line;
    int start_col = lex->col;
    char buf[HC_MAX_TOKEN_LEN];
    int i = 0;

    /* Check for hex: 0x or 0X */
    if (hc_peek(lex) == '0' &&
        (lex->src[lex->pos + 1] == 'x' || lex->src[lex->pos + 1] == 'X')) {
        hc_advance(lex); /* 0 */
        hc_advance(lex); /* x */
        buf[i++] = '0'; buf[i++] = 'x';
        while (!hc_is_at_end(lex) && isxdigit((unsigned char)hc_peek(lex))) {
            buf[i++] = hc_advance(lex);
        }
        buf[i] = '\0';
        lex->tok.type = HC_TOK_INT;
        lex->tok.int_val = strtoll(buf, NULL, 16);
        lex->tok.line = start_line;
        lex->tok.col = start_col;
        strncpy(lex->tok.text, buf, HC_MAX_TOKEN_LEN - 1);
        return HC_TOK_INT;
    }

    /* Check for binary: 0b or 0B */
    if (hc_peek(lex) == '0' &&
        (lex->src[lex->pos + 1] == 'b' || lex->src[lex->pos + 1] == 'B')) {
        hc_advance(lex); /* 0 */
        hc_advance(lex); /* b */
        int64_t val = 0;
        while (!hc_is_at_end(lex) && (hc_peek(lex) == '0' || hc_peek(lex) == '1')) {
            val = (val << 1) | (hc_advance(lex) - '0');
        }
        lex->tok.type = HC_TOK_INT;
        lex->tok.int_val = val;
        lex->tok.line = start_line;
        lex->tok.col = start_col;
        snprintf(lex->tok.text, HC_MAX_TOKEN_LEN, "0b%lld", (long long)val);
        return HC_TOK_INT;
    }

    /* Decimal or float */
    bool is_float = false;
    while (!hc_is_at_end(lex) && isdigit((unsigned char)hc_peek(lex))) {
        buf[i++] = hc_advance(lex);
    }

    /* Check for decimal point */
    if (!hc_is_at_end(lex) && hc_peek(lex) == '.' &&
        isdigit((unsigned char)lex->src[lex->pos + 1])) {
        is_float = true;
        buf[i++] = hc_advance(lex); /* . */
        while (!hc_is_at_end(lex) && isdigit((unsigned char)hc_peek(lex))) {
            buf[i++] = hc_advance(lex);
        }
    }

    /* Check for exponent: e/E */
    if (!hc_is_at_end(lex) && (hc_peek(lex) == 'e' || hc_peek(lex) == 'E')) {
        is_float = true;
        buf[i++] = hc_advance(lex);
        if (!hc_is_at_end(lex) && (hc_peek(lex) == '+' || hc_peek(lex) == '-'))
            buf[i++] = hc_advance(lex);
        while (!hc_is_at_end(lex) && isdigit((unsigned char)hc_peek(lex)))
            buf[i++] = hc_advance(lex);
    }

    buf[i] = '\0';

    if (is_float) {
        lex->tok.type = HC_TOK_FLOAT;
        lex->tok.float_val = strtod(buf, NULL);
    } else {
        lex->tok.type = HC_TOK_INT;
        lex->tok.int_val = strtoll(buf, NULL, 10);
    }

    lex->tok.line = start_line;
    lex->tok.col = start_col;
    strncpy(lex->tok.text, buf, HC_MAX_TOKEN_LEN - 1);
    return lex->tok.type;
}

/* ── Lex String ─────────────────────────────────────────────────── */

static HCTokenType hc_lex_string(HCLexer *lex) {
    int start_line = lex->line;
    hc_advance(lex); /* Opening " */

    char buf[HC_MAX_STRING_LEN];
    int i = 0;

    while (!hc_is_at_end(lex) && hc_peek(lex) != '"') {
        if (hc_peek(lex) == '\\') {
            hc_advance(lex); /* \ */
            switch (hc_peek(lex)) {
                case 'n':  buf[i++] = '\n'; hc_advance(lex); break;
                case 't':  buf[i++] = '\t'; hc_advance(lex); break;
                case 'r':  buf[i++] = '\r'; hc_advance(lex); break;
                case '\\': buf[i++] = '\\'; hc_advance(lex); break;
                case '"':  buf[i++] = '"';  hc_advance(lex); break;
                case '0':  buf[i++] = '\0'; hc_advance(lex); break;
                default:   buf[i++] = hc_advance(lex); break;
            }
        } else {
            buf[i++] = hc_advance(lex);
        }
        if (i >= HC_MAX_STRING_LEN - 1) break;
    }

    if (!hc_is_at_end(lex))
        hc_advance(lex); /* Closing " */

    buf[i] = '\0';
    lex->tok.type = HC_TOK_STRING;
    strncpy(lex->tok.text, buf, HC_MAX_TOKEN_LEN - 1);
    lex->tok.line = start_line;
    lex->tok.col = lex->col;
    return HC_TOK_STRING;
}

/* ── Lex Character Literal ──────────────────────────────────────── */

static HCTokenType hc_lex_char(HCLexer *lex) {
    hc_advance(lex); /* Opening ' */
    int64_t val;

    if (hc_peek(lex) == '\\') {
        hc_advance(lex);
        switch (hc_peek(lex)) {
            case 'n':  val = '\n'; hc_advance(lex); break;
            case 't':  val = '\t'; hc_advance(lex); break;
            case 'r':  val = '\r'; hc_advance(lex); break;
            case '\\': val = '\\'; hc_advance(lex); break;
            case '\'': val = '\''; hc_advance(lex); break;
            case '0':  val = '\0'; hc_advance(lex); break;
            default:   val = hc_advance(lex); break;
        }
    } else {
        val = hc_advance(lex);
    }

    if (hc_peek(lex) == '\'')
        hc_advance(lex); /* Closing ' */

    lex->tok.type = HC_TOK_CHAR;
    lex->tok.int_val = val;
    lex->tok.line = lex->line;
    lex->tok.col = lex->col;
    snprintf(lex->tok.text, HC_MAX_TOKEN_LEN, "'%c'", (int)val);
    return HC_TOK_CHAR;
}

/* ── Lex Identifier or Keyword ──────────────────────────────────── */

static HCTokenType hc_lex_ident(HCLexer *lex) {
    int start_line = lex->line;
    int start_col = lex->col;
    char buf[HC_MAX_IDENT_LEN];
    int i = 0;

    while (!hc_is_at_end(lex) &&
           (isalnum((unsigned char)hc_peek(lex)) || hc_peek(lex) == '_')) {
        if (i < HC_MAX_IDENT_LEN - 1)
            buf[i++] = hc_advance(lex);
        else
            hc_advance(lex); /* Skip excess */
    }
    buf[i] = '\0';

    /* Check for keyword */
    for (size_t k = 0; k < N_KEYWORDS; k++) {
        if (strcmp(buf, hc_keywords[k].name) == 0) {
            lex->tok.type = hc_keywords[k].type;
            strncpy(lex->tok.text, buf, HC_MAX_TOKEN_LEN - 1);
            lex->tok.line = start_line;
            lex->tok.col = start_col;
            return lex->tok.type;
        }
    }

    /* Not a keyword — it's an identifier */
    lex->tok.type = HC_TOK_IDENT;
    strncpy(lex->tok.text, buf, HC_MAX_TOKEN_LEN - 1);
    lex->tok.line = start_line;
    lex->tok.col = start_col;
    return HC_TOK_IDENT;
}

/* ── Main Lexer: Next Token ─────────────────────────────────────── */

HCTokenType hc_lex_next(HCLexer *lex) {
    hc_skip_whitespace(lex);

    if (hc_is_at_end(lex)) {
        lex->tok.type = HC_TOK_EOF;
        lex->tok.text[0] = '\0';
        return HC_TOK_EOF;
    }

    char c = hc_peek(lex);
    int start_line = lex->line;
    int start_col = lex->col;

    /* Numbers */
    if (isdigit((unsigned char)c)) {
        return hc_lex_number(lex);
    }

    /* Identifiers and keywords */
    if (isalpha((unsigned char)c) || c == '_') {
        return hc_lex_ident(lex);
    }

    /* String literals */
    if (c == '"') {
        return hc_lex_string(lex);
    }

    /* Character literals */
    if (c == '\'') {
        return hc_lex_char(lex);
    }

    /* Operators and delimiters — consume and handle multi-char ops */
    hc_advance(lex);

    switch (c) {
        case '(': lex->tok.type = HC_TOK_LPAREN; break;
        case ')': lex->tok.type = HC_TOK_RPAREN; break;
        case '{': lex->tok.type = HC_TOK_LBRACE; break;
        case '}': lex->tok.type = HC_TOK_RBRACE; break;
        case '[': lex->tok.type = HC_TOK_LBRACKET; break;
        case ']': lex->tok.type = HC_TOK_RBRACKET; break;
        case ',': lex->tok.type = HC_TOK_COMMA; break;
        case ';': lex->tok.type = HC_TOK_SEMI; break;
        case ':': lex->tok.type = HC_TOK_COLON; break;
        case '?': lex->tok.type = HC_TOK_QUESTION; break;
        case '~': lex->tok.type = HC_TOK_TILDE; break;

        case '+':
            if (hc_peek(lex) == '+')      { hc_advance(lex); lex->tok.type = HC_TOK_PLUS_PLUS; }
            else if (hc_peek(lex) == '=') { hc_advance(lex); lex->tok.type = HC_TOK_PLUS_EQ; }
            else                           { lex->tok.type = HC_TOK_PLUS; }
            break;

        case '-':
            if (hc_peek(lex) == '-')      { hc_advance(lex); lex->tok.type = HC_TOK_MINUS_MINUS; }
            else if (hc_peek(lex) == '=') { hc_advance(lex); lex->tok.type = HC_TOK_MINUS_EQ; }
            else if (hc_peek(lex) == '>') { hc_advance(lex); lex->tok.type = HC_TOK_ARROW; }
            else                           { lex->tok.type = HC_TOK_MINUS; }
            break;

        case '*':
            if (hc_peek(lex) == '=')      { hc_advance(lex); lex->tok.type = HC_TOK_STAR_EQ; }
            else                           { lex->tok.type = HC_TOK_STAR; }
            break;

        case '/':
            if (hc_peek(lex) == '=')      { hc_advance(lex); lex->tok.type = HC_TOK_SLASH_EQ; }
            else                           { lex->tok.type = HC_TOK_SLASH; }
            break;

        case '%':
            if (hc_peek(lex) == '=')      { hc_advance(lex); lex->tok.type = HC_TOK_PERCENT_EQ; }
            else                           { lex->tok.type = HC_TOK_PERCENT; }
            break;

        case '&':
            if (hc_peek(lex) == '&')      { hc_advance(lex); lex->tok.type = HC_TOK_AND; }
            else if (hc_peek(lex) == '=') { hc_advance(lex); lex->tok.type = HC_TOK_AMP_EQ; }
            else                           { lex->tok.type = HC_TOK_AMP; }
            break;

        case '|':
            if (hc_peek(lex) == '|')      { hc_advance(lex); lex->tok.type = HC_TOK_OR; }
            else if (hc_peek(lex) == '=') { hc_advance(lex); lex->tok.type = HC_TOK_PIPE_EQ; }
            else                           { lex->tok.type = HC_TOK_PIPE; }
            break;

        case '^':
            if (hc_peek(lex) == '=')      { hc_advance(lex); lex->tok.type = HC_TOK_CARET_EQ; }
            else                           { lex->tok.type = HC_TOK_CARET; }
            break;

        case '=':
            if (hc_peek(lex) == '=')      { hc_advance(lex); lex->tok.type = HC_TOK_EQ; }
            else                           { lex->tok.type = HC_TOK_ASSIGN; }
            break;

        case '!':
            if (hc_peek(lex) == '=')      { hc_advance(lex); lex->tok.type = HC_TOK_NE; }
            else                           { lex->tok.type = HC_TOK_BANG; }
            break;

        case '<':
            if (hc_peek(lex) == '<') {
                hc_advance(lex);
                if (hc_peek(lex) == '=')  { hc_advance(lex); lex->tok.type = HC_TOK_SHL_EQ; }
                else                       { lex->tok.type = HC_TOK_SHL; }
            } else if (hc_peek(lex) == '=') { hc_advance(lex); lex->tok.type = HC_TOK_LE; }
            else                             { lex->tok.type = HC_TOK_LT; }
            break;

        case '>':
            if (hc_peek(lex) == '>') {
                hc_advance(lex);
                if (hc_peek(lex) == '=')  { hc_advance(lex); lex->tok.type = HC_TOK_SHR_EQ; }
                else                       { lex->tok.type = HC_TOK_SHR; }
            } else if (hc_peek(lex) == '=') { hc_advance(lex); lex->tok.type = HC_TOK_GE; }
            else                             { lex->tok.type = HC_TOK_GT; }
            break;

        case '.':
            lex->tok.type = HC_TOK_DOT;
            break;

        case '#':
            /* Preprocessor-like directives */
            if (isalpha((unsigned char)hc_peek(lex))) {
                char dir[16];
                int di = 0;
                while (isalpha((unsigned char)hc_peek(lex)) && di < 15)
                    dir[di++] = hc_advance(lex);
                dir[di] = '\0';
                if (strcmp(dir, "include") == 0) lex->tok.type = HC_TOK_INCLUDE;
                else if (strcmp(dir, "define") == 0) lex->tok.type = HC_TOK_DEFINE;
                else { hc_lex_error(lex, "unknown directive"); lex->tok.type = HC_TOK_EOF; }
            } else {
                hc_lex_error(lex, "expected directive after #");
                lex->tok.type = HC_TOK_EOF;
            }
            break;

        default:
            hc_lex_error(lex, "unexpected character");
            lex->tok.type = HC_TOK_EOF;
            break;
    }

    lex->tok.line = start_line;
    lex->tok.col = start_col;
    snprintf(lex->tok.text, HC_MAX_TOKEN_LEN, "%c", c);

    return lex->tok.type;
}

/* ── Expect Token ───────────────────────────────────────────────── */

int hc_lex_expect(HCLexer *lex, HCTokenType expected) {
    if (lex->tok.type == expected) {
        hc_lex_next(lex);
        return 0;
    }
    hc_lex_error(lex, "unexpected token");
    return -1;
}
