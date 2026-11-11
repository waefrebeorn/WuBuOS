/*
 * wubu_pkg.h  --  WuBuOS Package Manager
 *
 * Flatpak-style package management:
 *   - Install/remove/list .wubu container packages
 *   - Dependency resolution (simple DAG)
 *   - Package registry with metadata
 *   - Repository sources
 */

#ifndef WUBU_PKG_H
#define WUBU_PKG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define PKG_MAX_NAME     64
#define PKG_MAX_VERSION  32
#define PKG_MAX_DEPS     16
#define PKG_MAX_REPO     8
#define PKG_MAX_REGISTRY 256
#define PKG_MAX_DESC     128

typedef enum {
    PKG_STATE_NONE     = 0,
    PKG_STATE_AVAILABLE = 1,
    PKG_STATE_INSTALLED = 2,
    PKG_STATE_BROKEN   = 3,
} PkgState;

typedef struct {
    char     name[PKG_MAX_NAME];
    char     version[PKG_MAX_VERSION];
    char     desc[PKG_MAX_DESC];
    PkgState state;
    char     deps[PKG_MAX_DEPS][PKG_MAX_NAME];
    int      ndeps;
    uint64_t size;
    char     repo[PKG_MAX_REPO];
} PkgEntry;

typedef struct {
    char     url[256];
    char     name[PKG_MAX_NAME];
    int      enabled;
} PkgRepo;

typedef struct {
    PkgEntry  entries[PKG_MAX_REGISTRY];
    int       n_entries;
    PkgRepo   repos[PKG_MAX_REPO];
    int       n_repos;
    int       readonly;
} PkgManager;

/* -- Lifecycle ---------------------------------------------- */
void pkg_init(PkgManager *mgr);
void pkg_shutdown(PkgManager *mgr);

/* -- Registry ----------------------------------------------- */
int  pkg_register(PkgManager *mgr, const char *name, const char *version,
                  const char *desc, uint64_t size);
int  pkg_unregister(PkgManager *mgr, const char *name);
int  pkg_count(PkgManager *mgr);
PkgEntry *pkg_find(PkgManager *mgr, const char *name);

/* -- Dependencies ------------------------------------------- */
int  pkg_add_dep(PkgManager *mgr, const char *name, const char *dep);
int  pkg_check_deps(PkgManager *mgr, const char *name);

/* -- Install/Remove ----------------------------------------- */
int  pkg_install(PkgManager *mgr, const char *name);
int  pkg_remove(PkgManager *mgr, const char *name);
int  pkg_is_installed(PkgManager *mgr, const char *name);
int  pkg_installed_count(PkgManager *mgr);

/* -- Repositories ------------------------------------------- */
int  pkg_add_repo(PkgManager *mgr, const char *name, const char *url);
int  pkg_remove_repo(PkgManager *mgr, const char *name);

/* -- Query -------------------------------------------------- */
int  pkg_list_installed(PkgManager *mgr, PkgEntry *out, int max);

/* -- Versions and constraints -------------------------------- */
/* Compare two version strings: -1, 0, +1.  Components compare NUMERICALLY, so
 * 1.10 > 1.9 and "1.0" == "1.0.0". */
int  pkg_version_cmp(const char *a, const char *b);

/* Does `version` satisfy `constraint`?  An empty constraint accepts all.
 * Supported: "", ">=X", "<=X", "=X", ">X", "<X", "X.*" and a bare "X" (=). */
int  pkg_satisfies(const char *version, const char *constraint);

/* -- Dependency resolution ----------------------------------- */
typedef enum {
    PKG_RESOLVE_OK = 0,
    PKG_RESOLVE_CYCLE,
    PKG_RESOLVE_MISSING,
    PKG_RESOLVE_NOT_FOUND,
    PKG_RESOLVE_CONFLICT,
    PKG_RESOLVE_BAD_ARGUMENT
} PkgResolveStatus;

const char *pkg_resolve_status_name(PkgResolveStatus s);

/* Resolve `target` into a POST-ORDER install list: every dependency before its
 * dependent, a diamond emitted once, the target last.  Returns a status; on a
 * non-OK status *out_n is 0. */
PkgResolveStatus pkg_resolve_order(PkgManager *mgr, const char *target,
                                   char out[][PKG_MAX_NAME], int max, int *out_n);

/* Which INSTALLED packages depend on `name`?  Used to refuse a removal that
 * would leave dependents broken. */
int pkg_dependents(PkgManager *mgr, const char *name,
                   char out[][PKG_MAX_NAME], int max);

#endif
