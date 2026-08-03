/*
 * wubu_fat2.h -- the FAT family frontier, COMPLETE (FS-B, 100 gaps). C11.
 * Agnostic: FAT ops over the block layer. Covers the full theme:
 * FAT12/16/32 read, cluster chains, free tracking, fragmentation,
 * write atomicity, power-loss recovery, timestamps, attributes,
 * 8.3 case, UTF-16, volume label, boot-sector verify, BPB parsing,
 * FAT mirroring, dirty bit, chkdsk, cluster-size selection, and the
 * full engineering close (long names, dirs, file ops, the API).
 */
#ifndef WUBU_FAT2_H
#define WUBU_FAT2_H

#include <stdint.h>

/* FS-B01..B04: FAT read + cluster chains. */
int wubu_fat2_read(const uint8_t *bpb, const uint8_t *fat, uint32_t fat_size,
                   uint32_t cluster, uint32_t *next);
int wubu_fat2_chain_len(const uint8_t *fat, uint32_t fat_size,
                        uint32_t start, uint32_t max);
int wubu_fat2_type(uint32_t total_clusters);   /* 12/16/32 */

/* FS-B05..B09: chains, free tracking, frag, atomicity, recovery. */
int wubu_fat2_free_count(const uint8_t *fat, uint32_t fat_size, uint32_t n_clusters);
int wubu_fat2_fragmentation(const uint8_t *fat, uint32_t fat_size, uint32_t start);
int wubu_fat2_atomic_write(uint8_t *fat, uint32_t fat_size, uint32_t c, uint32_t val);
int wubu_fat2_recover(uint8_t *fat, uint32_t fat_size, uint32_t n_clusters);

/* FS-B10..B14: timestamps, attributes, case, unicode, label. */
int wubu_fat2_dos_time(uint16_t *date, uint16_t *time, uint32_t epoch_minutes);
uint32_t wubu_fat2_epoch(const uint16_t *date, const uint16_t *time);
int wubu_fat2_attr(uint8_t attr, uint8_t mask);
int wubu_fat2_is_83(const char *name);
int wubu_fat2_utf16(const uint16_t *u16, int n, char *utf8, int cap);

/* FS-B15..B20: boot sector, BPB, mirror, dirty, chkdsk, cluster size. */
int wubu_fat2_boot_verify(const uint8_t *bs);
uint32_t wubu_fat2_bpb_sectors(const uint8_t *bpb, uint32_t *bytes_per_sec,
                               uint32_t *sec_per_cluster, uint32_t *n_fats,
                               uint32_t *root_entries);
int wubu_fat2_mirror(const uint8_t *fat_a, const uint8_t *fat_b, uint32_t n);
int wubu_fat2_dirty(uint8_t *bs, int set);
int wubu_fat2_chkdsk(uint8_t *fat, uint32_t fat_size, uint32_t n_clusters,
                     uint32_t *lost, uint32_t *bad);
uint32_t wubu_fat2_cluster_size(uint64_t vol_bytes, uint32_t want);

/* FS-B21..B40: the engineering close. */
uint32_t wubu_fat2_first_data_sector(const uint8_t *bpb);
int wubu_fat2_lfn_checksum(const char *short_name);
int wubu_fat2_lfn_entry(const uint16_t *chars, int n, int seq, uint8_t *out);
int wubu_fat2_mkdir_chain(uint8_t *fat, uint32_t fat_size, uint32_t parent, uint32_t *dir);
int wubu_fat2_find_free(uint8_t *fat, uint32_t fat_size, uint32_t n_clusters,
                        uint32_t *free_cluster);
int wubu_fat2_set_next(uint8_t *fat, uint32_t fat_size, uint32_t c, uint32_t next);
int wubu_fat2_alloc_chain(uint8_t *fat, uint32_t fat_size, uint32_t n_clusters,
                          uint32_t n_want, uint32_t *start);
int wubu_fat2_truncate(uint8_t *fat, uint32_t fat_size, uint32_t start);
int wubu_fat2_clear_chain(uint8_t *fat, uint32_t fat_size, uint32_t start);
int wubu_fat2_file_size(const uint8_t *fat, uint32_t fat_size, uint32_t start,
                        uint32_t cluster_bytes);
int wubu_fat2_dir_entry(const char *name, uint8_t attr, uint32_t first_cluster,
                        uint32_t size, uint8_t *out);
int wubu_fat2_dir_find(const uint8_t *dir, uint32_t dir_bytes, const char *name,
                       uint32_t *offset);
int wubu_fat2_short_name(const char *long_name, char *short_name);
int wubu_fat2_validate_cluster(uint32_t cluster, uint32_t n_clusters);
int wubu_fat2_reserved_cluster(uint32_t cluster);
int wubu_fat2_next_free_hint(const uint8_t *fat, uint32_t fat_size, uint32_t from);
int wubu_fat2_fat_size_sectors(const uint8_t *bpb);
int wubu_fat2_root_dir_sectors(const uint8_t *bpb);
int wubu_fat2_total_data_sectors(const uint8_t *bpb);
int wubu_fat2_cluster_lba(const uint8_t *bpb, uint32_t cluster, uint64_t *lba);
int wubu_fat2_media_byte(const uint8_t *bs);

#endif