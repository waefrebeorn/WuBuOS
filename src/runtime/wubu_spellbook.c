/*
 * wubu_spellbook.c -- scan, validate, resolve and cast spells.
 *
 * A spell book is a directory tree:
 *
 *     <root>/<book>/<genre>/<name>.spell
 *
 * The broad need is the BOOK, the category inside it is the GENRE, and the
 * reusable module is the SPELL.  The manifest is `key value` text, parsed here
 * with no external dependency.
 *
 * Everything in this file is fail-closed and reports rather than repairs: a
 * duplicate name, a missing source, a layout mismatch, an unknown key and two
 * spells claiming the same capability are all RECORDED, and the caller decides.
 * Silently picking a winner is how a module gets hijacked.
 */
#include "wubu_spell.h"

#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct wubu_spellbook {
    wubu_spell_t            spells[WUBU_SPELL_MAX];
    size_t                  spell_count;

    char                  **errors;          /* malloc'd */
    size_t                  error_count;
    size_t                  error_cap;

    wubu_spell_conflict_t   conflicts[WUBU_SPELL_MAX];
    size_t                  conflict_count;

    char                  **unmet;           /* malloc'd capability names */
    size_t                  unmet_count;
    size_t                  unmet_cap;

    char                    root[512];
};

/* ------------------------------------------------------------------ helpers */

