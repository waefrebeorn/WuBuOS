/*
 * fat32.c  --  My Seed FAT32 Filesystem Implementation
 *
 * Clean C11 reimplementation of ZealOS FAT32 design.
 * Reference: ZealOS/src/Kernel/BlkDev/FileSysFAT.ZC (1087 lines)
 *
 * Porting notes:
 *   - ZealOS uses try/catch for drive locking  --  we use spinlocks
 *   - ZealOS BlkRead/BlkWrite → fat32_blk_ops callbacks
 *   - ZealOS CDirEntry → fat32_file_info
 *   - ZealOS Date2Struct/Dos2CDate → simplified epoch-based timestamps
 *   - All cluster numbers are uint32_t (ZealOS uses I64)
 */

#include "fat32.h"
#include "fat32_internal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>

/* -- Internal Helpers --------------------------------------------- */

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
static void fat_cache_invalidate(fat32_volume *vol) {
    vol->cached_fat_sector = 0xFFFFFFFF;
}

/* -- Internal: 8.3 Name Conversion ------------------------------- */

/* Convert "filename.ext" → 8.3 format (space-padded, uppercased) */

/* -- Internal: DOS Date/Time -------------------------------------- */

static void datetime_to_dos(time_t t, uint16_t *dos_time, uint16_t *dos_date) {
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

/* -- Volume Management -------------------------------------------- */

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

/* -- Format ------------------------------------------------------- */

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

/* -- Cluster Operations ------------------------------------------- */


/* -- Directory Operations ----------------------------------------- */

int fat32_dir_read(fat32_volume *vol, uint32_t dir_cluster,
                   fat32_dir_callback cb, void *ctx) {
    if (!vol->mounted) return -1;

    if (dir_cluster == 0) dir_cluster = vol->root_cluster;

    int count = 0;
    uint32_t cluster = dir_cluster;
    uint8_t *buf = (uint8_t *)malloc(vol->bytes_per_cluster);
    if (!buf) return -1;

    char lfn[FAT32_MAX_NAME + 1];
    memset(lfn, 0, sizeof(lfn));
    uint8_t lfn_chk = 0;

    while (cluster >= 2 && cluster < FAT32_CLUSTER_EOC) {
        uint64_t lba = fat32_cluster_to_lba(vol, cluster);
        int rc = vol->blk.read(vol->blk.ctx, lba, vol->sectors_per_cluster, buf);
        if (rc != 0) { free(buf); return -1; }

        uint32_t entries_per_cluster = vol->bytes_per_cluster / FAT32_DIR_ENTRY_SIZE;
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            fat32_dir_entry *de = (fat32_dir_entry *)(buf + i * FAT32_DIR_ENTRY_SIZE);

            /* End of directory */
            if ((uint8_t)de->name[0] == 0x00) { free(buf); return count; }

            /* Deleted entry  --  skip */
            if ((uint8_t)de->name[0] == 0xE5) {
                memset(lfn, 0, sizeof(lfn));
                continue;
            }

            /* Long filename entry */
            if (de->attr == FAT32_ATTR_LFN) {
                fat32_lfn_entry *lfn_ent = (fat32_lfn_entry *)de;
                int seq = lfn_ent->ord & 0x3F;

                if (lfn_ent->ord & 0x40) {
                    /* Start of LFN sequence */
                    memset(lfn, 0, sizeof(lfn));
                    lfn_chk = lfn_ent->checksum;
                }

                /* Extract characters from this LFN entry */
                int char_idx = (seq - 1) * FAT32_LFN_CHARS;
                uint16_t chars[FAT32_LFN_CHARS];
                memcpy(chars + 0, lfn_ent->name1, 10);
                memcpy(chars + 5, lfn_ent->name2, 12);
                memcpy(chars + 11, lfn_ent->name3, 4);

                for (int j = 0; j < FAT32_LFN_CHARS && char_idx + j < FAT32_MAX_NAME; j++) {
                    if (chars[j] == 0x0000 || chars[j] == 0xFFFF) break;
                    lfn[char_idx + j] = (char)(chars[j] & 0xFF);
                }
                continue;
            }

            /* Volume ID or . / .. entries  --  skip in listing */
            if (de->attr & FAT32_ATTR_VOLUME_ID) {
                memset(lfn, 0, sizeof(lfn));
                continue;
            }

            /* Regular entry  --  build file info */
            fat32_file_info info;
            memset(&info, 0, sizeof(info));

            if (lfn[0] != '\0') {
                /* Use long name from LFN entries */
                strncpy(info.name, lfn, FAT32_MAX_NAME - 1);
                info.name[FAT32_MAX_NAME - 1] = '\0';
            } else {
                /* Use 8.3 name  --  name[8] and ext[3] are contiguous in packed struct */
                char name83[11];
                memcpy(name83, de->name, 8);
                memcpy(name83 + 8, de->ext, 3);
                name_from_83(name83, info.name, FAT32_MAX_NAME + 1);
            }

            info.first_cluster = (uint32_t)de->cluster_hi << 16 | de->cluster_lo;
            info.file_size = de->file_size;
            info.attr = de->attr;
            info.dir_cluster = cluster;
            info.dir_offset = i;

            count++;
            if (cb && !cb(&info, ctx)) { free(buf); return count; }

            /* Reset LFN buffer */
            memset(lfn, 0, sizeof(lfn));
        }

        /* Next cluster in chain */
        cluster = fat32_next_cluster(vol, cluster);
    }

    free(buf);
    return count;
}

