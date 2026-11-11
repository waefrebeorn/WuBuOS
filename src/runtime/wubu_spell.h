/*
 * wubu_spell.h -- the WuBu module format: a SPELL.
 *
 * WuBuOS's answer to the DLL.  A DLL is a loadable module with an export table
 * and metadata; a SPELL is the same idea with a WuBu name and a stricter
 * contract:
 *
 *   - a spell is REUSABLE -- it can be cast by more than one consumer, and
 *     more than once (casting is idempotent);
 *   - a spell DECLARES what it provides and what it needs, so the book can
 *     resolve a need to a provider without loading anything;
 *   - a spell is ORGANIZED -- it lives in a book (a broad need) and a genre
 *     (a category inside that book);
 *   - a spell is FAIL-CLOSED -- casting fails if a need is unmet, rather than
 *     silently producing a half-working module.
 *
 * The on-disk form is a `<name>.spell` manifest: a small `key value` text file
 * that is the module's export table, its dependency list and its address in the
 * book.  It is deliberately NOT code -- like a DLL's import/export directory,
 * it describes the module rather than implementing it.
 *
 * C11, opaque struct, libc only.
 */
#ifndef WUBU_SPELL_H
#define WUBU_SPELL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WUBU_SPELL_MAX       128
#define WUBU_SPELL_STR        64
#define WUBU_SPELL_LIST       32

typedef struct {
    char     name[WUBU_SPELL_STR];
    char     version[WUBU_SPELL_STR];
    char     book[WUBU_SPELL_STR];     /* the broad need      */
    char     genre[WUBU_SPELL_STR];    /* the category in it  */
    char     origin[WUBU_SPELL_STR];   /* "first-party" | "third-party" */
    char     provides[WUBU_SPELL_LIST][WUBU_SPELL_STR];
    size_t   provides_count;
    char     needs[WUBU_SPELL_LIST][WUBU_SPELL_STR];
    size_t   needs_count;
    /* Platform prerequisites (a driver, libc, a kernel feature).  Reported,
     * NOT resolved: they are not capabilities another spell provides, so
     * treating them as unmet NEEDS would be a false alarm. */
    char     requires[WUBU_SPELL_LIST][WUBU_SPELL_STR];
    size_t   requires_count;
    char     sources[WUBU_SPELL_LIST][WUBU_SPELL_STR];
    size_t   sources_count;
    char     tests[WUBU_SPELL_LIST][WUBU_SPELL_STR];
    size_t   tests_count;
    bool     cast;                     /* has been cast       */
} wubu_spell_t;

typedef struct wubu_spellbook wubu_spellbook_t;

/* Scan a spell book root.  Every `*.spell` under it is read; `book` and
 * `genre` are taken from the manifest, and the directory layout is expected to
 * agree with them (a mismatch is reported, not corrected). */
wubu_spellbook_t *wubu_spellbook_open(const char *root);
void              wubu_spellbook_close(wubu_spellbook_t *b);

size_t                wubu_spellbook_count(const wubu_spellbook_t *b);
const wubu_spell_t   *wubu_spellbook_at(const wubu_spellbook_t *b, size_t i);
const wubu_spell_t   *wubu_spellbook_find(const wubu_spellbook_t *b, const char *name);

/* What provides this capability?  NULL when nothing does. */
const wubu_spell_t   *wubu_spellbook_resolve(const wubu_spellbook_t *b,
                                             const char *capability);

/* Cast a spell: mark it active, casting its needs first.  Returns 0 on
 * success, -1 when the spell is unknown or a need is unmet.  Casting an
 * already-cast spell is a no-op (idempotent). */
int                   wubu_spellbook_cast(wubu_spellbook_t *b, const char *name);

/* Spells in a given book (and, when genre is non-NULL, that genre). */
size_t                wubu_spellbook_list(const wubu_spellbook_t *b,
                                          const char *book, const char *genre,
                                          const wubu_spell_t **out, size_t cap);

/* Problems found while scanning: duplicates, missing sources, layout
 * mismatches, unknown keys, unparsable lines. */
size_t      wubu_spellbook_error_count(const wubu_spellbook_t *b);
const char *wubu_spellbook_error_at(const wubu_spellbook_t *b, size_t i);

/* Two spells claiming the same capability: a conflict the caller must resolve
 * explicitly, because silently picking one is how a module gets hijacked. */
typedef struct {
    char            capability[WUBU_SPELL_STR];
    const char     *a;
    const char     *b;
} wubu_spell_conflict_t;

size_t                        wubu_spellbook_conflict_count(const wubu_spellbook_t *b);
const wubu_spell_conflict_t  *wubu_spellbook_conflict_at(const wubu_spellbook_t *b, size_t i);

/* Needs that no spell provides.  These are NOT scan errors -- a spell may
 * legitimately need something the OS or a driver supplies -- but they are
 * reported so the gap is visible rather than assumed away. */
size_t      wubu_spellbook_unmet_count(const wubu_spellbook_t *b);
const char *wubu_spellbook_unmet_at(const wubu_spellbook_t *b, size_t i);

#endif /* WUBU_SPELL_H */
