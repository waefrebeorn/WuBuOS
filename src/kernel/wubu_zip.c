/*
 * wubu_zip.c -- the IN-KERNEL ZIP reader.
 *
 * Parses the End-Of-Central-Directory record + central directory of an
 * APPNOTE.TXT ZIP, then inflates members via wubu_inflate (the kernel's
 * own DEFLATE). No host libz, no host unzip. Allocations use the kernel
 * heap (mem_alloc/mem_free).
 *
 * Supports the two methods used by real game archives:
 *   0 = stored (no compression)
 *   8 = deflate (RFC 1951)
 */
#include "wubu_zip.h"
#include "wubu_zlib.h"
#include "memory.h"    /* mem_alloc / mem_free / mem_calloc (freestanding or hosted) */
#include <string.h>

/* ── little-endian readers ───────────────────────── */
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1]<<8)); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)(p[0] | (p[1]<<8) | (p[2]<<16) | (p[3]<<24));
}

#define SIG_EOCD  0x06054b50u   /* "PK\5\6" */
#define SIG_CDHDR 0x02014b50u   /* "PK\1\2" */
#define SIG_LOCHDR 0x04034b50u /* "PK\3\4" */

/* ── open: parse EOCD + central directory ────────── */
int wubu_zip_open(const uint8_t *data, uint32_t len, wubu_zip_archive *z) {
    if (!data || !z || len < 22) return -1;
    /* scan backwards for the EOCD signature */
    uint32_t start = (len > 65557) ? len - 65557 : 0;
    uint32_t eocd = (uint32_t)-1;
    for (uint32_t i = len - 22; i >= start; i--) {
        if (i + 22 <= len && rd32(data + i) == SIG_EOCD) { eocd = i; break; }
    }
    if (eocd == (uint32_t)-1) return -1;

    uint32_t cd_off = rd32(data + eocd + 16);
    uint16_t n = (uint16_t)rd16(data + eocd + 10);
    if (cd_off > len || n == 0) return -1;

    z->data = data;
    z->len = len;
    z->n = n;
    z->entries = (wubu_zip_entry *)mem_calloc(n, sizeof(wubu_zip_entry));
    if (!z->entries) return -1;

    uint32_t p = cd_off;
    for (uint32_t i = 0; i < n; i++) {
        if (p + 46 > len) { z->n = i; break; }
        if (rd32(data + p) != SIG_CDHDR) return -1;
        wubu_zip_entry *e = &z->entries[i];
        e->method    = rd16(data + p + 10);
        e->crc32     = rd32(data + p + 16);
        e->comp_size = rd32(data + p + 20);
        e->uncomp_size = rd32(data + p + 24);
        e->local_off = rd32(data + p + 42);
        e->name_len  = rd16(data + p + 28);
        e->extra_len = rd16(data + p + 30);
        uint16_t cl = rd16(data + p + 32);   /* comment length */
        uint32_t nl = e->name_len;
        if (nl > sizeof(e->name) - 1) nl = sizeof(e->name) - 1;
        memcpy(e->name, data + p + 46, nl);
        e->name[nl] = '\0';
        p += 46u + e->name_len + e->extra_len + cl;
        if (p > len) { z->n = i + 1; break; }
    }
    return 0;
}

uint32_t wubu_zip_count(const wubu_zip_archive *z) { return z ? z->n : 0; }
const wubu_zip_entry *wubu_zip_entry_at(const wubu_zip_archive *z, uint32_t i) {
    if (!z || i >= z->n) return NULL;
    return &z->entries[i];
}

int32_t wubu_zip_find(const wubu_zip_archive *z, const char *name) {
    if (!z || !name) return -1;
    for (uint32_t i = 0; i < z->n; i++) {
        if (strcmp(z->entries[i].name, name) == 0) return (int32_t)i;
    }
    return -1;
}

/* ── extract one entry ───────────────────────────────────────────── */
int wubu_zip_extract(const wubu_zip_archive *z, uint32_t i,
                     uint8_t **out, uint32_t *out_len) {
    if (!z || i >= z->n || !out || !out_len) return -1;
    const wubu_zip_entry *e = &z->entries[i];

    if (e->local_off + 30 > z->len) return -1;
    const uint8_t *lh = z->data + e->local_off;
    if (rd32(lh) != SIG_LOCHDR) return -1;
    uint16_t nl = rd16(lh + 26);
    uint16_t el = rd16(lh + 28);
    uint32_t comp_off = e->local_off + 30 + nl + el;
    if (comp_off + e->comp_size > z->len) return -1;

    uint32_t cap = e->uncomp_size ? e->uncomp_size : e->comp_size + 1;
    uint8_t *buf = (uint8_t *)mem_alloc(cap);
    if (!buf) return -1;

    int rc;
    if (e->method == 0) {                 /* stored */
        if (e->comp_size > cap) { mem_free(buf); return -1; }
        memcpy(buf, z->data + comp_off, e->comp_size);
        rc = 0;
    } else if (e->method == 8) {          /* deflate */
        uint32_t got = 0, consumed = 0;
        rc = wubu_inflate_raw(z->data + comp_off, e->comp_size,
                              buf, cap, &got, &consumed);
        if (rc == 0) {
            if (got != e->uncomp_size && e->uncomp_size) rc = -1; /* size mismatch */
        }
        *out_len = got;
    } else {
        mem_free(buf);
        return -1;                        /* unsupported method */
    }

    if (rc != 0) { mem_free(buf); return -1; }
    *out = buf;
    *out_len = (e->method == 0) ? e->comp_size : (*out_len);
    return 0;
}

void wubu_zip_close(wubu_zip_archive *z) {
    if (z && z->entries) { mem_free(z->entries); z->entries = NULL; z->n = 0; }
}
