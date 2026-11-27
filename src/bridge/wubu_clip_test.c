/* wubu_clip_test.c -- the gate for D4 stage A: a clipboard with types and a preference rule.
 *
 * The behaviour that matters for a paste handler is the ORDER: ask for the richest type you
 * can accept, fall back through the list, and never get two versions of the same type. Each
 * of those is a case here, plus the refusals that keep the model honest (a MIME type with
 * parameters is not a type we store; NULL bytes with a length is not data).
 */
#include "wubu_clip.h"
#include <stdio.h>
#include <string.h>

static int pass, fail;

static void check(int cond, const char *what) {
    if (cond) { pass++; return; }
    printf("  [FAIL] %s\n", what);
    fail++;
}

int main(void) {
    printf("=== wubu_clip (gate D4 stage A) ===\n");
    WubuClip c;
    wubu_clip_init(&c);
    check(wubu_clip_count(&c) == 0, "a fresh clipboard is empty");

    static const char text[] = "hello clipboard";
    static const char html[] = "<b>hello clipboard</b>";
    static const unsigned char png[4] = { 0x89, 'P', 'N', 'G' };

    check(wubu_clip_set(&c, "text/plain", text, sizeof text - 1) == WUBU_CLIP_OK, "set text/plain");
    check(wubu_clip_count(&c) == 1, "one type stored");
    const WubuClipItem *it = wubu_clip_get(&c, "text/plain");
    check(it && it->len == sizeof text - 1 && memcmp(it->data, text, it->len) == 0, "text/plain round trips");

    check(wubu_clip_set(&c, "text/html", html, sizeof html - 1) == WUBU_CLIP_OK, "set text/html");
    check(wubu_clip_set(&c, "image/png", png, sizeof png) == WUBU_CLIP_OK, "set image/png");
    check(wubu_clip_count(&c) == 3, "three representations of one item");

    /* preference order is data: a rich editor asks for html first, a terminal asks for text */
    static const char *const rich[] = { "text/html", "text/plain" };
    static const char *const term[] = { "text/plain" };
    static const char *const img[]  = { "image/png", "text/plain" };
    static const char *const none[] = { "application/x-nothing", "image/gif" };
    check(wubu_clip_best(&c, rich, 2) == wubu_clip_get(&c, "text/html"), "rich app gets html");
    check(wubu_clip_best(&c, term, 1) == wubu_clip_get(&c, "text/plain"), "terminal gets plain text");
    check(wubu_clip_best(&c, img, 2) == wubu_clip_get(&c, "image/png"), "image request gets png");
    check(wubu_clip_best(&c, none, 2) == NULL, "an unsatisfiable request returns nothing, not a wrong type");

    /* the same MIME twice is ONE entry with the newer bytes */
    unsigned gen = c.generation;
    static const char text2[] = "replaced";
    check(wubu_clip_set(&c, "text/plain", text2, sizeof text2 - 1) == WUBU_CLIP_OK, "replace text/plain");
    check(wubu_clip_count(&c) == 3, "replacing does not add an entry");
    it = wubu_clip_get(&c, "text/plain");
    check(it && it->len == sizeof text2 - 1 && memcmp(it->data, text2, it->len) == 0, "the newer bytes win");
    check(c.generation > gen, "a replacement bumps the generation");

    /* refusals */
    check(wubu_clip_set(&c, "text", text, 1) == WUBU_CLIP_BAD_MIME, "a bare type is refused");
    check(wubu_clip_set(&c, "/plain", text, 1) == WUBU_CLIP_BAD_MIME, "a missing type is refused");
    check(wubu_clip_set(&c, "text/plan/e", text, 1) == WUBU_CLIP_BAD_MIME, "two slashes refused");
    check(wubu_clip_set(&c, "text/plain;charset=utf-8", text, 1) == WUBU_CLIP_BAD_MIME,
          "a MIME parameter is not a type: parameters belong to the paste, not the entry");
    check(wubu_clip_set(&c, "text/plain", NULL, 5) == WUBU_CLIP_ARGS, "NULL bytes with a length is refused");
    check(wubu_clip_set(NULL, "text/plain", text, 1) == WUBU_CLIP_ARGS, "a null clipboard is refused");
    check(wubu_clip_mime_ok("application/x-wubu-archive") == 1, "hyphen and x- types are valid");

    /* capacity is stated, not surprising */
    {
        WubuClip full;
        wubu_clip_init(&full);
        char mime[32];
        int rc = WUBU_CLIP_OK;
        for (int i = 0; i < WUBU_CLIP_MAX_ITEMS; i++) {
            snprintf(mime, sizeof mime, "application/x-t%d", i);
            rc = wubu_clip_set(&full, mime, text, 1);
            if (rc != WUBU_CLIP_OK) break;
        }
        check(rc == WUBU_CLIP_OK && wubu_clip_count(&full) == WUBU_CLIP_MAX_ITEMS, "the clipboard fills to its cap");
        check(wubu_clip_set(&full, "application/x-overflow", text, 1) == WUBU_CLIP_FULL, "one type too many is refused by name");
    }

    /* clearing is visible to a watcher, not indistinguishable from never-set */
    gen = c.generation;
    wubu_clip_clear(&c);
    check(wubu_clip_count(&c) == 0 && wubu_clip_get(&c, "text/plain") == NULL, "clear empties every type");
    check(c.generation > gen, "clearing bumps the generation");

    printf("\n=== %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
