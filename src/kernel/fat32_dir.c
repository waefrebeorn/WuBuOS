/* fat32_dir.c -- directory enumeration, lookup, create, delete (leaf module).
 * Opaque fat32_volume via fat32_internal.h. C11, minimal includes. */
#include "fat32_internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
