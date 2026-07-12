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

#include "fat32.h"   /* fat32_volume, macros, public API */

/* FAT entry read/write (shared by cluster ops + create/delete/write) */
int fat_read_entry(fat32_volume *vol, uint32_t cluster, uint32_t *out);
int fat_write_entry(fat32_volume *vol, uint32_t cluster, uint32_t value);

/* 8.3 name conversion + helpers (shared by dir_read/create/find) */
void name_to_83(const char *src, char name83[11]);
void name_from_83(const char name83[11], char *dst, size_t dst_size);
uint8_t lfn_checksum(const char name83[11]);
int name_cmp(const char *a, const char *b);

#endif /* WUBU_FAT32_INTERNAL_H */
