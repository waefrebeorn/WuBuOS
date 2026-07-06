/*
 * holyc_lexer.c  --  HolyC Lexer
 * Tokenizes HolyC source text into a stream of tokens.
 * Self-contained, C11, minimal includes.
 */

#include "holyc_types.h"
#include <stdarg.h>

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/* Forward declarations */
HCTokenType hc_lex_next(HCLexer *lex);
static void hc_skip_whitespace(HCLexer *lex);

/* -- Keyword Table ------------------------------------------------ */

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
    {"Bool",     HC_KW_BOOL},

    /* C-compatible type keywords (aliases) */
    {"void",     HC_KW_U0},
    {"char",     HC_KW_I8},
    {"short",    HC_KW_I16},
    {"int",      HC_KW_I32},
    {"long",     HC_KW_I64},
    {"float",    HC_KW_F64},
    {"double",   HC_KW_F64},
    {"bool",     HC_KW_BOOL},

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

/* -- Lexer Helpers ------------------------------------------------ */

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

static void hc_lex_error(HCLexer *lex, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(lex->error, sizeof(lex->error), fmt, ap);
    va_end(ap);
    lex->has_error = true;
}

static HCTokenType hc_make_token(HCLexer *lex, HCTokenType type) {
    lex->tok.type = type;
    return type;
}

static HCTokenType hc_make_ident_or_keyword(HCLexer *lex, const char *text) {
    for (size_t i = 0; i < N_KEYWORDS; i++) {
        if (strcmp(text, hc_keywords[i].name) == 0) {
            return hc_make_token(lex, hc_keywords[i].type);
        }
    }
    strncpy(lex->tok.text, text, HC_MAX_TOKEN_LEN - 1);
    lex->tok.text[HC_MAX_TOKEN_LEN - 1] = '\0';
    return hc_make_token(lex, HC_TOK_IDENT);
}

/* -- Public API --------------------------------------------------- */

void hc_lex_init(HCLexer *lex, const char *source) {
    memset(lex, 0, sizeof(*lex));
    lex->src = source;
    lex->pos = 0;
    lex->line = 1;
    lex->col = 1;
    lex->has_error = false;
    lex->tok.type = HC_TOK_EOF;
    lex->tok.text[0] = '\0';
    hc_lex_next(lex);  /* Prime the first token */
}

static void hc_skip_whitespace(HCLexer *lex) {
    while (!hc_is_at_end(lex)) {
        char c = hc_peek(lex);
        if (c == ' ' || c == '\t' || c == '\r') {
            hc_advance(lex);
        } else if (c == '\n') {
            hc_advance(lex);
        } else {
            break;
        }
    }
}

static HCTokenType hc_scan_number(HCLexer *lex) {
    char buf[HC_MAX_TOKEN_LEN];
    int i = 0;
    bool is_hex = false;
    bool is_bin = false;

    if (hc_peek(lex) == '0') {
        hc_advance(lex);
        char c = hc_peek(lex);
        if (c == 'x' || c == 'X') {
            is_hex = true;
            hc_advance(lex);
        } else if (c == 'b' || c == 'B') {
            is_bin = true;
            hc_advance(lex);
        }
    }

    while (!hc_is_at_end(lex) && i < HC_MAX_TOKEN_LEN - 1) {
        char c = hc_peek(lex);
        if (is_hex && isxdigit((unsigned char)c)) {
            buf[i++] = hc_advance(lex);
        } else if (is_bin && (c == '0' || c == '1')) {
            buf[i++] = hc_advance(lex);
        } else if (!is_hex && !is_bin && isdigit((unsigned char)c)) {
            buf[i++] = hc_advance(lex);
        } else {
            break;
        }
    }
    buf[i] = '\0';

    if (is_hex) {
        lex->tok.int_val = strtoll(buf, NULL, 16);
    } else if (is_bin) {
        lex->tok.int_val = strtoll(buf, NULL, 2);
    } else {
        lex->tok.int_val = strtoll(buf, NULL, 10);
    }
    return hc_make_token(lex, HC_TOK_INT);
}

