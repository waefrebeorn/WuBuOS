/*
 * wubu_ns_pkg.c -- WuBuOS Namespace Bridge: packages as a 9P control
 * plane (rip off pacman/Chaotic-AUR "prebuilt binaries, no local compile"
 * vibe, do it better through /n).
 *
 * Exposes a package registry as /n/pkg/{install,remove,list,repos,addrepo}.
 * install/remove/addrepo are action files whose writes dispatch to the pure
 * fns below; list/repos are live views of pkg_list_installed/pkg repos.
 * Reuses the existing wubu_pkg_* API -- no duplicated package logic.
 *
 * C11, opaque structs, minimal includes. Uses g_ns_root/ns_mkdir/ns_write
 * from wubu_ns_fs.c via wubu_ns_bridge_internal.h.
 */

#include "wubu_ns_bridge.h"
#include "wubu_ns_bridge_internal.h"
#include "wubu_pkg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int pkg_state_str(PkgState s, char *out, size_t n) {
    switch (s) {
        case PKG_STATE_INSTALLED:  return snprintf(out, n, "installed");
        case PKG_STATE_AVAILABLE:  return snprintf(out, n, "available");
        case PKG_STATE_BROKEN:     return snprintf(out, n, "broken");
        default:                   return snprintf(out, n, "none");
    }
}

/* Rewrite /n/pkg/list from the currently installed packages. */
static int pkg_refresh_list(PkgManager *mgr) {
    PkgEntry out[PKG_MAX_REGISTRY];
    int n = pkg_list_installed(mgr, out, PKG_MAX_REGISTRY);
    char buf[8192];
    size_t off = 0;
    buf[0] = '\0';
    for (int i = 0; i < n; i++) {
        char st[32];
        pkg_state_str(out[i].state, st, sizeof(st));
        int w = snprintf(buf + off, sizeof(buf) - off,
                         "%s\t%s\t%s\t%llu\n",
                         out[i].name, out[i].version, st,
                         (unsigned long long)out[i].size);
        if (w < 0 || (size_t)w >= sizeof(buf) - off) { buf[off] = '\0'; break; }
        off += (size_t)w;
    }
    char sub[4096];
    snprintf(sub, sizeof(sub), "pkg/list");
    return ns_write(sub, buf);
}

/* Rewrite /n/pkg/repos from the enabled repositories (Chaotic-AUR vibe). */
static int pkg_refresh_repos(PkgManager *mgr) {
    char buf[4096];
    size_t off = 0;
    buf[0] = '\0';
    for (int i = 0; i < mgr->n_repos; i++) {
        if (!mgr->repos[i].enabled) continue;
        int w = snprintf(buf + off, sizeof(buf) - off, "%s\t%s\n",
                         mgr->repos[i].name, mgr->repos[i].url);
        if (w < 0 || (size_t)w >= sizeof(buf) - off) { buf[off] = '\0'; break; }
        off += (size_t)w;
    }
    char sub[4096];
    snprintf(sub, sizeof(sub), "pkg/repos");
    return ns_write(sub, buf);
}

int wubu_ns_publish_pkg(PkgManager *mgr) {
    if (!g_ns_root || !mgr) return -1;
    if (ns_mkdir("pkg") != 0) return -1;

    if (pkg_refresh_list(mgr) != 0) return -1;
    if (pkg_refresh_repos(mgr) != 0) return -1;

    char sub[4096];
    snprintf(sub, sizeof(sub), "pkg/install");
    if (ns_write(sub, "# write a package name to install it\n") != 0) return -1;
    snprintf(sub, sizeof(sub), "pkg/remove");
    if (ns_write(sub, "# write a package name to remove it\n") != 0) return -1;
    snprintf(sub, sizeof(sub), "pkg/addrepo");
    if (ns_write(sub, "# write 'name url' to enable a repository\n") != 0) return -1;
    return 0;
}

int wubu_ns_pkg_install(PkgManager *mgr, const char *name) {
    if (!mgr || !name) return -1;
    int rc = pkg_install(mgr, name);
    if (rc == 0) pkg_refresh_list(mgr);   /* keep /n/pkg/list live */
    return rc;
}

int wubu_ns_pkg_remove(PkgManager *mgr, const char *name) {
    if (!mgr || !name) return -1;
    int rc = pkg_remove(mgr, name);
    if (rc == 0) pkg_refresh_list(mgr);
    return rc;
}

int wubu_ns_pkg_add_repo(PkgManager *mgr, const char *name, const char *url) {
    if (!mgr || !name) return -1;
    int rc = pkg_add_repo(mgr, name, url ? url : "");
    if (rc == 0) pkg_refresh_repos(mgr);  /* keep /n/pkg/repos live */
    return rc;
}
