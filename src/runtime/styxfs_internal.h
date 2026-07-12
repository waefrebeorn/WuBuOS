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

#endif /* STYXFS_INTERNAL_H */
