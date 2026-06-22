/*
 * styxfs.c  --  StyxFS: 9P2000/Styx Filesystem for .wubu Containers
 *
 * Implements a filesystem namespace backed by .wubu containers.
 * Each .wubu container is exposed as a file in the Styx namespace.
 * Mount points allow composing multiple container repositories.
 */

#include "styxfs.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

/* -- Internal Helpers ------------------------------------------ */

/* Simple in-memory filesystem node */
typedef enum {
    STYXFS_NODE_ROOT = 0,
    STYXFS_NODE_DIR,
    STYXFS_NODE_FILE,
} styxfs_node_type;

typedef struct styxfs_node {
    char name[256];
    styxfs_node_type type;
    uint64_t qid_path;
    uint32_t mode;
    uint32_t atime;
    uint32_t mtime;
    uint64_t length;
    uint8_t *data;          /* For files: file contents */
    size_t data_size;
    struct styxfs_node *parent;
    struct styxfs_node *children;  /* For dirs: linked list of children */
    struct styxfs_node *next_sibling;
} styxfs_node_t;

static styxfs_node_t *g_root_node = NULL;

static styxfs_node_t *find_child(styxfs_node_t *parent, const char *name) {
    if (!parent) return NULL;
    for (styxfs_node_t *c = parent->children; c; c = c->next_sibling) {
        if (strcmp(c->name, name) == 0) return c;
    }
    return NULL;
}

static styxfs_node_t *create_node(const char *name, styxfs_node_type type, styxfs_node_t *parent) {
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

static void add_child(styxfs_node_t *parent, styxfs_node_t *child) {
    if (!parent || !child) return;
    child->next_sibling = parent->children;
    parent->children = child;
}

static styxfs_node_t *resolve_path_nodes(const char *path) {
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
        cur = find_child(cur, p);
        if (!cur) return NULL;
        if (!slash) break;
        p = slash + 1;
    }
    return cur;
}

static uint64_t next_qid_path(styxfs_server_t *srv) {
    return ++srv->next_qid_path;
}

static styxfs_file_t *file_alloc(styxfs_server_t *srv) {
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (!srv->open_files[i].in_use) {
            memset(&srv->open_files[i], 0, sizeof(styxfs_file_t));
            srv->open_files[i].in_use = 1;
            return &srv->open_files[i];
        }
    }
    return NULL;
}

static styxfs_file_t *file_lookup(styxfs_server_t *srv, uint64_t qid_path) {
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == qid_path)
            return &srv->open_files[i];
    }
    return NULL;
}

static void file_free(styxfs_server_t *srv, uint64_t qid_path) {
    styxfs_file_t *f = file_lookup(srv, qid_path);
    if (f) {
        if (f->container_payload) free(f->container_payload);
        if (f->write_buf) free(f->write_buf);
        f->in_use = 0;
    }
}

/* Path normalization: ensure leading slash, no trailing slash (except root) */
static void normalize_path(char *path) {
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
    styxfs_file_t *root = file_lookup(srv, 0);
    if (root) return 0;
    root = file_alloc(srv);
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
    styxfs_file_t *dir = file_alloc(srv);
    if (!dir) { free(m); return -1; }
    dir->qid_path = next_qid_path(srv);
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
    (void)fs_path; /* In full impl, scan fs_path for .wubu files */
    ensure_root(srv);
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

/* Helper: get styxfs_server_t from base */
static styxfs_server_t *get_server(styx_server_t *base) {
    /* base is the first member of styxfs_server_t */
    return (styxfs_server_t *)base;
}

/* Helper: resolve a fid to its path */
static const char *fid_to_path(styxfs_server_t *srv, uint32_t fid) {
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == (uint64_t)fid)
            return srv->open_files[i].path;
    }
    return "/";
}

/* Helper: build a full path from a base path and walk name */
static void build_path(char *out, size_t out_size, const char *base, const char *name) {
    if (strcmp(base, "/") == 0) {
        snprintf(out, out_size, "/%s", name);
    } else {
        snprintf(out, out_size, "%s/%s", base, name);
    }
}

