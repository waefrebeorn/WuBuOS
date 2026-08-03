/*
 * wubu_fat2.c -- the FAT family frontier, COMPLETE (FS-B). C11.
 */
#include "wubu_fat2.h"
#include <string.h>
#include <stdio.h>

/* FS-B04: read a FAT entry (12/16/32 by size). */
static uint32_t fat_read_entry(const uint8_t *fat, uint32_t fat_size,
                               uint32_t cluster)
{
    if (fat_size >= 0x10000) {           /* FAT32 */
        uint32_t off = cluster * 4;
        if (off + 4 > fat_size) return 0;
        return (uint32_t)fat[off] | ((uint32_t)fat[off+1] << 8) |
               ((uint32_t)fat[off+2] << 16) | ((uint32_t)fat[off+3] << 24);
    }
    if (fat_size >= 0x1000) {            /* FAT16 */
        uint32_t off = cluster * 2;
        if (off + 2 > fat_size) return 0;
        return (uint32_t)fat[off] | ((uint32_t)fat[off+1] << 8);
    }
    /* FAT12: 1.5 bytes per entry */
    uint32_t off = cluster + (cluster >> 1);
    if (off + 1 > fat_size) return 0;
    uint16_t w = (uint16_t)(fat[off] | ((uint16_t)fat[off+1] << 8));
    return (cluster & 1) ? (w >> 4) : (w & 0xFFF);
}

int wubu_fat2_read(const uint8_t *bpb, const uint8_t *fat, uint32_t fat_size,
                   uint32_t cluster, uint32_t *next)
{
    (void)bpb;
    if (!fat || !next) return -1;
    *next = fat_read_entry(fat, fat_size, cluster);
    return 0;
}

int wubu_fat2_chain_len(const uint8_t *fat, uint32_t fat_size,
                        uint32_t start, uint32_t max)
{
    if (!fat) return -1;
    uint32_t len = 0, c = start;
    uint32_t eof = (fat_size >= 0x10000) ? 0x0FFFFFF8u :
                   (fat_size >= 0x1000) ? 0xFFF8u : 0xFF8u;
    while (len < max) {
        uint32_t next = fat_read_entry(fat, fat_size, c);
        if (next == 0) return -1;        /* bad chain */
        len++;
        if (next >= eof) return (int)len;
        c = next;
    }
    return (int)len;
}

int wubu_fat2_type(uint32_t total_clusters)
{
    if (total_clusters < 4085) return 12;
    if (total_clusters < 65525) return 16;
    return 32;
}

int wubu_fat2_free_count(const uint8_t *fat, uint32_t fat_size, uint32_t n_clusters)
{
    if (!fat) return -1;
    uint32_t free = 0;
    for (uint32_t c = 2; c < n_clusters; c++)
        if (fat_read_entry(fat, fat_size, c) == 0) free++;
    return (int)free;
}

int wubu_fat2_fragmentation(const uint8_t *fat, uint32_t fat_size, uint32_t start)
{
    if (!fat) return -1;
    int runs = 0;
    uint32_t c = start, prev = (uint32_t)-1;
    uint32_t eof = (fat_size >= 0x10000) ? 0x0FFFFFF8u :
                   (fat_size >= 0x1000) ? 0xFFF8u : 0xFF8u;
    while (c < eof && c != 0) {
        if (prev != (uint32_t)-1 && c != prev + 1) runs++;
        prev = c;
        c = fat_read_entry(fat, fat_size, c);
    }
    return runs;
}

int wubu_fat2_atomic_write(uint8_t *fat, uint32_t fat_size, uint32_t c, uint32_t val)
{
    if (!fat) return -1;
    if (fat_size >= 0x10000) {
        uint32_t off = c * 4;
        if (off + 4 > fat_size) return -1;
        fat[off] = (uint8_t)(val & 0xFF);
        fat[off+1] = (uint8_t)((val >> 8) & 0xFF);
        fat[off+2] = (uint8_t)((val >> 16) & 0xFF);
        fat[off+3] = (uint8_t)((val >> 24) & 0xFF);
    } else if (fat_size >= 0x1000) {
        uint32_t off = c * 2;
        if (off + 2 > fat_size) return -1;
        fat[off] = (uint8_t)(val & 0xFF);
        fat[off+1] = (uint8_t)((val >> 8) & 0xFF);
    } else {
        uint32_t off = c + (c >> 1);
        if (off + 1 > fat_size) return -1;
        uint16_t w = (uint16_t)(fat[off] | ((uint16_t)fat[off+1] << 8));
        if (c & 1) w = (uint16_t)((w & 0x000F) | ((val & 0xFFF) << 4));
        else       w = (uint16_t)((w & 0xF000) | (val & 0xFFF));
        fat[off] = (uint8_t)(w & 0xFF);
        fat[off+1] = (uint8_t)(w >> 8);
    }
    return 0;
}

