/*
 * wubu_spell_resolve.h -- dependency resolution over the .spell module container.
 *
 * WHY THIS EXISTS
 *   wubu_spellmod.h declares a complete dependency MODEL: every module carries a
 *   capped provides[]/needs[] list, and wubu_spellmod_provides() answers whether it
 *   satisfies a given capability. Nothing consumed it -- there was no loader, no
 *   search path, and no act of resolving one module against another. So the format
 *   looked finished while the feature did not exist at all.
 *
 *   This module is that missing act. It takes already-opened modules (whoever
 *   supplies them -- a directory scan, a package install, an in-memory build) and
 *   answers the questions a loader needs answered before it loads anything:
 *
 *     - which registered module provides capability X?
 *     - given a root module, in what order must modules load so that every `need`
 *       is already present when its consumer loads?
 *     - is the graph unsatisfiable (a need nobody provides)?
 *     - does it contain a cycle (A needs B needs A)?
 *
 * WHAT IT DOES NOT DO
 *   It does not touch the filesystem, allocate or copy module bytes, or interpret
 *   code. It is pure graph work over borrowed handles, which is why it links with
 *   nothing beyond wubu_spellmod.h.
 *
 * OWNERSHIP
 *   The registry BORROWS modules: handles added with wubu_spell_registry_add() must
 *   outlive the registry. Freeing the registry frees only its own bookkeeping.
 *   That mirrors how wubu_spellmod_open() borrows the image buffer rather than
 *   copying it -- the same lifetime rules apply one layer up.
 */
#ifndef WUBU_SPELL_RESOLVE_H
#define WUBU_SPELL_RESOLVE_H

#include <stdbool.h>
#include "../runtime/wubu_spellmod.h"

#define WUBU_SPELL_MAX_MODULES 64

/* Outcome codes. Kept as plain ints so callers can switch without pulling in
 * anything extra; -1/-2 are failures, everything else is not. */
#define WUBU_SPELL_OK            0
#define WUBU_SPELL_UNSATISFIED  -1   /* a needed capability has no provider */
#define WUBU_SPELL_CYCLE        -2   /* the dependency graph is cyclic */
#define WUBU_SPELL_NOMEM        -3
#define WUBU_SPELL_INVALID      -4   /* NULL argument, unknown root */

typedef struct wubu_spell_resolve wubu_spell_resolve;

/* Empty registry, or NULL on allocation failure. */
wubu_spell_resolve *wubu_spell_resolve_new(void);

/* Frees only the registry's bookkeeping -- the borrowed modules are untouched. */
void wubu_spell_resolve_free(wubu_spell_resolve *r);

/* Register an opened module. Returns 0, WUBU_SPELL_NOMEM when full, or
 * WUBU_SPELL_INVALID for a NULL module. Duplicate names replace nothing: the first
 * registration of a name wins, which makes repeat calls idempotent. */
int wubu_spell_registry_add(wubu_spell_resolve *r, wubu_spellmod_t *m);

/* Find the registered module that provides `capability`, or NULL if none does.
 * First match wins when several do; ties are not an error here because the plan
 * walk reports ambiguity only where it changes an outcome. */
/* Same as wubu_spell_registry_add(), but the registry TAKES OWNERSHIP of the image
 * buffer: wubu_spell_resolve_free() will close the module and release the bytes.
 *
 * Only for images the registry loaded itself -- a directory scan cannot hand its
 * buffers to anyone else, because open() borrows them.  Passing a buffer you still
 * hold a pointer to elsewhere leads to a double free; when in doubt, use the plain
 * add() and keep the lifetime question explicit at your call site. */
int wubu_spell_registry_add_owned(wubu_spell_resolve *r, wubu_spellmod_t *m,
                                  uint8_t *image);

wubu_spellmod_t *wubu_spell_find_provider(wubu_spell_resolve *r,
                                          const char *capability);

/* Build a load plan for `root_name`, dependencies FIRST.
 *
 * Returns WUBU_SPELL_OK with the ordered handles in `order` and the count in
 * `*count`; WUBU_SPELL_UNSATISFIED when some need has no provider anywhere in the
 * registry; WUBU_SPELL_CYCLE when the graph contains one; WUBU_SPELL_INVALID when
 * the root is not registered; WUBU_SPELL_NOMEM when the plan exceeds `cap`.
 *
 * On failure `order` may be partially filled and `*count` reflects how far the walk
 * got -- callers must treat any non-OK return as "load nothing", never as a prefix
 * to act on. A half-loaded module set is worse than refusing.
 */
int wubu_spell_plan(wubu_spell_resolve *r, const char *root_name,
                    wubu_spellmod_t **order, int *count, int cap);

/* Default module directory: $WUBU_SPELL_DIR when set, else
 * "$HOME/.local/share/wubu/spells".  Returns 0 and writes a NUL-terminated path, or
 * -1 when neither yields anything usable.  The path is not created here -- scanning
 * a directory that does not exist yet is a normal first-boot condition, not an error
 * to hide, so callers see the absence from wubu_spell_scan() instead. */
int wubu_spell_default_dir(char *out, size_t outsz);

/* Scan `dir` for *.spell files, open each and register it.
 *
 * Returns the number registered, or -1 when the directory cannot be opened at all.
 * Individual bad files are SKIPPED, never fatal: one corrupt or unrelated file must
 * not stop the rest of the modules being available.  Files larger than
 * WUBU_SPELL_FILE_MAX are skipped for the same reason -- a bounded read keeps a
 * stray multi-gigabyte file from being loaded whole.
 *
 * This is discovery only; it answers "what modules exist", not "what should load".
 * Ordering still comes from wubu_spell_plan().
 */
#define WUBU_SPELL_FILE_MAX (1u << 20)
int wubu_spell_scan(wubu_spell_resolve *r, const char *dir);

#endif /* WUBU_SPELL_RESOLVE_H */