/* Helper: check if a path is a mount point or under one */
static int path_is_mounted(styxfs_server_t *srv, const char *path) {
    for (styxfs_mount_t *m = srv->mounts; m; m = m->next) {
        size_t mlen = strlen(m->path);
        if (strncmp(path, m->path, mlen) == 0 &&
            (path[mlen] == '/' || path[mlen] == '\0')) {
            return 1;
        }
    }
    return 0;
}

/* Helper: count immediate children under a path (mount points) */
static int count_children(styxfs_server_t *srv, const char *path) {
    int count = 0;
    size_t plen = strlen(path);
    for (styxfs_mount_t *m = srv->mounts; m; m = m->next) {
        /* Check if m->path is a direct child of path */
        if (strncmp(m->path, path, plen) == 0 && m->path[plen] == '/') {
            const char *rest = m->path + plen + 1;
            if (strchr(rest, '/') == NULL) count++;
        }
    }
    return count;
}

int styxfs_attach_cb(styx_server_t *base, uint32_t fid, const char *aname) {
    styxfs_server_t *srv = get_server(base);
    (void)aname;
    /* Attach: associate fid with root directory */
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && 
            srv->open_files[i].qid_path == (uint64_t)fid) {
            /* Already allocated by base Styx */
            return 0;
        }
    }
    /* Allocate if not already done by base */
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (!srv->open_files[i].in_use) {
            memset(&srv->open_files[i], 0, sizeof(styxfs_file_t));
            srv->open_files[i].in_use = 1;
            srv->open_files[i].qid_path = fid;
            srv->open_files[i].qid_type = STX_QTDIR;
            srv->open_files[i].qid_version = 1;
            strcpy(srv->open_files[i].path, "/");
            return 0;
        }
    }
    return -1;
}

int styxfs_walk_cb(styx_server_t *base, uint32_t fid, uint32_t newfid,
                    const char **wname, int nwname,
                    styx_qid_t *qids, int *nwqid) {
    styxfs_server_t *srv = get_server(base);
    *nwqid = 0;
    
    if (nwname == 0) {
        /* Clone fid: newfid gets same path */
        const char *src_path = fid_to_path(srv, fid);
        for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
            if (!srv->open_files[i].in_use) {
                memset(&srv->open_files[i], 0, sizeof(styxfs_file_t));
                srv->open_files[i].in_use = 1;
                srv->open_files[i].qid_path = newfid;
                srv->open_files[i].qid_type = STX_QTDIR;
                srv->open_files[i].qid_version = 1;
                strncpy(srv->open_files[i].path, src_path, STYXFS_MAX_PATH - 1);
                qids[0].type = STX_QTDIR;
                qids[0].version = 1;
                qids[0].path = (uint64_t)newfid;
                *nwqid = 1;
                return 0;
            }
        }
        return -1;
    }
    
    char cur_path_buf[STYXFS_MAX_PATH];
    strncpy(cur_path_buf, fid_to_path(srv, fid), STYXFS_MAX_PATH - 1);
    
    for (int i = 0; i < nwname && i < 16; i++) {
        char new_path[STYXFS_MAX_PATH];
        build_path(new_path, sizeof(new_path), cur_path_buf, wname[i]);
        
        /* Check if this path exists (is a mount point or under one) */
        int exists = path_is_mounted(srv, new_path);
        
        /* Also check if it's the root or a direct mount */
        if (!exists) {
            for (styxfs_mount_t *m = srv->mounts; m; m = m->next) {
                if (strcmp(m->path, new_path) == 0) { exists = 1; break; }
            }
        }
        /* Root always exists */
        if (strcmp(new_path, "/") == 0) exists = 1;
        
        if (!exists) {
            /* Path doesn't exist  --  stop walk here */
            return 0;
        }
        
        /* Set QID for this step */
        qids[i].type = path_is_mounted(srv, new_path) ? STX_QTDIR : STX_QTFILE;
        qids[i].version = 1;
        qids[i].path = (uint64_t)(*nwqid + 1);
        (*nwqid)++;
        
        /* Update current path for next component */
        strncpy(cur_path_buf, new_path, STYXFS_MAX_PATH - 1);
    }
    
    /* Allocate newfid if walk succeeded */
    if (*nwqid > 0 && newfid != fid) {
        for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
            if (!srv->open_files[i].in_use) {
                memset(&srv->open_files[i], 0, sizeof(styxfs_file_t));
                srv->open_files[i].in_use = 1;
                srv->open_files[i].qid_path = newfid;
                srv->open_files[i].qid_type = (nwname == (*nwqid)) ? STX_QTDIR : STX_QTFILE;
                srv->open_files[i].qid_version = 1;
                /* Build final path */
                char final_path[STYXFS_MAX_PATH];
                strncpy(final_path, fid_to_path(srv, fid), STYXFS_MAX_PATH - 1);
                for (int j = 0; j < nwname && j < *nwqid; j++) {
                    char tmp[STYXFS_MAX_PATH];
                    build_path(tmp, sizeof(tmp), final_path, wname[j]);
                    strncpy(final_path, tmp, STYXFS_MAX_PATH - 1);
                }
                strncpy(srv->open_files[i].path, final_path, STYXFS_MAX_PATH - 1);
                break;
            }
        }
    }
    
    return 0;
}

