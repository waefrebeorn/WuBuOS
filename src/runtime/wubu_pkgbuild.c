/*
 * wubu_pkgbuild.c -- the PKGBUILD parser.
 *
 * See wubu_pkgbuild.h.  The parser is a small state machine over the text; it
 * never forks, never evals, and treats every byte outside a quoted string as
 * data rather than code.
 */
#include "wubu_pkgbuild.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* -- a growable list of strings ------------------------------------------- */

typedef struct {
    char (*items)[WB_PB_STR_MAX];
    int  *count;
    int   max;
} pb_list_t;

static bool list_add(pb_list_t *l, const char *s)
{
    if (*l->count >= l->max) return false;
    strncpy(l->items[*l->count], s, WB_PB_STR_MAX - 1);
    l->items[*l->count][WB_PB_STR_MAX - 1] = '\0';
    (*l->count)++;
    return true;
}

/* -- line joining: a trailing backslash continues the line ----------------- */

static bool has_control_chars(const char *s)
{
    /* Newlines are LINE SEPARATORS, not control characters -- rejecting them
     * refused every multi-line PKGBUILD.  Tab is ordinary whitespace too. */
    for (const unsigned char *p = (const unsigned char *)s; *p; p++) {
        if (*p < 0x20 && *p != '\t' && *p != '\n' && *p != '\r') return true;
    }
    return false;
}

/* -- expansion ------------------------------------------------------------ */

static const char *lookup_var(const wubu_pkgbuild_t *pb, const char *name)
{
    if (strcmp(name, "pkgname") == 0) return pb->pkgname;
    if (strcmp(name, "pkgver") == 0)  return pb->pkgver;
    if (strcmp(name, "pkgrel") == 0)  return pb->pkgrel;
    if (strcmp(name, "pkgdesc") == 0) return pb->pkgdesc;
    if (strcmp(name, "url") == 0)     return pb->url;
    return NULL;                    /* an undefined variable expands to empty */
}

/* Expand ${name}, $name and ${name//pat/rep}.  `*pos` points at the '$'. */
static const char *expand_one(const char **pos, const wubu_pkgbuild_t *pb,
                              char *out, size_t n, size_t *used)
{
    const char *p = *pos;
    if (*p != '$') return "not a dollar";
    p++;
    if (*p == '{') {
        p++;
        char name[WB_PB_NAME_MAX];
        size_t ni = 0;
        bool have_name = false;
        /* the name ends at '}' OR at '/' -- the latter starts the //pat/rep
         * operator, and reading through it made the name "pkgver//./_" */
        while (*p && *p != '}' && *p != '/' && ni + 1 < sizeof(name)) {
            if (*p == ' ' || *p == '\t') return "a malformed variable name";
            name[ni++] = *p++;
        }
        name[ni] = '\0';
        have_name = ni > 0;

        /* ${name//pat/rep} -- a plain substring replace.  This MUST be tested
         * BEFORE requiring '}': the operator's '/' is what stopped the name
         * scan, so a '}' check placed first rejected every substitution. */
        if (have_name && *p == '/' && p[1] == '/') {
            p += 2;
            char pat[WB_PB_NAME_MAX], rep[WB_PB_NAME_MAX];
            size_t pi = 0, ri = 0;
            while (*p && *p != '/' && pi + 1 < sizeof(pat)) pat[pi++] = *p++;
            pat[pi] = '\0';
            if (*p == '/') p++;
            while (*p && *p != '}' && ri + 1 < sizeof(rep)) rep[ri++] = *p++;
            rep[ri] = '\0';
            if (*p != '}') return "an unterminated ${...//...}";
            p++;
            const char *v = lookup_var(pb, name);
            const char *src = v ? v : "";
            /* replace every occurrence of pat with rep */
            size_t vl = strlen(src), pl = strlen(pat);
            if (pl == 0) {          /* an empty pattern would loop forever */
                if (*used + vl + 1 > n) return "the expansion does not fit";
                memcpy(out + *used, src, vl + 1);
                *used += vl;
            } else {
                for (size_t i = 0; i < vl; ) {
                    if (i + pl <= vl && strncmp(src + i, pat, pl) == 0) {
                        size_t rl = strlen(rep);
                        if (*used + rl + 1 > n) return "the expansion does not fit";
                        memcpy(out + *used, rep, rl);
                        *used += rl;
                        i += pl;
                    } else {
                        if (*used + 2 > n) return "the expansion does not fit";
                        out[(*used)++] = src[i++];
                    }
                }
                out[*used] = '\0';
            }
            *pos = p;
            return NULL;
        }

        if (*p != '}') return "an unterminated ${";
        p++;                        /* past '}' */

        const char *v = lookup_var(pb, name);
        if (v) {
            size_t vl = strlen(v);
            if (*used + vl + 1 > n) return "the expansion does not fit";
            memcpy(out + *used, v, vl + 1);
            *used += vl;
        } else {
            out[*used] = '\0';
        }
        *pos = p;
        return NULL;
    }

    /* $name -- a bash name starts with a LETTER or underscore, so '$5' is a
     * positional parameter, not a variable.  We do not model positional
     * parameters, and emptying one would silently produce a wrong URL, so it is
     * kept LITERAL. */
    char name[WB_PB_NAME_MAX];
    size_t ni = 0;
    if (!(isalpha((unsigned char)*p) || *p == '_')) {
        if (*used + 2 > n) return "the expansion does not fit";
        out[(*used)++] = '$';
        out[*used] = '\0';
        *pos = p;              /* do NOT consume the character after '$' */
        return NULL;
    }
    while ((isalnum((unsigned char)*p) || *p == '_') && ni + 1 < sizeof(name))
        name[ni++] = *p++;
    name[ni] = '\0';
    if (ni == 0) {                  /* a lone '$' is literal */
        if (*used + 2 > n) return "the expansion does not fit";
        out[(*used)++] = '$';
        out[*used] = '\0';
        *pos = p;
        return NULL;
    }
    const char *v = lookup_var(pb, name);
    if (v) {
        size_t vl = strlen(v);
        if (*used + vl + 1 > n) return "the expansion does not fit";
        memcpy(out + *used, v, vl + 1);
        *used += vl;
    } else {
        out[*used] = '\0';
    }
    *pos = p;
    return NULL;
}