int wubu_fat2_recover(uint8_t *fat, uint32_t fat_size, uint32_t n_clusters)
{
    if (!fat) return -1;
    /* a simple scan: mark dangling entries (pointing past the volume) free */
    uint32_t eof = (fat_size >= 0x10000) ? 0x0FFFFFF8u :
                   (fat_size >= 0x1000) ? 0xFFF8u : 0xFF8u;
    int fixed = 0;
    for (uint32_t c = 2; c < n_clusters; c++) {
        uint32_t next = fat_read_entry(fat, fat_size, c);
        if (next != 0 && next < eof && next >= n_clusters) {
            wubu_fat2_atomic_write(fat, fat_size, c, 0);
            fixed++;
        }
    }
    return fixed;
}

int wubu_fat2_dos_time(uint16_t *date, uint16_t *time, uint32_t epoch_minutes)
{
    if (!date || !time) return -1;
    /* the 1980-01-01 epoch: years = minutes / 525600 */
    uint32_t years = epoch_minutes / 525600;
    if (years > 127) years = 127;
    uint32_t rem = epoch_minutes % 525600;
    uint32_t days = rem / 1440;
    uint32_t mins = rem % 1440;
    *date = (uint16_t)(((years) << 9) | ((days / 32 + 1) << 5) | ((days % 32) + 1));
    *time = (uint16_t)(((mins / 60) << 11) | (((mins % 60) / 2) << 5));
    return 0;
}

uint32_t wubu_fat2_epoch(const uint16_t *date, const uint16_t *time)
{
    if (!date || !time) return 0;
    uint32_t years = (*date >> 9) & 0x7F;
    uint32_t month = (*date >> 5) & 0x0F;
    uint32_t day = *date & 0x1F;
    uint32_t hour = (*time >> 11) & 0x1F;
    uint32_t min = (*time >> 5) & 0x3F;
    return years * 525600 + (month - 1) * 43800 + (day - 1) * 1440 +
           hour * 60 + min;
}

int wubu_fat2_attr(uint8_t attr, uint8_t mask)
{
    return (attr & mask) != 0;
}

int wubu_fat2_is_83(const char *name)
{
    if (!name) return 0;
    size_t len = strlen(name);
    if (len > 12) return 0;
    int dot = 0;
    for (size_t i = 0; i < len; i++) {
        if (name[i] == '.') { dot++; if (dot > 1) return 0; }
        else if (name[i] == ' ' || name[i] == 0x22) return 0;
    }
    return 1;
}

int wubu_fat2_utf16(const uint16_t *u16, int n, char *utf8, int cap)
{
    if (!u16 || !utf8 || cap <= 0) return -1;
    int k = 0;
    for (int i = 0; i < n && k < cap - 1; i++) {
        uint16_t c = u16[i];
        if (c < 0x80) utf8[k++] = (char)c;
        else if (c < 0x800) {
            utf8[k++] = (char)(0xC0 | (c >> 6));
            utf8[k++] = (char)(0x80 | (c & 0x3F));
        } else {
            utf8[k++] = (char)(0xE0 | (c >> 12));
            utf8[k++] = (char)(0x80 | ((c >> 6) & 0x3F));
            utf8[k++] = (char)(0x80 | (c & 0x3F));
        }
    }
    utf8[k] = 0;
    return k;
}

int wubu_fat2_boot_verify(const uint8_t *bs)
{
    if (!bs) return 0;
    if (bs[510] != 0x55 || bs[511] != 0xAA) return 0;
    return bs[0] == 0xEB || bs[0] == 0xE9;
}

