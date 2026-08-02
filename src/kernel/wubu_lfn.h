/*
 * wubu_lfn.h  --  FAT32 VFAT Long File Name codec (self-contained)
 *
 * Gap A16: the FAT32 driver was 8.3-only. This module encodes/decodes
 * the VFAT LFN directory-entry chain: 13 UTF-16LE chars per entry,
 * the 0x0F attribute byte, the ordinal/checksum fields, and the
 * reverse chain reconstruction. Pure C11 + opaque-friendly: the caller
 * supplies the buffers (freestanding-safe, no heap).
 */
#ifndef WUBU_LFN_H
#define WUBU_LFN_H

#include <stdint.h>
#include <stddef.h>

#define WUBU_LFN_CHARS_PER_ENTRY  13   /* UTF-16LE code units per entry */
#define WUBU_LFN_ENTRY_SZ         32   /* a full 32-byte dir entry      */
#define WUBU_LFN_MAX_NAME         260  /* max reconstructed chars       */
#define WUBU_LFN_MAX_ENTRIES      20   /* ceil(260/13) */

/* LFN entry attribute (the 0x0F magic byte). */
#define WUBU_LFN_ATTR  0x0F

/* Encode one 13-char chunk (offset is the chunk index, 0-based) into a
 * 32-byte LFN entry. `chunk` is 13 UTF-16LE code units (may be shorter;
 * the remainder is padded with 0xFFFF + the terminator 0x0000). The
 * caller zeroes the entry first. */
void wubu_lfn_encode_chunk(uint8_t entry[WUBU_LFN_ENTRY_SZ],
                           const uint16_t *chunk, int nchars,
                           int offset, int total_entries,
                           uint8_t checksum);

/* Decode one 32-byte LFN entry into up to 13 UTF-16LE code units.
 * Returns the number of units decoded (stops at the 0x0000 terminator
 * or the 0xFFFF padding). Returns -1 if the entry is not an LFN entry
 * (attribute != 0x0F). */
int wubu_lfn_decode_chunk(const uint8_t entry[WUBU_LFN_ENTRY_SZ],
                          uint16_t out[WUBU_LFN_CHARS_PER_ENTRY]);

/* Build the full LFN entry chain for `name` (the base name, no path):
 * returns the number of entries written into `entries` (each 32 bytes)
 * or 0 if the name fits 8.3 (no LFN entries needed). */
int wubu_lfn_build_entries(const char *name,
                           uint8_t entries[][WUBU_LFN_ENTRY_SZ],
                           int max_entries);

/* Reconstruct the long name from a chain of LFN entries (given in
 * on-disk order, i.e. LAST chunk first). Returns 0 on success (the
 * name in `out`), -1 on a malformed chain. */
int wubu_lfn_reconstruct(const uint8_t entries[][WUBU_LFN_ENTRY_SZ],
                         int n_entries, char *out, size_t outsz);

/* VFAT checksum of an 11-byte 8.3 name (exposed for the dir layer). */
uint8_t wubu_lfn_checksum(const char name83[11]);

#endif
