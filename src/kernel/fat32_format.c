/* fat32_format.c -- mount/unmount/format/validate (volume lifecycle, leaf module).
 * Opaque fat32_volume via fat32_internal.h. C11, minimal includes. */
#include "fat32_internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

int fat32_mount(fat32_volume *vol, const fat32_blk_ops *blk) {
    memset(vol, 0, sizeof(*vol));
    memcpy(&vol->blk, blk, sizeof(*blk));
    fat_cache_invalidate(vol);

    /* Read boot sector */
    fat32_boot_sector bs;
    int rc = blk->read(blk->ctx, 0, 1, &bs);
    if (rc != 0) return -1;

    /* Validate signature */
    if (bs.signature != 0xAA55) return -1;
    if (bs.bytes_per_sector != FAT32_SECTOR_SIZE) return -1;
    if (bs.sectors_per_cluster == 0) return -1;
    if (bs.sectors_per_fat == 0) return -1;
    if (bs.root_cluster < 2) return -1;

    /* Populate volume fields */
    vol->mounted = true;
    vol->sectors_per_cluster = bs.sectors_per_cluster;
    vol->bytes_per_cluster = bs.sectors_per_cluster * FAT32_SECTOR_SIZE;
    vol->reserved_sectors = bs.reserved_sectors;
    vol->num_fats = bs.num_fats;
    vol->sectors_per_fat = bs.sectors_per_fat;
    vol->root_cluster = bs.root_cluster;
    vol->fat1_lba = bs.reserved_sectors;
    vol->fat2_lba = vol->fat1_lba + bs.sectors_per_fat;
    vol->data_lba = vol->fat2_lba + bs.sectors_per_fat;
    vol->total_clusters = (uint32_t)((blk->n_sectors - vol->data_lba) / bs.sectors_per_cluster) + 2;
    vol->fs_info_lba = bs.fs_info_sector;

    /* Read FSInfo for free cluster count */
    if (bs.fs_info_sector > 0) {
        uint8_t fsinfo_buf[FAT32_SECTOR_SIZE];
        rc = blk->read(blk->ctx, bs.fs_info_sector, 1, fsinfo_buf);
        if (rc == 0) {
            /* Check FSInfo signatures: 0x41615252 at 0, 0x61417272 at 484, 0xAA550000 at 508 */
            uint32_t sig1, sig2, sig3;
            memcpy(&sig1, fsinfo_buf, 4);
            memcpy(&sig2, fsinfo_buf + 484, 4);
            memcpy(&sig3, fsinfo_buf + 508, 4);
            if (sig1 == 0x41615252 && sig2 == 0x61417272 && sig3 == 0xAA550000) {
                memcpy(&vol->free_clusters, fsinfo_buf + 488, 4);
                memcpy(&vol->next_free, fsinfo_buf + 492, 4);
                if (vol->next_free < 2) vol->next_free = 2;
            }
        }
    }

    return 0;
}

void fat32_unmount(fat32_volume *vol) {
    if (vol->fat_cache) {
        free(vol->fat_cache);
        vol->fat_cache = NULL;
    }
    vol->mounted = false;
}