int styxfs_open_cb(styx_server_t *base, uint32_t fid, int mode,
                    styx_qid_t *qid) {
    styxfs_server_t *srv = get_server(base);
    
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == (uint64_t)fid) {
            srv->open_files[i].mode = mode;
            qid->type = srv->open_files[i].qid_type;
            qid->version = srv->open_files[i].qid_version;
            qid->path = srv->open_files[i].qid_path;
            return 0;
        }
    }
    return -1;
}

int styxfs_create_cb(styx_server_t *base, uint32_t fid, const char *name,
                      uint32_t perm, int mode, styx_qid_t *qid) {
    styxfs_server_t *srv = get_server(base);
    (void)mode;
    
    if (srv->readonly) return -1;
    
    const char *parent_path = fid_to_path(srv, fid);
    char new_path[STYXFS_MAX_PATH];
    build_path(new_path, sizeof(new_path), parent_path, name);
    
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (!srv->open_files[i].in_use) {
            memset(&srv->open_files[i], 0, sizeof(styxfs_file_t));
            srv->open_files[i].in_use = 1;
            srv->open_files[i].qid_path = next_qid_path(srv);
            srv->open_files[i].qid_type = (perm & 0x80000000) ? STX_QTDIR : STX_QTFILE;
            srv->open_files[i].qid_version = 1;
            strncpy(srv->open_files[i].path, new_path, STYXFS_MAX_PATH - 1);
            srv->open_files[i].mode = 1;
            
            qid->type = srv->open_files[i].qid_type;
            qid->version = 1;
            qid->path = srv->open_files[i].qid_path;
            return 0;
        }
    }
    return -1;
}

