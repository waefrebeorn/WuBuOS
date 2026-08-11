/*
 * wubu_kvfs.c — KV namespace layer (G1: path-addressable KV cache).
 *
 * Metal port of wubuwizard/src/wubu_kvfs.c into WuBuOS kernel.
 * Kernel-owned: the KV cache IS the OS's address space, exported to
 * the AGI via /n/kv/ over 9P and fed back into the training loop as
 * world-state deltas.
 *
 * Mount table = FNV-1a open-addressing hash (O(1) lookup, one probe
 * per path segment). Handles precompute abs_offset/abs_limit so the
 * hot read/write is a bounds check + memcpy — zero string ops.
 *
 * C11, opaque structs, minimal includes. No third-party deps.
 */
#define _POSIX_C_SOURCE 200809L
#include "wubu_kvfs.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define KVFS_SLOT_EMPTY 0u
#define KVFS_SLOT_LIVE  1u
#define KVFS_SLOT_TOMB  2u

/* ---- namespace handle ---- */
struct wubu_kvfs {
    wubu_kvfs_mount_t *mounts;
    int                n_mounts;
    int                cap_mounts;
    int                n_live;
    uint32_t           block_size;
    uint32_t           total_blocks;
    uint32_t           used_blocks;
    uint64_t          *hash_keys;
    int32_t            *hash_vals;
    uint8_t            *hash_state;
    int                 n_slots;
};

/* A resolved handle: precomputed absolute offset + capacity. */
struct wubu_kvfs_handle {
    const wubu_kvfs_t *fs;
    size_t             abs_offset;
    size_t             abs_limit;
};

/* ---- globals (the kernel singleton) ---- */
wubu_kvfs_t *g_wubu_kvfs = NULL;
float       *g_wubu_kv_base = NULL;
size_t       g_wubu_kv_capacity = 0;

/* ---- FNV-1a hash ---- */
static uint64_t fnv1a(const char *s) {
    uint64_t h = 0xcbf29ce484222325ULL;
    while (*s) {
        h ^= (uint8_t)*s;
        h *= 0x100000001b3ULL;
        s++;
    }
    return h;
}

/* ---- mount table growth ---- */
static int kvfs_grow(wubu_kvfs_t *fs) {
    int new_cap = fs->cap_mounts ? fs->cap_mounts * 2 : 16;
    size_t old = fs->n_mounts * sizeof(wubu_kvfs_mount_t);
    size_t nu  = new_cap * sizeof(wubu_kvfs_mount_t);
    wubu_kvfs_mount_t *nm = (wubu_kvfs_mount_t *)calloc(new_cap, sizeof(wubu_kvfs_mount_t));
    if (!nm) return -1;
    memcpy(nm, fs->mounts, old);
    free(fs->mounts);
    fs->mounts = nm;
    fs->cap_mounts = new_cap;
    return 0;
}

/* ---- hash table rehash ---- */
static int kvfs_rehash(wubu_kvfs_t *fs, int new_slots) {
    uint64_t *nk = (uint64_t *)calloc(new_slots, sizeof(uint64_t));
    int32_t  *nv = (int32_t *)calloc(new_slots, sizeof(int32_t));
    uint8_t  *ns = (uint8_t *)calloc(new_slots, sizeof(uint8_t));
    if (!nk || !nv || !ns) {
        free(nk); free(nv); free(ns);
        return -1;
    }
    uint64_t *ok = fs->hash_keys;
    int32_t  *ov = fs->hash_vals;
    uint8_t  *os = fs->hash_state;
    fs->hash_keys = nk;
    fs->hash_vals = nv;
    fs->hash_state = ns;
    fs->n_slots = new_slots;
    /* re-insert all live mounts */
    for (int i = 0; i < fs->n_mounts; i++) {
        if (fs->mounts[i].n_blocks == 0) continue;
        uint64_t h = fnv1a(fs->mounts[i].path);
        int slot = (int)(h % (uint64_t)new_slots);
        for (int p = 0; p < new_slots; p++) {
            if (fs->hash_state[slot] == KVFS_SLOT_EMPTY) {
                fs->hash_state[slot] = KVFS_SLOT_LIVE;
                fs->hash_keys[slot] = h;
                fs->hash_vals[slot] = i;
                break;
            }
            slot = (slot + 1) % new_slots;
        }
    }
    free(ok); free(ov); free(os);
    return 0;
}

