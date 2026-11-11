/*
 * wubu_vkd3d_precache.c -- VKD3D slice 15: the shader pre-cache (fossilize).
 *
 * See wubu_vkd3d_precache.h.  The file format is deliberately simple:
 *
 *   magic[8] | version | vendor | device | driver | entry_count | entries[]
 *
 * followed by the blobs.  A header that does not validate makes the WHOLE file
 * unusable -- never a partial read.
 */
#include "wubu_vkd3d_precache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct wubu_vkd3d_precache {
    wubu_vkd3d_precache_fp_t fp;
    uint32_t                 count;
    wubu_vkd3d_precache_entry_t *entries;      /* malloc'd */
    uint8_t                 **blobs;           /* one malloc per entry */
    bool                     valid;
    bool                     modified;
};

static void put_u32(FILE *f, uint32_t v)
{
    uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8),
                     (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
    fwrite(b, 1, 4, f);
}

static bool get_u32(FILE *f, uint32_t *v)
{
    uint8_t b[4];
    if (fread(b, 1, 4, f) != 4) return false;
    *v = (uint32_t)b[0] | ((uint32_t)b[1] << 8) |
         ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
    return true;
}

static void put_u64(FILE *f, uint64_t v)
{
    for (int i = 0; i < 8; i++) {
        uint8_t b = (uint8_t)(v >> (i * 8));
        fwrite(&b, 1, 1, f);
    }
}

static bool get_u64(FILE *f, uint64_t *v)
{
    uint8_t b[8];
    if (fread(b, 1, 8, f) != 8) return false;
    uint64_t r = 0;
    for (int i = 0; i < 8; i++) r |= (uint64_t)b[i] << (i * 8);
    *v = r;
    return true;
}

wubu_vkd3d_precache_t *wubu_vkd3d_precache_open(const char *path,
                                                const wubu_vkd3d_precache_fp_t *fp)
{
    if (!path || !fp) return NULL;

    FILE *f = fopen(path, "rb");
    if (!f) return NULL;          /* no cache yet: a fresh, empty one */

    char magic[8];
    uint32_t version = 0, vendor = 0, device = 0, driver = 0, count = 0;
    bool ok = (fread(magic, 1, 8, f) == 8) &&
              memcmp(magic, VKD3D_PRECACHE_MAGIC, 8) == 0 &&
              get_u32(f, &version) &&
              get_u32(f, &vendor) &&
              get_u32(f, &device) &&
              get_u32(f, &driver) &&
              get_u32(f, &count);

    /* -- the whole file is discarded unless EVERYTHING validates -- */
    if (!ok || version != VKD3D_PRECACHE_VERSION ||
        vendor != fp->vendor_id || device != fp->device_id ||
        driver != fp->driver_version ||
        count > VKD3D_PRECACHE_MAX_ENTRIES) {
        fclose(f);
        return NULL;
    }

    wubu_vkd3d_precache_t *c = (wubu_vkd3d_precache_t *)calloc(1, sizeof(*c));
    if (!c) { fclose(f); return NULL; }
    c->fp = *fp;
    c->count = count;
    c->valid = true;
    c->entries = (wubu_vkd3d_precache_entry_t *)
                 calloc(count ? count : 1, sizeof(*c->entries));
    c->blobs = (uint8_t **)calloc(count ? count : 1, sizeof(*c->blobs));
    if (!c->entries || !c->blobs) {
        fclose(f);
        free(c->entries); free(c->blobs); free(c);
        return NULL;
    }

    /* read the entry table */
    for (uint32_t i = 0; i < count; i++) {
        uint32_t key = 0, size = 0;
        uint64_t off = 0;
        if (!get_u32(f, &key) || !get_u32(f, &size) || !get_u64(f, &off)) {
            c->valid = false;
            break;
        }
        c->entries[i].key = key;
        c->entries[i].blob_size = size;
        c->entries[i].blob_offset = off;
    }

    /* read each blob, bounds-checked */
    for (uint32_t i = 0; c->valid && i < count; i++) {
        wubu_vkd3d_precache_entry_t *e = &c->entries[i];
        if (e->blob_size == 0 || e->blob_size > VKD3D_PRECACHE_MAX_BYTES ||
            e->blob_offset > VKD3D_PRECACHE_MAX_BYTES) {
            c->valid = false;
            break;
        }
        if (fseek(f, (long)e->blob_offset, SEEK_SET) != 0) {
            c->valid = false;
            break;
        }
        c->blobs[i] = (uint8_t *)malloc(e->blob_size);
        if (!c->blobs[i] || fread(c->blobs[i], 1, e->blob_size, f) != e->blob_size) {
            c->valid = false;
            break;
        }
    }

    fclose(f);
    if (!c->valid) {
        /* a partially-read cache must not be used at all */
        wubu_vkd3d_precache_destroy(c);
        return NULL;
    }
    return c;
}

wubu_vkd3d_precache_t *
wubu_vkd3d_precache_create(const wubu_vkd3d_precache_fp_t *fp)
{
    if (!fp) return NULL;
    wubu_vkd3d_precache_t *c = (wubu_vkd3d_precache_t *)calloc(1, sizeof(*c));
    if (!c) return NULL;
    c->fp = *fp;
    c->valid = true;
    return c;
}