static char *xstrdup(const char *s)
{
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

static void book_error(wubu_spellbook_t *b, const char *fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    if (b->error_count == b->error_cap) {
        size_t cap = b->error_cap ? b->error_cap * 2 : 16;
        char **grown = (char **)realloc(b->errors, cap * sizeof(char *));
        if (!grown) return;
        b->errors = grown;
        b->error_cap = cap;
    }
    b->errors[b->error_count++] = xstrdup(buf);
}

static void book_unmet(wubu_spellbook_t *b, const char *cap)
{
    for (size_t i = 0; i < b->unmet_count; i++)
        if (strcmp(b->unmet[i], cap) == 0) return;

    if (b->unmet_count == b->unmet_cap) {
        size_t cap = b->unmet_cap ? b->unmet_cap * 2 : 16;
        char **grown = (char **)realloc(b->unmet, cap * sizeof(char *));
        if (!grown) return;
        b->unmet = grown;
        b->unmet_cap = cap;
    }
    b->unmet[b->unmet_count++] = xstrdup(cap);
}

static bool has_cap(const wubu_spell_t *s, const char *cap)
{
    for (size_t i = 0; i < s->provides_count; i++)
        if (strcmp(s->provides[i], cap) == 0) return true;
    return false;
}

static const wubu_spell_t *find_spell(const wubu_spellbook_t *b, const char *name)
{
    for (size_t i = 0; i < b->spell_count; i++)
        if (strcmp(b->spells[i].name, name) == 0) return &b->spells[i];
    return NULL;
}

static void set_str(char *dst, size_t cap, const char *src)
{
    snprintf(dst, cap, "%s", src);
}

static void add_list(char out[][WUBU_SPELL_STR], size_t *count, const char *val)
{
    if (*count >= WUBU_SPELL_LIST) return;
    set_str(out[*count], WUBU_SPELL_STR, val);
    (*count)++;
}

/* --------------------------------------------------------------- the parser */

typedef struct {
    const char *key;
    int         kind;          /* 0 = scalar, 1 = list */
    int         field;         /* which field */
} spell_key_t;

enum { F_SPELL, F_VERSION, F_BOOK, F_GENRE, F_ORIGIN,
       F_PROVIDES, F_NEEDS, F_REQUIRES, F_SOURCE, F_TEST };

static bool key_lookup(const char *k, spell_key_t *out)
{
    static const spell_key_t keys[] = {
        { "spell",    0, F_SPELL    },
        { "version",  0, F_VERSION  },
        { "book",     0, F_BOOK     },
        { "genre",    0, F_GENRE    },
        { "origin",   0, F_ORIGIN   },
        { "provides", 1, F_PROVIDES },
        { "needs",    1, F_NEEDS    },
        { "requires", 1, F_REQUIRES },
        { "source",   1, F_SOURCE   },
        { "test",     1, F_TEST     },
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        if (strcmp(k, keys[i].key) == 0) { *out = keys[i]; return true; }
    }
    return false;
}

/* Parse one manifest into `sp`, reporting problems against `b`.
 * `dir` is the spell's directory (for source existence checks). */
static void parse_manifest(wubu_spellbook_t *b, wubu_spell_t *sp,
                           const char *path, const char *dir)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        book_error(b, "%s: cannot open", path);
        return;
    }

    char line[512];
    int lineno = 0;
    while (fgets(line, sizeof(line), f)) {
        lineno++;

        /* comments: '#' anywhere begins one (documented; values must not
         * contain '#') */
        char *hash = strchr(line, '#');
        if (hash) *hash = '\0';

        /* trim */
        char *p = line;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        size_t len = strlen(p);
        while (len > 0 && (p[len - 1] == ' ' || p[len - 1] == '\t' ||
                           p[len - 1] == '\r' || p[len - 1] == '\n'))
            p[--len] = '\0';
        if (len == 0) continue;

        char *sep = strpbrk(p, " \t");
        if (!sep) {
            book_error(b, "%s:%d: not a 'key value' line: %s", path, lineno, p);
            continue;
        }
        *sep = '\0';
        char *val = sep + 1;
        while (*val == ' ' || *val == '\t') val++;

        spell_key_t k;
        if (!key_lookup(p, &k)) {
            book_error(b, "%s:%d: unknown key '%s'", path, lineno, p);
            continue;
        }
        if (*val == '\0') {
            book_error(b, "%s:%d: key '%s' has no value", path, lineno, p);
            continue;
        }

        switch (k.field) {
        case F_SPELL:    set_str(sp->name, sizeof(sp->name), val); break;
        case F_VERSION:  set_str(sp->version, sizeof(sp->version), val); break;
        case F_BOOK:     set_str(sp->book, sizeof(sp->book), val); break;
        case F_GENRE:    set_str(sp->genre, sizeof(sp->genre), val); break;
        case F_ORIGIN:   set_str(sp->origin, sizeof(sp->origin), val); break;
        case F_PROVIDES: add_list(sp->provides, &sp->provides_count, val); break;
        case F_NEEDS:    add_list(sp->needs, &sp->needs_count, val); break;
        case F_REQUIRES: add_list(sp->requires, &sp->requires_count, val); break;
        case F_SOURCE:   add_list(sp->sources, &sp->sources_count, val); break;
        case F_TEST:     add_list(sp->tests, &sp->tests_count, val); break;
        default: break;
        }
    }
    fclose(f);

    if (sp->name[0] == '\0')
        book_error(b, "%s: manifest has no 'spell <name>'", path);

    /* Every declared source must exist beside the manifest: a spell that
     * cannot be built is not a reusable brick. */
    for (size_t i = 0; i < sp->sources_count; i++) {
        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", dir, sp->sources[i]);
        FILE *t = fopen(full, "r");
        if (!t) {
            book_error(b, "%s: declared source '%s' is missing",
                       path, sp->sources[i]);
            continue;
        }
        fclose(t);
    }
}

/* ---------------------------------------------------------------- the walker */