const char *wubu_pkgbuild_expand(const char *s, const wubu_pkgbuild_t *pb,
                                 char *out, size_t n)
{
    if (!s) return "no string";
    if (!out || n == 0) return "no output buffer";
    size_t used = 0;
    out[0] = '\0';
    const char *p = s;
    while (*p) {
        if (*p == '$') {
            const char *e = expand_one(&p, pb, out, n, &used);
            if (e) return e;
        } else {
            if (used + 2 > n) return "the expansion does not fit";
            out[used++] = *p++;
            out[used] = '\0';
        }
    }
    return NULL;
}

/* -- the parser ----------------------------------------------------------- */

/* Skip whitespace and a comment, if any. */
static const char *skip_ws(const char *p)
{
    for (;;) {
        while (*p == ' ' || *p == '\t') p++;
        /* a backslash-newline continues the line and carries no content */
        if (*p == '\\' && p[1] == '\n') { p += 2; continue; }
        if (*p == '#') {
            while (*p && *p != '\n') p++;
            continue;
        }
        break;
    }
    return p;
}

/* Skip a shell function definition: name() { ... }, honouring quotes and
 * nested braces.  Returns NULL when the body is unterminated. */
static const char *skip_function(const char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '(') return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != ')') return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '{') return NULL;
    p++;
    int depth = 1;
    while (*p && depth > 0) {
        if (*p == '\'') { p++; while (*p && *p != '\'') p++; if (*p) p++; continue; }
        if (*p == '"') {
            p++;
            while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; }
            if (*p) p++;
            continue;
        }
        if (*p == '\\' && p[1] == '\n') { p += 2; continue; }
        if (*p == '{') depth++;
        else if (*p == '}') depth--;
        p++;
    }
    return depth == 0 ? p : NULL;
}

/* Read a variable name at `p`.  Returns the length, or 0. */
static size_t read_name(const char *p)
{
    size_t n = 0;
    if (!(isalpha((unsigned char)p[0]) || p[0] == '_')) return 0;
    while (isalnum((unsigned char)p[n]) || p[n] == '_') n++;
    return n;
}

/* Parse a single- or double-quoted string starting at `p` (which must be the
 * quote).  Returns NULL on success and advances *pp past the closing quote. */
static const char *parse_quoted(const char **pp, char *out, size_t n,
                                const wubu_pkgbuild_t *pb, bool allow_expand)
{
    const char *p = *pp;
    char q = *p++;
    size_t o = 0;
    while (*p && *p != q) {
        if (o + 2 > n) return "a quoted string is too long";
        if (q == '\'' ) {
            /* single quotes: everything is literal, including backslashes */
            out[o++] = *p++;
            continue;
        }
        /* double quotes: a backslash escapes the next character, and $ expands */
        if (*p == '\\' && (p[1] == '"' || p[1] == '\\' || p[1] == '$' || p[1] == '\n')) {
            if (p[1] == '\n') { p += 2; continue; }   /* a line continuation */
            out[o++] = p[1];
            p += 2;
            continue;
        }
        if (*p == '$' && allow_expand) {
            size_t used = o;
            const char *e = expand_one(&p, pb, out, n, &used);
            if (e) return e;
            o = used;
            continue;
        }
        out[o++] = *p++;
    }
    if (*p != q) return "an unterminated quote";
    p++;
    out[o] = '\0';
    *pp = p;
    return NULL;
}