static HCTokenType hc_scan_string(HCLexer *lex) {
    char quote = hc_advance(lex); /* consume opening quote */
    int i = 0;
    while (!hc_is_at_end(lex) && i < HC_MAX_STRING_LEN - 1) {
        char c = hc_peek(lex);
        if (c == quote) {
            hc_advance(lex);
            break;
        }
        if (c == '\\') {
            hc_advance(lex);
            char esc = hc_peek(lex);
            switch (esc) {
                case 'n': lex->tok.str_val[i++] = '\n'; break;
                case 't': lex->tok.str_val[i++] = '\t'; break;
                case 'r': lex->tok.str_val[i++] = '\r'; break;
                case '\\': lex->tok.str_val[i++] = '\\'; break;
                case '"': lex->tok.str_val[i++] = '"'; break;
                case '\'': lex->tok.str_val[i++] = '\''; break;
                case '0': lex->tok.str_val[i++] = '\0'; break;
                default: lex->tok.str_val[i++] = esc; break;
            }
            hc_advance(lex);
        } else {
            lex->tok.str_val[i++] = hc_advance(lex);
        }
    }
    lex->tok.str_val[i] = '\0';
    return hc_make_token(lex, quote == '"' ? HC_TOK_STRING : HC_TOK_CHAR);
}

static HCTokenType hc_scan_identifier(HCLexer *lex) {
    char buf[HC_MAX_TOKEN_LEN];
    int i = 0;
    while (!hc_is_at_end(lex) && i < HC_MAX_TOKEN_LEN - 1) {
        char c = hc_peek(lex);
        if (isalnum((unsigned char)c) || c == '_') {
            buf[i++] = hc_advance(lex);
        } else {
            break;
        }
    }
    buf[i] = '\0';
    return hc_make_ident_or_keyword(lex, buf);
}

