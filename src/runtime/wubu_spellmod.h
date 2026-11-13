/*
 * wubu_spellmod.h -- the .spell loadable-module container.
 *
 * WHY THIS EXISTS
 *   The house naming law says a Windows .dll is a WuBuOS `.spell`. But `.spell`
 *   already means something real in this tree: a TEXT MANIFEST describing a
 *   source spell (the per-spell manifests under src/spells, read by
 *   wubu_spellbook.c).  Giving
 *   the extension a second, unrelated meaning would be exactly the overloaded
 *   name the covenant forbids.
 *
 *   So this is not a competing format. It is the COMPILED FORM of the same
 *   thing: the manifest's identity fields (name, version, provides, needs) are
 *   embedded verbatim, followed by the loadable payload and an export table --
 *   the part a linker actually needs and the manifest never had.
 *
 *   A text `.spell` and a binary `.spell` are told apart by content, not by
 *   name, the same way a PE starts with "MZ" and a script starts with text:
 *   this format begins with "WUSPELL\0". Source manifests live under
 *   src/spells/**; compiled modules install elsewhere (~/.local/lib/wubu/
 *   spells/), so the two never collide on one filename.
 *
 * FAILURE POLICY
 *   Everything here fails closed. A bad magic, unknown version, truncated
 *   record, absurd count or export pointing outside the code blob returns NULL
 *   or -1 and leaves the caller's output untouched. A module that lies about
 *   its own layout must never be half-accepted, because the alternative is
 *   jumping into an address that does not exist.
 */
#ifndef WUBU_SPELLMOD_H
#define WUBU_SPELLMOD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WUBU_SPELLMOD_MAGIC   "WUSPELL"
#define WUBU_SPELLMOD_VERSION 1

#define WUBU_SPELLMOD_NAME_MAX 64
#define WUBU_SPELLMOD_STR_MAX  32
#define WUBU_SPELLMOD_LIST_MAX 32
#define WUBU_SPELLMOD_EXPORT_MAX 256

typedef struct {
    char     name[WUBU_SPELLMOD_NAME_MAX];
    char     version[WUBU_SPELLMOD_STR_MAX];
    char     provides[WUBU_SPELLMOD_LIST_MAX][WUBU_SPELLMOD_STR_MAX];
    uint32_t provides_count;
    char     needs[WUBU_SPELLMOD_LIST_MAX][WUBU_SPELLMOD_STR_MAX];
    uint32_t needs_count;
} wubu_spellmod_info_t;

/* Parse and validate a .spell module image.
 * Returns NULL on any structural problem; the buffer must outlive the handle
 * (nothing is copied, which keeps loading a large module free). */
typedef struct wubu_spellmod wubu_spellmod_t;
wubu_spellmod_t *wubu_spellmod_open(const uint8_t *data, size_t len);
void             wubu_spellmod_close(wubu_spellmod_t *m);

/* Identity from the embedded manifest. */
int wubu_spellmod_info(const wubu_spellmod_t *m, wubu_spellmod_info_t *out);

/* Resolve an export by name.
 * On success returns 0 and sets *offset/*size (size 0 means "unknown length").
 * Returns -1 if the module is NULL, the name is empty or no such export
 * exists -- and leaves the outputs untouched. */
int wubu_spellmod_export(const wubu_spellmod_t *m, const char *name,
                         uint32_t *offset, uint32_t *size);

/* The code blob this module carries, or NULL when it has none. */
const uint8_t *wubu_spellmod_code(const wubu_spellmod_t *m, uint32_t *len);

/* True when the module declares that it provides `capability`.
 * Used before loading to answer "does this spell give me what I need?" */
bool wubu_spellmod_provides(const wubu_spellmod_t *m, const char *capability);

#endif /* WUBU_SPELLMOD_H */