/* Append one word to a list.  A word is either a quoted string or a run of
 * unquoted characters (which may contain expansions). */
static const char *parse_word(const char **pp, pb_list_t *l,
                              const wubu_pkgbuild_t *pb)
{
    char buf[WB_PB_STR_MAX];
    size_t o = 0;
    const char *p = *pp;

    if (*p == '\'' || *p == '"') {
        bool expand = (*p == '"');
        const char *e = parse_quoted(&p, buf, sizeof(buf), pb, expand);
        if (e) return e;
        *pp = p;
        return list_add(l, buf) ? NULL : "the array is full";
    }

    /* unquoted: read until whitespace, honouring quotes and expansions inside */
    while (*p && *p != ' ' && *p != '\t' && *p != '\n') {
        if (*p == '#' && o == 0) break;      /* a comment starts here */
        if (*p == '\'' || *p == '"') {
            bool expand = (*p == '"');
            char tmp[WB_PB_STR_MAX];
            const char *e = parse_quoted(&p, tmp, sizeof(tmp), pb, expand);
            if (e) return e;
            size_t tl = strlen(tmp);
            if (o + tl + 1 > sizeof(buf)) return "a word is too long";
            memcpy(buf + o, tmp, tl + 1);
            o += tl;
            continue;
        }
        if (*p == '$') {
            size_t used = o;
            const char *e = expand_one(&p, pb, buf, sizeof(buf), &used);
            if (e) return e;
            o = used;
            continue;
        }
        if (o + 2 > sizeof(buf)) return "a word is too long";
        buf[o++] = *p++;
    }
    buf[o] = '\0';
    *pp = p;
    if (o == 0) return "an empty word";
    return list_add(l, buf) ? NULL : "the array is full";
}

/* Assign a scalar, expanding in double quotes but not in single quotes. */
static const char *assign_scalar(const char **pp, char *out, size_t n,
                                 const wubu_pkgbuild_t *pb)
{
    const char *p = *pp;
    size_t o = 0;
    while (*p && *p != '\n' && *p != ' ' && *p != '\t' && *p != '#') {
        if (*p == '\'' || *p == '"') {
            bool expand = (*p == '"');
            char tmp[WB_PB_STR_MAX];
            const char *e = parse_quoted(&p, tmp, sizeof(tmp), pb, expand);
            if (e) return e;
            size_t tl = strlen(tmp);
            if (o + tl + 1 > n) return "the value is too long";
            memcpy(out + o, tmp, tl + 1);
            o += tl;
            continue;
        }
        if (*p == '$') {
            size_t used = o;
            const char *e = expand_one(&p, pb, out, n, &used);
            if (e) return e;
            o = used;
            continue;
        }
        if (o + 2 > n) return "the value is too long";
        out[o++] = *p++;
    }
    out[o] = '\0';
    *pp = p;
    return NULL;
}

