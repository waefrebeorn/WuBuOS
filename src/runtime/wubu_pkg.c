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
    /* Refuse to remove a package that an INSTALLED package still depends on.
     * The old code deleted it regardless, leaving dependents broken -- and a
     * broken dependent is worse than a refused removal, because the user is
     * told what to remove first. */
    for (int i = 0; i < mgr->n_entries; i++) {
        PkgEntry *o = &mgr->entries[i];
        if (o->state != PKG_STATE_INSTALLED) continue;
        for (int d = 0; d < o->ndeps; d++) {
            if (strcmp(o->deps[d], name) == 0) return -1;
        }
    }
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

/* -- versions and constraints -------------------------------------------- */

/* Compare one dot-separated component: numeric components numerically, and a
 * numeric component sorts BEFORE a non-numeric one so "1.0" < "1.0a". */
static int pkg_cmp_component(const char **pa, const char **pb)
{
    const char *a = *pa, *b = *pb;
    /* An EXHAUSTED side is the component "0": "1.0" == "1.0.0".  Treating the
     * end of the string as "the shorter version is less" makes 1.0 < 1.0.0,
     * which is wrong -- a missing component is zero, not absent. */
    bool a_empty = (*a == '\0');
    bool b_empty = (*b == '\0');
    if (a_empty && b_empty) return 0;

    bool a_num = a_empty || (*a >= '0' && *a <= '9');
    bool b_num = b_empty || (*b >= '0' && *b <= '9');

    if (a_num && b_num) {
        unsigned long va = a_empty ? 0u : strtoul(a, (char **)&a, 10);
        unsigned long vb = b_empty ? 0u : strtoul(b, (char **)&b, 10);
        *pa = a; *pb = b;
        if (va < vb) return -1;
        if (va > vb) return 1;
        return 0;
    }
    if (a_num != b_num) return a_num ? -1 : 1;

    const char *ae = strchr(a, '.');
    const char *be = strchr(b, '.');
    size_t al = ae ? (size_t)(ae - a) : strlen(a);
    size_t bl = be ? (size_t)(be - b) : strlen(b);
    size_t m = al < bl ? al : bl;
    int c = strncmp(a, b, m);
    *pa = ae ? ae : a + al;
    *pb = be ? be : b + bl;
    if (c != 0) return c < 0 ? -1 : 1;
    if (al < bl) return -1;
    if (al > bl) return 1;
    return 0;
}

int pkg_version_cmp(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a || *b) {
        while (*a == '.') a++;
        while (*b == '.') b++;
        if (!*a && !*b) break;
        /* do NOT bail out here: the exhausted side is the component "0", and
         * the remaining components still decide (1.0 == 1.0.0, 1.0 < 1.0.1). */
        const char *pa = a, *pb = b;
        int c = pkg_cmp_component(&pa, &pb);
        if (c != 0) return c;
        a = pa; b = pb;
    }
    return 0;
}

static int pkg_sat_simple(const char *v, const char *want, int mode)
{
    int c = pkg_version_cmp(v, want);
    switch (mode) {
    case 0: return c == 0;     /* =  */
    case 1: return c >= 0;     /* >= */
    case 2: return c <= 0;     /* <= */
    case 3: return c > 0;      /* >  */
    case 4: return c < 0;      /* <  */
    default: return 0;
    }
}

int pkg_satisfies(const char *version, const char *constraint)
{
    if (!version) return 0;
    if (!constraint || constraint[0] == '\0') return 1;

    size_t cl = strlen(constraint);
    if (cl >= 2 && constraint[cl - 1] == '*' && constraint[cl - 2] == '.')
        return strncmp(version, constraint, cl - 2) == 0;    /* "1.*" */

    if (constraint[0] == '>') {
        if (constraint[1] == '=') return pkg_sat_simple(version, constraint + 2, 1);
        return pkg_sat_simple(version, constraint + 1, 3);
    }
    if (constraint[0] == '<') {
        if (constraint[1] == '=') return pkg_sat_simple(version, constraint + 2, 2);
        return pkg_sat_simple(version, constraint + 1, 4);
    }
    if (constraint[0] == '=' && constraint[1] == '=')
        return pkg_sat_simple(version, constraint + 2, 0);
    if (constraint[0] == '=')
        return pkg_sat_simple(version, constraint + 1, 0);
    return pkg_sat_simple(version, constraint, 0);           /* bare version */
}

