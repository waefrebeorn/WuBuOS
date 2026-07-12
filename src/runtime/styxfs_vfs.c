/*
 * styxfs_vfs.c -- StyxFS in-memory VFS tree + open-file table.
 * Extracted from the monolithic styxfs.c. Self-contained: depends only on
 * styxfs.h / styxfs_server.h / styxfs_internal.h. C11, no god headers.
 */
#define _GNU_SOURCE
#include "styxfs.h"
#include "styxfs_internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

/* -- Internal Helpers ------------------------------------------ */

/* g_root_node is declared extern in styxfs_internal.h; defined here. */
styxfs_node_t *g_root_node = NULL;

styxfs_node_t *styxfs_find_child(styxfs_node_t *parent, const char *name) {
    if (!parent) return NULL;
    for (styxfs_node_t *c = parent->children; c; c = c->next_sibling) {
        if (strcmp(c->name, name) == 0) return c;
    }
    return NULL;
}

styxfs_node_t *styxfs_create_node(const char *name, styxfs_node_type type, styxfs_node_t *parent) {
    styxfs_node_t *n = (styxfs_node_t *)calloc(1, sizeof(styxfs_node_t));
    if (!n) return NULL;
    strncpy(n->name, name, 255);
    n->type = type;
    n->mode = (type == STYXFS_NODE_DIR) ? 040755 : 0100644;
    n->atime = (uint32_t)time(NULL);
    n->mtime = n->atime;
    n->parent = parent;
    return n;
}

void styxfs_add_child(styxfs_node_t *parent, styxfs_node_t *child) {
    if (!parent || !child) return;
    child->next_sibling = parent->children;
    parent->children = child;
}

styxfs_node_t *styxfs_resolve_path_nodes(const char *path) {
    if (!g_root_node) return NULL;
    if (strcmp(path, "/") == 0) return g_root_node;
    
    char buf[STYXFS_MAX_PATH];
    strncpy(buf, path, STYXFS_MAX_PATH - 1);
    buf[STYXFS_MAX_PATH - 1] = '\0';
    
    styxfs_node_t *cur = g_root_node;
    char *p = buf + 1; /* skip leading / */
    while (*p) {
        char *slash = strchr(p, '/');
        if (slash) *slash = '\0';
        if (*p == '\0') break;
        cur = styxfs_find_child(cur, p);
        if (!cur) return NULL;
        if (!slash) break;
        p = slash + 1;
    }
    return cur;
}

uint64_t styxfs_next_qid_path(styxfs_server_t *srv) {
    return ++srv->next_qid_path;
}

styxfs_file_t *styxfs_file_alloc(styxfs_server_t *srv) {
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (!srv->open_files[i].in_use) {
            memset(&srv->open_files[i], 0, sizeof(styxfs_file_t));
            srv->open_files[i].in_use = 1;
            return &srv->open_files[i];
        }
    }
    return NULL;
}

styxfs_file_t *styxfs_file_lookup(styxfs_server_t *srv, uint64_t qid_path) {
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == qid_path)
            return &srv->open_files[i];
    }
    return NULL;
}

void styxfs_file_free(styxfs_server_t *srv, uint64_t qid_path) {
    styxfs_file_t *f = styxfs_file_lookup(srv, qid_path);
    if (f) {
        if (f->container_payload) free(f->container_payload);
        if (f->write_buf) free(f->write_buf);
        f->in_use = 0;
    }
}

/* Path normalization: ensure leading slash, no trailing slash (except root) */
void normalize_path(char *path) {
    if (!path || !*path) { strcpy(path, "/"); return; }
    if (path[0] != '/') {
        char tmp[STYXFS_MAX_PATH];
        snprintf(tmp, sizeof(tmp), "/%s", path);
        strcpy(path, tmp);
    }
    size_t len = strlen(path);
    while (len > 1 && path[len - 1] == '/') {
        path[len - 1] = '\0';
        len--;
    }
}

/* -- Server Lifecycle ----------------------------------------- */

void styxfs_init(styxfs_server_t *srv) {
    memset(srv, 0, sizeof(*srv));
    srv->readonly = 0;
    srv->next_qid_path = 1;
}

/* Create a root directory entry */
static int ensure_root(styxfs_server_t *srv) {
    styxfs_file_t *root = styxfs_file_lookup(srv, 0);
    if (root) return 0;
    root = styxfs_file_alloc(srv);
    if (!root) return -1;
    root->qid_path = 0;
    root->qid_type = STX_QTDIR;
    root->qid_version = 1;
    strcpy(root->path, "/");
    return 0;
}

