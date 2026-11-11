/* wubu_json.c -- Minimal JSON parser.
 *
 * Self-contained module extracted from wubu_settings.c. Uses the shared token
 * arena + state declared in wubu_json.h. Minimal includes.
 */

#include "wubu_json.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

JsonToken g_json_tokens[JSON_MAX_TOKENS];
int g_json_token_count = 0;
int g_json_parse_error = 0;

static JsonToken *json_alloc_token(void) {
    if (g_json_token_count >= JSON_MAX_TOKENS) { g_json_parse_error = 1; return NULL; }
    JsonToken *t = &g_json_tokens[g_json_token_count++];
    memset(t, 0, sizeof(*t));
    t->child_capacity = 8;
    t->children = calloc(t->child_capacity, sizeof(JsonToken));
    return t;
}

static void json_skip_ws(const char **p) {
    while (**p && isspace((unsigned char)**p)) (*p)++;
}

static bool json_match(const char **p, const char *s) {
    json_skip_ws(p);
    if (strncmp(*p, s, strlen(s)) == 0) {
        *p += strlen(s);
        return true;
    }
    return false;
}

JsonToken *json_parse_value(const char **p);
static JsonToken *json_parse_string(const char **p);
static JsonToken *json_parse_number(const char **p);
static JsonToken *json_parse_object(const char **p);
static JsonToken *json_parse_array(const char **p);

static JsonToken *json_parse_string(const char **p) {
    if (!json_match(p, "\"")) return NULL;
    JsonToken *t = json_alloc_token(); if (!t) return NULL;
    t->type = JSON_TYPE_STRING;
    const char *start = *p;
    while (**p && **p != '"') {
        if (**p == '\\' && *(*p + 1)) (*p) += 2;
        else (*p)++;
    }
    size_t len = *p - start;
    t->str_value = malloc(len + 1);
    memcpy(t->str_value, start, len);
    t->str_value[len] = '\0';
    json_match(p, "\"");
    return t;
}

static JsonToken *json_parse_number(const char **p) {
    json_skip_ws(p);
    if (!isdigit(**p) && **p != '-') return NULL;
    JsonToken *t = json_alloc_token(); if (!t) return NULL;
    t->type = JSON_TYPE_NUMBER;
    const char *start = *p;
    if (**p == '-') (*p)++;
    while (isdigit(**p)) (*p)++;
    if (**p == '.') { (*p)++; while (isdigit(**p)) (*p)++; }
    size_t len = *p - start;
    char buf[64];
    memcpy(buf, start, len);
    buf[len] = '\0';
    t->int_value = atoi(buf);
    return t;
}

static JsonToken *json_parse_bool(const char **p) {
    json_skip_ws(p);
    JsonToken *t = json_alloc_token(); if (!t) return NULL;
    t->type = JSON_TYPE_BOOL;
    if (strncmp(*p, "true", 4) == 0) { t->bool_value = true; *p += 4; return t; }
    if (strncmp(*p, "false", 5) == 0) { t->bool_value = false; *p += 5; return t; }
    if (strncmp(*p, "null", 4) == 0) { t->type = JSON_TYPE_NULL; *p += 4; return t; }
    g_json_parse_error = 1; return NULL;
}

static JsonToken *json_parse_object(const char **p) {
    if (!json_match(p, "{")) return NULL;
    JsonToken *t = json_alloc_token(); if (!t) return NULL;
    t->type = JSON_TYPE_OBJECT;
    while (true) {
        json_skip_ws(p);
        if (json_match(p, "}")) break;
        JsonToken *key = json_parse_string(p); if (!key) { g_json_parse_error = 1; break; }
        json_skip_ws(p); json_match(p, ":"); json_skip_ws(p);
        JsonToken *val = json_parse_value(p); if (!val) { g_json_parse_error = 1; break; }
        val->key = key->str_value;
        if (t->child_count >= t->child_capacity) {
            t->child_capacity *= 2;
            t->children = realloc(t->children, t->child_capacity * sizeof(JsonToken));
        }
        t->children[t->child_count++] = *val;
        json_skip_ws(p); json_match(p, ",");
    }
    return t;
}

static JsonToken *json_parse_array(const char **p) {
    if (!json_match(p, "[")) return NULL;
    JsonToken *t = json_alloc_token(); if (!t) return NULL;
    t->type = JSON_TYPE_ARRAY;
    while (true) {
        json_skip_ws(p);
        if (json_match(p, "]")) break;
        JsonToken *val = json_parse_value(p); if (!val) { g_json_parse_error = 1; break; }
        if (t->child_count >= t->child_capacity) {
            t->child_capacity *= 2;
            t->children = realloc(t->children, t->child_capacity * sizeof(JsonToken));
        }
        t->children[t->child_count++] = *val;
        json_skip_ws(p); json_match(p, ",");
    }
    return t;
}

JsonToken *json_parse_value(const char **p) {
    json_skip_ws(p);
    if (**p == '"') return json_parse_string(p);
    if (**p == '{') return json_parse_object(p);
    if (**p == '[') return json_parse_array(p);
    if (isdigit(**p) || **p == '-') return json_parse_number(p);
    return json_parse_bool(p);
}

static void json_free_recursive(JsonToken *t) {
    if (!t) return;
    if (t->type == JSON_TYPE_STRING) free(t->str_value);
    if (t->key) free(t->key);
    for (int i = 0; i < t->child_count; i++) json_free_recursive(&t->children[i]);
    free(t->children);
}

void json_free(JsonToken *root) {
    json_free_recursive(root);
    g_json_token_count = 0;
}

JsonToken *json_find(JsonToken *obj, const char *key) {
    if (!obj || obj->type != JSON_TYPE_OBJECT) return NULL;
    for (int i = 0; i < obj->child_count; i++)
        if (obj->children[i].key && strcmp(obj->children[i].key, key) == 0)
            return &obj->children[i];
    return NULL;
}