/* -- resolution ---------------------------------------------------------- */

const char *pkg_resolve_status_name(PkgResolveStatus s)
{
    switch (s) {
    case PKG_RESOLVE_OK:           return "ok";
    case PKG_RESOLVE_CYCLE:        return "cycle";
    case PKG_RESOLVE_MISSING:      return "missing dependency";
    case PKG_RESOLVE_NOT_FOUND:    return "package not found";
    case PKG_RESOLVE_CONFLICT:     return "version conflict";
    case PKG_RESOLVE_BAD_ARGUMENT: return "bad argument";
    default:                       return "unknown";
    }
}

int pkg_dependents(PkgManager *mgr, const char *name,
                   char out[][PKG_MAX_NAME], int max)
{
    if (!mgr || !name) return 0;
    int n = 0;
    for (int i = 0; i < mgr->n_entries && n < max; i++) {
        PkgEntry *o = &mgr->entries[i];
        if (o->state != PKG_STATE_INSTALLED) continue;
        for (int d = 0; d < o->ndeps; d++) {
            if (strcmp(o->deps[d], name) == 0) {
                strncpy(out[n], o->name, PKG_MAX_NAME - 1);
                out[n][PKG_MAX_NAME - 1] = '\0';
                n++;
                break;
            }
        }
    }
    return n;
}

typedef struct {
    PkgManager *mgr;
    char (*out)[PKG_MAX_NAME];
    int   max;
    int   written;
    uint8_t *mark;          /* 0 = unvisited, 1 = on the stack, 2 = done */
    PkgResolveStatus status;
} ResolveCtx;

static void pkg_dfs(ResolveCtx *c, PkgEntry *p)
{
    if (c->status != PKG_RESOLVE_OK) return;

    int idx = (int)(p - c->mgr->entries);
    if (c->mark[idx] == 1) { c->status = PKG_RESOLVE_CYCLE; return; }
    if (c->mark[idx] == 2) return;                 /* already emitted (a diamond) */
    c->mark[idx] = 1;

    for (int d = 0; d < p->ndeps; d++) {
        PkgEntry *dep = pkg_find(c->mgr, p->deps[d]);
        if (!dep) { c->status = PKG_RESOLVE_MISSING; return; }
        pkg_dfs(c, dep);
        if (c->status != PKG_RESOLVE_OK) return;
    }

    c->mark[idx] = 2;
    if (c->written >= c->max) { c->status = PKG_RESOLVE_BAD_ARGUMENT; return; }
    strncpy(c->out[c->written], p->name, PKG_MAX_NAME - 1);
    c->out[c->written][PKG_MAX_NAME - 1] = '\0';
    c->written++;
}

PkgResolveStatus pkg_resolve_order(PkgManager *mgr, const char *target,
                                   char out[][PKG_MAX_NAME], int max, int *out_n)
{
    if (out_n) *out_n = 0;
    if (!mgr || !target || !out || max <= 0) return PKG_RESOLVE_BAD_ARGUMENT;
    PkgEntry *root = pkg_find(mgr, target);
    if (!root) return PKG_RESOLVE_NOT_FOUND;

    uint8_t mark[PKG_MAX_REGISTRY];
    memset(mark, 0, sizeof(mark));

    ResolveCtx c;
    memset(&c, 0, sizeof(c));
    c.mgr = mgr; c.out = out; c.max = max; c.mark = mark;
    c.status = PKG_RESOLVE_OK;

    pkg_dfs(&c, root);
    if (c.status != PKG_RESOLVE_OK) return c.status;
    if (out_n) *out_n = c.written;
    return PKG_RESOLVE_OK;
}
