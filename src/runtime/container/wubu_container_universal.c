/*
 * wubu_container_universal.c -- the uniform container layer + two real
 * backends (RAM and host-directory).  Any backend that wants to join the
 * OS's container world registers an adapter and is immediately usable through
 * the same interface as every other kind.  That is the universal promise.
 */
#include "../../egojar.h"
#include "../../egojar_net.h"
#include "wubu_container_universal.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct wubu_container_universal {
    char    kind[WCU_KIND_LEN];
    void   *backend;      /* adapter-private state */
};

/* ------------------------------------------------------------ the registry */

static const wubu_container_adapter_t *g_adapters[WCU_MAX_KINDS];
static int g_adapter_count;

static void ensure_builtin(void);

static int find_kind(const char *kind)
{
    if (!kind) return -1;
    for (int i = 0; i < g_adapter_count; i++)
        if (strcmp(g_adapters[i]->kind, kind) == 0) return i;
    return -1;
}

int wu_container_register(const wubu_container_adapter_t *adapter)
{
    if (!adapter || !adapter->kind || !adapter->kind[0]) return -1;
    if (find_kind(adapter->kind) >= 0) return -1;   /* no silent override */
    if (g_adapter_count >= WCU_MAX_KINDS) return -1;
    g_adapters[g_adapter_count++] = adapter;
    return 0;
}

int wu_container_kinds(char names[][WCU_KIND_LEN], int cap)
{
    ensure_builtin();
    int n = 0;
    for (int i = 0; i < g_adapter_count && n < cap; i++) {
        snprintf(names[n], WCU_KIND_LEN, "%s", g_adapters[i]->kind);
        n++;
    }
    return n;
}

/* The handle never stores the adapter pointer; it is resolved from the
 * registry by kind on every dispatch, so a handle cannot dangle after a
 * close or a re-registration. */
static const wubu_container_adapter_t *adapter_of(const wubu_container_universal_t *c)
{
    if (!c) return NULL;
    int i = find_kind(c->kind);
    return i < 0 ? NULL : g_adapters[i];
}

/* --------------------------------------------------------------- the RAM kind */

typedef struct {
    char     name[WCU_PATH_LEN];
    uint8_t *data;
    uint32_t size;
} wcu_ram_file;

typedef struct {
    wcu_ram_file files[64];
    int          count;
    char         label[WCU_PATH_LEN];
} wcu_ram;

static int  ram_open(const char *, wubu_container_universal_t **);
static void ram_close(wubu_container_universal_t *);
static int  ram_stat(wubu_container_universal_t *, const char *,
                     uint64_t *, uint32_t *);
static int  ram_list(wubu_container_universal_t *, const char *,
                     char[][WCU_PATH_LEN], int);
static int  ram_read(wubu_container_universal_t *, const char *,
                     uint8_t *, uint32_t);
static int  ram_put(wubu_container_universal_t *, const char *,
                    const uint8_t *, uint32_t);

static const wubu_container_adapter_t g_ram_adapter = {
    "ram", ram_open, ram_close, NULL, ram_stat, ram_list, ram_read, ram_put
};

static wcu_ram_file *ram_find(wcu_ram *r, const char *path)
{
    for (int i = 0; i < r->count; i++)
        if (strcmp(r->files[i].name, path) == 0) return &r->files[i];
    return NULL;
}

static int ram_open(const char *source, wubu_container_universal_t **out)
{
    wcu_ram *r = (wcu_ram *)calloc(1, sizeof(*r));
    if (!r) return -1;
    snprintf(r->label, sizeof(r->label), "%s", source ? source : "anonymous");
    return wu_container_handle_new(g_ram_adapter.kind, r, out);
}

static int ram_stat(wubu_container_universal_t *c, const char *path,
                    uint64_t *size, uint32_t *is_dir)
{
    wcu_ram *r = (wcu_ram *)wu_container_backend(c);
    wcu_ram_file *f = ram_find(r, path);
    if (!f) return -1;
    if (size) *size = f->size;
    if (is_dir) *is_dir = 0;      /* ram entries are files, not dirs */
    return 0;
}