static void walk(wubu_spellbook_t *b, const char *dir, const char *rel)
{
    DIR *d = opendir(dir);
    if (!d) return;

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;

        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", dir, e->d_name);

        char child_rel[512];
        if (rel[0] == '\0')
            snprintf(child_rel, sizeof(child_rel), "%s", e->d_name);
        else
            snprintf(child_rel, sizeof(child_rel), "%s/%s", rel, e->d_name);

        DIR *sub = opendir(full);
        if (sub) {
            closedir(sub);
            walk(b, full, child_rel);
            continue;
        }

        size_t n = strlen(e->d_name);
        if (n < 7 || strcmp(e->d_name + n - 6, ".spell") != 0) continue;

        if (b->spell_count >= WUBU_SPELL_MAX) {
            book_error(b, "%s: more than %d spells", full, WUBU_SPELL_MAX);
            continue;
        }

        wubu_spell_t *sp = &b->spells[b->spell_count];
        memset(sp, 0, sizeof(*sp));
        parse_manifest(b, sp, full, dir);

        /* The DIRECTORY layout must agree with the manifest: the layout is
         *   <root>/<book>/<genre>/<spell>/<spell>.spell
         * so the book is the first component, the genre the second, and the
         * leaf DIRECTORY is the spell's own name.  A spell filed under the
         * wrong book is exactly how a book stops being findable. */
        char want_book[WUBU_SPELL_STR] = {0};
        char want_genre[WUBU_SPELL_STR] = {0};
        char want_leaf[WUBU_SPELL_STR] = {0};
        snprintf(want_book, sizeof(want_book), "%s", rel);
        char *slash = strchr(want_book, '/');
        if (slash) {
            *slash = '\0';
            char *rest = slash + 1;
            char *slash2 = strchr(rest, '/');
            if (slash2) {
                *slash2 = '\0';
                snprintf(want_leaf, sizeof(want_leaf), "%s", slash2 + 1);
            }
            snprintf(want_genre, sizeof(want_genre), "%s", rest);
        }
        if (sp->book[0] && strcmp(sp->book, want_book) != 0)
            book_error(b, "%s: manifest says book '%s' but it is filed under '%s'",
                       full, sp->book, want_book);
        if (sp->genre[0] && strcmp(sp->genre, want_genre) != 0)
            book_error(b, "%s: manifest says genre '%s' but it is filed under '%s'",
                       full, sp->genre, want_genre);
        if (want_leaf[0] && strcmp(sp->name, want_leaf) != 0)
            book_error(b, "%s: spell is named '%s' but its directory is '%s'",
                       full, sp->name, want_leaf);

        b->spell_count++;
    }
    closedir(d);
}

/* ---------------------------------------------------------------- public API */

wubu_spellbook_t *wubu_spellbook_open(const char *root)
{
    if (!root || !*root) return NULL;

    wubu_spellbook_t *b = (wubu_spellbook_t *)calloc(1, sizeof(*b));
    if (!b) return NULL;
    set_str(b->root, sizeof(b->root), root);

    walk(b, root, "");

    /* duplicates: the same spell name registered twice */
    for (size_t i = 0; i < b->spell_count; i++) {
        for (size_t j = i + 1; j < b->spell_count; j++) {
            if (strcmp(b->spells[i].name, b->spells[j].name) == 0)
                book_error(b, "duplicate spell name '%s'", b->spells[i].name);
        }
    }

    /* conflicts: two spells claiming the same capability */
    for (size_t i = 0; i < b->spell_count; i++) {
        for (size_t k = 0; k < b->spells[i].provides_count; k++) {
            const char *cap = b->spells[i].provides[k];
            for (size_t j = i + 1; j < b->spell_count; j++) {
                if (!has_cap(&b->spells[j], cap)) continue;
                if (b->conflict_count >= WUBU_SPELL_MAX) break;
                wubu_spell_conflict_t *c = &b->conflicts[b->conflict_count++];
                memset(c, 0, sizeof(*c));
                set_str(c->capability, sizeof(c->capability), cap);
                c->a = b->spells[i].name;
                c->b = b->spells[j].name;
            }
        }
    }

    /* unmet needs: needed by someone, provided by no one */
    for (size_t i = 0; i < b->spell_count; i++) {
        for (size_t k = 0; k < b->spells[i].needs_count; k++) {
            const char *need = b->spells[i].needs[k];
            bool provided = false;
            for (size_t j = 0; j < b->spell_count; j++) {
                if (has_cap(&b->spells[j], need)) { provided = true; break; }
            }
            if (!provided) book_unmet(b, need);
        }
    }

    return b;
}

