/*
 * wubu_pkg.c  --  WuBuOS Package Manager Implementation
 */

#include "wubu_pkg.h"
#include <string.h>
#include <stdio.h>

void pkg_init(PkgManager *mgr) {
    memset(mgr, 0, sizeof(*mgr));
}

void pkg_shutdown(PkgManager *mgr) {
    memset(mgr, 0, sizeof(*mgr));
}

int pkg_register(PkgManager *mgr, const char *name, const char *version,
                 const char *desc, uint64_t size) {
    if (!mgr || !name || mgr->n_entries >= PKG_MAX_REGISTRY) return -1;
    if (pkg_find(mgr, name)) return -1; /* already registered */
    PkgEntry *e = &mgr->entries[mgr->n_entries++];
    strncpy(e->name, name, PKG_MAX_NAME - 1);
    strncpy(e->version, version ? version : "0.1", PKG_MAX_VERSION - 1);
    strncpy(e->desc, desc ? desc : "", PKG_MAX_DESC - 1);
    e->size = size;
    e->state = PKG_STATE_AVAILABLE;
    e->ndeps = 0;
    return 0;
}

int pkg_unregister(PkgManager *mgr, const char *name) {
    if (!mgr || !name) return -1;
    for (int i = 0; i < mgr->n_entries; i++) {
        if (strcmp(mgr->entries[i].name, name) == 0) {
            for (int j = i; j < mgr->n_entries - 1; j++)
                mgr->entries[j] = mgr->entries[j + 1];
            mgr->n_entries--;
            return 0;
        }
    }
    return -1;
}

int pkg_count(PkgManager *mgr) {
    return mgr ? mgr->n_entries : 0;
}

PkgEntry *pkg_find(PkgManager *mgr, const char *name) {
    if (!mgr || !name) return NULL;
    for (int i = 0; i < mgr->n_entries; i++)
        if (strcmp(mgr->entries[i].name, name) == 0)
            return &mgr->entries[i];
    return NULL;
}

int pkg_add_dep(PkgManager *mgr, const char *name, const char *dep) {
    if (!mgr || !name || !dep) return -1;
    PkgEntry *e = pkg_find(mgr, name);
    if (!e || e->ndeps >= PKG_MAX_DEPS) return -1;
    strncpy(e->deps[e->ndeps], dep, PKG_MAX_NAME - 1);
    e->ndeps++;
    return 0;
}

int pkg_check_deps(PkgManager *mgr, const char *name) {
    if (!mgr || !name) return -1;
    PkgEntry *e = pkg_find(mgr, name);
    if (!e) return -1;
    for (int i = 0; i < e->ndeps; i++) {
        PkgEntry *d = pkg_find(mgr, e->deps[i]);
        if (!d || d->state != PKG_STATE_INSTALLED) return 0;
    }
    return 1;
}

int pkg_install(PkgManager *mgr, const char *name) {
    if (!mgr || !name) return -1;
    if (mgr->readonly) return -1;
    PkgEntry *e = pkg_find(mgr, name);
    if (!e) return -1;
    if (e->state == PKG_STATE_INSTALLED) return 0; /* already installed */
    e->state = PKG_STATE_INSTALLED;
    return 0;
}

int pkg_remove(PkgManager *mgr, const char *name) {
    if (!mgr || !name) return -1;
    if (mgr->readonly) return -1;
    PkgEntry *e = pkg_find(mgr, name);
    if (!e || e->state != PKG_STATE_INSTALLED) return -1;
    e->state = PKG_STATE_AVAILABLE;
    return 0;
}

int pkg_is_installed(PkgManager *mgr, const char *name) {
    PkgEntry *e = pkg_find(mgr, name);
    return e && e->state == PKG_STATE_INSTALLED;
}

int pkg_installed_count(PkgManager *mgr) {
    if (!mgr) return 0;
    int count = 0;
    for (int i = 0; i < mgr->n_entries; i++)
        if (mgr->entries[i].state == PKG_STATE_INSTALLED) count++;
    return count;
}

int pkg_add_repo(PkgManager *mgr, const char *name, const char *url) {
    if (!mgr || !name || mgr->n_repos >= PKG_MAX_REPO) return -1;
    PkgRepo *r = &mgr->repos[mgr->n_repos++];
    strncpy(r->name, name, PKG_MAX_NAME - 1);
    strncpy(r->url, url ? url : "", 255);
    r->enabled = 1;
    return 0;
}

int pkg_remove_repo(PkgManager *mgr, const char *name) {
    if (!mgr || !name) return -1;
    for (int i = 0; i < mgr->n_repos; i++) {
        if (strcmp(mgr->repos[i].name, name) == 0) {
            for (int j = i; j < mgr->n_repos - 1; j++)
                mgr->repos[j] = mgr->repos[j + 1];
            mgr->n_repos--;
            return 0;
        }
    }
    return -1;
}

int pkg_list_installed(PkgManager *mgr, PkgEntry *out, int max) {
    if (!mgr || !out) return 0;
    int count = 0;
    for (int i = 0; i < mgr->n_entries && count < max; i++)
        if (mgr->entries[i].state == PKG_STATE_INSTALLED)
            out[count++] = mgr->entries[i];
    return count;
}
