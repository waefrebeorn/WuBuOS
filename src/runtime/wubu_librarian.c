/*
 * wubu_librarian.c -- the INTEROP LIBRARIAN (Track D2).
 *
 * The Styx translator model says a foreign OS root is a file server you
 * attach. But "all OS apps run in our stack" needs more than per-ABI
 * translation (D1 did paths + syscalls): the systems have to FIND each
 * other and CALL across boundaries. That is the librarian.
 *
 * Three jobs:
 *   1. REGISTRY -- every participant (WuBu native, Windows PE, Unix, DOS)
 *      registers what it IS: its name, ABI, namespace root, and the
 *      capabilities it offers. `ls` of the whole interop surface.
 *   2. RESOLVER -- an app asks "who offers X?" or "where is app Y?" and the
 *      librarian answers with the participant's ABI + namespace path, so a
 *      call can be routed. Apps find each other by capability, not by
 *      hardcoded path.
 *   3. GROWING TRANSLATION TABLE -- when a call crosses an ABI boundary,
 *      the librarian records the mapping (from_abi, to_abi, op, how it was
 *      served). The FIRST crossing computes it (via wubu_abi_path +
 *      wubu_syscall_shim); every later crossing is a direct lookup. The
 *      library literally builds itself as it goes -- no giant hardcoded
 *      table, it accumulates what it has actually had to translate.
 *
 * Fail-closed: an unknown participant or an un-offered capability resolves
 * to "not found", never a guess. C11, fixed arrays, no malloc.
 */

#include "wubu_librarian.h"
#include "wubu_syscall_shim.h"
#include "wubu_abi_path.h"
#include <string.h>
#include <stdio.h>

/* -- the registry ---------------------------------------------------- */
static wubu_lib_entry_t g_reg[WUBU_LIB_MAX];
static int g_reg_n = 0;

/* -- the growing translation dictionary ------------------------------ */
static wubu_lib_xlate_t g_dict[WUBU_LIB_MAX_XLATE];
static int g_dict_n = 0;

int wubu_librarian_register(const char *name, wubu_abi_t abi,
                            const char *ns_root, const char *caps) {
    if (!name || !ns_root || g_reg_n >= WUBU_LIB_MAX) return -1;
    wubu_lib_entry_t *e = &g_reg[g_reg_n];
    snprintf(e->name, sizeof(e->name), "%s", name);
    snprintf(e->ns_root, sizeof(e->ns_root), "%s", ns_root);
    snprintf(e->caps, sizeof(e->caps), "%s", caps ? caps : "");
    e->abi = abi;
    e->active = 1;
    g_reg_n++;
    return 0;
}

/* Resolve a participant by exact name. Returns its entry or NULL. */
const wubu_lib_entry_t *wubu_librarian_find(const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < g_reg_n; i++)
        if (g_reg[i].active && strcmp(g_reg[i].name, name) == 0)
            return &g_reg[i];
    return NULL;
}

/* Resolve the FIRST participant that offers a capability (substring match
 * on its caps list). Returns its entry or NULL -- fail-closed. */
const wubu_lib_entry_t *wubu_librarian_who_offers(const char *cap) {
    if (!cap) return NULL;
    for (int i = 0; i < g_reg_n; i++)
        if (g_reg[i].active && strstr(g_reg[i].caps, cap) != NULL)
            return &g_reg[i];
    return NULL;
}

int wubu_librarian_count(void) { return g_reg_n; }

/* -- the growing translation table ----------------------------------- */

/* Look up a cached cross-ABI translation. Returns 1 + fills *out on a hit,
 * 0 on a miss (caller computes it and calls _record). */
int wubu_librarian_xlate_lookup(wubu_abi_t from, wubu_abi_t to,
                                wubu_shu_op_t op, wubu_lib_xlate_t *out) {
    for (int i = 0; i < g_dict_n; i++) {
        wubu_lib_xlate_t *d = &g_dict[i];
        if (d->from == from && d->to == to && d->op == op) {
            if (out) *out = *d;
            return 1;
        }
    }
    return 0;
}

/* Record a learned cross-ABI translation (the "building as it goes" step).
 * Returns 0 on success, -1 if the table is full or the entry already
 * exists (idempotent -- re-learning the same mapping is a no-op). */
int wubu_librarian_xlate_record(wubu_abi_t from, wubu_abi_t to,
                                wubu_shu_op_t op, const char *how) {
    wubu_lib_xlate_t probe;
    if (wubu_librarian_xlate_lookup(from, to, op, &probe)) return 0; /* have it */
    if (g_dict_n >= WUBU_LIB_MAX_XLATE || !how) return -1;
    wubu_lib_xlate_t *d = &g_dict[g_dict_n];
    d->from = from; d->to = to; d->op = op;
    snprintf(d->how, sizeof(d->how), "%s", how);
    g_dict_n++;
    return 0;
}

int wubu_librarian_xlate_count(void) { return g_dict_n; }

/* The full interop call: route a call from one participant to another.
 * Resolves the target, translates the syscall for the target ABI, and
 * records the cross-ABI mapping so it's a lookup next time. Fills
 * target_root (the namespace path to serve from) and served_op. Returns 0
 * on success, -1 if the target isn't registered or the op is refused. */
int wubu_librarian_route(const char *from_name, const char *to_name,
                         wubu_foreign_syscall_t sc, const char *path_arg,
                         char *target_root, size_t root_cap,
                         wubu_shu_op_t *served_op) {
    const wubu_lib_entry_t *tgt = wubu_librarian_find(to_name);
    if (!tgt) return -1;
    const wubu_lib_entry_t *src = wubu_librarian_find(from_name);
    wubu_abi_t from_abi = src ? src->abi : WUBU_ABI_UNIX;

    char tpath[512];
    wubu_shu_op_t op = wubu_syscall_shim_translate(tgt->abi, sc, path_arg,
                                                   tpath, sizeof tpath);
    if (op == WUBU_SHU_REFUSE) return -1;

    /* learn the crossing so next time it's a direct lookup */
    wubu_librarian_xlate_record(from_abi, tgt->abi, op,
                                wubu_syscall_shim_op_name(op));

    if (target_root && root_cap) {
        if (op == WUBU_SHU_FILE)
            snprintf(target_root, root_cap, "%s", tpath);
        else
            snprintf(target_root, root_cap, "%s", tgt->ns_root);
    }
    if (served_op) *served_op = op;
    return 0;
}