int fat32_find(fat32_volume *vol, uint32_t dir_cluster,
               const char *name, fat32_file_info *out) {
    /* Simple linear scan  --  fine for small directories */
    if (dir_cluster == 0) dir_cluster = vol->root_cluster;

    uint32_t cluster = dir_cluster;
    uint8_t *buf = (uint8_t *)malloc(vol->bytes_per_cluster);
    if (!buf) return -1;

    char lfn[FAT32_MAX_NAME + 1];
    memset(lfn, 0, sizeof(lfn));

    while (cluster >= 2 && cluster < FAT32_CLUSTER_EOC) {
        uint64_t lba = fat32_cluster_to_lba(vol, cluster);
        int rc = vol->blk.read(vol->blk.ctx, lba, vol->sectors_per_cluster, buf);
        if (rc != 0) { free(buf); return -1; }

        uint32_t entries_per_cluster = vol->bytes_per_cluster / FAT32_DIR_ENTRY_SIZE;
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            fat32_dir_entry *de = (fat32_dir_entry *)(buf + i * FAT32_DIR_ENTRY_SIZE);

            if ((uint8_t)de->name[0] == 0x00) { free(buf); return -1; }
            if ((uint8_t)de->name[0] == 0xE5) { memset(lfn, 0, sizeof(lfn)); continue; }
            if (de->attr == FAT32_ATTR_LFN) {
                fat32_lfn_entry *lfn_ent = (fat32_lfn_entry *)de;
                int seq = lfn_ent->ord & 0x3F;
                if (lfn_ent->ord & 0x40) memset(lfn, 0, sizeof(lfn));
                int char_idx = (seq - 1) * FAT32_LFN_CHARS;
                uint16_t chars[FAT32_LFN_CHARS];
                memcpy(chars + 0, lfn_ent->name1, 10);
                memcpy(chars + 5, lfn_ent->name2, 12);
                memcpy(chars + 11, lfn_ent->name3, 4);
                for (int j = 0; j < FAT32_LFN_CHARS && char_idx + j < FAT32_MAX_NAME; j++) {
                    if (chars[j] == 0x0000 || chars[j] == 0xFFFF) break;
                    lfn[char_idx + j] = (char)(chars[j] & 0xFF);
                }
                continue;
            }
            if (de->attr & FAT32_ATTR_VOLUME_ID) { memset(lfn, 0, sizeof(lfn)); continue; }

            /* Build name */
            char entry_name[FAT32_MAX_NAME + 1];
            if (lfn[0] != '\0') {
                strncpy(entry_name, lfn, FAT32_MAX_NAME);
                entry_name[FAT32_MAX_NAME] = '\0';
            } else {
                char name83[11];
                memcpy(name83, de->name, 8);
                memcpy(name83 + 8, de->ext, 3);
                name_from_83(name83, entry_name, FAT32_MAX_NAME + 1);
            }

            if (name_cmp(entry_name, name) == 0) {
                if (out) {
                    memset(out, 0, sizeof(*out));
                    strncpy(out->name, entry_name, FAT32_MAX_NAME - 1);
                    out->name[FAT32_MAX_NAME - 1] = '\0';
                    out->first_cluster = (uint32_t)de->cluster_hi << 16 | de->cluster_lo;
                    out->file_size = de->file_size;
                    out->attr = de->attr;
                    out->dir_cluster = cluster;
                    out->dir_offset = i;
                }
                free(buf);
                return 0;
            }

            memset(lfn, 0, sizeof(lfn));
        }

        cluster = fat32_next_cluster(vol, cluster);
    }

    free(buf);
    return -1;  /* Not found */
}

