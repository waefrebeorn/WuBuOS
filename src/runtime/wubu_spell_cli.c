/*
 * wubu_spell_cli.c -- the spell-book command line.
 *
 *   wubu_spell list   [book] [genre]   list spells in the book
 *   wubu_spell show   <name>           show one spell's manifest
 *   wubu_spell cast   <name>           cast a spell (resolves needs, fail-closed)
 *   wubu_spell check                  validate the whole book
 *
 * The book root defaults to src/spells and can be overridden with
 * -r <dir> or WUBU_SPELLBOOK, so the same tool works on a vendor tree.
 *
 * C11, libc only.
 */
#include "wubu_spell.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *g_root = "src/spells";

static void usage(void)
{
    printf("usage: wubu_spell [-r <book>] <command> [args]\n"
           "  list  [book] [genre]   list spells\n"
           "  show  <name>           show one spell\n"
           "  cast  <name>           cast a spell\n"
           "  check                  validate the book\n");
}

static void print_spell(const wubu_spell_t *s)
{
    printf("%s %s  [book=%s genre=%s origin=%s]\n",
           s->name, s->version, s->book, s->genre,
           s->origin[0] ? s->origin : "first-party");
    for (size_t i = 0; i < s->provides_count; i++)
        printf("    provides %s\n", s->provides[i]);
    for (size_t i = 0; i < s->needs_count; i++)
        printf("    needs    %s\n", s->needs[i]);
    for (size_t i = 0; i < s->requires_count; i++)
        printf("    requires %s\n", s->requires[i]);
    printf("    sources  %zu, tests %zu%s\n",
           s->sources_count, s->tests_count, s->cast ? ", CAST" : "");
}

static int report_problems(const wubu_spellbook_t *b)
{
    size_t errs = wubu_spellbook_error_count(b);
    for (size_t i = 0; i < errs; i++)
        printf("  error: %s\n", wubu_spellbook_error_at(b, i));

    size_t confs = wubu_spellbook_conflict_count(b);
    for (size_t i = 0; i < confs; i++) {
        const wubu_spell_conflict_t *c = wubu_spellbook_conflict_at(b, i);
        printf("  conflict: '%s' is provided by both '%s' and '%s'\n",
               c->capability, c->a, c->b);
    }

    size_t unmet = wubu_spellbook_unmet_count(b);
    for (size_t i = 0; i < unmet; i++)
        printf("  unmet need: %s\n", wubu_spellbook_unmet_at(b, i));

    return (errs || confs) ? 1 : 0;
}

int main(int argc, char **argv)
{
    int argi = 1;
    if (argi < argc && strcmp(argv[argi], "-r") == 0) {
        if (argi + 1 >= argc) { usage(); return 2; }
        g_root = argv[argi + 1];
        argi += 2;
    } else {
        const char *env = getenv("WUBU_SPELLBOOK");
        if (env && *env) g_root = env;
    }
    if (argi >= argc) { usage(); return 2; }

    const char *cmd = argv[argi++];
    wubu_spellbook_t *b = wubu_spellbook_open(g_root);
    if (!b) {
        printf("wubu_spell: cannot open spell book '%s'\n", g_root);
        return 1;
    }

    int rc = 0;

    if (strcmp(cmd, "list") == 0) {
        const char *book  = (argi < argc) ? argv[argi++] : NULL;
        const char *genre = (argi < argc) ? argv[argi++] : NULL;

        const wubu_spell_t *out[WUBU_SPELL_MAX];
        size_t n = wubu_spellbook_list(b, book, genre, out, WUBU_SPELL_MAX);

        /* grouped by book, then genre, so the organization is visible */
        const char *last_book = NULL;
        const char *last_genre = NULL;
        for (size_t i = 0; i < n; i++) {
            const wubu_spell_t *s = out[i];
            if (!last_book || strcmp(last_book, s->book) != 0) {
                printf("%s/\n", s->book);
                last_book = s->book;
                last_genre = NULL;
            }
            if (!last_genre || strcmp(last_genre, s->genre) != 0) {
                printf("  %s/\n", s->genre);
                last_genre = s->genre;
            }
            printf("    %s %s%s\n", s->name, s->version, s->cast ? "  (cast)" : "");
        }
        if (n == 0) printf("(no spells match)\n");
        printf("%zu spell%s\n", n, n == 1 ? "" : "s");
    }
    else if (strcmp(cmd, "show") == 0) {
        if (argi >= argc) { usage(); rc = 2; }
        else {
            const wubu_spell_t *s = wubu_spellbook_find(b, argv[argi]);
            if (!s) { printf("no such spell '%s'\n", argv[argi]); rc = 1; }
            else print_spell(s);
        }
    }
    else if (strcmp(cmd, "cast") == 0) {
        if (argi >= argc) { usage(); rc = 2; }
        else {
            const char *name = argv[argi];
            if (wubu_spellbook_cast(b, name) != 0) {
                printf("cannot cast '%s'\n", name);
                rc = report_problems(b);
                if (rc == 0) rc = 1;
            } else {
                printf("cast '%s'\n", name);
            }
        }
    }
    else if (strcmp(cmd, "check") == 0) {
        rc = report_problems(b);
        printf("%zu spell%s, %zu error%s, %zu conflict%s, %zu unmet need%s\n",
               wubu_spellbook_count(b), wubu_spellbook_count(b) == 1 ? "" : "s",
               wubu_spellbook_error_count(b), wubu_spellbook_error_count(b) == 1 ? "" : "s",
               wubu_spellbook_conflict_count(b), wubu_spellbook_conflict_count(b) == 1 ? "" : "s",
               wubu_spellbook_unmet_count(b), wubu_spellbook_unmet_count(b) == 1 ? "" : "s");
    }
    else {
        usage();
        rc = 2;
    }

    wubu_spellbook_close(b);
    return rc;
}
