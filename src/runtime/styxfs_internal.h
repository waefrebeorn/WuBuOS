/* styxfs_internal.h -- Internal helpers shared by styxfs sub-modules.
 * Public API + types in styxfs.h / styxfs_server.h. The utility functions
 * (mount/file resolution, container load) live in styxfs_util.c and are
 * declared here so styxfs.c links the SAME implementation (no double-coding).
 */

#ifndef STYXFS_INTERNAL_H
#define STYXFS_INTERNAL_H

#include "styxfs.h"
#include "styxfs_server.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* -- Shared internal helpers ------------------------------------- */
void normalize_path(char *path);
void build_path(char *out, size_t out_size, const char *base, const char *name);
int  path_is_mounted(styxfs_server_t *srv, const char *path);
int  count_children(styxfs_server_t *srv, const char *path);
void styxfs_path_to_host(styxfs_server_t *srv, const char *path,
                         char *out, size_t out_size);
styxfs_mount_t *styxfs_find_mount(styxfs_server_t *srv, const char *path, char *rel_path);
styxfs_file_t *styxfs_resolve(styxfs_server_t *srv, const char *path, int create_if_missing);
int  styxfs_load_container(const char *path, WUBU_HEADER *out_hdr, uint8_t **out_payload, size_t *out_size);
int  styxfs_is_wubu_container(const char *path);

/* -- Shared internal state/functions (styxfs.c was split into
 *    styxfs_vfs.c / styxfs_callbacks.c / styxfs_posix.c; the symbols below
 *    are file-static in the original and are now shared across those
 *    siblings via this header. No double-coding: single definition each.) -- */

/* Private in-memory filesystem node (originally file-static in styxfs.c). */
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

extern styxfs_node_t *g_root_node;

styxfs_node_t *styxfs_find_child(styxfs_node_t *parent, const char *name);
styxfs_node_t *styxfs_create_node(const char *name, styxfs_node_type type, styxfs_node_t *parent);
void styxfs_add_child(styxfs_node_t *parent, styxfs_node_t *child);
styxfs_node_t *styxfs_resolve_path_nodes(const char *path);
uint64_t styxfs_next_qid_path(styxfs_server_t *srv);

styxfs_file_t *styxfs_file_alloc(styxfs_server_t *srv);
styxfs_file_t *styxfs_file_lookup(styxfs_server_t *srv, uint64_t qid_path);
void styxfs_file_free(styxfs_server_t *srv, uint64_t qid_path);

styxfs_server_t *styxfs_get_server(styx_server_t *base);
const char *styxfs_fid_to_path(styxfs_server_t *srv, uint32_t fid);

#endif /* STYXFS_INTERNAL_H */