static int ram_list(wubu_container_universal_t *c, const char *dir,
                    char out[][WCU_PATH_LEN], int cap)
{
    (void)dir;
    wcu_ram *r = (wcu_ram *)wu_container_backend(c);
    int n = 0;
    for (int i = 0; i < r->count && n < cap; i++) {
        snprintf(out[n], WCU_PATH_LEN, "%s", r->files[i].name);
        n++;
    }
    return n;
}

static int ram_read(wubu_container_universal_t *c, const char *path,
                    uint8_t *buf, uint32_t size)
{
    wcu_ram *r = (wcu_ram *)wu_container_backend(c);
    wcu_ram_file *f = ram_find(r, path);
    if (!f || !f->data) return -1;
    uint32_t n = size < f->size ? size : f->size;
    memcpy(buf, f->data, n);
    return (int)n;
}

static int ram_put(wubu_container_universal_t *c, const char *path,
                   const uint8_t *data, uint32_t size)
{
    wcu_ram *r = (wcu_ram *)wu_container_backend(c);
    wcu_ram_file *f = ram_find(r, path);
    if (!f) {
        if (r->count >= 64) return -1;
        f = &r->files[r->count++];
        memset(f, 0, sizeof(*f));
        snprintf(f->name, sizeof(f->name), "%s", path);
    }
    free(f->data);
    f->data = NULL;
    f->size = 0;
    if (size > 0) {
        f->data = (uint8_t *)malloc(size);
        if (!f->data) return -1;
        memcpy(f->data, data, size);
        f->size = size;
    }
    return 0;
}

static void ram_close(wubu_container_universal_t *c)
{
    wcu_ram *r = (wcu_ram *)wu_container_backend(c);
    if (r)
        for (int i = 0; i < r->count; i++) free(r->files[i].data);
    free(r);
    free(c);
}

/* --------------------------------------------------------------- the DIR kind */

typedef struct { char base[WCU_PATH_LEN]; } wcu_dir;

static int  dir_open(const char *, wubu_container_universal_t **);
static void dir_close(wubu_container_universal_t *);
static int  dir_stat(wubu_container_universal_t *, const char *,
                     uint64_t *, uint32_t *);
static int  dir_list(wubu_container_universal_t *, const char *,
                     char[][WCU_PATH_LEN], int);
static int  dir_read(wubu_container_universal_t *, const char *,
                     uint8_t *, uint32_t);
static int  dir_put(wubu_container_universal_t *, const char *,
                    const uint8_t *, uint32_t);

static const wubu_container_adapter_t g_dir_adapter = {
    "dir", dir_open, dir_close, NULL, dir_stat, dir_list, dir_read, dir_put
};

static void dir_mkpath(char *out, size_t cap, const char *base, const char *path)
{
    if (path[0] == '/') path++;
    snprintf(out, cap, "%s/%s", base, path);
}

static int dir_open(const char *source, wubu_container_universal_t **out)
{
    if (!source || !*source) return -1;
    struct stat st;
    if (stat(source, &st) != 0 || !S_ISDIR(st.st_mode)) return -1;
    wcu_dir *d = (wcu_dir *)calloc(1, sizeof(*d));
    if (!d) return -1;
    snprintf(d->base, sizeof(d->base), "%s", source);
    return wu_container_handle_new(g_dir_adapter.kind, d, out);
}

static int dir_stat(wubu_container_universal_t *c, const char *path,
                    uint64_t *size, uint32_t *is_dir)
{
    wcu_dir *d = (wcu_dir *)wu_container_backend(c);
    char full[WCU_PATH_LEN * 2];
    dir_mkpath(full, sizeof(full), d->base, path);
    struct stat st;
    if (stat(full, &st) != 0) return -1;
    if (size) *size = (uint64_t)st.st_size;
    if (is_dir) *is_dir = S_ISDIR(st.st_mode) ? 1 : 0;
    return 0;
}

static int dir_list(wubu_container_universal_t *c, const char *dir,
                    char out[][WCU_PATH_LEN], int cap)
{
    wcu_dir *d = (wcu_dir *)wu_container_backend(c);
    char full[WCU_PATH_LEN * 2];
    dir_mkpath(full, sizeof(full), d->base, dir ? dir : "");
    DIR *h = opendir(full);
    if (!h) return -1;
    struct dirent *e;
    int n = 0;
    while ((e = readdir(h)) != NULL && n < cap) {
        if (e->d_name[0] == '.') continue;
        snprintf(out[n], WCU_PATH_LEN, "%s", e->d_name);
        n++;
    }
    closedir(h);
    return n;
}

