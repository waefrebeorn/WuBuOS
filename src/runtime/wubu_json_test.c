/* wubu_json_test.c -- the slerm gate for SLERM-2010.
 *
 * The old parser accepted `{"a" 1}`, `{"a":1 "b":2}`, duplicate keys, `1.5` as 1,
 * `1e3` as 1 and undecoded escapes. Each of those is a case here, in the direction
 * that should now FAIL to parse -- because a manifest two readers disagree about is
 * worse than one that does not load.
 */
#include "wubu_json.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int pass, fail;

static JsonToken *doc(const char *s) { return json_parse_document(s, strlen(s)); }

static void ok_parses(const char *text) {
    JsonToken *t = doc(text);
    if (t) { pass++; json_free(t); }
    else { printf("  [FAIL] should parse: %s\n", text); fail++; }
}

static void refuses(const char *text, const char *why) {
    JsonToken *t = doc(text);
    if (!t) pass++;
    else { printf("  [FAIL] should refuse (%-22s): %s\n", why, text); fail++; json_free(t); }
}

static void num_is(const char *text, double want) {
    JsonToken *t = doc(text);
    if (!t || t->type != JSON_TYPE_NUMBER) { printf("  [FAIL] number: %s\n", text); fail++; if (t) json_free(t); return; }
    if (fabs(t->num_value - want) <= 1e-12 * (fabs(want) + 1)) pass++;
    else { printf("  [FAIL] number %-10s got %.17g want %.17g\n", text, t->num_value, want); fail++; }
    json_free(t);
}

static void str_is(const char *text, const char *want) {
    JsonToken *t = doc(text);
    if (!t || t->type != JSON_TYPE_STRING || !t->str_value) { printf("  [FAIL] string: %s\n", text); fail++; if (t) json_free(t); return; }
    if (strcmp(t->str_value, want) == 0) pass++;
    else { printf("  [FAIL] string %-18s got \"%s\" want \"%s\"\n", text, t->str_value, want); fail++; }
    json_free(t);
}

int main(void) {
    printf("=== wubu_json (slerm gate, SLERM-2010) ===\n");

    /* structure + lookup */
    {
        JsonToken *t = doc("{\"a\":1,\"b\":[true,false,null],\"c\":{\"d\":\"x\"}}");
        if (t && t->type == JSON_TYPE_OBJECT && t->child_count == 3) pass++;
        else { printf("  [FAIL] object shape\n"); fail++; }
        JsonToken *a = t ? json_find(t, "a") : NULL;
        JsonToken *b = t ? json_find(t, "b") : NULL;
        JsonToken *c = t ? json_find(t, "c") : NULL;
        JsonToken *d = c ? json_find(c, "d") : NULL;
        if (a && a->type == JSON_TYPE_NUMBER && a->num_value == 1.0 && a->int_value == 1 &&
            b && b->type == JSON_TYPE_ARRAY && b->child_count == 3 &&
            b->children[0].type == JSON_TYPE_BOOL && b->children[0].bool_value &&
            b->children[1].type == JSON_TYPE_BOOL && !b->children[1].bool_value &&
            b->children[2].type == JSON_TYPE_NULL &&
            d && d->str_value && strcmp(d->str_value, "x") == 0) pass++;
        else { printf("  [FAIL] lookup / nested types\n"); fail++; }
        if (!json_find(t, "missing")) pass++; else { printf("  [FAIL] found a missing key\n"); fail++; }
        json_free(t);
    }

    /* numbers: the old atoi path read 1.5 as 1 and 1e3 as 1 */
    num_is("1.5", 1.5);
    num_is("1e3", 1000.0);
    num_is("-0.25", -0.25);
    num_is("6.02e23", 6.02e23);
    num_is("0", 0.0);
    num_is("-12", -12.0);

    /* escapes are decoded, including a surrogate pair */
    str_is("\"a\\nb\"", "a\nb");
    str_is("\"tab\\there\"", "tab\there");
    str_is("\"quote\\\"inside\"", "quote\"inside");
    str_is("\"back\\\\slash\"", "back\\slash");
    str_is("\"\\u00e9\"", "\xc3\xa9");
    str_is("\"\\ud83d\\ude00\"", "\xf0\x9f\x98\x80");
    str_is("\"caf\\u00e9 \\u2713\"", "caf\xc3\xa9 \xe2\x9c\x93");

    /* the lenient cases that must now be refusals */
    refuses("{\"a\" 1}", "no colon");
    refuses("{\"a\":1 \"b\":2}", "no comma");
    refuses("{\"a\":1,}", "trailing comma");
    refuses("[1,]", "trailing comma");
    refuses("{\"a\":1}{\"b\":2}", "two documents");
    refuses("{\"a\":1} x", "trailing garbage");
    refuses("{\"a\":1,\"a\":2}", "duplicate key");
    refuses("{\"a\":}", "missing value");
    refuses("{\"a\":1", "unterminated object");
    refuses("\"unterminated", "unterminated string");
    refuses("\"bad \\q escape\"", "unknown escape");
    refuses("\"\\ud800\"", "lone high surrogate");
    refuses("\"\\udc00\"", "lone low surrogate");
    refuses("01", "leading zero");
    refuses("1.", "trailing dot");
    refuses(".5", "bare fraction");
    refuses("-", "bare minus");
    refuses("+1", "leading plus");
    refuses("nul", "bad literal");
    refuses("", "empty document");

    /* depth is bounded rather than recursing into the stack */
    {
        char deep[512];
        size_t i = 0;
        for (int k = 0; k < 40; k++) deep[i++] = '[';
        for (int k = 0; k < 40; k++) deep[i++] = ']';
        deep[i] = '\0';
        refuses(deep, "40 levels deep");
    }

    /* arrays of values, and an empty document's two shapes */
    ok_parses("[]");
    ok_parses("{}");
    ok_parses("[1,2,[3,[4]],\"s\",true,null]");
    ok_parses("{\"k\":[{\"n\":null}]}");

    printf("\n=== %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