/* ---- find mount index by exact path in hash table ---- */
static int kvfs_find_slot(const wubu_kvfs_t *fs, const char *path) {
    uint64_t h = fnv1a(path);
    int mask = fs->n_slots - 1;
    int slot = (int)(h & (uint64_t)mask);
    for (int p = 0; p < fs->n_slots; p++) {
        if (fs->hash_state[slot] == KVFS_SLOT_EMPTY) return -1;
        if (fs->hash_state[slot] == KVFS_SLOT_LIVE &&
            fs->hash_keys[slot] == h)
            return slot;
        slot = (slot + 1) & mask;
    }
    return -1;
}

/* ---- find the longest-prefix mounted parent of `path` ---- */
static int kvfs_find_mount(const wubu_kvfs_t *fs, const char *path) {
    /* Walk up the path segments: try full path, then strip last segment, etc. */
    char buf[256];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    size_t len = strlen(buf);
    /* Ensure no trailing slash (except root "/") */
    while (len > 1 && buf[len - 1] == '/') buf[--len] = '\0';
    while (1) {
        int slot = kvfs_find_slot(fs, buf);
        if (slot >= 0) return fs->hash_vals[slot];
        /* strip last path segment */
        char *slash = strrchr(buf, '/');
        if (!slash || slash == buf) break;  /* reached "/" */
        *slash = '\0';
    }
    /* try root "/" */
    if (buf[0] == '/') return kvfs_find_slot(fs, "/");
    return -1;
}

/* ---- create ---- */
wubu_kvfs_t *wubu_kvfs_create(uint32_t block_size, uint32_t total_blocks) {
    wubu_kvfs_t *fs = (wubu_kvfs_t *)calloc(1, sizeof(wubu_kvfs_t));
    if (!fs) return NULL;
    fs->block_size = block_size;
    fs->total_blocks = total_blocks;
    fs->cap_mounts = 0;
    fs->n_mounts = 0;
    fs->n_live = 0;
    fs->used_blocks = 0;
    /* hash table: round up to power of 2, min 16 */
    int slots = 16;
    while (slots < (int)(total_blocks / 4) && slots < 4096) slots *= 2;
    if (kvfs_rehash(fs, slots) != 0) {
        free(fs);
        return NULL;
    }
    /* Pre-mount the canonical / (root) so root-relative paths resolve. */
    fs->mounts = (wubu_kvfs_mount_t *)calloc(1, sizeof(wubu_kvfs_mount_t));
    if (!fs->mounts) { free(fs); return NULL; }
    fs->cap_mounts = 1;
    /* root mount: covers everything, offset 0 */
    strncpy(fs->mounts[0].path, "/", 255);
    fs->mounts[0].start_block = 0;
    fs->mounts[0].n_blocks = total_blocks;
    fs->mounts[0].block_size = block_size;
    fs->mounts[0].abs_offset = 0;
    fs->mounts[0].abs_limit = (size_t)total_blocks * block_size;
    fs->n_mounts = 1;
    fs->n_live = 1;
    /* insert root into hash */
    uint64_t h = fnv1a("/");
    fs->hash_keys[0] = h;
    fs->hash_vals[0] = 0;
    fs->hash_state[0] = KVFS_SLOT_LIVE;
    return fs;
}