/* -- Mount Operations ----------------------------------------- */

int styxfs_mount(styxfs_server_t *srv, const char *path, const char *source, int is_repo) {
    if (!srv || !path || !source) return -1;
    ensure_root(srv);

    /* Allocate mount entry */
    styxfs_mount_t *m = (styxfs_mount_t*)calloc(1, sizeof(styxfs_mount_t));
    if (!m) return -1;
    strncpy(m->path, path, STYXFS_MAX_PATH - 1);
    strncpy(m->source, source, STYXFS_MAX_PATH - 1);
    m->is_container_repo = is_repo;

    /* Add to mount list */
    m->next = srv->mounts;
    srv->mounts = m;

    /* Create directory entry for mount point */
    styxfs_file_t *dir = styxfs_file_alloc(srv);
    if (!dir) { free(m); return -1; }
    dir->qid_path = styxfs_next_qid_path(srv);
    dir->qid_type = STX_QTDIR;
    dir->qid_version = 1;
    strncpy(dir->path, path, STYXFS_MAX_PATH - 1);
    normalize_path(dir->path);

    return 0;
}

int styxfs_unmount(styxfs_server_t *srv, const char *path) {
    if (!srv || !path) return -1;
    styxfs_mount_t **pp = &srv->mounts;
    while (*pp) {
        if (strcmp((*pp)->path, path) == 0) {
            styxfs_mount_t *m = *pp;
            *pp = m->next;
            free(m);
            return 0;
        }
        pp = &(*pp)->next;
    }
    return -1;
}

void styxfs_set_readonly(styxfs_server_t *srv, int readonly) {
    if (srv) srv->readonly = readonly;
}

/* -- Scan Repository ------------------------------------------ */

int styxfs_scan_repo(styxfs_server_t *srv, const char *mount_path, const char *fs_path) {
    if (!srv || !mount_path) return -1;
    ensure_root(srv);

    /* Open directory and scan for .wubu files */
    DIR *d = opendir(fs_path);
    if (!d) {
        /* Directory doesn't exist - not an error, just nothing to scan */
        return 0;
    }

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (styxfs_is_wubu_container(de->d_name)) {
            /* Build full path to container */
            char container_path[STYXFS_MAX_PATH];
            snprintf(container_path, sizeof(container_path), "%s/%s", fs_path, de->d_name);

            /* Load container header to verify */
            WUBU_HEADER hdr;
            uint8_t *payload = NULL;
            size_t payload_size = 0;
            if (styxfs_load_container(container_path, &hdr, &payload, &payload_size) == 0) {
                /* Register as a file in the namespace */
                char namespace_path[STYXFS_MAX_PATH];
                if (strcmp(mount_path, "/") == 0) {
                    snprintf(namespace_path, sizeof(namespace_path), "/%s", de->d_name);
                } else {
                    snprintf(namespace_path, sizeof(namespace_path), "%s/%s", mount_path, de->d_name);
                }

                /* Create file entry */
                styxfs_file_t *f = styxfs_file_alloc(srv);
                if (f) {
                    f->qid_path = styxfs_next_qid_path(srv);
                    f->qid_type = STX_QTFILE;
                    f->qid_version = 1;
                    strncpy(f->path, namespace_path, STYXFS_MAX_PATH - 1);
                    f->container_hdr = hdr;
                    f->container_payload = payload;
                    f->payload_size = payload_size;
                    f->file_mode = 0100755; /* Executable container */
                    f->is_dir = false;
                    f->atime = (uint32_t)time(NULL);
                    f->mtime = f->atime;
                } else if (payload) {
                    free(payload);
                }
            }
        }
    }
    closedir(d);
    return 0;
}

/* -- Styx Serve (dispatch) ------------------------------------ */

int styxfs_serve(styxfs_server_t *srv,
                  const uint8_t *inbuf, uint32_t inlen,
                  uint8_t *outbuf, uint32_t *outlen) {
    if (!srv || !inbuf || !outbuf || !outlen) return -1;
    /* Delegate to base styx_serve with our callbacks */
    return styx_serve(&srv->base, inbuf, inlen, outbuf, outlen);
}

/* -- Callbacks ------------------------------------------------ */
