/*
 * wubu_zip.h -- the IN-KERNEL ZIP (APPNOTE.TXT) reader.
 *
 * Used to open real game archives that are ZIP containers: OpenArena
 * 0.8.8 ships as openarena-0.8.8.zip (method 0 store + method 8 deflate).
 * The kernel reads the central directory, locates a member, and inflates
 * it through wubu_inflate (the kernel's own DEFLATE — no libz, no host
 * unzip).
 *
 * Freestanding: allocations go through mem_alloc/mem_free. The archive
 * bytes are supplied by the caller (e.g. a VBE/ISO-backed reader or a
 * pre-loaded buffer); the kernel owns only the parsed entry table.
 */
#ifndef WUBU_ZIP_H
#define WUBU_ZIP_H

#include <stdint.h>
#include <stddef.h>

#ifdef WUBU_KERNEL
#  include "memory.h"
#else
#  include <stdlib.h>
#  include <string.h>
#endif

typedef struct wubu_zip_entry {
    char     name[256];      /* NUL-terminated entry path ('/' separators) */
    uint16_t method;         /* 0=store, 8=deflate */
    uint32_t comp_size;      /* compressed size in the local header */
    uint32_t uncomp_size;    /* uncompressed size */
    uint32_t crc32;          /* CRC-32 of the uncompressed data */
    uint32_t local_off;      /* offset of the local header in the file */
    uint16_t name_len;
    uint16_t extra_len;
} wubu_zip_entry;

typedef struct wubu_zip_archive {
    const uint8_t *data;
    uint32_t len;
    wubu_zip_entry *entries;
    uint32_t n;
} wubu_zip_archive;

/* Open an in-memory ZIP by parsing its end-of-central-directory record.
 * Returns 0 on success, -1 on malformed input. */
int wubu_zip_open(const uint8_t *data, uint32_t len, wubu_zip_archive *z);

/* Number of central-directory entries. */
uint32_t wubu_zip_count(const wubu_zip_archive *z);

/* Entry accessor (0 <= i < count). */
const wubu_zip_entry *wubu_zip_entry_at(const wubu_zip_archive *z, uint32_t i);

/* Find entry index by name (case-sensitive, NUL-terminated). Returns -1. */
int32_t wubu_zip_find(const wubu_zip_archive *z, const char *name);

/* Extract entry `i`: allocates *out_len bytes via mem_alloc, inflates if
 * needed. Caller frees with mem_free. Returns 0 on success, -1 on error. */
int wubu_zip_extract(const wubu_zip_archive *z, uint32_t i,
                     uint8_t **out, uint32_t *out_len);

/* Free the central-directory table (does NOT free the data buffer). */
void wubu_zip_close(wubu_zip_archive *z);

#endif /* WUBU_ZIP_H */
