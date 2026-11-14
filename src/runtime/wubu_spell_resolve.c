/*
 * wubu_spell_resolve.c -- see wubu_spell_resolve.h for why this exists.
 *
 * Pure graph work over borrowed module handles: no filesystem, no byte copying,
 * no code interpretation. One leaf, one concern.
 */
#include "wubu_spell_resolve.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct wubu_spell_resolve {
    wubu_spellmod_t *mods[WUBU_SPELL_MAX_MODULES];
    /* Images the registry LOADED ITSELF (via wubu_spell_scan).  Modules added by
     * hand with wubu_spell_registry_add() borrow their buffer and are not listed
     * here.  Two ownership modes, kept apart on purpose: mixing them is how a module
     * ends up pointing at released memory. */
    uint8_t *owned_images[WUBU_SPELL_MAX_MODULES];
    int count;
};

/* Modules are opaque, so identity goes through the public info accessor.
 *
 * The caller supplies the buffer: returning a pointer to one shared static would make
 * `strcmp(mod_name(a), mod_name(b))` compare b against itself, because both arguments
 * are evaluated before strcmp runs -- which makes every module look like a duplicate
 * of the previous one. */
static int mod_name(const wubu_spellmod_t *m, char *out, size_t outsz)
{
    if (!m || !out || outsz == 0) return -1;
    wubu_spellmod_info_t info;
    if (wubu_spellmod_info(m, &info) != 0) { out[0] = '\0'; return -1; }
    memcpy(out, info.name, outsz < WUBU_SPELLMOD_NAME_MAX ? outsz : WUBU_SPELLMOD_NAME_MAX);
    out[outsz - 1] = '\0';
    return 0;
}

/* Compare two modules by name without either side clobbering the other. */
static bool same_module(const wubu_spellmod_t *a, const wubu_spellmod_t *b)
{
    char na[WUBU_SPELLMOD_NAME_MAX], nb[WUBU_SPELLMOD_NAME_MAX];
    if (mod_name(a, na, sizeof(na)) != 0 || mod_name(b, nb, sizeof(nb)) != 0)
        return false;
    return strcmp(na, nb) == 0;
}


/* Visit state for the plan walk. Kept in the registry rather than on the stack so a
 * deep chain cannot grow the C stack alongside the recursion it mirrors. */
enum {
    VISIT_UNSEEN = 0,
    VISIT_OPEN,        /* on the current path -- seeing it again is a cycle */
    VISIT_DONE         /* already planned; nothing to do */
};

typedef struct {
    wubu_spellmod_t *m;
    int state;
} visit_entry;

wubu_spell_resolve *wubu_spell_resolve_new(void)
{
    return calloc(1, sizeof(wubu_spell_resolve));
}

void wubu_spell_resolve_free(wubu_spell_resolve *r)
{
    if (!r) return;

    /* Release what the registry loaded itself.  Handles added by hand are the
     * caller's -- freeing them here would invalidate modules somebody else still
     * holds a pointer to. */
    for (int i = 0; i < r->count; i++) {
        if (r->owned_images[i]) {
            wubu_spellmod_close(r->mods[i]);
            free(r->owned_images[i]);
        }
    }

    free(r);
}

int wubu_spell_registry_add(wubu_spell_resolve *r, wubu_spellmod_t *m)
{
    if (!r || !m) return WUBU_SPELL_INVALID;
    if (r->count >= WUBU_SPELL_MAX_MODULES) return WUBU_SPELL_NOMEM;

    /* First registration of a name wins, so re-adding is harmless and the registry
     * cannot silently change which module a capability resolves to. */
    for (int i = 0; i < r->count; i++)
        if (same_module(r->mods[i], m))
            return WUBU_SPELL_OK;

    r->mods[r->count++] = m;
    return WUBU_SPELL_OK;
}

int wubu_spell_registry_add_owned(wubu_spell_resolve *r, wubu_spellmod_t *m,
                                  uint8_t *image)
{
    if (!r || !m || !image) return WUBU_SPELL_INVALID;
    if (r->count >= WUBU_SPELL_MAX_MODULES) return WUBU_SPELL_NOMEM;

    /* Same first-registration-wins rule as the plain add; a duplicate name must not
     * leave the registry holding an image nobody will ever release. */
    char mn[WUBU_SPELLMOD_NAME_MAX];
    if (mod_name(m, mn, sizeof(mn)) != 0) { free(image); return WUBU_SPELL_INVALID; }
    for (int i = 0; i < r->count; i++) {
        char cur[WUBU_SPELLMOD_NAME_MAX];
        if (mod_name(r->mods[i], cur, sizeof(cur)) != 0) continue;
        if (strcmp(cur, mn) == 0) { free(image); return WUBU_SPELL_OK; }
    }

    r->owned_images[r->count] = image;
    r->mods[r->count++] = m;
    return WUBU_SPELL_OK;
}

