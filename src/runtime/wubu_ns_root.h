/*
 * wubu_ns_root.h -- per-process /n root view derived from the capability set.
 *
 * WuBuOS has no ambient authority: what a process sees under /n is exactly
 * the set of namespace grants its capabilities give it (NAMESPACE #583).
 * This module is that view: a table of grants {source subtree, mount point
 * in /n, rights}, and the resolution rules over it.
 *
 * Relationship to the existing owners (deliberate, not duplicate):
 *   - wubu_cap.h owns the CAPABILITY MODEL (kinds, WUBU_RIGHT_* bitmap,
 *     tokens). Here rights are only *consumed*: a grant carries the
 *     WUBU_RIGHT_* mask the capability delegated.
 *   - wubu_ns_fs.c / wubu_ns_bridge.c own the ON-DISK /n tree (g_ns_root,
 *     ns_mkdir, /n/svc, /n/bottles). This module owns the PROCESS VIEW
 *     layered above it: which of those subtrees this process may touch.
 *   - wubu_ns_mount.c owns the SHARED bind/union mount table. A root view
 *     consults its own grants; mounts are how the system composes the
 *     sources the grants point at.
 *
 * Semantics:
 *   - lookup is longest-prefix, component-aligned ("/n/a" does not match
 *     "/n/ab"), and fails closed on any ungranted path.
 *   - fork() copies the grant table (capability inheritance, #601);
 *     exec() drops grants flagged EPHEMERAL and keeps the rest.
 *   - rebase() prepends a base to every grant's /n mount point -- the
 *     namespace unshare-equivalent (#602).
 *   - a CONTAINER-class view hides HOST_CP grants (#600). Sharing across
 *     containers happens ONLY by minting + adopting a share handle (#599):
 *     no ambient cross-view visibility exists.
 *
 * C11, opaque struct, no dynamic allocation except the object itself.
 */
#ifndef WUBU_NS_ROOT_H
#define WUBU_NS_ROOT_H

#include <stdint.h>
#include <stddef.h>

#define WUBU_NS_ROOT_MAX_GRANTS 32
#define WUBU_NS_ROOT_MAX_PATH  512
#define WUBU_NS_ROOT_MAX_NAME  64

/* View classes. A CONTAINER view masks host control-plane grants. */
enum {
    WUBU_NS_ROOT_HOST      = 0,
    WUBU_NS_ROOT_CONTAINER = 1
};

/* Grant flags. */
#define WUBU_NS_GRANT_HOST_CP    0x1u   /* host control-plane subtree (#600) */
#define WUBU_NS_GRANT_EPHEMERAL  0x2u   /* dropped on exec (#601) */

typedef struct wubu_ns_root wubu_ns_root_t;

/* A minted share handle: the ONLY way a subtree crosses views (#599). */
typedef struct {
    char     src[WUBU_NS_ROOT_MAX_PATH];
    uint64_t rights;
    uint64_t issuer_id;    /* the view that minted it */
    uint64_t magic;        /* guards against forged handles */
} wubu_ns_share_t;

/* Create a view. name is copied; class is WUBU_NS_ROOT_HOST/CONTAINER. */
wubu_ns_root_t *wubu_ns_root_create(const char *name, int root_class);
void             wubu_ns_root_free(wubu_ns_root_t *r);
const char      *wubu_ns_root_name(const wubu_ns_root_t *r);
int              wubu_ns_root_class(const wubu_ns_root_t *r);

/* Grant: bind source subtree `src` at namespace path `at` with a rights
 * mask (WUBU_RIGHT_* from wubu_cap.h). Duplicate `at` is rejected (-2);
 * full table -3; bad args -1. */
int  wubu_ns_root_grant(wubu_ns_root_t *r, const char *src,
                        const char *at, uint64_t rights, unsigned flags);
/* Revoke by mount point. 0 removed, -1 no such grant. */
int  wubu_ns_root_revoke(wubu_ns_root_t *r, const char *at);
/* Number of live grants in the view. */
int  wubu_ns_root_grant_count(const wubu_ns_root_t *r);

/* Longest-prefix lookup through the view. 0 = visible: src_out receives
 * the underlying source path, rights_out the grant's rights mask.
 * Fail-closed: -1 (ungranted, hidden by container class, or bad args). */
int  wubu_ns_root_lookup(const wubu_ns_root_t *r, const char *path,
                         uint64_t *rights_out, char src_out[WUBU_NS_ROOT_MAX_PATH]);

/* Rights check on a path: 1 = the view grants `rights`, 0 = denied. */
int  wubu_ns_root_allows(const wubu_ns_root_t *r, const char *path,
                         uint64_t rights);

/* fork(): the child inherits a copy of the grant table (#601). */
wubu_ns_root_t *wubu_ns_root_fork(const wubu_ns_root_t *parent);
/* exec(): drop EPHEMERAL grants, keep the persistent set (#601).
 * Returns the number of grants dropped (0 = nothing was ephemeral). */
int  wubu_ns_root_exec(wubu_ns_root_t *r);
/* rebase(): prepend new_base to every grant's mount point (#602). */
int  wubu_ns_root_rebase(wubu_ns_root_t *r, const char *new_base);

/* Cross-container sharing (#599): mint a handle for the grant at `at`
 * (NULL if there is no such grant), then adopt it into another view at
 * `at2`. Adoption is the ONLY cross-view transfer; the handle's rights
 * cap what the adopting view receives. */
wubu_ns_share_t *wubu_ns_root_share(const wubu_ns_root_t *r, const char *at);
int  wubu_ns_root_adopt(wubu_ns_root_t *r, const wubu_ns_share_t *sh,
                        const char *at);
void wubu_ns_share_free(wubu_ns_share_t *sh);

#endif /* WUBU_NS_ROOT_H */