/* ---- mount ---- */
int wubu_kvfs_mount(wubu_kvfs_t *fs, const char *path,
                    uint32_t start_block, uint32_t n_blocks) {
    if (!fs || !path || n_blocks == 0) return -1;
    if (start_block + n_blocks > fs->total_blocks) return -1;
    if (wubu_kvfs_lookup(fs, path, NULL, NULL) == 0) return -1; /* already mounted */

    if (fs->n_mounts >= fs->cap_mounts) {
        if (kvfs_grow(fs) != 0) return -1;
    }
    int idx = fs->n_mounts;
    fs->n_mounts++;
    wubu_kvfs_mount_t *m = &fs->mounts[idx];
    memset(m, 0, sizeof(*m));
    strncpy(m->path, path, 255);
    m->path[255] = '\0';
    m->start_block = start_block;
    m->n_blocks = n_blocks;
    m->block_size = fs->block_size;
    m->abs_offset = (size_t)start_block * fs->block_size;
    m->abs_limit =  (size_t)(start_block + n_blocks) * fs->block_size;
    fs->used_blocks += n_blocks;
    fs->n_live++;

    /* insert into hash */
    if (fs->n_live * 2 > fs->n_slots) {
        if (kvfs_rehash(fs, fs->n_slots * 2) != 0) return -1;
    }
    uint64_t h = fnv1a(path);
    int mask = fs->n_slots - 1;
    int slot = (int)(h & (uint64_t)mask);
    for (int p = 0; p < fs->n_slots; p++) {
        if (fs->hash_state[slot] == KVFS_SLOT_EMPTY ||
            fs->hash_state[slot] == KVFS_SLOT_TOMB) {
            fs->hash_state[slot] = KVFS_SLOT_LIVE;
            fs->hash_keys[slot] = h;
            fs->hash_vals[slot] = idx;
            return 0;
        }
        slot = (slot + 1) & mask;
    }
    return -1; /* should not happen if rehash kept load factor low */
}

/* ---- unmount ---- */
int wubu_kvfs_unmount(wubu_kvfs_t *fs, const char *path) {
    if (!fs || !path) return -1;
    int slot = kvfs_find_slot(fs, path);
    if (slot < 0) return -1;
    int idx = fs->hash_vals[slot];
    /* Don't allow unmounting root "/" */
    if (strcmp(fs->mounts[idx].path, "/") == 0) return -1;
    fs->hash_state[slot] = KVFS_SLOT_TOMB;
    fs->mounts[idx].n_blocks = 0;  /* mark dead */
    fs->n_live--;
    return 0;
}

/* ---- lookup ---- */
int wubu_kvfs_lookup(const wubu_kvfs_t *fs, const char *path,
                     uint32_t *out_block, size_t *out_offset) {
    if (!fs || !path) return -1;
    int idx = kvfs_find_mount(fs, path);
    if (idx < 0) return -1;
    const wubu_kvfs_mount_t *m = &fs->mounts[idx];
    /* offset within the mount */
    size_t rel = 0;
    if (strcmp(m->path, "/") != 0 && strcmp(m->path, path) != 0) {
        /* path is under the mount: compute relative offset from the
         * suffix after the mount path. For the kernel, a simple
         * segment-hash offset suffices (the training loop uses
         * fixed paths, not arbitrary sub-files). */
        size_t mlen = strlen(m->path);
        const char *sub = path + mlen;
        while (*sub == '/') sub++;
        rel = fnv1a(sub) % (m->abs_limit - m->abs_offset);
    }
    if (out_block)  *out_block  = m->start_block;
    if (out_offset) *out_offset = (size_t)rel;
    return 0;
}

/* ---- hot handle ---- */
wubu_kvfs_handle_t *wubu_kvfs_open(const wubu_kvfs_t *fs, const char *path) {
    uint32_t blk; size_t off;
    if (wubu_kvfs_lookup(fs, path, &blk, &off) != 0) return NULL;
    const wubu_kvfs_mount_t *m = NULL;
    /* find the mount struct for the resolved path */
    int idx = kvfs_find_mount(fs, path);
    if (idx < 0 || idx >= fs->n_mounts) return NULL;
    m = &fs->mounts[idx];
    wubu_kvfs_handle_t *h = (wubu_kvfs_handle_t *)calloc(1, sizeof(*h));
    if (!h) return NULL;
    h->fs = fs;
    h->abs_offset = m->abs_offset + off;
    h->abs_limit = m->abs_limit;
    return h;
}