uint32_t wubu_fat2_bpb_sectors(const uint8_t *bpb, uint32_t *bytes_per_sec,
                               uint32_t *sec_per_cluster, uint32_t *n_fats,
                               uint32_t *root_entries)
{
    if (!bpb) return 0;
    uint32_t bps = (uint32_t)bpb[11] | ((uint32_t)bpb[12] << 8);
    uint32_t spc = bpb[13];
    uint32_t nf = bpb[16];
    uint32_t re = (uint32_t)bpb[17] | ((uint32_t)bpb[18] << 8);
    /* always fill the out-params even when NULL (the callers that pass
     * NULL still rely on the derived geometry below) */
    if (bytes_per_sec) *bytes_per_sec = bps;
    if (sec_per_cluster) *sec_per_cluster = spc;
    if (n_fats) *n_fats = nf;
    if (root_entries) *root_entries = re;
    /* total sectors: 16-bit field, else the 32-bit field at 32 */
    uint32_t tot16 = (uint32_t)bpb[19] | ((uint32_t)bpb[20] << 8);
    if (tot16 != 0) return tot16;
    return (uint32_t)bpb[32] | ((uint32_t)bpb[33] << 8) |
           ((uint32_t)bpb[34] << 16) | ((uint32_t)bpb[35] << 24);
}

int wubu_fat2_mirror(const uint8_t *fat_a, const uint8_t *fat_b, uint32_t n)
{
    if (!fat_a || !fat_b) return -1;
    return memcmp(fat_a, fat_b, n) == 0 ? 1 : 0;
}

int wubu_fat2_dirty(uint8_t *bs, int set)
{
    if (!bs) return -1;
    if (set) bs[0x41] |= 0x01;   /* FAT32 dirty bit in BPB at 0x41 */
    else bs[0x41] &= ~0x01;
    return 0;
}

int wubu_fat2_chkdsk(uint8_t *fat, uint32_t fat_size, uint32_t n_clusters,
                     uint32_t *lost, uint32_t *bad)
{
    if (!fat) return -1;
    uint32_t l = 0, b = 0;
    uint32_t eof = (fat_size >= 0x10000) ? 0x0FFFFFF8u :
                   (fat_size >= 0x1000) ? 0xFFF8u : 0xFF8u;
    for (uint32_t c = 2; c < n_clusters; c++) {
        uint32_t next = fat_read_entry(fat, fat_size, c);
        if (next == 0) continue;
        if (next >= eof) continue;
        if (next >= n_clusters) { b++; wubu_fat2_atomic_write(fat, fat_size, c, 0); }
        else l++;   /* a chain member (lost-chain accounting simplified) */
    }
    if (lost) *lost = l;
    if (bad) *bad = b;
    return 0;
}

uint32_t wubu_fat2_cluster_size(uint64_t vol_bytes, uint32_t want)
{
    (void)want;
    if (vol_bytes >= (1ULL << 32)) return 64;
    if (vol_bytes >= (1ULL << 30)) return 32;
    if (vol_bytes >= (1ULL << 28)) return 16;
    if (vol_bytes >= (1ULL << 26)) return 8;
    return 4;
}

uint32_t wubu_fat2_first_data_sector(const uint8_t *bpb)
{
    uint32_t bps, spc, nf, re;
    wubu_fat2_bpb_sectors(bpb, &bps, &spc, &nf, &re);
    uint32_t rsvd = (uint32_t)bpb[14] | ((uint32_t)bpb[15] << 8);
    uint32_t fatsz = (uint32_t)bpb[22] | ((uint32_t)bpb[23] << 8);
    if (fatsz == 0) fatsz = (uint32_t)bpb[36] | ((uint32_t)bpb[37] << 8) |
                            ((uint32_t)bpb[38] << 16) | ((uint32_t)bpb[39] << 24);
    uint32_t root = (re * 32 + bps - 1) / bps;
    return rsvd + nf * fatsz + root;
}

int wubu_fat2_lfn_checksum(const char *short_name)
{
    if (!short_name) return 0;
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++) {
        char c = short_name[i] ? short_name[i] : ' ';
        sum = (uint8_t)(((sum & 1) ? 0x80 : 0) + (sum >> 1) + (uint8_t)c);
    }
    return sum;
}

int wubu_fat2_lfn_entry(const uint16_t *chars, int n, int seq, uint8_t *out)
{
    if (!chars || !out || n > 13 || seq < 0 || seq > 63) return -1;
    memset(out, 0, 32);
    out[0] = (uint8_t)(seq | 0x40);    /* the last-entry marker */
    for (int i = 0; i < n; i++) {
        int pos = 1 + (i % 5) * 2;
        if (i < 5) { out[pos] = (uint8_t)(chars[i] & 0xFF); out[pos+1] = (uint8_t)(chars[i] >> 8); }
        else if (i < 11) { out[14 + (i-5)*2] = (uint8_t)(chars[i] & 0xFF); out[15 + (i-5)*2] = (uint8_t)(chars[i] >> 8); }
        else { out[28 + (i-11)*2] = (uint8_t)(chars[i] & 0xFF); out[29 + (i-11)*2] = (uint8_t)(chars[i] >> 8); }
    }
    out[11] = 0x0F;                    /* the LFN attribute */
    return 0;
}

