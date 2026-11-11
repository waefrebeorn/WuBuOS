/*
 * wubu_vkd3d_precache.h -- VKD3D slice 15: the shader pre-cache (fossilize).
 *
 * Slice 4 caches pipelines IN MEMORY; this persists that cache to disk so a
 * second launch does not recompile shaders -- the stutter VKD3D calls "pipeline
 * stutter" and Steam ships Fossilize to avoid.
 *
 * The rules that make a disk cache safe rather than a source of corruption:
 *
 *   * A pipeline is DEVICE-SPECIFIC.  A cache built on one GPU is not valid on
 *     another, so the file is bound to vendor id + device id + driver version
 *     and must be DISCARDED on a mismatch -- not merged, not reused.
 *   * The cache has a VERSION.  A format change invalidates every entry.
 *   * A truncated or corrupt file is discarded whole, never used partially:
 *     half a pipeline blob is worse than no blob.
 *   * The cache is BOUNDED.  When full, new entries are rejected rather than
 *     evicting -- the same rule slice 4's in-memory cache uses, and for the
 *     same reason: an eviction turns a warm cache into a cold one.
 *   * A save is ATOMIC (write a temp file, then rename) so a crash mid-write
 *     cannot leave a half-written cache that a later launch would trust.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_VKD3D_PRECACHE_H
#define WUBU_VKD3D_PRECACHE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define VKD3D_PRECACHE_MAGIC     "WBVKD3D1"
#define VKD3D_PRECACHE_VERSION   1u
#define VKD3D_PRECACHE_MAX_BYTES (16u * 1024u * 1024u)
#define VKD3D_PRECACHE_MAX_ENTRIES 4096u

/* The device fingerprint a cache is bound to. */
typedef struct {
    uint32_t vendor_id;
    uint32_t device_id;
    uint32_t driver_version;
} wubu_vkd3d_precache_fp_t;

/* An on-disk entry: the slice-4 PSO key plus where its blob lives. */
typedef struct {
    uint32_t key;
    uint32_t blob_size;
    uint64_t blob_offset;
} wubu_vkd3d_precache_entry_t;

typedef struct wubu_vkd3d_precache wubu_vkd3d_precache_t;

/* Open (or create) a cache file.  `fp` is the device the cache must belong to.
 * Returns NULL when the file is absent (a fresh cache), unreadable, or belongs
 * to a DIFFERENT device/version -- in every one of those cases a caller should
 * treat it as an empty cache rather than an error. */
wubu_vkd3d_precache_t *wubu_vkd3d_precache_open(const char *path,
                                                const wubu_vkd3d_precache_fp_t *fp);

/* Create an EMPTY in-memory cache for this device.  open() returns NULL when
 * the file is absent; this is what a caller uses instead of treating a cold
 * start as an error. */
wubu_vkd3d_precache_t *
wubu_vkd3d_precache_create(const wubu_vkd3d_precache_fp_t *fp);

void wubu_vkd3d_precache_destroy(wubu_vkd3d_precache_t *c);

/* Look up a PSO key.  Returns the entry, or NULL for a miss (a miss is not an
 * error -- it is the normal cold-cache case). */
const wubu_vkd3d_precache_entry_t *
wubu_vkd3d_precache_lookup(const wubu_vkd3d_precache_t *c, uint32_t key);

/* Read an entry's blob.  Returns the size read, or -1 for a bad offset/size
 * (which must never happen for an entry from a validated file). */
long wubu_vkd3d_precache_read(const wubu_vkd3d_precache_t *c,
                              const wubu_vkd3d_precache_entry_t *e,
                              void *out, size_t out_size);

/* Add an entry.  Returns 0 on success, -1 when the cache is full.  Adding a
 * key that already exists REFRESHES it (a recompile is still a valid cache). */
int wubu_vkd3d_precache_add(wubu_vkd3d_precache_t *c, uint32_t key,
                            const void *blob, uint32_t blob_size);

/* Write the cache back ATOMICALLY.  Returns 0 on success. */
int wubu_vkd3d_precache_save(wubu_vkd3d_precache_t *c, const char *path);

uint32_t wubu_vkd3d_precache_size(const wubu_vkd3d_precache_t *c);
bool     wubu_vkd3d_precache_valid(const wubu_vkd3d_precache_t *c);

#endif /* WUBU_VKD3D_PRECACHE_H */
