/*
 * wubu_ns_mount.h -- the /n bind/union mount table.
 *
 * The namespace-layer mount table: bind a source subtree at another /n
 * path, stack layers in a union, and enforce read-only mounts at the VFS
 * callback boundary. This is the SHARED table the whole system composes
 * through -- distinct from the per-process view (wubu_ns_root.c, which
 * decides WHO can see a mount) and from the 9P file server's own
 * container-repository mounts (styxfs.h styxfs_mount_t, which lives inside
 * the styxfs server). The styxfs callbacks consult wubu_ns_mount_allows()
 * before serving a write-class operation; resolution here runs BEFORE the
 * server layer sees a path.
 *
 * NAMESPACE #582 (bind-mount), #586 (read-only enforcement at the VFS
 * callback layer), #597 (union readdir merge).
 *
 * C11, opaque struct, hosted-portable (dirent for the layer listing).
 */
#ifndef WUBU_NS_MOUNT_H
#define WUBU_NS_MOUNT_H

#include <stddef.h>
#include <stdint.h>

#define WUBU_NS_MOUNT_MAX_BINDS 64
#define WUBU_NS_MOUNT_MAX_PATH  512
#define WUBU_NS_MOUNT_MAX_NAME  64
#define WUBU_NS_MOUNT_MAX_ENTRIES 256

/* Bind flags. */
#define WUBU_NS_MOUNT_RO     0x1u   /* writes denied at this mount */
#define WUBU_NS_MOUNT_UNION  0x2u   /* stackable: readdir merges layers */

typedef struct wubu_ns_mount_table wubu_ns_mount_table_t;

wubu_ns_mount_table_t *wubu_ns_mount_table_new(void);
void   wubu_ns_mount_table_free(wubu_ns_mount_table_t *t);
int    wubu_ns_mount_bind_count(const wubu_ns_mount_table_t *t);

/* Bind `src` at namespace path `at`. A second bind at the SAME `at` is
 * allowed only when both are UNION (stacked layers, in bind order);
 * otherwise it is rejected. 0 ok; -1 bad args; -2 non-union duplicate;
 * -3 full table. */
int    wubu_ns_bind(wubu_ns_mount_table_t *t, const char *src,
                    const char *at, unsigned flags);

/* Unbind every layer at `at` (the whole union stack). Returns the number
 * of binds removed, or -1 when nothing was bound there. */
int    wubu_ns_unbind(wubu_ns_mount_table_t *t, const char *at);

/* Resolve a namespace path through the table: the longest component-
 * aligned bind prefix wins (first layer of a union for path resolution).
 * 0 ok (src_out + flags_out filled); -1 unbound path or bad args. */
int    wubu_ns_resolve(wubu_ns_mount_table_t *t, const char *npath,
                       char src_out[WUBU_NS_MOUNT_MAX_PATH],
                       unsigned *flags_out);

/* The VFS-callback-layer gate (#586): may an operation needing
 * `op_rights` (a WUBU_RIGHT_* mask) proceed on `npath`? Every write-class
 * bind at the matching prefix is consulted: 1 = allowed, 0 = denied
 * (a read-only mount covers the path). Unbound paths deny. */
int    wubu_ns_mount_allows(wubu_ns_mount_table_t *t, const char *npath,
                            uint64_t op_rights);

/* Union readdir merge (#597): list the entries of every layer bound at
 * `at`, in bind order, first occurrence of a name wins, into `names`
 * (up to max). Returns the entry count, -1 on bad args. Layers whose
 * source cannot be opened contribute nothing (a missing layer is not
 * an error -- unions are allowed to have holes). */
int    wubu_ns_mount_readdir(wubu_ns_mount_table_t *t, const char *at,
                             char names[][WUBU_NS_MOUNT_MAX_NAME], int max);

#endif /* WUBU_NS_MOUNT_H */