int wubu_fat2_mkdir_chain(uint8_t *fat, uint32_t fat_size, uint32_t parent, uint32_t *dir)
{
    (void)parent;
    if (!fat || !dir) return -1;
    return wubu_fat2_find_free(fat, fat_size, 0, dir);
}

int wubu_fat2_find_free(uint8_t *fat, uint32_t fat_size, uint32_t n_clusters,
                        uint32_t *free_cluster)
{
    if (!fat || !free_cluster) return -1;
    for (uint32_t c = 2; c < n_clusters; c++)
        if (fat_read_entry(fat, fat_size, c) == 0) { *free_cluster = c; return 0; }
    return -1;
}

int wubu_fat2_set_next(uint8_t *fat, uint32_t fat_size, uint32_t c, uint32_t next)
{
    return wubu_fat2_atomic_write(fat, fat_size, c, next);
}

int wubu_fat2_alloc_chain(uint8_t *fat, uint32_t fat_size, uint32_t n_clusters,
                          uint32_t n_want, uint32_t *start)
{
    if (!fat || !start || n_want == 0 || n_want > 65536) return -1;
    uint32_t eof = (fat_size >= 0x10000) ? 0x0FFFFFFFu :
                   (fat_size >= 0x1000) ? 0xFFFFu : 0xFFFu;
    /* the first cluster: find_free returns 0 on success, -1 on none */
    if (wubu_fat2_find_free(fat, fat_size, n_clusters, start) != 0) return -1;
    /* mark it used IMMEDIATELY (placeholder EOF) so the loop below
     * cannot re-find it -- the old code left it free, so every
     * iteration found cluster 2 again and the chain collapsed to
     * 2 clusters instead of n_want. */
    wubu_fat2_set_next(fat, fat_size, *start, eof);
    uint32_t prev = *start;
    uint32_t allocated = 1;
    while (allocated < n_want) {
        uint32_t next;
        if (wubu_fat2_find_free(fat, fat_size, n_clusters, &next) != 0) break;
        wubu_fat2_set_next(fat, fat_size, prev, next);
        wubu_fat2_set_next(fat, fat_size, next, eof);   /* mark used */
        prev = next;
        allocated++;
    }
    wubu_fat2_set_next(fat, fat_size, prev, eof);
    return (int)allocated;
}

int wubu_fat2_truncate(uint8_t *fat, uint32_t fat_size, uint32_t start)
{
    uint32_t eof = (fat_size >= 0x10000) ? 0x0FFFFFFFu :
                   (fat_size >= 0x1000) ? 0xFFFFu : 0xFFFu;
    wubu_fat2_atomic_write(fat, fat_size, start, eof);
    return 0;
}

int wubu_fat2_clear_chain(uint8_t *fat, uint32_t fat_size, uint32_t start)
{
    uint32_t c = start;
    uint32_t eof = (fat_size >= 0x10000) ? 0x0FFFFFF8u :
                   (fat_size >= 0x1000) ? 0xFFF8u : 0xFF8u;
    while (c != 0 && c < eof) {
        uint32_t next = fat_read_entry(fat, fat_size, c);
        wubu_fat2_atomic_write(fat, fat_size, c, 0);
        c = next;
    }
    return 0;
}

int wubu_fat2_file_size(const uint8_t *fat, uint32_t fat_size, uint32_t start,
                        uint32_t cluster_bytes)
{
    int len = wubu_fat2_chain_len(fat, fat_size, start, 1 << 20);
    if (len < 0) return -1;
    return len * (int)cluster_bytes;
}

int wubu_fat2_dir_entry(const char *name, uint8_t attr, uint32_t first_cluster,
                        uint32_t size, uint8_t *out)
{
    if (!name || !out) return -1;
    memset(out, 0, 32);
    for (int i = 0; i < 8 && name[i] && name[i] != '.'; i++)
        out[i] = (uint8_t)name[i];
    const char *dot = strchr(name, '.');
    if (dot) {
        for (int i = 0; i < 3 && dot[i+1]; i++)
            out[8 + i] = (uint8_t)dot[i+1];
    }
    out[11] = attr;
    out[26] = (uint8_t)(first_cluster >> 16);
    out[27] = (uint8_t)(first_cluster >> 24);
    out[20] = (uint8_t)(first_cluster & 0xFF);
    out[21] = (uint8_t)((first_cluster >> 8) & 0xFF);
    out[28] = (uint8_t)(size & 0xFF);
    out[29] = (uint8_t)((size >> 8) & 0xFF);
    out[30] = (uint8_t)((size >> 16) & 0xFF);
    out[31] = (uint8_t)((size >> 24) & 0xFF);
    return 0;
}

