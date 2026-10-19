/*
 * vsl_nt_ob.h -- NT object namespace over /n (see vsl_nt_ob.c).
 */
#ifndef WUBU_VSL_NT_OB_H
#define WUBU_VSL_NT_OB_H

#include <stddef.h>
#include <stdint.h>
#include "wubu_cap/wubu_cap.h"

#define NT_STATUS_SUCCESS               0x00000000u
#define NT_STATUS_INVALID_PARAMETER     0xC000000Du
#define NT_STATUS_ACCESS_DENIED         0xC0000022u
#define NT_STATUS_OBJECT_NAME_NOT_FOUND 0xC0000034u
#define NT_STATUS_OBJECT_NAME_COLLISION 0xC0000035u
#define NT_STATUS_OBJECT_PATH_INVALID   0xC0000039u
#define NT_STATUS_BUFFER_TOO_SMALL      0xC0000023u
#define NT_STATUS_NO_MORE_ENTRIES       0x8000001Au
#define NT_STATUS_NO_MEMORY             0xC0000017u

#define OB_MAX_NODES 64
#define OB_PATH_MAX  256

#define OB_TYPE_FILE      1u
#define OB_TYPE_DIRECTORY 2u
#define OB_TYPE_SYMLINK   3u

#define OB_REPARSE_SYMLINK 0xA000000Cu   /* IO_REPARSE_TAG_SYMLINK */

#define OB_RIGHT_READ   (1ull << 0)
#define OB_RIGHT_WRITE  (1ull << 1)
#define OB_RIGHT_DELETE (1ull << 2)

typedef struct ob_node {
    char     path[OB_PATH_MAX];
    char     target[OB_PATH_MAX];   /* symlink destination, canonical */
    uint32_t type;
    uint32_t reparse_tag;
    uint32_t is_dir;
    uint32_t handle_refs;
    uint32_t handle_id;
    uint64_t rights;
    wubu_cap_token_t cap;   /* STYX_NODE capability backing this node, if bound */
    int      has_cap;
} ob_node_t;

typedef struct ob_tree {
    ob_node_t nodes[OB_MAX_NODES];
    int       count;
} ob_tree_t;

void ob_tree_init(ob_tree_t *t);
int  ob_tree_add(ob_tree_t *t, const char *path, uint32_t is_dir, uint64_t rights);
int  ob_tree_symlink(ob_tree_t *t, const char *path, const char *target);

/* Windows/NT path -> canonical lowercase /n path. Rejects paths that do not sit
 * under /n and any ".." that would escape it. */
int vsl_nt_ob_canon(const char *in, char *out, size_t outsz);

/* NT access mask -> 9P open mode. Returns ACCESS_DENIED and sets *unmapped when
 * the mask names bits with no 9P equivalent. */
int vsl_nt_access_to_9p(uint32_t mask, int *mode, uint32_t *unmapped);

/* Open by name. Fails with ACCESS_DENIED unless the node's rights cover what the
 * mask asks for; follows reparse links. */
int vsl_nt_ob_open(ob_tree_t *t, const char *name, uint32_t access, int32_t pid,
                   uint32_t *handle, uint64_t *rights);

/* Bind a node to a real STYX_NODE capability. The rights requested by an open
 * are then checked by wubu_cap_resolve, and an ungranted caller is denied. */
int vsl_nt_ob_bind_cap(ob_tree_t *t, const char *path, uint64_t cap_rights,
                       int32_t owner_pid, wubu_cap_token_t *out);

/* NT-named entry point: the Object Manager's open-by-name over the /n tree. */
int ObOpenByKeyName(ob_tree_t *t, const char *name, uint32_t access, int32_t pid,
                    uint32_t *handle, uint64_t *rights);

/* Resolve a name (following links) to its canonical path. */
int vsl_nt_ob_resolve(ob_tree_t *t, const char *name, char *out, size_t outsz);

int vsl_nt_ob_mkdir(ob_tree_t *t, const char *name);

/* Paged directory read. *cursor carries the position between calls; *more says
 * whether another entry follows. Returns NO_MORE_ENTRIES at the end. */
int vsl_nt_ob_query_dir(ob_tree_t *t, const char *dir, int *cursor,
                        char *name, size_t namesz, int *more);

#endif /* WUBU_VSL_NT_OB_H */
