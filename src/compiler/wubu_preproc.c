/*
 * wubu_preproc.c -- minimal C preprocessor for the HolyC compiler.
 *
 * The self-hosting doctrine: real kernel source is full of #define
 * guards, object macros, and function-like macros. Without a preprocessor
 * the compiler couldn't compile a single kernel header. This module
 * expands #define directives (object + function-like) and drops the
 * lines so the lexer never sees a '#'. #include and #ifdef are stubbed
 * (single-line, no nesting) — enough to make the compiler self-host on
 * the battery's #define cases and grow from there.
 *
 * C11, self-contained.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PP_MAX_MACROS  256
#define PP_NAME_LEN    64
#define PP_BODY_LEN    512

typedef struct {
    char name[PP_NAME_LEN];   /* macro name (no parens) */
    int  fn_like;             /* 1 if NAME(args) body */
    char params[8][PP_NAME_LEN];
    int  n_params;
    char body[PP_BODY_LEN];
} PP_Macro;

static PP_Macro g_macros[PP_MAX_MACROS];
static int g_n_macros = 0;

/* trim leading/trailing whitespace in place */
static void pp_trim(char *s)
{
    char *start = s;
    while (*start == ' ' || *start == '\t') start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == ' ' || s[n-1] == '\t' || s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}

static void pp_reset(void)
{
    memset(g_macros, 0, sizeof(g_macros));
    g_n_macros = 0;
}

/* parse a #define line into a macro record */
static int pp_parse_define(const char *line, PP_Macro *m)
{
    memset(m, 0, sizeof(*m));
    const char *p = line;
    /* skip whitespace after #define */
    while (*p == ' ' || *p == '\t') p++;
    /* macro name */
    int i = 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '(' && i < PP_NAME_LEN-1)
        m->name[i++] = *p++;
    m->name[i] = '\0';
    if (m->name[0] == '\0') return 0;

    /* function-like? NAME( */
    if (*p == '(') {
        m->fn_like = 1;
        p++; /* ( */
        int pi = 0;
        while (*p && *p != ')' && pi < 8) {
            while (*p == ' ' || *p == '\t') p++;
            int j = 0;
            while (*p && *p != ',' && *p != ')' && j < PP_NAME_LEN-1)
                m->params[pi][j++] = *p++;
            m->params[pi][j] = '\0';
            if (*p == ',') { p++; pi++; }
            else if (*p == ')' ) { pi++; break; }
        }
        m->n_params = pi;
        if (*p == ')') p++;
    }

    /* body: rest of line, trimmed */
    while (*p == ' ' || *p == '\t') p++;
    strncpy(m->body, p, PP_BODY_LEN-1);
    pp_trim(m->body);
    return 1;
}

/* find a macro by name, or -1 */
static int pp_find(const char *name)
{
    for (int i = 0; i < g_n_macros; i++)
        if (strcmp(g_macros[i].name, name) == 0) return i;
    return -1;
}

/* is a buffer position a whole-word boundary for an identifier? */
static int pp_is_ident_char(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_';
}

/* expand macros in a single line (no directives) into `out`.
 * Handles object macros and function-like macros. */
