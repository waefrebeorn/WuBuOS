/* wubu_json.h -- Minimal JSON parser (reusable infrastructure).
 *
 * Extracted from wubu_settings.c. Self-contained: a tiny recursive-descent
 * JSON reader over a static token arena. No external deps beyond libc.
 * Opaque-ish: callers use JsonToken* + the accessors below.
 */

#ifndef WUBU_JSON_H
#define WUBU_JSON_H

#include <stdbool.h>
#include <stddef.h>

#define JSON_MAX_TOKENS 512

typedef enum {
    JSON_TYPE_OBJECT,
    JSON_TYPE_ARRAY,
    JSON_TYPE_STRING,
    JSON_TYPE_NUMBER,
    JSON_TYPE_BOOL,
    JSON_TYPE_NULL
} JsonType;

typedef struct JsonToken_s JsonToken;

struct JsonToken_s {
    JsonType type;
    char *key;
    char *str_value;
    int int_value;      /* kept for existing callers: integral values only */
    double num_value;   /* every number, including fractions and exponents */
    bool bool_value;
    JsonToken *children;
    int child_count;
    int child_capacity;
};

/* Parser arena + state (defined once in wubu_json.c). */
extern JsonToken g_json_tokens[JSON_MAX_TOKENS];
extern int g_json_token_count;
extern int g_json_parse_error;

/* -- Public API --------------------------------------------------- */
/* Parse one value from *p, advancing *p past it. Lenient entry point kept for
 * existing callers; new code should use json_parse_document(). */
JsonToken *json_parse_value(const char **p);

/* Parse a complete document from [text, text+len): whitespace, one value, optional
 * trailing whitespace, nothing else. Returns NULL on any violation. Strict where the
 * lenient path is not: decoded escapes, real doubles, duplicate keys refused, ':' and
 * ',' required, depth bounded. */
JsonToken *json_parse_document(const char *text, size_t len);
JsonToken *json_find(JsonToken *obj, const char *key);
void       json_free(JsonToken *root);

#endif /* WUBU_JSON_H */
