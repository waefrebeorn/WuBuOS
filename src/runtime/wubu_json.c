/* wubu_json.c -- JSON reader for manifests, settings and API replies.
 *
 * Own implementation (docs/GOAL-SLERM.json, SLERM-2010). The earlier version was
 * lenient in the specific ways that make two readers disagree about the same
 * document: numbers went through atoi (so 1.5 read as 1 and 1e3 as 1), string
 * escapes were copied through undecoded, `{"a" 1}` and `{"a":1 "b":2}` were accepted
 * because the ':' and ',' matches were ignored, duplicate keys were kept, and
 * recursion had no depth limit. Strictness is the feature here: a manifest is a
 * contract, and a parser that guesses is a contract that cannot be checked.
 *
 * Still one owner: same header, same token type, same arena. No allocation is added
 * beyond what callers already had.
 */
#include "wubu_json.h"
#include "wubu_utf8.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

JsonToken g_json_tokens[JSON_MAX_TOKENS];
int g_json_token_count = 0;
int g_json_parse_error = 0;

#define JSON_MAX_DEPTH 32

static const char *g_p;          /* cursor for the strict path */
static const char *g_start;      /* for error offsets in debug output */

static void fail_at(const char *why) {
    if (getenv("WUBU_JSON_DEBUG")) {
        long off = (long)(g_p - g_start);
        fprintf(stderr, "[json] %s at offset %ld: %.24s\n", why, off, g_p);
    }
    g_json_parse_error = 1;
}
static int g_depth;

static void skip_ws(void) {
    while (*g_p == ' ' || *g_p == '\t' || *g_p == '\n' || *g_p == '\r') g_p++;
}

static JsonToken *alloc_token(JsonType type) {
    if (g_json_token_count >= JSON_MAX_TOKENS) { g_json_parse_error = 1; return NULL; }
    JsonToken *t = &g_json_tokens[g_json_token_count++];
    memset(t, 0, sizeof *t);
    t->type = type;
    return t;
}

/* Append a child. The token arena is fixed, so children live in one malloc'd block
 * that grows by doubling; a child is copied in, and its own pointer fields are
 * transferred, never shared. */
static int push_child(JsonToken *parent, JsonToken *child) {
    if (!parent || !child) return -1;
    if (parent->child_count >= parent->child_capacity) {
        int cap = parent->child_capacity ? parent->child_capacity * 2 : 4;
        JsonToken *grown = realloc(parent->children, (size_t)cap * sizeof *grown);
        if (!grown) { g_json_parse_error = 1; return -1; }
        parent->children = grown;
        parent->child_capacity = cap;
    }
    parent->children[parent->child_count++] = *child;
    return 0;
}