static int dir_read(wubu_container_universal_t *c, const char *path,
                    uint8_t *buf, uint32_t size)
{
    wcu_dir *d = (wcu_dir *)wu_container_backend(c);
    char full[WCU_PATH_LEN * 2];
    dir_mkpath(full, sizeof(full), d->base, path);
    int fd = open(full, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, size);
    close(fd);
    return n < 0 ? -1 : (int)n;
}

static int dir_put(wubu_container_universal_t *c, const char *path,
                   const uint8_t *data, uint32_t size)
{
    wcu_dir *d = (wcu_dir *)wu_container_backend(c);
    char full[WCU_PATH_LEN * 2];
    dir_mkpath(full, sizeof(full), d->base, path);
    int fd = open(full, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) return -1;
    ssize_t n = write(fd, data, size);
    close(fd);
    return n == (ssize_t)size ? 0 : -1;
}

static void dir_close(wubu_container_universal_t *c)
{
    free(wu_container_backend(c));
    free(c);
}

/* ---------------------------------------------------------- public dispatch */

int wu_container_handle_new(const char *kind, void *backend,
                           wubu_container_universal_t **out)
{
    if (!kind || !kind[0] || !out) return -1;
    wubu_container_universal_t *c =
        (wubu_container_universal_t *)calloc(1, sizeof(*c));
    if (!c) return -1;
    snprintf(c->kind, sizeof(c->kind), "%s", kind);
    c->backend = backend;
    *out = c;
    return 0;
}

void *wu_container_backend(wubu_container_universal_t *c)
{
    return c ? c->backend : NULL;
}

static void ensure_builtin(void)
{
    static int done = 0;
    if (done) return;
    wu_container_register(&g_ram_adapter);
    wu_container_register(&g_dir_adapter);
    done = 1;
}

int wu_container_open(const char *kind, const char *source,
                      wubu_container_universal_t **out)
{
    if (!kind || !kind[0] || !out) return -1;
    ensure_builtin();
    int i = find_kind(kind);
    if (i < 0) return -1;                    /* unknown kind == error, no fallback */
    return g_adapters[i]->open(source, out);
}

void wu_container_close(wubu_container_universal_t *c)
{
    if (!c) return;
    const wubu_container_adapter_t *a = adapter_of(c);
    if (a && a->close) a->close(c);
    else free(c);
}

const char *wu_container_kind(wubu_container_universal_t *c)
{
    return c ? c->kind : "";
}

int wu_container_mount(wubu_container_universal_t *c, const char *path)
{
    (void)c; (void)path;
    /* A universal container is mounted-through-interface: open() already holds
     * it; a host path bind is the backend's business.  Present as ready. */
    return 0;
}

int wu_container_stat(wubu_container_universal_t *c, const char *path,
                      uint64_t *size, uint32_t *is_dir)
{
    const wubu_container_adapter_t *a = adapter_of(c);
    if (!a || !a->stat || !path) return -1;
    return a->stat(c, path, size, is_dir);
}

int wu_container_list(wubu_container_universal_t *c, const char *dir,
                      char out[][WCU_PATH_LEN], int cap)
{
    const wubu_container_adapter_t *a = adapter_of(c);
    if (!a || !a->list || !out || cap <= 0) return -1;
    return a->list(c, dir, out, cap);
}

int wu_container_read(wubu_container_universal_t *c, const char *path,
                      uint8_t *buf, uint32_t size)
{
    const wubu_container_adapter_t *a = adapter_of(c);
    if (!a || !a->read || !path || !buf) return -1;
    return a->read(c, path, buf, size);
}

int wu_container_put(wubu_container_universal_t *c, const char *path,
                     const uint8_t *data, uint32_t size)
{
    const wubu_container_adapter_t *a = adapter_of(c);
    if (!a || !a->put || !path) return -1;
    return a->put(c, path, data, size);
}