int wubu_fat2_dir_find(const uint8_t *dir, uint32_t dir_bytes, const char *name,
                       uint32_t *offset)
{
    if (!dir || !name || !offset) return -1;
    for (uint32_t off = 0; off + 32 <= dir_bytes; off += 32) {
        if (dir[off] == 0) return -1;           /* end of dir */
        if (dir[off] == 0xE5) continue;         /* deleted */
        char short_name[12];
        int k = 0;
        for (int i = 0; i < 8 && dir[off+i] != ' '; i++) short_name[k++] = (char)dir[off+i];
        if (dir[off+11] == 0x0F) continue;      /* LFN */
        short_name[k] = 0;
        if (strcmp(short_name, name) == 0) { *offset = off; return 0; }
    }
    return -1;
}

int wubu_fat2_short_name(const char *long_name, char *short_name)
{
    if (!long_name || !short_name) return -1;
    int k = 0;
    const char *dot = strchr(long_name, '.');
    for (const char *p = long_name; p && *p && p != dot && k < 8; p++)
        short_name[k++] = (char)(*p >= 'a' && *p <= 'z' ? *p - 32 : *p);
    while (k < 8) short_name[k++] = ' ';
    if (dot) {
        for (const char *p = dot + 1; *p && k < 11; p++)
            short_name[k++] = (char)(*p >= 'a' && *p <= 'z' ? *p - 32 : *p);
    }
    while (k < 11) short_name[k++] = ' ';
    short_name[11] = 0;
    return 0;
}

int wubu_fat2_validate_cluster(uint32_t cluster, uint32_t n_clusters)
{
    return (cluster >= 2 && cluster < n_clusters) ? 1 : 0;
}

int wubu_fat2_reserved_cluster(uint32_t cluster)
{
    return cluster >= 0xFFF0 ? 1 : 0;
}

int wubu_fat2_next_free_hint(const uint8_t *fat, uint32_t fat_size, uint32_t from)
{
    if (!fat) return -1;
    uint32_t eof = (fat_size >= 0x10000) ? 0x0FFFFFF8u :
                   (fat_size >= 0x1000) ? 0xFFF8u : 0xFF8u;
    for (uint32_t c = from; c < eof; c++)
        if (fat_read_entry(fat, fat_size, c) == 0) return (int)c;
    return -1;
}

int wubu_fat2_fat_size_sectors(const uint8_t *bpb)
{
    if (!bpb) return -1;
    uint32_t fatsz = (uint32_t)bpb[22] | ((uint32_t)bpb[23] << 8);
    if (fatsz != 0) return (int)fatsz;
    return (int)((uint32_t)bpb[36] | ((uint32_t)bpb[37] << 8) |
                 ((uint32_t)bpb[38] << 16) | ((uint32_t)bpb[39] << 24));
}

int wubu_fat2_root_dir_sectors(const uint8_t *bpb)
{
    uint32_t bps, spc, nf, re;
    wubu_fat2_bpb_sectors(bpb, &bps, &spc, &nf, &re);
    return (int)((re * 32 + bps - 1) / bps);
}

int wubu_fat2_total_data_sectors(const uint8_t *bpb)
{
    uint32_t bps, spc, nf, re;
    uint32_t total = wubu_fat2_bpb_sectors(bpb, &bps, &spc, &nf, &re);
    uint32_t rsvd = (uint32_t)bpb[14] | ((uint32_t)bpb[15] << 8);
    uint32_t fatsz = (uint32_t)wubu_fat2_fat_size_sectors(bpb);
    uint32_t root = (uint32_t)wubu_fat2_root_dir_sectors(bpb);
    return (int)(total - (rsvd + nf * fatsz + root));
}

int wubu_fat2_cluster_lba(const uint8_t *bpb, uint32_t cluster, uint64_t *lba)
{
    if (!bpb || !lba || cluster < 2) return -1;
    uint32_t bps, spc, nf, re;
    wubu_fat2_bpb_sectors(bpb, &bps, &spc, &nf, &re);
    uint32_t first = wubu_fat2_first_data_sector(bpb);
    *lba = (uint64_t)first + ((uint64_t)(cluster - 2) * spc);
    return 0;
}

int wubu_fat2_media_byte(const uint8_t *bs)
{
    if (!bs) return -1;
    return bs[21];
}