int wubu_kvfs_handle_read(const wubu_kvfs_handle_t *h,
                          const float *kv_base, float *dst, size_t n) {
    if (!h || !kv_base || !dst) return -1;
    if (h->abs_offset + n > h->abs_limit) return -1;
    memcpy(dst, kv_base + h->abs_offset, n * sizeof(float));
    return 0;
}

int wubu_kvfs_handle_write(const wubu_kvfs_handle_t *h,
                           float *kv_base, const float *src, size_t n) {
    if (!h || !kv_base || !src) return -1;
    if (h->abs_offset + n > h->abs_limit) return -1;
    memcpy(kv_base + h->abs_offset, src, n * sizeof(float));
    return 0;
}

size_t wubu_kvfs_handle_offset(const wubu_kvfs_handle_t *h) {
    return h ? h->abs_offset : 0;
}

size_t wubu_kvfs_handle_capacity(const wubu_kvfs_handle_t *h) {
    return h ? (h->abs_limit - h->abs_offset) : 0;
}

void wubu_kvfs_handle_close(wubu_kvfs_handle_t *h) {
    free(h);
}

uint32_t wubu_kvfs_block_size(const wubu_kvfs_t *fs) {
    return fs ? fs->block_size : 0;
}

/* ---- convenience ---- */
int wubu_kvfs_read(const wubu_kvfs_t *fs, const char *path,
                   const float *kv_base, float *dst, size_t n) {
    wubu_kvfs_handle_t *h = wubu_kvfs_open(fs, path);
    if (!h) return -1;
    int rc = wubu_kvfs_handle_read(h, kv_base, dst, n);
    wubu_kvfs_handle_close(h);
    return rc;
}

int wubu_kvfs_write(wubu_kvfs_t *fs, const char *path,
                    float *kv_base, const float *src, size_t n) {
    wubu_kvfs_handle_t *h = wubu_kvfs_open(fs, path);
    if (!h) return -1;
    int rc = wubu_kvfs_handle_write(h, kv_base, src, n);
    wubu_kvfs_handle_close(h);
    return rc;
}

/* ---- JSON snapshot ---- */
char *wubu_kvfs_snapshot_json(const wubu_kvfs_t *fs, size_t *out_len) {
    if (!fs) return NULL;
    /* rough: ~80 bytes per mount */
    size_t cap = 128 + (size_t)fs->n_live * 80;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    int n = snprintf(buf, cap, "{\"block_size\":%u,\"total_blocks\":%u,"
                         "\"used_blocks\":%u,\"mounts\":[",
                     fs->block_size, fs->total_blocks, fs->used_blocks);
    for (int i = 0; i < fs->n_mounts && n < (int)cap - 1; i++) {
        if (fs->mounts[i].n_blocks == 0) continue;
        n += snprintf(buf + n, cap - n, "%s{\"path\":\"%s\",\"start\":%u,\"blocks\":%u}",
                      (n > 0 && buf[n-1] != '[') ? "," : "",
                      fs->mounts[i].path,
                      fs->mounts[i].start_block,
                      fs->mounts[i].n_blocks);
    }
    n += snprintf(buf + n, cap - n, "]}");
    if (out_len) *out_len = (size_t)n;
    return buf;
}

int wubu_kvfs_mount_count(const wubu_kvfs_t *fs) {
    return fs ? fs->n_live : 0;
}

void wubu_kvfs_free(wubu_kvfs_t *fs) {
    if (!fs) return;
    free(fs->mounts);
    free(fs->hash_keys);
    free(fs->hash_vals);
    free(fs->hash_state);
    free(fs);
}

/* ---- kernel init ---- */
int wubu_kvfs_kernel_init(uint32_t block_size, uint32_t total_blocks) {
    if (g_wubu_kvfs) return -1; /* already initialized */
    g_wubu_kvfs = wubu_kvfs_create(block_size, total_blocks);
    if (!g_wubu_kvfs) return -1;
    size_t total_floats = (size_t)total_blocks * block_size;
    /* The KV tensor backing store — ring-0 memory. */
    g_wubu_kv_base = (float *)calloc(total_floats, sizeof(float));
    if (!g_wubu_kv_base) {
        wubu_kvfs_free(g_wubu_kvfs);
        g_wubu_kvfs = NULL;
        return -1;
    }
    g_wubu_kv_capacity = total_floats;
    return 0;
}

