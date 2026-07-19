/*
 * fat32.c  --  WuBuOS FAT32 Filesystem (facade)
 *
 * This file is the public entry point only. The real work is split into
 * self-contained leaf modules behind the opaque fat32_volume (fat32_internal.h):
 *   - fat32_fat.c    : FAT entry read/write + cache
 *   - fat32_dir.c    : directory enumerate / find / create / delete
 *   - fat32_file.c   : open / close / read / write / seek
 *   - fat32_format.c : mount / unmount / format / validate
 *   - fat32_cluster.c: cluster-chain walk / alloc / free (shared by all)
 *
 * Each module includes only fat32_internal.h + <stdlib/string/time.h>. No god
 * header, no duplicated logic. The facade below holds the single shared
 * DOS-timestamp helper (datetime_to_dos) used by the dir/file modules.
 */

#include "fat32_internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* -- Internal: DOS Date/Time --------------------------------------
 * Shared by fat32_dir.c (create) and fat32_file.c (close/write-time).
 * Convert a Unix epoch to the FAT 16-bit date / time packed fields. */
void datetime_to_dos(time_t t, uint16_t *dos_time, uint16_t *dos_date) {
    struct tm *tm_info = gmtime(&t);
    if (!tm_info) {
        *dos_time = 0;
        *dos_date = 0x0021;  /* 1980-01-01 */
        return;
    }
    int year = tm_info->tm_year + 1900;
    if (year < 1980) year = 1980;
    *dos_date = (uint16_t)((tm_info->tm_mday) | ((tm_info->tm_mon + 1) << 5) | ((year - 1980) << 9));
    *dos_time = (uint16_t)((tm_info->tm_sec / 2) | (tm_info->tm_min << 5) | (tm_info->tm_hour << 11));
}
