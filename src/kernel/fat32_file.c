/* fat32_file.c -- open/close/read/write/seek on a FAT32 file (leaf module).
 * Opaque fat32_volume via fat32_internal.h. C11, minimal includes. */
#include "fat32_internal.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

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

    /* Store dir location for O(1) close */
    fp->dir_cluster = info.dir_cluster;
    fp->dir_offset = info.dir_offset;

    return 0;
}

void fat32_close(fat32_file *fp) {
    if (!fp || !fp->vol) return;

    /* O(1) directory entry update using stored dir_cluster/dir_offset */
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
