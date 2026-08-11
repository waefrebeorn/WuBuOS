/*
 * wubu_kvfs.h — KV namespace layer (G1: path-addressable KV cache).
 *
 * Metal port of wubuwizard/src/wubu_kvfs.c into the WuBuOS kernel
 * (THE BODY). The kernel is the KV-as-FS substrate: the AGI training
 * loop reads /n/kv/ via 9P, and the OS writes world-state snapshots
 * into /kv/world/ at every tick.
 *
 * C11, opaque structs, minimal includes. No third-party deps.
 * Mirrors the Brain's wubu_kvfs API exactly (boundary contract).
 */
#ifndef WUBU_KVFS_KERNEL_H
#define WUBU_KVFS_KERNEL_H

#include <stddef.h>
#include <stdint.h>

/* Opaque namespace handle */
typedef struct wubu_kvfs wubu_kvfs_t;

/* Opaque resolved-path handle: precomputed (offset, limit). */
typedef struct wubu_kvfs_handle wubu_kvfs_handle_t;

/* A mounted KV region: path prefix → contiguous block range. */
typedef struct {
    char     path[256];
    uint32_t start_block;
    uint32_t n_blocks;
    uint32_t block_size;
    size_t   abs_offset;
    size_t   abs_limit;
} wubu_kvfs_mount_t;

/* Create a KV namespace. block_size = floats per block. */
wubu_kvfs_t *wubu_kvfs_create(uint32_t block_size, uint32_t total_blocks);

/* Mount a KV region at path. 0=ok, -1=clash/overrun. */
int wubu_kvfs_mount(wubu_kvfs_t *fs, const char *path,
                    uint32_t start_block, uint32_t n_blocks);

/* Unmount a subtree (-1 if not found). */
int wubu_kvfs_unmount(wubu_kvfs_t *fs, const char *path);

/* Resolve a path to (block, offset). -1 if unmounted. */
int wubu_kvfs_lookup(const wubu_kvfs_t *fs, const char *path,
                     uint32_t *out_block, size_t *out_offset);

/* ---- hot path: resolve once, use many ---- */

/* Resolve a path to an opaque handle. NULL if unmounted. */
wubu_kvfs_handle_t *wubu_kvfs_open(const wubu_kvfs_t *fs, const char *path);

/* Read n floats from kv_base at the handle's precomputed offset. */
int wubu_kvfs_handle_read(const wubu_kvfs_handle_t *h,
                          const float *kv_base, float *dst, size_t n_floats);

/* Write n floats from src into kv_base at the handle's offset. */
int wubu_kvfs_handle_write(const wubu_kvfs_handle_t *h,
                           float *kv_base, const float *src, size_t n_floats);

/* Offset of the handle's first float (for diagnostics). */
size_t wubu_kvfs_handle_offset(const wubu_kvfs_handle_t *h);

/* Byte capacity of the handle's mount (in floats). */
size_t wubu_kvfs_handle_capacity(const wubu_kvfs_handle_t *h);

/* Release a handle (does not free the mount table). */
void wubu_kvfs_handle_close(wubu_kvfs_handle_t *h);

uint32_t wubu_kvfs_block_size(const wubu_kvfs_t *fs);

/* Convenience: one-shot path read/write. */
int wubu_kvfs_read(const wubu_kvfs_t *fs, const char *path,
                   const float *kv_base, float *dst, size_t n_floats);
int wubu_kvfs_write(wubu_kvfs_t *fs, const char *path,
                    float *kv_base, const float *src, size_t n_floats);

/* JSON view of the namespace (for /n/kv/snapshot). caller frees. */
char *wubu_kvfs_snapshot_json(const wubu_kvfs_t *fs, size_t *out_len);

int  wubu_kvfs_mount_count(const wubu_kvfs_t *fs);
void wubu_kvfs_free(wubu_kvfs_t *fs);

/* Kernel integration: the global KV namespace singleton + backing store. */
extern wubu_kvfs_t *g_wubu_kvfs;
extern float        *g_wubu_kv_base;   /* the actual KV tensor (ring-0) */
extern size_t        g_wubu_kv_capacity;

/* Called at kernel init; returns 0 on success, -1 on failure. */
int wubu_kvfs_kernel_init(uint32_t block_size, uint32_t total_blocks);

/* ---- THE DOCTRINE: the OS state IS the KV cache, served as 9P ----
 * These route /n/kv/* reads to the live KV tensor (not the on-disk
 * placeholder), so the filesystem and the KV cache are the SAME store.
 * Used by the Styx/9P server's read/write callbacks. */
const char *wubu_kvfs_route_path(const char *disk_path, const char *ns_root);

/* Serve a read of a KV path from the live tensor. Returns 0 on success. */
int wubu_kvfs_route_read(const char *kv_path, uint64_t offset,
                         uint32_t count, uint8_t *data, uint32_t *nread);

/* Serve a write to a KV path into the live tensor. Returns 0 on success. */
int wubu_kvfs_route_write(const char *kv_path, uint64_t offset,
                          uint32_t count, const uint8_t *data,
                          uint32_t *nwritten);

#endif /* WUBU_KVFS_KERNEL_H */