int styxfs_read_cb(styx_server_t *base, uint32_t fid,
                    uint64_t offset, uint32_t count,
                    uint8_t *data, uint32_t *nread) {
    styxfs_server_t *srv = get_server(base);
    *nread = 0;
    
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == (uint64_t)fid) {
            if (srv->open_files[i].qid_type == STX_QTDIR) {
                /* Read directory: return stat entries for children */
                const char *path = srv->open_files[i].path;
                
                /* Simple directory listing: iterate mounts */
                uint32_t buf_off = 0;
                uint64_t dir_offset = offset;
                int entry_count = 0;
                
                for (styxfs_mount_t *m = srv->mounts; m; m = m->next) {
                    size_t plen = strlen(path);
                    if (strcmp(path, "/") == 0) {
                        /* Root: list top-level mount points */
                        const char *slash = strchr(m->path + 1, '/');
                        int name_len = slash ? (int)(slash - (m->path + 1)) : (int)strlen(m->path + 1);
                        
                        /* Skip entries before offset */
                        if ((uint64_t)entry_count < dir_offset) { entry_count++; continue; }
                        
                        /* Build stat entry */
                        char entry_name[256];
                        strncpy(entry_name, m->path + 1, (size_t)name_len);
                        entry_name[name_len] = '\0';
                        
                        /* Skip duplicates */
                        int dup = 0;
                        for (styxfs_mount_t *p = srv->mounts; p != m; p = p->next) {
                            const char *pslash = strchr(p->path + 1, '/');
                            int plen2 = pslash ? (int)(pslash - (p->path + 1)) : (int)strlen(p->path + 1);
                            if (plen2 == name_len && strncmp(p->path + 1, entry_name, (size_t)name_len) == 0) {
                                dup = 1; break;
                            }
                        }
                        if (dup) { entry_count++; continue; }
                        
                        /* Check if we have room */
                        uint32_t need = (uint32_t)(2 + 4 + 8 + 4 + 4 + 4 + 8 + 2 + strlen(entry_name));
                        if (buf_off + need > count) break;
                        
                        /* Build 9P stat structure */
                        styx_dir_t dir;
                        memset(&dir, 0, sizeof(dir));
                        dir.type = 0;
                        dir.dev = 0;
                        dir.qid.type = STX_QTDIR;
                        dir.qid.version = 1;
                        dir.qid.path = (uint64_t)(entry_count + 100);
                        dir.mode = 040755;
                        dir.atime = (uint32_t)time(NULL);
                        dir.mtime = dir.atime;
                        dir.length = 0;
                        strncpy(dir.name, entry_name, sizeof(dir.name) - 1);
                        strcpy(dir.uid, "wubu");
                        strcpy(dir.gid, "wubu");
                        strcpy(dir.muid, "wubu");
                        
                        /* Pack stat into data buffer */
                        uint32_t dir_size_pos = buf_off;
                        buf_off += 2; /* size field, filled later */
                        uint32_t dir_start = buf_off;
                        styx_put16(data + buf_off, dir.type); buf_off += 2;
                        styx_put32(data + buf_off, dir.dev); buf_off += 4;
                        data[buf_off++] = dir.qid.type;
                        styx_put32(data + buf_off, dir.qid.version); buf_off += 4;
                        styx_put64(data + buf_off, dir.qid.path); buf_off += 8;
                        styx_put32(data + buf_off, dir.mode); buf_off += 4;
                        styx_put32(data + buf_off, dir.atime); buf_off += 4;
                        styx_put32(data + buf_off, dir.mtime); buf_off += 4;
                        styx_put64(data + buf_off, dir.length); buf_off += 8;
                        buf_off += styx_putstr(data + buf_off, dir.name);
                        buf_off += styx_putstr(data + buf_off, dir.uid);
                        buf_off += styx_putstr(data + buf_off, dir.gid);
                        buf_off += styx_putstr(data + buf_off, dir.muid);
                        
                        /* Fill in stat size */
                        uint32_t dir_total = buf_off - dir_start;
                        styx_put16(data + dir_size_pos, (uint16_t)dir_total);
                        
                        entry_count++;
                    }
                }
                
                *nread = buf_off;
            } else {
                /* Read file data */
                if (srv->open_files[i].container_payload && srv->open_files[i].payload_size > 0) {
                    uint64_t file_size = (uint64_t)srv->open_files[i].payload_size;
                    if (offset >= file_size) { *nread = 0; return 0; }
                    uint64_t avail = file_size - offset;
                    uint32_t to_read = (count < avail) ? count : (uint32_t)avail;
                    if (data && to_read > 0) {
                        memcpy(data, srv->open_files[i].container_payload + offset, to_read);
                    }
                    *nread = to_read;
                } else {
                    *nread = 0;
                }
            }
            return 0;
        }
    }
    return -1;
}

