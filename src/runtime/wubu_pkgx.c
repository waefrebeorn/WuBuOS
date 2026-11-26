/*
 * wubu_pkgx.c -- see the header. Registration validates; enumeration sorts; resolution accepts either the
 * name or the family name, because a launch path in the wild has both.
 */
#include "wubu_pkgx.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

const char *wubu_pkgx_default_root(void)
{
    const char *env = getenv("WUBU_PKGS");
    if (env && *env) return env;
    static char root[WUBU_PKGX_ROOT_MAX];
    const char *home = getenv("HOME");
    snprintf(root, sizeof root, "%s/.local/share/wubu/packages", (home && *home) ? home : "/root");
    return root;
}

unsigned wubu_pkgx_family_hash(const char *publisher)
{
    /* FNV-1a, 32-bit. Chosen because it is short, stable, dependency-free and documented here -- NOT
     * because it matches Microsoft, whose hash is theirs. */
    unsigned h = 2166136261u;
    if (publisher)
        for (const unsigned char *p = (const unsigned char *)publisher; *p; p++)
            h = (h ^ *p) * 16777619u;
    return h;
}

int wubu_pkgx_family_name(const char *name, const char *publisher, char *out, size_t cap)
{
    if (!name || !*name || !out || cap == 0) return -1;
    unsigned h = wubu_pkgx_family_hash(publisher);
    snprintf(out, cap, "%s_%08x", name, h);
    return 0;
}

static char *trim(char *s)
{
    while (*s == ' ' || *s == '\t') s++;
    size_t n = strlen(s);
    while (n && (s[n - 1] == '\n' || s[n - 1] == '\r' || s[n - 1] == ' ' || s[n - 1] == '\t')) s[--n] = 0;
    return s;
}

/* An exec entry that is absolute or escapes the package directory is refused: a packaged app must not be
 * able to name a binary outside its own payload. */
static int exec_is_safe(const char *e)
{
    if (!e || !*e) return 0;
    if (e[0] == '/') return 0;
    if (strstr(e, "..")) return 0;
    return 1;
}

int wubu_pkgx_register(const char *dir, WubuPkgApp *out)
{
    if (!dir || !*dir) return -1;
    char conf[WUBU_PKGX_ROOT_MAX + 32];
    snprintf(conf, sizeof conf, "%s/package.conf", dir);
    FILE *f = fopen(conf, "r");
    if (!f) return 2;

    char name[WUBU_PKGX_NAME_MAX] = "", version[WUBU_PKGX_VER_MAX] = "1.0.0";
    char publisher[128] = "", exec[WUBU_PKGX_EXEC_MAX] = "";
    char line[512];
    while (fgets(line, sizeof line, f)) {
        char *s = trim(line);
        if (!*s || *s == '#') continue;
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = 0;
        char *k = trim(s), *v = trim(eq + 1);
        if (!strcmp(k, "name")) snprintf(name, sizeof name, "%s", v);
        else if (!strcmp(k, "version")) snprintf(version, sizeof version, "%s", v);
        else if (!strcmp(k, "publisher")) snprintf(publisher, sizeof publisher, "%s", v);
        else if (!strcmp(k, "exec")) snprintf(exec, sizeof exec, "%s", v);
    }
    fclose(f);

    if (!*name || !exec_is_safe(exec)) return 2;

    WubuPkgApp a;
    memset(&a, 0, sizeof a);
    snprintf(a.name, sizeof a.name, "%s", name);
    snprintf(a.version, sizeof a.version, "%s", version);
    snprintf(a.exec, sizeof a.exec, "%s", exec);
    snprintf(a.root, sizeof a.root, "%s", dir);
    wubu_pkgx_family_name(name, publisher, a.family, sizeof a.family);

    /* the declared entry point must actually be there, and be executable */
    char full[WUBU_PKGX_ROOT_MAX + WUBU_PKGX_EXEC_MAX + 2];
    snprintf(full, sizeof full, "%s/%s", a.root, a.exec);
    struct stat st;
    if (stat(full, &st) != 0 || !S_ISREG(st.st_mode) || !(st.st_mode & S_IXUSR)) return 2;

    if (out) *out = a;
    return 0;
}

static int cmp_name(const void *x, const void *y)
{
    return strcmp(((const WubuPkgApp *)x)->name, ((const WubuPkgApp *)y)->name);
}

int wubu_pkgx_list(WubuPkgRegistry *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof *out);
    DIR *d = opendir(wubu_pkgx_default_root());
    if (!d) return 0;                       /* no packages installed: empty, not an error */
    struct dirent *e;
    while ((e = readdir(d)) && out->count < WUBU_PKGX_MAX) {
        if (e->d_name[0] == '.') continue;
        char dir[WUBU_PKGX_ROOT_MAX + 128];
        snprintf(dir, sizeof dir, "%s/%s", wubu_pkgx_default_root(), e->d_name);
        WubuPkgApp a;
        if (wubu_pkgx_register(dir, &a) == 0) out->a[out->count++] = a;
    }
    closedir(d);
    qsort(out->a, (size_t)out->count, sizeof out->a[0], cmp_name);
    return out->count;
}

int wubu_pkgx_resolve(const char *id, WubuPkgApp *out)
{
    if (!id || !out) return -1;
    WubuPkgRegistry r;
    wubu_pkgx_list(&r);
    for (int i = 0; i < r.count; i++)
        if (!strcmp(r.a[i].name, id) || !strcmp(r.a[i].family, id)) { *out = r.a[i]; return 0; }
    return 1;
}

static int rm_tree(const char *path)
{
    /* one level of files, which is what a payload is here; refuses to follow a symlinked dir */
    DIR *d = opendir(path);
    if (!d) return -1;
    struct dirent *e;
    while ((e = readdir(d))) {
        if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
        char p[1024];
        snprintf(p, sizeof p, "%s/%s", path, e->d_name);
        struct stat st;
        if (lstat(p, &st) == 0 && S_ISDIR(st.st_mode)) rm_tree(p);
        else unlink(p);
    }
    closedir(d);
    return rmdir(path);
}

int wubu_pkgx_unregister(const char *name)
{
    if (!name || !*name) return -1;
    if (strchr(name, '/') || strstr(name, "..")) return 1;   /* a name is a name, not a path */
    char dir[WUBU_PKGX_ROOT_MAX + 128];
    snprintf(dir, sizeof dir, "%s/%s", wubu_pkgx_default_root(), name);
    struct stat st;
    if (stat(dir, &st) != 0 || !S_ISDIR(st.st_mode)) return 1;
    return rm_tree(dir) == 0 ? 0 : 1;
}

int wubu_pkgx_exec_path(const WubuPkgApp *p, char *out, size_t cap)
{
    if (!p || !out || cap == 0) return -1;
    snprintf(out, cap, "%s/%s", p->root, p->exec);
    return 0;
}