void wubu_spellbook_close(wubu_spellbook_t *b)
{
    if (!b) return;
    for (size_t i = 0; i < b->error_count; i++) free(b->errors[i]);
    free(b->errors);
    for (size_t i = 0; i < b->unmet_count; i++) free(b->unmet[i]);
    free(b->unmet);
    free(b);
}

size_t wubu_spellbook_count(const wubu_spellbook_t *b)
{
    return b ? b->spell_count : 0;
}

const wubu_spell_t *wubu_spellbook_at(const wubu_spellbook_t *b, size_t i)
{
    if (!b || i >= b->spell_count) return NULL;
    return &b->spells[i];
}

const wubu_spell_t *wubu_spellbook_find(const wubu_spellbook_t *b, const char *name)
{
    return (b && name) ? find_spell(b, name) : NULL;
}

const wubu_spell_t *wubu_spellbook_resolve(const wubu_spellbook_t *b,
                                           const char *capability)
{
    if (!b || !capability) return NULL;
    for (size_t i = 0; i < b->spell_count; i++)
        if (has_cap(&b->spells[i], capability)) return &b->spells[i];
    return NULL;
}

size_t wubu_spellbook_list(const wubu_spellbook_t *b, const char *book,
                           const char *genre, const wubu_spell_t **out,
                           size_t cap)
{
    if (!b || !out || cap == 0) return 0;
    size_t n = 0;
    for (size_t i = 0; i < b->spell_count && n < cap; i++) {
        const wubu_spell_t *s = &b->spells[i];
        if (book && s->book[0] && strcmp(s->book, book) != 0) continue;
        if (genre && s->genre[0] && strcmp(s->genre, genre) != 0) continue;
        out[n++] = s;
    }
    return n;
}

/* Cast with dependency order.  `depth` guards against a need cycle. */
static int cast_one(wubu_spellbook_t *b, const wubu_spell_t *s, int depth)
{
    if (!s) return -1;
    if (s->cast) return 0;                 /* idempotent */
    if (depth > WUBU_SPELL_MAX) return -1; /* a need cycle */

    for (size_t i = 0; i < s->needs_count; i++) {
        const wubu_spell_t *p = wubu_spellbook_resolve(b, s->needs[i]);
        if (!p) {
            book_error(b, "cannot cast '%s': need '%s' is unmet",
                       s->name, s->needs[i]);
            return -1;
        }
        if (strcmp(p->name, s->name) == 0) {
            book_error(b, "spell '%s' needs itself", s->name);
            return -1;
        }
        if (cast_one(b, p, depth + 1) != 0) return -1;
    }

    /* resolve() returns a const pointer into the table; the cast flag lives in
     * the same array, so a non-const view of the same element is safe. */
    wubu_spell_t *mutable_s = (wubu_spell_t *)s;
    mutable_s->cast = true;
    return 0;
}

int wubu_spellbook_cast(wubu_spellbook_t *b, const char *name)
{
    if (!b || !name) return -1;
    const wubu_spell_t *s = find_spell(b, name);
    if (!s) {
        book_error(b, "cannot cast '%s': no such spell", name);
        return -1;
    }
    return cast_one(b, s, 0);
}

size_t wubu_spellbook_error_count(const wubu_spellbook_t *b)
{
    return b ? b->error_count : 0;
}

const char *wubu_spellbook_error_at(const wubu_spellbook_t *b, size_t i)
{
    if (!b || i >= b->error_count) return NULL;
    return b->errors[i];
}

size_t wubu_spellbook_conflict_count(const wubu_spellbook_t *b)
{
    return b ? b->conflict_count : 0;
}

const wubu_spell_conflict_t *wubu_spellbook_conflict_at(const wubu_spellbook_t *b,
                                                        size_t i)
{
    if (!b || i >= b->conflict_count) return NULL;
    return &b->conflicts[i];
}

size_t wubu_spellbook_unmet_count(const wubu_spellbook_t *b)
{
    return b ? b->unmet_count : 0;
}

const char *wubu_spellbook_unmet_at(const wubu_spellbook_t *b, size_t i)
{
    if (!b || i >= b->unmet_count) return NULL;
    return b->unmet[i];
}
