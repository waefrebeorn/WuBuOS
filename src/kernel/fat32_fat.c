/* fat32_fat.c -- FAT entry read/write + cache (leaf module).
 * Opaque fat32_volume via fat32_internal.h. C11, minimal includes. */
#include "fat32_internal.h"
#include <stdlib.h>
#include <string.h>

/* Read a single FAT entry (4 bytes) for given cluster */
int fat_read_entry(fat32_volume *vol, uint32_t cluster, uint32_t *out) {
    /* FAT entries are 4 bytes each, 128 per sector */
    uint32_t entries_per_sector = FAT32_SECTOR_SIZE / 4;
    uint32_t fat_sector = cluster / entries_per_sector;
    uint32_t fat_offset = cluster % entries_per_sector;
    uint64_t lba = vol->fat1_lba + fat_sector;

    /* Cache the FAT sector if not already cached */
    if (!vol->fat_cache || fat_sector != vol->cached_fat_sector) {
        if (!vol->fat_cache)
            vol->fat_cache = (uint32_t *)malloc(FAT32_SECTOR_SIZE);
        if (!vol->fat_cache) return -1;

        int rc = vol->blk.read(vol->blk.ctx, lba, 1, vol->fat_cache);
        if (rc != 0) return -1;
        vol->cached_fat_sector = fat_sector;
    }

    *out = vol->fat_cache[fat_offset] & 0x0FFFFFFF;
    return 0;
}

/* Write a single FAT entry */
int fat_write_entry(fat32_volume *vol, uint32_t cluster, uint32_t value) {
    uint32_t entries_per_sector = FAT32_SECTOR_SIZE / 4;
    uint32_t fat_sector = cluster / entries_per_sector;
    uint32_t fat_offset = cluster % entries_per_sector;
    uint64_t lba = vol->fat1_lba + fat_sector;

    /* Read-modify-write the FAT sector */
    if (!vol->fat_cache)
        vol->fat_cache = (uint32_t *)malloc(FAT32_SECTOR_SIZE);
    if (!vol->fat_cache) return -1;

    /* If wrong sector cached, read the right one */
    if (fat_sector != vol->cached_fat_sector) {
        int rc = vol->blk.read(vol->blk.ctx, lba, 1, vol->fat_cache);
        if (rc != 0) return -1;
        vol->cached_fat_sector = fat_sector;
    }

    vol->fat_cache[fat_offset] = (vol->fat_cache[fat_offset] & 0xF0000000) | (value & 0x0FFFFFFF);

    /* Write to both FAT copies */
    int rc = vol->blk.write(vol->blk.ctx, lba, 1, vol->fat_cache);
    if (rc != 0) return -1;
    if (vol->num_fats >= 2) {
        rc = vol->blk.write(vol->blk.ctx, vol->fat2_lba + fat_sector, 1, vol->fat_cache);
        if (rc != 0) return -1;
    }
    return 0;
}

/* Invalidate FAT cache */
void fat_cache_invalidate(fat32_volume *vol) {
    vol->cached_fat_sector = 0xFFFFFFFF;
}