int fat32_create(fat32_volume *vol, uint32_t dir_cluster,
                 const char *name, uint8_t attr, fat32_file_info *out) {
    if (!vol->mounted) return -1;
    if (!name || !name[0]) return -1;

    if (dir_cluster == 0) dir_cluster = vol->root_cluster;

    /* Check if already exists */
    fat32_file_info existing;
    if (fat32_find(vol, dir_cluster, name, &existing) == 0) {
        /* Already exists  --  return existing */
        if (out) *out = existing;
        return 0;
    }

    /* Allocate a cluster for the new file/directory */
    uint32_t new_cluster = fat32_alloc_clusters(vol, 1, 0);
    if (new_cluster == 0) return -1;

    /* If directory, initialize with . and .. entries */
    if (attr & FAT32_ATTR_DIRECTORY) {
        uint64_t lba = fat32_cluster_to_lba(vol, new_cluster);
        uint8_t *buf = (uint8_t *)malloc(vol->bytes_per_cluster);
        if (!buf) return -1;
        memset(buf, 0, vol->bytes_per_cluster);

        /* ". " entry */
        fat32_dir_entry *dot = (fat32_dir_entry *)buf;
        memset(dot->name, ' ', 8);
        memset(dot->ext, ' ', 3);
        dot->name[0] = '.';
        dot->attr = FAT32_ATTR_DIRECTORY;
        dot->cluster_lo = (uint16_t)(new_cluster & 0xFFFF);
        dot->cluster_hi = (uint16_t)(new_cluster >> 16);

        /* ".. " entry */
        fat32_dir_entry *dotdot = (fat32_dir_entry *)(buf + FAT32_DIR_ENTRY_SIZE);
        memset(dotdot->name, ' ', 8);
        memset(dotdot->ext, ' ', 3);
        dotdot->name[0] = '.';
        dotdot->name[1] = '.';
        dotdot->attr = FAT32_ATTR_DIRECTORY;
        dotdot->cluster_lo = (uint16_t)(dir_cluster & 0xFFFF);
        dotdot->cluster_hi = (uint16_t)(dir_cluster >> 16);

        vol->blk.write(vol->blk.ctx, lba, vol->sectors_per_cluster, buf);
        free(buf);
    }

    /* Find free directory entry slot */
    uint32_t cluster = dir_cluster;
    uint8_t *buf = (uint8_t *)malloc(vol->bytes_per_cluster);
    if (!buf) return -1;

    while (cluster >= 2 && cluster < FAT32_CLUSTER_EOC) {
        uint64_t lba = fat32_cluster_to_lba(vol, cluster);
        int rc = vol->blk.read(vol->blk.ctx, lba, vol->sectors_per_cluster, buf);
        if (rc != 0) { free(buf); return -1; }

        uint32_t entries_per_cluster = vol->bytes_per_cluster / FAT32_DIR_ENTRY_SIZE;
        for (uint32_t i = 0; i < entries_per_cluster; i++) {
            fat32_dir_entry *de = (fat32_dir_entry *)(buf + i * FAT32_DIR_ENTRY_SIZE);
            if ((uint8_t)de->name[0] == 0x00 || (uint8_t)de->name[0] == 0xE5) {
                /* Free slot  --  create entry here */
                memset(de, 0, sizeof(*de));
                char name83[11];
                name_to_83(name, name83);
                memcpy(de->name, name83, 8);
                memcpy(de->ext, name83 + 8, 3);
                de->attr = attr;
                de->cluster_lo = (uint16_t)(new_cluster & 0xFFFF);
                de->cluster_hi = (uint16_t)(new_cluster >> 16);
                if (!(attr & FAT32_ATTR_DIRECTORY))
                    de->file_size = 0;

                /* Set timestamp */
                time_t now = time(NULL);
                uint16_t dt, tm;
                datetime_to_dos(now, &tm, &dt);
                de->create_time = tm;
                de->create_date = dt;
                de->write_time = tm;
                de->write_date = dt;
                de->access_date = dt;

                /* Write back */
                rc = vol->blk.write(vol->blk.ctx, lba, vol->sectors_per_cluster, buf);
                free(buf);

                if (out) {
                    memset(out, 0, sizeof(*out));
                    strncpy(out->name, name, FAT32_MAX_NAME);
                    out->first_cluster = new_cluster;
                    out->file_size = 0;
                    out->attr = attr;
                    out->dir_cluster = cluster;
                    out->dir_offset = i;
                }
                return (rc == 0) ? 0 : -1;
            }
        }

        /* Need to extend directory  --  allocate another cluster */
        uint32_t next = fat32_next_cluster(vol, cluster);
        if (next == 0 || next >= FAT32_CLUSTER_EOC) {
            uint32_t new_dir_cluster = fat32_alloc_clusters(vol, 1, 0);
            if (new_dir_cluster == 0) { free(buf); return -1; }
            /* Link it */
            fat_write_entry(vol, cluster, new_dir_cluster);
            fat_write_entry(vol, new_dir_cluster, FAT32_CLUSTER_EOC);
            /* Zero out new cluster */
            uint64_t new_lba = fat32_cluster_to_lba(vol, new_dir_cluster);
            memset(buf, 0, vol->bytes_per_cluster);
            vol->blk.write(vol->blk.ctx, new_lba, vol->sectors_per_cluster, buf);
            cluster = new_dir_cluster;
        } else {
            cluster = next;
        }
    }

    free(buf);
    return -1;  /* No free slot found (shouldn't happen) */
}