wubu_spellmod_t *wubu_spell_find_provider(wubu_spell_resolve *r,
                                          const char *capability)
{
    if (!r || !capability) return NULL;
    for (int i = 0; i < r->count; i++)
        if (wubu_spellmod_provides(r->mods[i], capability))
            return r->mods[i];
    return NULL;
}

/* Look up a registered module by the name recorded at parse time. */

static wubu_spellmod_t *find_by_name(const wubu_spell_resolve *r, const char *name)
{
    for (int i = 0; i < r->count; i++) {
        char mn[WUBU_SPELLMOD_NAME_MAX];
        if (mod_name(r->mods[i], mn, sizeof(mn)) != 0) continue;
        if (strcmp(mn, name) == 0) return r->mods[i];
    }
    return NULL;
}

/*
 * Depth-first post-order walk: every dependency is appended BEFORE its consumer,
 * which is exactly what a load order needs.
 *
 * A module may appear as a dependency many times, so the visit slot is found-or-
 * created rather than assumed fresh -- a second visit to an OPEN module is a cycle,
 * while a DONE one is already planned and costs nothing.
 */
static int plan_walk(wubu_spell_resolve *r,
                     wubu_spellmod_t *m,
                     visit_entry *visits, int *visit_count,
                     wubu_spellmod_t **order, int *count, int cap)
{
    int idx = -1;
    for (int i = 0; i < *visit_count; i++)
        if (visits[i].m == m) { idx = i; break; }

    if (idx < 0) {
        if (*visit_count >= WUBU_SPELL_MAX_MODULES) return WUBU_SPELL_NOMEM;
        idx = (*visit_count)++;
        visits[idx].m = m;
        visits[idx].state = VISIT_UNSEEN;
    }

    if (visits[idx].state == VISIT_DONE) return WUBU_SPELL_OK;
    if (visits[idx].state == VISIT_OPEN) return WUBU_SPELL_CYCLE;

    visits[idx].state = VISIT_OPEN;

    wubu_spellmod_info_t info;
    if (wubu_spellmod_info(m, &info) != 0)
        return WUBU_SPELL_INVALID;

    for (uint32_t n = 0; n < info.needs_count; n++) {
        const char *need = info.needs[n];
        if (!*need) continue;

        wubu_spellmod_t *p = wubu_spell_find_provider(r, need);
        if (!p)
            return WUBU_SPELL_UNSATISFIED;   /* nobody provides it */

        /* A module that provides its own need has no dependency edge to walk --
         * treating it as one would report a self-sufficient module as a cycle. */
        if (p == m) continue;

        int rc = plan_walk(r, p, visits, visit_count, order, count, cap);
        if (rc != WUBU_SPELL_OK) return rc;
    }

    visits[idx].state = VISIT_DONE;

    if (*count >= cap) return WUBU_SPELL_NOMEM;
    order[(*count)++] = m;
    return WUBU_SPELL_OK;
}

int wubu_spell_plan(wubu_spell_resolve *r, const char *root_name,
                    wubu_spellmod_t **order, int *count, int cap)
{
    if (!r || !root_name || !order || !count || cap <= 0) return WUBU_SPELL_INVALID;

    wubu_spellmod_t *root = find_by_name(r, root_name);
    if (!root) return WUBU_SPELL_INVALID;

    /* At most one visit slot per registered module -- the walk only ever reaches
     * modules in the registry -- so a plain stack array suffices and no shared
     * state is needed. */
    visit_entry visits[WUBU_SPELL_MAX_MODULES];
    int visit_count = 0;
    *count = 0;

    int rc = plan_walk(r, root, visits, &visit_count, order, count, cap);

    /* On any failure the plan must be treated as unusable, so do not leave a partial
     * order behind for a caller to mistake for a prefix that is safe to act on. */
    if (rc != WUBU_SPELL_OK) *count = 0;
    return rc;
}