static int hexval(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

/* Decode a string body (cursor on the opening quote) into a fresh buffer. */
static char *parse_string_raw(void) {
    if (*g_p != '"') return NULL;
    g_p++;
    size_t cap = 32, len = 0;
    char *out = malloc(cap);
    if (!out) return NULL;
    for (;;) {
        unsigned char c = (unsigned char)*g_p;
        if (c == '\0') { free(out); fail_at("unterminated string"); return NULL; }
        if (c == '"') { g_p++; break; }
        if (c < 0x20) { free(out); fail_at("raw control byte in string"); return NULL; }
        char enc[4];
        size_t enc_len = 1;
        if (c == '\\') {
            g_p++;
            switch (*g_p) {
                case '"':  enc[0] = '"';  break;
                case '\\': enc[0] = '\\'; break;
                case '/':  enc[0] = '/';  break;
                case 'b':  enc[0] = '\b'; break;
                case 'f':  enc[0] = '\f'; break;
                case 'n':  enc[0] = '\n'; break;
                case 'r':  enc[0] = '\r'; break;
                case 't':  enc[0] = '\t'; break;
                case 'u': {
                    unsigned cp = 0;
                    for (int i = 0; i < 4; i++) {
                        int h = hexval((unsigned char)g_p[1 + i]);
                        if (h < 0) { free(out); g_json_parse_error = 1; return NULL; }
                        cp = (cp << 4) | (unsigned)h;
                    }
                    g_p += 4;
                    if (cp >= 0xD800u && cp <= 0xDBFFu) {          /* high surrogate */
                        if (g_p[1] != '\\' || g_p[2] != 'u') { free(out); g_json_parse_error = 1; return NULL; }
                        unsigned lo = 0;
                        for (int i = 0; i < 4; i++) {
                            int h = hexval((unsigned char)g_p[3 + i]);
                            if (h < 0) { free(out); g_json_parse_error = 1; return NULL; }
                            lo = (lo << 4) | (unsigned)h;
                        }
                        if (lo < 0xDC00u || lo > 0xDFFFu) { free(out); g_json_parse_error = 1; return NULL; }
                        cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                        g_p += 6;
                    } else if (cp >= 0xDC00u && cp <= 0xDFFFu) {    /* lone low surrogate */
                        free(out); g_json_parse_error = 1; return NULL;
                    }
                    enc_len = wubu_utf8_encode(cp, enc);
                    if (enc_len == 0) { free(out); g_json_parse_error = 1; return NULL; }
                    break;
                }
                default: free(out); fail_at("unknown escape"); return NULL;
            }
            g_p++;
        } else {
            enc[0] = (char)c;
            g_p++;
        }
        if (len + enc_len + 1 > cap) {
            cap = (len + enc_len + 1) * 2;
            char *grown = realloc(out, cap);
            if (!grown) { free(out); g_json_parse_error = 1; return NULL; }
            out = grown;
        }
        memcpy(out + len, enc, enc_len);
        len += enc_len;
    }
    out[len] = '\0';
    return out;
}

static JsonToken *parse_number(void) {
    const char *start = g_p;
    if (*g_p == '-') g_p++;
    if (!isdigit((unsigned char)*g_p)) { g_json_parse_error = 1; return NULL; }
    /* JSON forbids leading zeros ("01"); a reader that accepts them disagrees with
     * every strict writer about what the document says. */
    if (*g_p == '0' && isdigit((unsigned char)g_p[1])) { g_json_parse_error = 1; return NULL; }
    while (isdigit((unsigned char)*g_p)) g_p++;
    if (*g_p == '.') {
        g_p++;
        if (!isdigit((unsigned char)*g_p)) { g_json_parse_error = 1; return NULL; }
        while (isdigit((unsigned char)*g_p)) g_p++;
    }
    if (*g_p == 'e' || *g_p == 'E') {
        g_p++;
        if (*g_p == '+' || *g_p == '-') g_p++;
        if (!isdigit((unsigned char)*g_p)) { g_json_parse_error = 1; return NULL; }
        while (isdigit((unsigned char)*g_p)) g_p++;
    }
    size_t len = (size_t)(g_p - start);
    JsonToken *t = alloc_token(JSON_TYPE_NUMBER);
    if (!t) return NULL;
    char *buf = malloc(len + 1);
    if (!buf) { g_json_parse_error = 1; return NULL; }
    memcpy(buf, start, len);
    buf[len] = '\0';
    t->num_value = strtod(buf, NULL);
    free(buf);
    t->int_value = (int)t->num_value;
    return t;
}

static JsonToken *parse_value(void);

static JsonToken *parse_object(void) {
    if (++g_depth > JSON_MAX_DEPTH) { g_json_parse_error = 1; g_depth--; return NULL; }
    g_p++;                                   /* '{' */
    JsonToken *obj = alloc_token(JSON_TYPE_OBJECT);
    if (!obj) { g_depth--; return NULL; }
    skip_ws();
    if (*g_p == '}') { g_p++; g_depth--; return obj; }
    for (;;) {
        skip_ws();
        if (*g_p != '"') { fail_at("expected a quoted key"); g_depth--; return NULL; }
        char *key = parse_string_raw();
        if (!key) { g_depth--; return NULL; }
        for (int i = 0; i < obj->child_count; i++) {
            if (obj->children[i].key && strcmp(obj->children[i].key, key) == 0) {
                if (getenv("WUBU_JSON_DEBUG")) fprintf(stderr, "[json] duplicate key \"%s\"\n", key);
                free(key); g_json_parse_error = 1; g_depth--; return NULL;   /* duplicate key */
            }
        }
        skip_ws();
        if (*g_p != ':') { if (getenv("WUBU_JSON_DEBUG")) fprintf(stderr, "[json] expected ':' after \"%s\", found '%c'\n", key, *g_p); free(key); g_json_parse_error = 1; g_depth--; return NULL; }
        g_p++;
        skip_ws();
        JsonToken *val = parse_value();
        if (!val) { free(key); g_depth--; return NULL; }
        val->key = key;
        if (push_child(obj, val)) { g_depth--; return NULL; }
        skip_ws();
        if (*g_p == ',') { g_p++; continue; }
        if (*g_p == '}') { g_p++; g_depth--; return obj; }
        if (getenv("WUBU_JSON_DEBUG")) fprintf(stderr, "[json] expected ',' or '}', found '%c'\n", *g_p);
        g_json_parse_error = 1; g_depth--; return NULL;      /* neither , nor } */
    }
}

static JsonToken *parse_array(void) {
    if (++g_depth > JSON_MAX_DEPTH) { g_json_parse_error = 1; g_depth--; return NULL; }
    g_p++;                                   /* '[' */
    JsonToken *arr = alloc_token(JSON_TYPE_ARRAY);
    if (!arr) { g_depth--; return NULL; }
    skip_ws();
    if (*g_p == ']') { g_p++; g_depth--; return arr; }
    for (;;) {
        skip_ws();
        JsonToken *val = parse_value();
        if (!val) { g_depth--; return NULL; }
        if (push_child(arr, val)) { g_depth--; return NULL; }
        skip_ws();
        if (*g_p == ',') { g_p++; continue; }
        if (*g_p == ']') { g_p++; g_depth--; return arr; }
        g_json_parse_error = 1; g_depth--; return NULL;
    }
}

static JsonToken *parse_literal(const char *word, JsonType type, int bool_value, size_t n) {
    if (strncmp(g_p, word, n) != 0) { g_json_parse_error = 1; return NULL; }
    g_p += n;
    JsonToken *t = alloc_token(type);
    if (t) t->bool_value = bool_value != 0;
    return t;
}

static JsonToken *parse_value(void) {
    skip_ws();
    switch (*g_p) {
        case '{': return parse_object();
        case '[': return parse_array();
        case '"': {
            char *s = parse_string_raw();
            if (!s) return NULL;
            JsonToken *t = alloc_token(JSON_TYPE_STRING);
            if (!t) { free(s); return NULL; }
            t->str_value = s;
            return t;
        }
        case 't': return parse_literal("true", JSON_TYPE_BOOL, 1, 4);
        case 'f': return parse_literal("false", JSON_TYPE_BOOL, 0, 5);
        case 'n': return parse_literal("null", JSON_TYPE_NULL, 0, 4);
        default:
            if (*g_p == '-' || isdigit((unsigned char)*g_p)) return parse_number();
            g_json_parse_error = 1;
            return NULL;
    }
}

/* -- lenient entry point, kept for existing callers ------------------ */
JsonToken *json_parse_value(const char **p) {
    if (!p || !*p) return NULL;
    g_p = g_start = *p;
    g_depth = 0;
    JsonToken *t = parse_value();
    if (p) *p = g_p;
    if (getenv("WUBU_JSON_DEBUG"))
        fprintf(stderr, "[json] parse %s children=%d err=%d consumed=%ld at: %.70s\n",
                t ? "ok" : "FAILED", t ? t->child_count : -1, g_json_parse_error,
                (long)(g_p - g_start), g_p);
    return t;
}

JsonToken *json_parse_document(const char *text, size_t len) {
    if (!text || len == 0) return NULL;
    /* Work on a NUL-terminated copy: the scanner is written for text, and a manifest
     * is small. The copy is freed on every path. */
    char *copy = malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, text, len);
    copy[len] = '\0';
    g_json_tokens[0].type = JSON_TYPE_NULL;      /* silence uninitialised-read tools */
    g_json_token_count = 0;
    g_json_parse_error = 0;
    g_p = g_start = copy;
    g_depth = 0;
    JsonToken *root = parse_value();
    if (root) {
        skip_ws();
        if (*g_p != '\0') { fail_at("trailing data"); json_free(root); root = NULL; }
    }
    if (g_json_parse_error) {
        if (root) { json_free(root); }
        root = NULL;
    }
    free(copy);
    return root;
}

static void json_free_recursive(JsonToken *t) {
    if (!t) return;
    free(t->str_value);
    free(t->key);
    for (int i = 0; i < t->child_count; i++) json_free_recursive(&t->children[i]);
    free(t->children);
    t->children = NULL;
    t->child_count = t->child_capacity = 0;
}

void json_free(JsonToken *root) {
    json_free_recursive(root);
    g_json_token_count = 0;
    g_json_parse_error = 0;
}

JsonToken *json_find(JsonToken *obj, const char *key) {
    if (!obj || obj->type != JSON_TYPE_OBJECT) return NULL;
    for (int i = 0; i < obj->child_count; i++)
        if (obj->children[i].key && strcmp(obj->children[i].key, key) == 0)
            return &obj->children[i];
    return NULL;
}