/* ---- THE DOCTRINE: the OS state IS the KV cache, served as 9P ----
 *
 * The namespace /n tree is laid down on disk (so the host Styx server can
 * serve it), but the /n/kv subtree is LIVE: reads must return the kernel
 * KV tensor, not the on-disk placeholder. These helpers translate a
 * namespace disk path (e.g. "<ns_root>/kv/world/tick_3") into the KV
 * path ("/kv/world/tick_3") and serve the read straight from the tensor.
 */

/* Translate a namespace disk path to its KV path (e.g. "/kv/world/tick_3").
 * Returns a pointer into a static buffer, or NULL if the path is not under
 * the KV namespace (caller should fall back to a normal disk read). */
const char *wubu_kvfs_route_path(const char *disk_path, const char *ns_root)
{
    if (!disk_path || !ns_root) return NULL;
    const char *kv_marker = "/kv/";
    /* the disk path is <ns_root>/kv/... ; find the "/kv/" suffix */
    const char *found = strstr(disk_path, kv_marker);
    if (!found) return NULL;
    /* verify it's really under the ns_root (avoid matching unrelated paths) */
    if (strncmp(disk_path, ns_root, strlen(ns_root)) != 0) return NULL;
    return found;  /* "/kv/world/tick_3" */
}

/* Serve a read of a KV path from the live tensor. Reads n_floats = count/4.
 * Returns 0 on success (nread set), -1 if path unmounted/not-resolvable. */
int wubu_kvfs_route_read(const char *kv_path, uint64_t offset,
                         uint32_t count, uint8_t *data, uint32_t *nread)
{
    if (!kv_path || !g_wubu_kvfs || !g_wubu_kv_base) return -1;

    uint32_t blk; size_t rel;
    if (wubu_kvfs_lookup(g_wubu_kvfs, kv_path, &blk, &rel) != 0) return -1;

    /* The mount resolves to a base offset; rel is the hash of the suffix.
     * Read count/4 floats starting at (base + rel + offset/4). */
    const wubu_kvfs_mount_t *m = NULL;
    int idx = kvfs_find_mount(g_wubu_kvfs, kv_path);
    if (idx < 0 || idx >= g_wubu_kvfs->n_mounts) return -1;
    m = &g_wubu_kvfs->mounts[idx];

    size_t start_float = (size_t)(offset / 4);
    size_t n_floats = (size_t)(count / 4);
    if (n_floats == 0) n_floats = 1;
    size_t base = m->abs_offset + rel;

    if (base + n_floats > g_wubu_kv_capacity) return -1;
    memcpy(data, (const uint8_t *)(g_wubu_kv_base + base + start_float),
           n_floats * sizeof(float));
    *nread = (uint32_t)(n_floats * sizeof(float));
    return 0;
}

/* Serve a write to a KV path into the live tensor (mirror of route_read). */
int wubu_kvfs_route_write(const char *kv_path, uint64_t offset,
                          uint32_t count, const uint8_t *data, uint32_t *nwritten)
{
    if (!kv_path || !g_wubu_kvfs || !g_wubu_kv_base) return -1;

    uint32_t blk; size_t rel;
    if (wubu_kvfs_lookup(g_wubu_kvfs, kv_path, &blk, &rel) != 0) return -1;

    int idx = kvfs_find_mount(g_wubu_kvfs, kv_path);
    if (idx < 0 || idx >= g_wubu_kvfs->n_mounts) return -1;
    const wubu_kvfs_mount_t *m = &g_wubu_kvfs->mounts[idx];

    size_t start_float = (size_t)(offset / 4);
    size_t n_floats = (size_t)(count / 4);
    if (n_floats == 0) n_floats = 1;
    size_t base = m->abs_offset + rel;

    if (base + n_floats > g_wubu_kv_capacity) return -1;
    memcpy((uint8_t *)(g_wubu_kv_base + base + start_float), data,
           n_floats * sizeof(float));
    *nwritten = (uint32_t)(n_floats * sizeof(float));
    return 0;
}