int fat32_format(const fat32_blk_ops *blk, uint64_t sector_count,
                 const char *vol_name) {
    fat32_boot_sector bs;
    memset(&bs, 0, sizeof(bs));

    /* Boot jump */
    bs.jump[0] = 0xEB;  /* JMP short */
    bs.jump[1] = 0x58;  /* Offset to boot code */
    bs.jump[2] = 0x90;  /* NOP */
    memcpy(bs.oem_name, "MSWIN4.1", 8);

    bs.bytes_per_sector = FAT32_SECTOR_SIZE;

    /* Sectors per cluster based on volume size */
    uint64_t total_kb = sector_count / 2;
    if (total_kb <= 244000)       bs.sectors_per_cluster = 1;
    else if (total_kb <= 976000)  bs.sectors_per_cluster = 2;
    else if (total_kb <= 3056000) bs.sectors_per_cluster = 4;
    else if (total_kb <= 6112000) bs.sectors_per_cluster = 8;
    else if (total_kb <= 16777000) bs.sectors_per_cluster = 16;
    else if (total_kb <= 33554000) bs.sectors_per_cluster = 32;
    else                          bs.sectors_per_cluster = 64;

    bs.reserved_sectors = 32;
    bs.num_fats = 2;
    bs.root_entry_count = 0;
    bs.total_sectors_16 = 0;
    bs.media_descriptor = 0xF8;
    bs.sectors_per_fat_16 = 0;
    bs.total_sectors_32 = (uint32_t)sector_count;

    /* Calculate sectors per FAT */
    uint64_t data_sectors = sector_count - bs.reserved_sectors;
    uint32_t clusters = (uint32_t)(data_sectors / bs.sectors_per_cluster);
    bs.sectors_per_fat = (uint32_t)((clusters * 4 + FAT32_SECTOR_SIZE - 1) / FAT32_SECTOR_SIZE);
    /* Add margin */
    bs.sectors_per_fat += 1;

    bs.fat_flags = 0;
    bs.version = 0;
    bs.root_cluster = 2;
    bs.fs_info_sector = 1;
    bs.backup_boot_sector = 6;
    bs.drive_number = 0x80;
    bs.boot_sig = 0x29;
    bs.volume_serial = (uint32_t)time(NULL);
    memset(bs.volume_label, ' ', 11);
    if (vol_name) {
        size_t len = strlen(vol_name);
        if (len > 11) len = 11;
        memcpy(bs.volume_label, vol_name, len);
    } else {
        memcpy(bs.volume_label, "NO NAME    ", 11);
    }
    memcpy(bs.fs_type, "FAT32   ", 8);
    bs.signature = 0xAA55;

    /* Write boot sector */
    int rc = blk->write(blk->ctx, 0, 1, &bs);
    if (rc != 0) return -1;

    /* Write backup boot sector */
    rc = blk->write(blk->ctx, bs.backup_boot_sector, 1, &bs);
    if (rc != 0) return -1;

    /* Zero out FAT area and first few data sectors */
    uint8_t zero[FAT32_SECTOR_SIZE];
    memset(zero, 0, FAT32_SECTOR_SIZE);

    uint64_t fat_start = bs.reserved_sectors;
    uint64_t fat_end = fat_start + 2 * (uint64_t)bs.sectors_per_fat;
    for (uint64_t i = fat_start; i < fat_end; i++) {
        rc = blk->write(blk->ctx, i, 1, zero);
        if (rc != 0) return -1;
    }

    /* Initialize FAT entries 0 and 1 */
    uint32_t fat_init[128]; /* One sector of FAT entries */
    memset(fat_init, 0, sizeof(fat_init));
    fat_init[0] = 0x0FFFFFF8;  /* Media descriptor + EOC */
    fat_init[1] = 0x0FFFFFFF;  /* EOC marker */

    /* Allocate root directory cluster (cluster 2) */
    fat_init[2] = 0x0FFFFFFF;  /* Root dir: single cluster */

    rc = blk->write(blk->ctx, fat_start, 1, fat_init);
    if (rc != 0) return -1;
    /* Second FAT copy */
    rc = blk->write(blk->ctx, fat_start + bs.sectors_per_fat, 1, fat_init);
    if (rc != 0) return -1;

    /* Write FSInfo sector */
    uint8_t fsinfo[FAT32_SECTOR_SIZE];
    memset(fsinfo, 0, FAT32_SECTOR_SIZE);
    uint32_t fs_sig1 = 0x41615252;  /* "RRaA" */
    uint32_t fs_sig2 = 0x61417272;  /* "rrAa" */
    uint32_t fs_sig3 = 0xAA550000;
    uint32_t free_clus = clusters - 1;  /* Minus root dir */
    uint32_t next_free = 3;
    memcpy(fsinfo, &fs_sig1, 4);
    memcpy(fsinfo + 484, &fs_sig2, 4);
    memcpy(fsinfo + 488, &free_clus, 4);
    memcpy(fsinfo + 492, &next_free, 4);
    memcpy(fsinfo + 508, &fs_sig3, 4);

    rc = blk->write(blk->ctx, bs.fs_info_sector, 1, fsinfo);
    if (rc != 0) return -1;

    /* Zero out root directory cluster */
    uint64_t data_start = fat_end;
    for (uint32_t i = 0; i < bs.sectors_per_cluster; i++) {
        rc = blk->write(blk->ctx, data_start + i, 1, zero);
        if (rc != 0) return -1;
    }

    return 0;
}

int fat32_validate(fat32_volume *vol) {
    if (!vol->mounted) return -1;

    /* Check that root directory is accessible */
    uint64_t root_lba = fat32_cluster_to_lba(vol, vol->root_cluster);
    uint8_t *buf = (uint8_t *)malloc(vol->bytes_per_cluster);
    if (!buf) return -1;

    int rc = vol->blk.read(vol->blk.ctx, root_lba, vol->sectors_per_cluster, buf);
    free(buf);
    return (rc == 0) ? 0 : -1;
}

void fat32_info(fat32_volume *vol, char *buf, size_t buf_size) {
    if (!vol || !buf) return;
    snprintf(buf, buf_size,
        "FAT32 Volume: %u sectors/cluster, %u bytes/cluster\n"
        "  Total clusters: %u, Free: %u\n"
        "  Root cluster: %u, Data LBA: %llu\n"
        "  FAT1 LBA: %llu, FAT2 LBA: %llu\n",
        vol->sectors_per_cluster, vol->bytes_per_cluster,
        vol->total_clusters, vol->free_clusters,
        vol->root_cluster, (unsigned long long)vol->data_lba,
        (unsigned long long)vol->fat1_lba, (unsigned long long)vol->fat2_lba);
}
