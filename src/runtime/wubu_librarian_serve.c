/*
 * wubu_librarian_serve.c -- the librarian's LIVE interop entry point
 * (Track D2, wired).
 *
 * The librarian (registry + resolver + growing translation table) is now
 * the single front door for cross-system calls. This module is the thin
 * entry point any foreign-app support code (wubu_exec backends, a shim, an
 * app) calls to turn "participant A wants to do OP on path P against
 * participant B" into a concrete namespace path + WuBu operation, routing
 * through the registry and recording the crossing.
 *
 * It also carries the DEFAULT WuBu participants so the librarian is never
 * empty on a live system: the native shell, the KV store, the model
 * catalog, and the companion all self-register at init. A foreign app that
 * attaches later just registers itself and is immediately reachable.
 *
 * C11, minimal includes.
 */

#include "wubu_librarian_serve.h"
#include "wubu_librarian.h"
#include <string.h>
#include <stdio.h>

static int g_served = 0;

/* Register the default WuBu participants (idempotent). */
int wubu_librarian_serve_init(void) {
    if (g_served) return 0;
    wubu_librarian_register("shell",   WUBU_ABI_UNIX, "/n",            "ui launch apps");
    wubu_librarian_register("kv",      WUBU_ABI_UNIX, "/n/kv",         "store kv latent");
    wubu_librarian_register("models",  WUBU_ABI_UNIX, "/n/models",     "store model tensors");
    wubu_librarian_register("bonzi",   WUBU_ABI_UNIX, "/n/bonzi",      "companion memory");
    wubu_librarian_register("experts", WUBU_ABI_UNIX, "/n/experts",    "moe slot-bank");
    wubu_librarian_register("hive",    WUBU_ABI_UNIX, "/n/hive",       "diagnostics");
    g_served = 1;
    return 0;
}

/* The live interop call: from participant `from`, perform foreign syscall
 * `sc` on `path_arg` against participant `to`. Routes through the
 * librarian (resolve + translate + record), fills out_path (namespace path
 * to serve from) + *op. Returns 0 on success, -1 if refused. */
int wubu_librarian_serve(const char *from, const char *to,
                         wubu_foreign_syscall_t sc, const char *path_arg,
                         char *out_path, size_t cap, wubu_shu_op_t *op) {
    if (!g_served) wubu_librarian_serve_init();
    return wubu_librarian_route(from, to, sc, path_arg, out_path, cap, op);
}

/* Resolve a capability to its offering participant's namespace root (the
 * "who offers X, and where" query). Returns 0 + fills root on success, -1
 * if nobody offers it. */
int wubu_librarian_serve_who(const char *cap, char *root, size_t cap_sz) {
    if (!g_served) wubu_librarian_serve_init();
    const wubu_lib_entry_t *e = wubu_librarian_who_offers(cap);
    if (!e) return -1;
    if (root && cap_sz) snprintf(root, cap_sz, "%s", e->ns_root);
    return 0;
}