int styxfs_write_cb(styx_server_t *base, uint32_t fid,
                     uint64_t offset, uint32_t count,
                     const uint8_t *data, uint32_t *nwritten) {
    styxfs_server_t *srv = get_server(base);
    *nwritten = 0;
    
    if (srv->readonly) return -1;
    
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == (uint64_t)fid) {
            /* Simple write: extend buffer if needed */
            uint64_t end = offset + count;
            if (end > (uint64_t)srv->open_files[i].write_buf_size) {
                size_t new_size = (size_t)(end + 4096);
                uint8_t *new_buf = (uint8_t *)realloc(srv->open_files[i].write_buf, new_size);
                if (!new_buf) return -1;
                srv->open_files[i].write_buf = new_buf;
                srv->open_files[i].write_buf_size = new_size;
            }
            if (data && count > 0) {
                memcpy(srv->open_files[i].write_buf + offset, data, count);
            }
            srv->open_files[i].write_offset = (size_t)end;
            *nwritten = count;
            return 0;
        }
    }
    return -1;
}

int styxfs_clunk_cb(styx_server_t *base, uint32_t fid) {
    styxfs_server_t *srv = get_server(base);
    
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == (uint64_t)fid) {
            if (srv->open_files[i].container_payload) free(srv->open_files[i].container_payload);
            if (srv->open_files[i].write_buf) free(srv->open_files[i].write_buf);
            srv->open_files[i].in_use = 0;
            return 0;
        }
    }
    return 0;
}

int styxfs_remove_cb(styx_server_t *base, uint32_t fid) {
    styxfs_server_t *srv = get_server(base);
    if (srv->readonly) return -1;
    return styxfs_clunk_cb(base, fid);
}

int styxfs_stat_cb(styx_server_t *base, uint32_t fid,
                    styx_dir_t *dir) {
    styxfs_server_t *srv = get_server(base);
    
    memset(dir, 0, sizeof(*dir));
    
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == (uint64_t)fid) {
            dir->type = 0;
            dir->dev = 0;
            dir->qid.type = srv->open_files[i].qid_type;
            dir->qid.version = srv->open_files[i].qid_version;
            dir->qid.path = srv->open_files[i].qid_path;
            
            if (srv->open_files[i].qid_type == STX_QTDIR) {
                dir->mode = 040755;
                dir->length = 0;
            } else {
                dir->mode = 0100644;
                dir->length = (uint64_t)srv->open_files[i].payload_size;
                if (srv->open_files[i].write_buf) {
                    dir->length = (uint64_t)srv->open_files[i].write_offset;
                }
            }
            
            dir->atime = (uint32_t)time(NULL);
            dir->mtime = dir->atime;
            
            /* Extract name from path */
            const char *path = srv->open_files[i].path;
            const char *last_slash = strrchr(path, '/');
            strncpy(dir->name, last_slash ? last_slash + 1 : path, sizeof(dir->name) - 1);
            strcpy(dir->uid, "wubu");
            strcpy(dir->gid, "wubu");
            strcpy(dir->muid, "wubu");

            /* Initialize metadata fields for future wstat */
            if (srv->open_files[i].file_mode == 0) {
                srv->open_files[i].file_mode = (uint16_t)(srv->open_files[i].is_dir ? 040755 : 0100644);
            }
            if (srv->open_files[i].mtime == 0) {
                srv->open_files[i].mtime = dir->atime;
            }
            if (srv->open_files[i].atime == 0) {
                srv->open_files[i].atime = dir->atime;
            }

            return 0;
        }
    }
    return -1;
}

