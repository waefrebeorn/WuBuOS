/*
 * wubu_librarian.h -- the INTEROP LIBRARIAN (Track D2).
 *
 * Makes all the systems find each other and call across boundaries. Three
 * jobs: a REGISTRY (every participant registers its name/ABI/root/caps), a
 * RESOLVER (find by name or by offered capability), and a GROWING
 * TRANSLATION TABLE (each cross-ABI call is recorded so it's a lookup next
 * time -- the library builds itself as it goes). Fail-closed: unknown
 * participant or un-offered capability resolves to not-found, never a guess.
 */
#ifndef WUBU_LIBRARIAN_H
#define WUBU_LIBRARIAN_H

#include "wubu_abi_path.h"       /* wubu_abi_t */
#include "wubu_syscall_shim.h"   /* wubu_shu_op_t, wubu_foreign_syscall_t */

#define WUBU_LIB_MAX        64
#define WUBU_LIB_MAX_XLATE  256

/* A registered participant in the interop surface. */
typedef struct {
    char         name[64];
    wubu_abi_t   abi;
    char         ns_root[256];   /* its namespace root to serve from */
    char         caps[256];      /* space-separated capabilities it offers */
    int          active;
} wubu_lib_entry_t;

/* A learned cross-ABI translation. */
typedef struct {
    wubu_abi_t     from, to;
    wubu_shu_op_t  op;
    char           how[128];     /* how it was served */
} wubu_lib_xlate_t;

/* Registry: register a participant. Returns 0 on success, -1 if full. */
int wubu_librarian_register(const char *name, wubu_abi_t abi,
                            const char *ns_root, const char *caps);

/* Resolve a participant by exact name (NULL if not found). */
const wubu_lib_entry_t *wubu_librarian_find(const char *name);

/* Resolve the first participant offering a capability (NULL if none). */
const wubu_lib_entry_t *wubu_librarian_who_offers(const char *cap);

/* Number of registered participants. */
int wubu_librarian_count(void);

/* Translation table: look up a cached cross-ABI mapping (1 + fills *out on
 * a hit, 0 on a miss). */
int wubu_librarian_xlate_lookup(wubu_abi_t from, wubu_abi_t to,
                                wubu_shu_op_t op, wubu_lib_xlate_t *out);

/* Record a learned mapping (idempotent). Returns 0, or -1 if full. */
int wubu_librarian_xlate_record(wubu_abi_t from, wubu_abi_t to,
                                wubu_shu_op_t op, const char *how);

/* Number of learned translations. */
int wubu_librarian_xlate_count(void);

/* The full interop call: route a call from one participant to another.
 * Resolves the target, translates the syscall for its ABI, records the
 * crossing, and fills target_root (namespace path to serve from) +
 * served_op. Returns 0 on success, -1 if the target isn't registered or
 * the op is refused. */
int wubu_librarian_route(const char *from_name, const char *to_name,
                         wubu_foreign_syscall_t sc, const char *path_arg,
                         char *target_root, size_t root_cap,
                         wubu_shu_op_t *served_op);

#endif /* WUBU_LIBRARIAN_H */