int fat32_delete(fat32_volume *vol, uint32_t dir_cluster, const char *name) {
    if (!vol->mounted) return -1;
    if (dir_cluster == 0) dir_cluster = vol->root_cluster;

    fat32_file_info info;
    if (fat32_find(vol, dir_cluster, name, &info) != 0) return -1;

    /* Free cluster chain */
    if (info.first_cluster >= 2)
        fat32_free_chain(vol, info.first_cluster);

    /* Mark directory entry as deleted */
    uint64_t lba = fat32_cluster_to_lba(vol, info.dir_cluster);
    uint8_t *buf = (uint8_t *)malloc(vol->bytes_per_cluster);
    if (!buf) return -1;

    int rc = vol->blk.read(vol->blk.ctx, lba, vol->sectors_per_cluster, buf);
    if (rc != 0) { free(buf); return -1; }

    fat32_dir_entry *de = (fat32_dir_entry *)(buf + info.dir_offset * FAT32_DIR_ENTRY_SIZE);
    de->name[0] = 0xE5;  /* Deleted marker */

    rc = vol->blk.write(vol->blk.ctx, lba, vol->sectors_per_cluster, buf);
    free(buf);
    return (rc == 0) ? 0 : -1;
}

/* -- File I/O ----------------------------------------------------- */

int fat32_open(fat32_volume *vol, uint32_t dir_cluster,
               const char *name, const char *mode, fat32_file *fp) {
    if (!vol->mounted || !name || !mode) return -1;
    if (dir_cluster == 0) dir_cluster = vol->root_cluster;

    memset(fp, 0, sizeof(*fp));
    fp->vol = vol;

    fat32_file_info info;
    int found = fat32_find(vol, dir_cluster, name, &info);

    if (mode[0] == 'r' || (mode[0] == 'r' && mode[1] == 'w')) {
        if (found != 0) return -1;
        fp->first_cluster = info.first_cluster;
        fp->current_cluster = info.first_cluster;
        fp->file_size = info.file_size;
        fp->attr = info.attr;
        fp->write_mode = (mode[1] == 'w');
    } else if (mode[0] == 'w') {
        if (found == 0) {
            /* Delete existing and recreate */
            fat32_delete(vol, dir_cluster, name);
        }
        if (fat32_create(vol, dir_cluster, name, FAT32_ATTR_ARCHIVE, &info) != 0)
            return -1;
        fp->first_cluster = info.first_cluster;
        fp->current_cluster = info.first_cluster;
        fp->file_size = 0;
        fp->attr = info.attr;
        fp->write_mode = true;
    } else if (mode[0] == 'a') {
        if (found != 0) {
            if (fat32_create(vol, dir_cluster, name, FAT32_ATTR_ARCHIVE, &info) != 0)
                return -1;
            fp->first_cluster = info.first_cluster;
            fp->current_cluster = info.first_cluster;
            fp->file_size = 0;
        } else {
            fp->first_cluster = info.first_cluster;
            fp->current_cluster = info.first_cluster;
            fp->file_size = info.file_size;
            fp->pos = info.file_size;
            /* Walk to last cluster */
            uint32_t c = info.first_cluster;
            while (c >= 2) {
                uint32_t next = fat32_next_cluster(vol, c);
                if (next == 0 || next >= FAT32_CLUSTER_EOC) break;
                fp->current_cluster = next;
                c = next;
            }
        }
        fp->attr = info.attr;
        fp->write_mode = true;
    } else {
        return -1;
    }

    /* Cell 304: Store dir location for O(1) close */
    fp->dir_cluster = info.dir_cluster;
    fp->dir_offset = info.dir_offset;

    return 0;
}