int styxfs_wstat_cb(styx_server_t *base, uint32_t fid,
                     const styx_dir_t *dir) {
    styxfs_server_t *srv = get_server(base);
    if (srv->readonly) return -1;

    /* Find the open file by fid (qid_path) */
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && srv->open_files[i].qid_path == (uint64_t)fid) {
            /* Apply stat changes from the incoming dir structure */

            /* Update mode if set */
            if (dir->mode != 0) {
                /* Extract permission bits (lower 12 bits) and type bits */
                uint16_t new_perms = (uint16_t)(dir->mode & 0xFFF);
                uint16_t type_bits = (uint16_t)(dir->mode & 0xF000);
                /* Preserve the type bits from the existing mode, update permissions */
                if (srv->open_files[i].is_dir) {
                    srv->open_files[i].file_mode = (uint16_t)(040000 | (new_perms & 0777));
                } else {
                    srv->open_files[i].file_mode = (uint16_t)(0100000 | (new_perms & 0777));
                }
                /* If the type bits changed, update is_dir */
                if ((dir->mode & STX_DMDIR) && !srv->open_files[i].is_dir) {
                    srv->open_files[i].is_dir = true;
                }
            }

            /* Update name if provided (rename) */
            if (dir->name[0] != '\0' && strcmp(dir->name, ".") != 0 &&
                strcmp(dir->name, "..") != 0) {
                /* Extract the directory portion of the path */
                char *path_copy = strdup(srv->open_files[i].path);
                if (path_copy) {
                    char *last_slash = strrchr(path_copy, '/');
                    if (last_slash) {
                        /* Replace the filename portion */
                        *(last_slash + 1) = '\0';
                        size_t remaining = sizeof(srv->open_files[i].path) -
                                           (size_t)(last_slash - path_copy) - 1;
                        strncat(path_copy, dir->name, remaining - 1);
                        strncpy(srv->open_files[i].path, path_copy,
                                sizeof(srv->open_files[i].path) - 1);
                    } else {
                        /* No slash — just replace the whole name */
                        strncpy(srv->open_files[i].path, dir->name,
                                sizeof(srv->open_files[i].path) - 1);
                    }
                    free(path_copy);
                }
            }

            /* Update length if provided (truncate) */
            if (dir->length != 0 || (dir->mode & STX_OTRUNC)) {
                srv->open_files[i].payload_size = (size_t)dir->length;
                srv->open_files[i].write_offset = (size_t)dir->length;
            }

            /* Update mtime/atime */
            uint32_t now = (uint32_t)time(NULL);
            if (dir->mtime != 0) {
                srv->open_files[i].mtime = dir->mtime;
            } else {
                srv->open_files[i].mtime = now;
            }
            if (dir->atime != 0) {
                srv->open_files[i].atime = dir->atime;
            } else {
                srv->open_files[i].atime = now;
            }

            /* Bump qid version to indicate change */
            srv->open_files[i].qid_version++;

            return 0;
        }
    }
    return -1; /* fid not found */
}

/* -- Helper Utilities ----------------------------------------- */

styxfs_mount_t *styxfs_find_mount(styxfs_server_t *srv, const char *path, char *rel_path) {
    if (!srv || !path) return NULL;
    for (styxfs_mount_t *m = srv->mounts; m; m = m->next) {
        size_t mlen = strlen(m->path);
        if (strncmp(path, m->path, mlen) == 0) {
            if (rel_path) {
                const char *rest = path + mlen;
                if (*rest == '/') rest++;
                strncpy(rel_path, rest, STYXFS_MAX_PATH - 1);
            }
            return m;
        }
    }
    return NULL;
}

styxfs_file_t *styxfs_resolve(styxfs_server_t *srv, const char *path, int create_if_missing) {
    if (!srv || !path) return NULL;
    (void)create_if_missing;
    char norm[STYXFS_MAX_PATH];
    strncpy(norm, path, STYXFS_MAX_PATH - 1);
    norm[STYXFS_MAX_PATH - 1] = '\0';
    normalize_path(norm);
    for (int i = 0; i < STYXFS_MAX_OPEN_FILES; i++) {
        if (srv->open_files[i].in_use && strcmp(srv->open_files[i].path, norm) == 0)
            return &srv->open_files[i];
    }
    return NULL;
}

int styxfs_build_dirent(styxfs_server_t *srv, const char *path,
                         uint8_t *buf, uint32_t buf_size, uint32_t *out_size,
                         uint64_t offset, uint32_t count) {
    (void)srv; (void)path; (void)buf; (void)buf_size;
    (void)offset; (void)count;
    if (out_size) *out_size = 0;
    return 0;
}

int styxfs_load_container(const char *path, WUBU_HEADER *out_hdr, uint8_t **out_payload, size_t *out_size) {
    (void)path; (void)out_hdr; (void)out_payload; (void)out_size;
    return -1;
}

int styxfs_is_wubu_container(const char *path) {
    if (!path) return 0;
    size_t len = strlen(path);
    return len >= 5 && strcmp(path + len - 5, ".wubu") == 0;
}