void wubu_vkd3d_precache_destroy(wubu_vkd3d_precache_t *c)
{
    if (!c) return;
    if (c->blobs) {
        for (uint32_t i = 0; i < c->count; i++) free(c->blobs[i]);
        free(c->blobs);
    }
    free(c->entries);
    free(c);
}

const wubu_vkd3d_precache_entry_t *
wubu_vkd3d_precache_lookup(const wubu_vkd3d_precache_t *c, uint32_t key)
{
    if (!c || !c->valid) return NULL;
    for (uint32_t i = 0; i < c->count; i++) {
        if (c->entries[i].key == key) return &c->entries[i];
    }
    return NULL;                     /* a miss is not an error */
}

long wubu_vkd3d_precache_read(const wubu_vkd3d_precache_t *c,
                              const wubu_vkd3d_precache_entry_t *e,
                              void *out, size_t out_size)
{
    if (!c || !c->valid || !e || !out) return -1;
    uint32_t idx = (uint32_t)(e - c->entries);
    if (idx >= c->count) return -1;
    if (e->blob_size > out_size) return -1;
    memcpy(out, c->blobs[idx], e->blob_size);
    return (long)e->blob_size;
}

int wubu_vkd3d_precache_add(wubu_vkd3d_precache_t *c, uint32_t key,
                            const void *blob, uint32_t blob_size)
{
    if (!c || !c->valid || !blob || blob_size == 0) return -1;
    if (c->count >= VKD3D_PRECACHE_MAX_ENTRIES) return -1;   /* bounded */

    /* a recompile of the same key REFRESHES it rather than duplicating it */
    for (uint32_t i = 0; i < c->count; i++) {
        if (c->entries[i].key == key) {
            uint8_t *nb = (uint8_t *)malloc(blob_size);
            if (!nb) return -1;
            memcpy(nb, blob, blob_size);
            free(c->blobs[i]);
            c->blobs[i] = nb;
            c->entries[i].blob_size = blob_size;
            c->entries[i].blob_offset = 0;    /* recomputed on save */
            c->modified = true;
            return 0;
        }
    }

    uint32_t idx = c->count;
    wubu_vkd3d_precache_entry_t *ne = (wubu_vkd3d_precache_entry_t *)
        realloc(c->entries, (idx + 1) * sizeof(*c->entries));
    if (!ne) return -1;
    c->entries = ne;
    uint8_t **nb = (uint8_t **)realloc(c->blobs, (idx + 1) * sizeof(*c->blobs));
    if (!nb) return -1;
    c->blobs = nb;

    uint8_t *copy = (uint8_t *)malloc(blob_size);
    if (!copy) return -1;
    memcpy(copy, blob, blob_size);

    c->entries[idx].key = key;
    c->entries[idx].blob_size = blob_size;
    c->entries[idx].blob_offset = 0;
    c->blobs[idx] = copy;
    c->count = idx + 1;
    c->modified = true;
    return 0;
}

int wubu_vkd3d_precache_save(wubu_vkd3d_precache_t *c, const char *path)
{
    if (!c || !c->valid || !path) return -1;

    char tmp[4096];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);

    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;

    /* header */
    fwrite(VKD3D_PRECACHE_MAGIC, 1, 8, f);
    put_u32(f, VKD3D_PRECACHE_VERSION);
    put_u32(f, c->fp.vendor_id);
    put_u32(f, c->fp.device_id);
    put_u32(f, c->fp.driver_version);
    put_u32(f, c->count);

    /* the entry table, with blob offsets computed as we go.  The table is
     * 20 bytes per entry, so blobs start right after it. */
    /* each entry is key(4) + size(4) + offset(8) = 16 bytes -- NOT 20.  A
     * wrong stride here shifts every blob offset, so the file reads garbage
     * and the whole cache is discarded. */
    const uint64_t table_bytes = (uint64_t)c->count * 16u;
    uint64_t off = 8u + 5u * 4u + table_bytes;
    for (uint32_t i = 0; i < c->count; i++) {
        put_u32(f, c->entries[i].key);
        put_u32(f, c->entries[i].blob_size);
        put_u64(f, off);
        off += c->entries[i].blob_size;
    }
    for (uint32_t i = 0; i < c->count; i++) {
        if (fwrite(c->blobs[i], 1, c->entries[i].blob_size, f)
            != c->entries[i].blob_size) {
            fclose(f);
            remove(tmp);
            return -1;
        }
    }
    if (fclose(f) != 0) { remove(tmp); return -1; }

    /* ATOMIC: the rename is what makes a crash mid-write harmless. */
    if (rename(tmp, path) != 0) { remove(tmp); return -1; }
    c->modified = false;
    return 0;
}

uint32_t wubu_vkd3d_precache_size(const wubu_vkd3d_precache_t *c)
{
    return (c && c->valid) ? c->count : 0;
}

bool wubu_vkd3d_precache_valid(const wubu_vkd3d_precache_t *c)
{
    return c && c->valid;
}