void fat32_close(fat32_file *fp) {
    if (!fp || !fp->vol) return;

    /* Cell 304: O(1) directory entry update using stored dir_cluster/dir_offset */
    if (fp->write_mode && fp->first_cluster >= 2 && fp->dir_cluster >= 2) {
        uint8_t *buf = (uint8_t *)malloc(fp->vol->bytes_per_cluster);
        if (!buf) { memset(fp, 0, sizeof(*fp)); return; }

        uint64_t lba = fat32_cluster_to_lba(fp->vol, fp->dir_cluster);
        int rc = fp->vol->blk.read(fp->vol->blk.ctx, lba,
                                    fp->vol->sectors_per_cluster, buf);
        if (rc == 0) {
            fat32_dir_entry *de = (fat32_dir_entry *)(buf + fp->dir_offset * FAT32_DIR_ENTRY_SIZE);
            de->file_size = fp->file_size;
            /* Also update write time */
            time_t now = time(NULL);
            uint16_t dt, tm;
            datetime_to_dos(now, &tm, &dt);
            de->write_time = tm;
            de->write_date = dt;
            de->access_date = dt;

            fp->vol->blk.write(fp->vol->blk.ctx, lba,
                                fp->vol->sectors_per_cluster, buf);
        }
        free(buf);
    }

    memset(fp, 0, sizeof(*fp));
}

size_t fat32_read(fat32_file *fp, void *buf, size_t n) {
    if (!fp || !fp->vol || !buf) return 0;

    size_t remaining = n;
    if (fp->pos + remaining > fp->file_size)
        remaining = (size_t)(fp->file_size - fp->pos);

    if (remaining == 0) return 0;

    uint8_t *dst = (uint8_t *)buf;
    uint32_t cluster = fp->current_cluster;
    size_t total_read = 0;

    while (remaining > 0 && cluster >= 2 && cluster < FAT32_CLUSTER_EOC) {
        uint64_t lba = fat32_cluster_to_lba(fp->vol, cluster);
        uint32_t cluster_size = fp->vol->bytes_per_cluster;
        uint32_t offset_in_cluster = (uint32_t)(fp->pos % cluster_size);
        uint32_t bytes_in_cluster = cluster_size - offset_in_cluster;
        uint32_t to_read = (remaining < bytes_in_cluster) ? (uint32_t)remaining : bytes_in_cluster;

        if (offset_in_cluster == 0 && to_read == cluster_size) {
            /* Read full cluster(s) directly */
            int rc = fp->vol->blk.read(fp->vol->blk.ctx, lba,
                                        fp->vol->sectors_per_cluster, dst);
            if (rc != 0) break;
        } else {
            /* Partial read  --  need a temp buffer */
            uint8_t *tmp = (uint8_t *)malloc(cluster_size);
            if (!tmp) break;
            int rc = fp->vol->blk.read(fp->vol->blk.ctx, lba,
                                        fp->vol->sectors_per_cluster, tmp);
            if (rc != 0) { free(tmp); break; }
            memcpy(dst, tmp + offset_in_cluster, to_read);
            free(tmp);
        }

        dst += to_read;
        fp->pos += to_read;
        total_read += to_read;
        remaining -= to_read;

        if (offset_in_cluster + to_read >= cluster_size) {
            cluster = fat32_next_cluster(fp->vol, cluster);
            fp->current_cluster = cluster;
        }
    }

    return total_read;
}