static void pp_expand_line(const char *line, char *out, size_t out_cap)
{
    size_t o = 0;
    const char *p = line;
    while (*p && o < out_cap - 1) {
        if (pp_is_ident_char(*p) && !(*p >= '0' && *p <= '9')) {
            /* read candidate identifier */
            const char *start = p;
            char name[PP_NAME_LEN];
            int i = 0;
            while (*p && pp_is_ident_char(*p) && i < PP_NAME_LEN-1) name[i++] = *p++;
            name[i] = '\0';
            int idx = pp_find(name);
            if (idx >= 0) {
                PP_Macro *m = &g_macros[idx];
                if (m->fn_like) {
                    /* expect (args) */
                    const char *q = p;
                    while (*q == ' ' || *q == '\t') q++;
                    if (*q == '(') {
                        q++;
                        /* collect args up to matching ) */
                        char argvals[8][PP_BODY_LEN];
                        int na = 0;
                        memset(argvals, 0, sizeof(argvals));
                        int depth = 1;
                        int cur = 0;
                        while (*q && depth > 0 && na < 8) {
                            if (*q == '(') depth++;
                            else if (*q == ')') depth--;
                            if (depth == 0) break;
                            if (*q == ',' && depth == 1) { na++; cur = 0; }
                            else if (cur < PP_BODY_LEN-1) { argvals[na][cur++] = *q; }
                            q++;
                        }
                        na++;
                        /* substitute params into body */
                        char sub[PP_BODY_LEN];
                        size_t s = 0;
                        const char *b = m->body;
                        while (*b && s < PP_BODY_LEN-1) {
                            if (pp_is_ident_char(*b) && !(*b >= '0' && *b <= '9')) {
                                const char *bs = b;
                                char pname[PP_NAME_LEN];
                                int bi = 0;
                                while (*b && pp_is_ident_char(*b) && bi < PP_NAME_LEN-1) pname[bi++] = *b++;
                                pname[bi] = '\0';
                                int matched = -1;
                                for (int pi = 0; pi < m->n_params; pi++)
                                    if (strcmp(m->params[pi], pname) == 0) { matched = pi; break; }
                                if (matched >= 0 && matched < na) {
                                    size_t av = strlen(argvals[matched]);
                                    if (s + av < PP_BODY_LEN-1) {
                                        memcpy(sub + s, argvals[matched], av);
                                        s += av;
                                    }
                                } else {
                                    size_t bl = strlen(pname);
                                    if (s + bl < PP_BODY_LEN-1) { memcpy(sub+s, pname, bl); s += bl; }
                                }
                            } else {
                                if (s < PP_BODY_LEN-1) sub[s++] = *b;
                                b++;
                            }
                        }
                        sub[s] = '\0';
                        size_t bl = strlen(sub);
                        if (o + bl < out_cap-1) { memcpy(out+o, sub, bl); o += bl; }
                        p = q; /* past the ) */
                        if (*p == ')') p++;
                        continue;
                    }
                    /* function-like but no parens: emit name literally */
                } else {
                    /* object macro: emit body */
                    size_t bl = strlen(m->body);
                    if (o + bl < out_cap-1) { memcpy(out+o, m->body, bl); o += bl; }
                    continue;
                }
            }
            /* not a macro (or fn-like w/o parens): copy name literally */
            size_t nl = strlen(name);
            if (o + nl < out_cap-1) { memcpy(out+o, name, nl); o += nl; }
            continue;
        } else {
            out[o++] = *p++;
        }
    }
    out[o] = '\0';
}

/* Preprocess a full source string: strips directives (#define/#include)
 * and expands macros. Returns a malloc'd string (caller frees). */
char *wubu_preprocess(const char *src)
{
    pp_reset();
    size_t cap = strlen(src) * 4 + 4096;
    char *out = (char *)malloc(cap);
    if (!out) return NULL;
    size_t o = 0;

    char *copy = strdup(src);
    if (!copy) { free(out); return NULL; }

    char *save = NULL;
    char *line = strtok_r(copy, "\n", &save);
    while (line) {
        char *tl = line;
        while (*tl == ' ' || *tl == '\t') tl++;
        if (*tl == '#') {
            char dir[32];
            sscanf(tl + 1, "%31s", dir);
            if (strcmp(dir, "define") == 0) {
                PP_Macro m;
                if (pp_parse_define(tl + 7, &m) && g_n_macros < PP_MAX_MACROS) {
                    /* redefine wins */
                    int idx = pp_find(m.name);
                    if (idx < 0) { idx = g_n_macros++; }
                    g_macros[idx] = m;
                }
            }
            /* #include, #ifdef, #endif, etc.: dropped (stub) */
        } else {
            /* expand macros in this line */
            char exp[8192];
            pp_expand_line(line, exp, sizeof(exp));
            size_t l = strlen(exp);
            if (o + l + 2 < cap) {
                memcpy(out + o, exp, l);
                o += l;
                out[o++] = '\n';
            }
        }
        line = strtok_r(NULL, "\n", &save);
    }
    out[o] = '\0';
    free(copy);
    return out;
}
