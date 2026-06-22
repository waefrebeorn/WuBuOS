/*
 * styxfs_server.h  --  Styx/9P File Server Header
 */

#ifndef WUBU_STYXFS_SERVER_H
#define WUBU_STYXFS_SERVER_H

#include "styx.h"

/* Opaque server handle */
typedef struct styxfs_host_server styxfs_host_server_t;

/*
 * Create a Styx file server that exports a host directory tree.
 * root_path: Absolute path to the directory to export.
 * Returns server handle, or NULL on error.
 */
styx_server_t *styxfs_server_create(const char *root_path);

/*
 * Destroy a Styx file server and release all resources.
 */
void styxfs_server_destroy(styx_server_t *srv);

#endif /* WUBU_STYXFS_SERVER_H */