size_t fat32_write(fat32_file *fp, const void *buf, size_t n) {
    if (!fp || !fp->vol || !buf || !fp->write_mode) return 0;

    const uint8_t *src = (const uint8_t *)buf;
    size_t remaining = n;
    size_t total_written = 0;
    uint32_t cluster = fp->current_cluster;
    uint32_t prev_cluster = 0;  /* For linking when extending chain */

    /* If we have a current cluster, figure out prev for chain extension */
    if (cluster >= 2 && cluster < FAT32_CLUSTER_EOC) {
        /* Walk chain to find previous  --  only needed at write start */
        prev_cluster = fp->first_cluster;
        if (prev_cluster != cluster) {
            uint32_t c = prev_cluster;
            while (c >= 2 && c < FAT32_CLUSTER_EOC) {
                uint32_t next = fat32_next_cluster(fp->vol, c);
                if (next == cluster || next == 0 || next >= FAT32_CLUSTER_EOC) break;
                prev_cluster = c;
                c = next;
            }
        }
    }

    while (remaining > 0) {
        /* Ensure we have a cluster to write to */
        if (cluster < 2 || cluster >= FAT32_CLUSTER_EOC) {
            uint32_t new_cluster = fat32_alloc_clusters(fp->vol, 1, 0);
            if (new_cluster == 0) break;

            if (fp->first_cluster < 2) {
                fp->first_cluster = new_cluster;
            } else if (prev_cluster >= 2 && prev_cluster < FAT32_CLUSTER_EOC) {
                /* Link previous cluster to new one */
                fat_write_entry(fp->vol, prev_cluster, new_cluster);
            }
            fat_write_entry(fp->vol, new_cluster, FAT32_CLUSTER_EOC);
            cluster = new_cluster;
            fp->current_cluster = cluster;
        }

        uint64_t lba = fat32_cluster_to_lba(fp->vol, cluster);
        uint32_t cluster_size = fp->vol->bytes_per_cluster;
        uint32_t offset_in_cluster = (uint32_t)(fp->pos % cluster_size);
        uint32_t bytes_in_cluster = cluster_size - offset_in_cluster;
        uint32_t to_write = (remaining < bytes_in_cluster) ? (uint32_t)remaining : bytes_in_cluster;

        if (offset_in_cluster == 0 && to_write == cluster_size) {
            /* Full cluster write */
            int rc = fp->vol->blk.write(fp->vol->blk.ctx, lba,
                                         fp->vol->sectors_per_cluster, src);
            if (rc != 0) break;
        } else {
            /* Partial write  --  read-modify-write */
            uint8_t *tmp = (uint8_t *)malloc(cluster_size);
            if (!tmp) break;
            int rc = fp->vol->blk.read(fp->vol->blk.ctx, lba,
                                        fp->vol->sectors_per_cluster, tmp);
            if (rc != 0) { free(tmp); break; }
            memcpy(tmp + offset_in_cluster, src, to_write);
            rc = fp->vol->blk.write(fp->vol->blk.ctx, lba,
                                     fp->vol->sectors_per_cluster, tmp);
            free(tmp);
            if (rc != 0) break;
        }

        src += to_write;
        fp->pos += to_write;
        total_written += to_write;
        remaining -= to_write;

        if (fp->pos > fp->file_size)
            fp->file_size = (uint32_t)fp->pos;

        if (offset_in_cluster + to_write >= cluster_size) {
            prev_cluster = cluster;
            cluster = fat32_next_cluster(fp->vol, cluster);
            fp->current_cluster = cluster;
        }
    }

    return total_written;
}

int64_t fat32_seek(fat32_file *fp, int64_t offset, int whence) {
    if (!fp) return -1;

    int64_t new_pos;
    switch (whence) {
        case 0: new_pos = offset; break;                            /* SEEK_SET */
        case 1: new_pos = (int64_t)fp->pos + offset; break;        /* SEEK_CUR */
        case 2: new_pos = (int64_t)fp->file_size + offset; break;  /* SEEK_END */
        default: return -1;
    }

    if (new_pos < 0) return -1;
    fp->pos = (uint64_t)new_pos;

    /* Walk cluster chain to find the right cluster */
    uint32_t cluster = fp->first_cluster;
    uint64_t bytes_per_cluster = fp->vol->bytes_per_cluster;
    uint64_t target_cluster_idx = fp->pos / bytes_per_cluster;

    for (uint64_t i = 0; i < target_cluster_idx; i++) {
        uint32_t next = fat32_next_cluster(fp->vol, cluster);
        if (next == 0 || next >= FAT32_CLUSTER_EOC) break;
        cluster = next;
    }
    fp->current_cluster = cluster;

    return (int64_t)fp->pos;
}

/* -- Diagnostics -------------------------------------------------- */

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
