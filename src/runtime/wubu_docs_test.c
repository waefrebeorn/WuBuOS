/*
 * wubu_docs_test.c -- the docs must not lie.
 *
 * The user: "a lot of documents are very fluffy and out of date; a lot of
 * readmes read like schizophrenia instead of inform on function and goal."
 * A measured audit found 264 file references in the OS docs that point at
 * files which do not exist -- including whole subsystem maps (docs/MODULES.md,
 * docs/TOPOLOGY.md) that name files that were never written.
 *
 * This test makes the front-door documentation accountable: every full path it
 * names (src/..., tools/..., mk/..., docs/...) must exist.  A path that stops
 * existing is a build failure, so the "mind palace" cannot drift the way the
 * leaf READMEs have.
 *
 * The three docs under guard are the ones that describe the system as a whole
 * -- the walkthrough, the front door, and the spell book doctrine.  They are
 * deliberately few: a doc that is not maintained is a doc that will be opened
 * and contradicted.
 *
 * Glob tokens (a path containing '*') are resolved with glob(), so a shorthand
 * like `src/runtime/styx*.c` is a real check, not prose.
 */
#include <glob.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;
static int checks;
#define CHECK(c, m) do { \
    checks++; \
    if (!(c)) { printf("  FAIL: %s\n", m); failures++; } \
} while (0)

static const char *const DOCS[] = {
    "docs/MIND_PALACE.md",
    "docs/SAGA.md",
    "docs/MODULES.md",
    "docs/ECOSYSTEM.md",
    "README.md",
    "src/spells/README.md",
};

static const char *const EXTENSIONS[] = {
    ".c", ".h", ".md", ".py", ".sh", ".lua", ".json", ".conf", ".mk", ".spell",
};

static int has_known_ext(const char *tok)
{
    size_t n = strlen(tok);
    for (size_t i = 0; i < sizeof(EXTENSIONS) / sizeof(EXTENSIONS[0]); i++) {
        size_t el = strlen(EXTENSIONS[i]);
        if (n > el && strcmp(tok + n - el, EXTENSIONS[i]) == 0) return 1;
    }
    return 0;
}

/* Return the longest path-like prefix of s, or NULL. */
static const char *path_prefix(const char *s)
{
    static const char *const stems[] = { "src/", "tools/", "mk/", "docs/",
                                         "assets/", "scripts/" };
    for (size_t i = 0; i < sizeof(stems) / sizeof(stems[0]); i++)
        if (strncmp(s, stems[i], strlen(stems[i])) == 0) return s;
    return NULL;
}

static int is_delim(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' ||
           c == ',' || c == ';' || c == ')' || c == ']' || c == '}' ||
           c == '"' || c == '\'' || c == '`';
}

/* Check one path token.  Returns 1 if it exists (or is a glob with a match). */
static int check_path(const char *tok, const char *doc, int lineno)
{
    if (strchr(tok, '*')) {
        glob_t g;
        int r = glob(tok, 0, NULL, &g);
        int ok = (r == 0);
        if (ok) globfree(&g);
        return ok;
    }
    FILE *f = fopen(tok, "r");
    if (f) { fclose(f); return 1; }
    return 0;
}

/* Verify every full path in one doc. */
static void verify_doc(const char *doc)
{
    FILE *f = fopen(doc, "r");
    if (!f) {
        CHECK(0, doc);
        return;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    CHECK(sz > 200, doc);   /* a doc shorter than 200 bytes is a stub, not a doc */

    char line[2048];
    int lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;
        for (const char *p = line; *p; ) {
            const char *st = path_prefix(p);
            if (!st) { p++; continue; }
            /* capture the token from st up to the next delimiter */
            const char *e = st;
            while (*e && !is_delim(*e)) e++;
            /* skip roots that are directory references (trailing / or a bare
             * dir name with no extension), but CHECK bounded tokens */
            size_t len = (size_t)(e - st);
            if (len < 2) { p++; continue; }
            char *tok = (char *)malloc(len + 1);
            memcpy(tok, st, len);
            tok[len] = '\0';
            /* trim trailing punctuation that the capture may have kept */
            while (len > 0) {
                char c = tok[len - 1];
                if (c == '.' || c == '/' || c == ':' || c == '*') tok[--len] = '\0';
                else break;
            }
            if (strchr(tok, '*')) {
                /* keep a single trailing glob pattern */
                if (len > 2 && tok[len-1] == '*') tok[len-1] = '\0';
            }
            /* skip templated layouts like <book>/<genre>/<spell>/<spell>.spell */
            if (len > 0 && has_known_ext(tok) &&
                !strchr(tok, '<') && !strchr(tok, '>'))
                CHECK(check_path(tok, doc, lineno), tok);
            free(tok);
            if (e == st) p++;
            else p = e;
        }
    }
    fclose(f);
}

int main(void)
{
    printf("=== test_docs ===\n");
    for (size_t i = 0; i < sizeof(DOCS) / sizeof(DOCS[0]); i++)
        verify_doc(DOCS[i]);

    if (failures == 0) printf("=== test_docs: %d checks, 0 failed ===\n", checks);
    else printf("=== test_docs: %d failed of %d ===\n", failures, checks);
    return failures ? 1 : 0;
}