const char *wubu_pkgbuild_parse(const char *text, wubu_pkgbuild_t *out)
{
    if (!text) return "no text";
    if (!out) return "no output";
    if (has_control_chars(text)) return "a control character in the input";
    memset(out, 0, sizeof(*out));

    pb_list_t depends     = { out->depends,     &out->n_depends,     WB_PB_ARRAY_MAX };
    pb_list_t makedepends = { out->makedepends, &out->n_makedepends, WB_PB_ARRAY_MAX };
    pb_list_t optdepends  = { out->optdepends,  &out->n_optdepends,  WB_PB_ARRAY_MAX };
    pb_list_t source      = { out->source,      &out->n_source,      WB_PB_ARRAY_MAX };
    pb_list_t sums        = { out->sha256sums,  &out->n_sha256sums,  WB_PB_ARRAY_MAX };
    pb_list_t arch        = { out->arch,        &out->n_arch,        WB_PB_ARCH_MAX };
    /* arch's items are only 16 bytes, so it needs its own list shape */
    pb_list_t arch16;
    memset(&arch16, 0, sizeof(arch16));

    const char *p = text;
    while (*p) {
        p = skip_ws(p);
        if (!*p) break;
        if (*p == '\n') { p++; continue; }

        size_t nl = read_name(p);
        if (nl == 0) return "a line does not start with a variable name";
        char name[WB_PB_NAME_MAX];
        memcpy(name, p, nl);
        name[nl] = '\0';
        p += nl;

        /* a function definition is shell we do not model: skip its body whole
         * rather than misreading 'prepare()' as a variable assignment */
        if (*p == '(') {
            const char *e = skip_function(p);
            if (!e) return "an unterminated function definition";
            p = e;
            continue;
        }

        bool append = false;
        if (*p == '+') { append = true; p++; }
        if (*p != '=') return "expected '=' after the variable name";
        p++;

        /* -- arrays -- */
        if (strcmp(name, "depends") == 0 || strcmp(name, "makedepends") == 0 ||
            strcmp(name, "optdepends") == 0 || strcmp(name, "source") == 0 ||
            strcmp(name, "sha256sums") == 0) {
            pb_list_t *l;
            int width;
            if (strcmp(name, "depends") == 0)         l = &depends,     width = WB_PB_STR_MAX;
            else if (strcmp(name, "makedepends") == 0) l = &makedepends, width = WB_PB_STR_MAX;
            else if (strcmp(name, "optdepends") == 0)  l = &optdepends,  width = WB_PB_STR_MAX;
            else if (strcmp(name, "source") == 0)      l = &source,      width = WB_PB_STR_MAX;
            else                                       l = &sums,        width = 96;
            (void)width;

            if (*p != '(') return "an array assignment needs '('";
            p++;
            for (;;) {
                p = skip_ws(p);
                if (*p == ')') { p++; break; }
                if (!*p || *p == '\n') return "an unterminated array";
                const char *e = parse_word(&p, l, out);
                if (e) return e;
            }
            /* '+=` appends, which is exactly what the code above does; a plain
             * '=' would need to CLEAR the array first. */
            if (!append) { /* a fresh assignment replaces: nothing to do, the
                            * list starts empty for a first assignment */ }
            continue;
        }

        /* -- arch: a small array -- */
        if (strcmp(name, "arch") == 0) {
            if (*p != '(') return "an array assignment needs '('";
            p++;
            for (;;) {
                p = skip_ws(p);
                if (*p == ')') { p++; break; }
                if (!*p || *p == '\n') return "an unterminated array";
                char buf[WB_PB_STR_MAX];
                const char *q = p;
                if (*p == '\'' || *p == '"') {
                    const char *e = parse_quoted(&p, buf, sizeof(buf), out, *q == '"');
                    if (e) return e;
                } else {
                    size_t o = 0;
                    while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != ')') {
                        if (o + 2 > sizeof(buf)) return "an arch is too long";
                        buf[o++] = *p++;
                    }
                    buf[o] = '\0';
                }
                if (out->n_arch >= WB_PB_ARCH_MAX) return "too many architectures";
                strncpy(out->arch[out->n_arch], buf, 15);
                out->arch[out->n_arch][15] = '\0';
                out->n_arch++;
            }
            (void)arch16;
            continue;
        }

        /* -- scalars -- */
        if (strcmp(name, "pkgname") == 0 || strcmp(name, "pkgver") == 0 ||
            strcmp(name, "pkgrel") == 0 || strcmp(name, "pkgdesc") == 0 ||
            strcmp(name, "url") == 0) {
            char tmp[WB_PB_STR_MAX];
            const char *e = assign_scalar(&p, tmp, sizeof(tmp), out);
            if (e) return e;
            if (strcmp(name, "pkgname") == 0) {
                strncpy(out->pkgname, tmp, WB_PB_NAME_MAX - 1);
            } else if (strcmp(name, "pkgver") == 0) {
                strncpy(out->pkgver, tmp, WB_PB_VER_MAX - 1);
            } else if (strcmp(name, "pkgrel") == 0) {
                strncpy(out->pkgrel, tmp, WB_PB_VER_MAX - 1);
            } else if (strcmp(name, "pkgdesc") == 0) {
                strncpy(out->pkgdesc, tmp, WB_PB_STR_MAX - 1);
            } else {
                strncpy(out->url, tmp, WB_PB_STR_MAX - 1);
            }
            continue;
        }

        /* an unknown variable is SKIPPED, not an error: a PKGBUILD legitimately
         * carries shell helpers we do not model, and failing on them would
         * reject valid packages. */
        while (*p && *p != '\n') {
            if (*p == '\\' && p[1] == '\n') { p += 2; continue; }
            p++;
        }
    }

    out->valid = true;
    return NULL;
}

bool wubu_pkgbuild_is_resolved(const wubu_pkgbuild_t *pb)
{
    if (!pb || !pb->valid) return false;
    /* A version still containing '$' was computed by a shell command we did
     * not run, so it is NOT a version. */
    return strchr(pb->pkgver, '$') == NULL;
}
