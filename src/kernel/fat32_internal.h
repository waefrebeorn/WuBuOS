/*
 * fat32_internal.h -- internal (non-public) declarations for the
 * FAT32 filesystem modules split out of fat32.c.
 *
 * These were previously file-local `static` helpers. They are now shared
 * between fat32.c and the extracted leaf modules (fat32_name.c,
 * fat32_cluster.c), so they are promoted to non-static and declared here.
 * Public API stays in fat32.h. No god-header: this file only carries the
 * promoted helpers' signatures, nothing else.
 */
#ifndef WUBU_FAT32_INTERNAL_H
#define WUBU_FAT32_INTERNAL_H

#include <time.h>
#include "fat32.h"   /* fat32_volume (opaque fwd), macros, public API */

/* ====================================================================
 * Concrete volume state. OPAQUE to public consumers (fat32.h exposes only
 * a forward declaration `typedef struct fat32_volume fat32_volume;`); every
 * self-contained module below includes this header so it can reach the
 * fields. This is the single source of truth for the volume layout -- no
 * god-header, no field leakage into the public API.
 * NOTE: defined as `struct fat32_volume` (not a second typedef) so it matches
 * the forward typedef in fat32.h without a conflicting redefinition.
 * ==================================================================== */
struct fat32_volume {
    fat32_blk_ops     blk;          /* Block device operations */
    bool               mounted;     /* Volume is mounted and valid */
    uint32_t           sectors_per_cluster;
    uint32_t           bytes_per_cluster;
    uint32_t           reserved_sectors;
    uint32_t           num_fats;
    uint32_t           sectors_per_fat;
    uint32_t           root_cluster;
    uint64_t           fat1_lba;    /* First FAT table LBA */
    uint64_t           fat2_lba;    /* Second FAT table LBA */
    uint64_t           data_lba;    /* First data sector LBA */
    uint32_t           total_clusters;
    uint32_t           free_clusters; /* Cached count */
    uint32_t           next_free;     /* Hint for next alloc */
    uint64_t           fs_info_lba;
    /* Cached FAT sector */
    uint32_t           cached_fat_sector;
    uint32_t          *fat_cache;     /* One sector of FAT entries */
};

/* FAT entry read/write (shared by cluster ops + create/delete/write) */
int fat_read_entry(fat32_volume *vol, uint32_t cluster, uint32_t *out);
int fat_write_entry(fat32_volume *vol, uint32_t cluster, uint32_t value);
void fat_cache_invalidate(fat32_volume *vol);

/* DOS timestamp helper (shared by dir create + file write-time) */
void datetime_to_dos(time_t t, uint16_t *dos_time, uint16_t *dos_date);

/* 8.3 name conversion + helpers (shared by dir_read/create/find) */
void name_to_83(const char *src, char name83[11]);
void name_from_83(const char name83[11], char *dst, size_t dst_size);
uint8_t lfn_checksum(const char name83[11]);
int name_cmp(const char *a, const char *b);

#endif /* WUBU_FAT32_INTERNAL_H */