HCTokenType hc_lex_next(HCLexer *lex) {
    if (lex->has_error) return HC_TOK_EOF;

    hc_skip_whitespace(lex);

    if (hc_is_at_end(lex)) {
        return hc_make_token(lex, HC_TOK_EOF);
    }

    lex->tok.line = lex->line;
    lex->tok.col = lex->col;
    lex->tok.text[0] = '\0';

    char c = hc_advance(lex);

    switch (c) {
        /* Single-char tokens */
        case '(': return hc_make_token(lex, HC_TOK_LPAREN);
        case ')': return hc_make_token(lex, HC_TOK_RPAREN);
        case '{': return hc_make_token(lex, HC_TOK_LBRACE);
        case '}': return hc_make_token(lex, HC_TOK_RBRACE);
        case '[': return hc_make_token(lex, HC_TOK_LBRACKET);
        case ']': return hc_make_token(lex, HC_TOK_RBRACKET);
        case ',': return hc_make_token(lex, HC_TOK_COMMA);
        case ';': return hc_make_token(lex, HC_TOK_SEMI);
        case ':': return hc_make_token(lex, HC_TOK_COLON);
        case '?': return hc_make_token(lex, HC_TOK_QUESTION);
        case '.': return hc_make_token(lex, HC_TOK_DOT);
        case '~': return hc_make_token(lex, HC_TOK_TILDE);

        /* Multi-char operators */
        case '+':
            if (hc_peek(lex) == '+') { hc_advance(lex); return hc_make_token(lex, HC_TOK_INC); }
            if (hc_peek(lex) == '=') { hc_advance(lex); return hc_make_token(lex, HC_TOK_PLUS_ASSIGN); }
            return hc_make_token(lex, HC_TOK_PLUS);
        case '-':
            if (hc_peek(lex) == '-') { hc_advance(lex); return hc_make_token(lex, HC_TOK_DEC); }
            if (hc_peek(lex) == '=') { hc_advance(lex); return hc_make_token(lex, HC_TOK_MINUS_ASSIGN); }
            if (hc_peek(lex) == '>') { hc_advance(lex); return hc_make_token(lex, HC_TOK_ARROW); }
            return hc_make_token(lex, HC_TOK_MINUS);
        case '*':
            if (hc_peek(lex) == '=') { hc_advance(lex); return hc_make_token(lex, HC_TOK_STAR_ASSIGN); }
            return hc_make_token(lex, HC_TOK_STAR);
        case '/':
            if (hc_peek(lex) == '/') {
                /* Skip line comment */
                while (!hc_is_at_end(lex) && hc_peek(lex) != '\n') hc_advance(lex);
                return hc_lex_next(lex);
            }
            if (hc_peek(lex) == '*') {
                /* Skip block comment */
                hc_advance(lex);
                int depth = 1;
                while (!hc_is_at_end(lex) && depth > 0) {
                    if (hc_advance(lex) == '*' && hc_peek(lex) == '/') {
                        hc_advance(lex);
                        depth--;
                    } else if (hc_peek(lex) == '*' && hc_peek(lex) == '/') {
                        /* nested */
                    }
                }
                return hc_lex_next(lex);
            }
            if (hc_peek(lex) == '=') { hc_advance(lex); return hc_make_token(lex, HC_TOK_SLASH_ASSIGN); }
            return hc_make_token(lex, HC_TOK_SLASH);
        case '%':
            if (hc_peek(lex) == '=') { hc_advance(lex); return hc_make_token(lex, HC_TOK_PERCENT_ASSIGN); }
            return hc_make_token(lex, HC_TOK_PERCENT);
        case '&':
            if (hc_peek(lex) == '&') { hc_advance(lex); return hc_make_token(lex, HC_TOK_AND); }
            if (hc_peek(lex) == '=') { hc_advance(lex); return hc_make_token(lex, HC_TOK_AMP_ASSIGN); }
            return hc_make_token(lex, HC_TOK_AMP);
        case '|':
            if (hc_peek(lex) == '|') { hc_advance(lex); return hc_make_token(lex, HC_TOK_OR); }
            if (hc_peek(lex) == '=') { hc_advance(lex); return hc_make_token(lex, HC_TOK_PIPE_ASSIGN); }
            return hc_make_token(lex, HC_TOK_PIPE);
        case '^':
            if (hc_peek(lex) == '=') { hc_advance(lex); return hc_make_token(lex, HC_TOK_CARET_ASSIGN); }
            return hc_make_token(lex, HC_TOK_CARET);
        case '!':
            if (hc_peek(lex) == '=') { hc_advance(lex); return hc_make_token(lex, HC_TOK_NE); }
            return hc_make_token(lex, HC_TOK_BANG);
        case '=':
            if (hc_peek(lex) == '=') { hc_advance(lex); return hc_make_token(lex, HC_TOK_EQ); }
            return hc_make_token(lex, HC_TOK_ASSIGN);
        case '<':
            if (hc_peek(lex) == '<') { hc_advance(lex); if (hc_peek(lex) == '=') { hc_advance(lex); return hc_make_token(lex, HC_TOK_SHL_ASSIGN); } return hc_make_token(lex, HC_TOK_SHL); }
            if (hc_peek(lex) == '=') { hc_advance(lex); return hc_make_token(lex, HC_TOK_LE); }
            return hc_make_token(lex, HC_TOK_LT);
        case '>':
            if (hc_peek(lex) == '>') { hc_advance(lex); if (hc_peek(lex) == '=') { hc_advance(lex); return hc_make_token(lex, HC_TOK_SHR_ASSIGN); } return hc_make_token(lex, HC_TOK_SHR); }
            if (hc_peek(lex) == '=') { hc_advance(lex); return hc_make_token(lex, HC_TOK_GE); }
            return hc_make_token(lex, HC_TOK_GT);

        /* Literals */
        case '"':
        case '\'':
            return hc_scan_string(lex);

        default:
            if (isdigit((unsigned char)c)) {
                lex->pos--; /* step back */
                lex->col--;
                return hc_scan_number(lex);
            }
            if (isalpha((unsigned char)c) || c == '_') {
                lex->pos--; /* step back */
                lex->col--;
                return hc_scan_identifier(lex);
            }
            hc_lex_error(lex, "Unexpected character: '%c'", c);
            return HC_TOK_EOF;
    }
}

HCTokenType hc_lex_peek(HCLexer *lex) {
    return lex->tok.type;
}

int hc_lex_expect(HCLexer *lex, HCTokenType expected) {
    if (lex->tok.type != expected) {
        hc_lex_error(lex, "Expected token %d, got %d", expected, lex->tok.type);
        return -1;
    }
    return 0;
}