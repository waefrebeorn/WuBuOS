/*
 * fw_media.c  --  WuBuFW media stack: MBR/GPT partition scan + FAT12/16/32
 *                 read-only filesystem.
 *
 * A UEFI implementation must be able to find \EFI\BOOT\BOOTX64.EFI on a FAT
 * ESP. This is a self-contained FAT reader (no reuse of the kernel's fat32
 * module, which assumes a hosted allocator) covering the three FAT widths,
 * LFN reassembly, and cluster-chain traversal with a one-sector FAT cache.
 */

#include "fw.h"
#include "fw_block.h"

#define SECTOR 512
#define MAX_VOLUMES 8

struct fw_volume {
    int blk;                      /* index into the block layer */
    uint64_t    lba_start;      /* partition first LBA          */
    uint64_t    lba_count;
    /* BPB-derived geometry */
    uint32_t    bytes_per_sec;
    uint32_t    sec_per_clus;
    uint32_t    reserved_sec;
    uint32_t    num_fats;
    uint32_t    root_entries;   /* FAT12/16 only                */
    uint32_t    fat_size;       /* sectors per FAT              */
    uint32_t    total_sec;
    uint32_t    root_cluster;   /* FAT32 only                   */
    uint32_t    first_data_sec;
    uint32_t    cluster_count;
    int         fat_bits;       /* 12, 16 or 32                 */
    char        label[12];     /* volume label (11+null)        */
    /* one-sector FAT cache */
    uint64_t    fat_cached_lba;
    uint8_t     fat_cache[SECTOR * 2];   /* 2 sectors: FAT12 straddle */
    uint8_t     valid;
};

struct fw_openfile {
    struct fw_volume *vol;
    uint32_t first;      /* first data cluster */
    uint32_t size;
    uint64_t pos;
};

static struct fw_volume g_vol[MAX_VOLUMES];
static int g_nvol;

/* -- raw partition I/O --------------------------------------------- */

static int vol_read(struct fw_volume *v, uint64_t lba, uint32_t n, void *buf) {
    if (lba + n > v->lba_count) return -1;
    return fw_block_read(v->blk, v->lba_start + lba, n, buf);
}

/* -- BPB parse ------------------------------------------------------ */

static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t rd32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int fat_mount(struct fw_volume *v) {
    uint8_t bs[SECTOR];
    if (vol_read(v, 0, 1, bs) != 0) return -1;
    if (rd16(bs + 510) != 0xAA55) return -1;

    v->bytes_per_sec = rd16(bs + 11);
    v->sec_per_clus  = bs[13];
    v->reserved_sec  = rd16(bs + 14);
    v->num_fats      = bs[16];
    v->root_entries  = rd16(bs + 17);
    uint32_t total16 = rd16(bs + 19);
    uint32_t fat16sz = rd16(bs + 22);
    uint32_t total32 = rd32(bs + 32);
    uint32_t fat32sz = rd32(bs + 36);

    if (v->bytes_per_sec != SECTOR) return -1;
    if (v->sec_per_clus == 0 || (v->sec_per_clus & (v->sec_per_clus - 1))) return -1;
    if (v->num_fats == 0 || v->num_fats > 4) return -1;
    if (v->reserved_sec == 0) return -1;

    v->fat_size  = fat16sz ? fat16sz : fat32sz;
    v->total_sec = total16 ? total16 : total32;
    if (v->fat_size == 0 || v->total_sec == 0) return -1;

    uint32_t root_sec = ((v->root_entries * 32) + (SECTOR - 1)) / SECTOR;
    v->first_data_sec = v->reserved_sec + v->num_fats * v->fat_size + root_sec;
    if (v->first_data_sec >= v->total_sec) return -1;
    v->cluster_count = (v->total_sec - v->first_data_sec) / v->sec_per_clus;

    if (v->cluster_count < 4085)       v->fat_bits = 12;
    else if (v->cluster_count < 65525) v->fat_bits = 16;
    else                               v->fat_bits = 32;

    v->root_cluster    = (v->fat_bits == 32) ? rd32(bs + 44) : 0;
    v->fat_cached_lba  = (uint64_t)-1;
    v->valid           = 1;
    return 0;
}

/* -- FAT chain ------------------------------------------------------ */

static uint32_t fat_next(struct fw_volume *v, uint32_t clus) {
    uint64_t bit_off;
    switch (v->fat_bits) {
    case 12: bit_off = (uint64_t)clus * 12; break;
    case 16: bit_off = (uint64_t)clus * 16; break;
    default: bit_off = (uint64_t)clus * 32; break;
    }
    uint64_t byte_off = bit_off / 8;
    uint64_t sec = v->reserved_sec + byte_off / SECTOR;
    if (v->fat_cached_lba != sec) {
        if (vol_read(v, sec, 2, v->fat_cache) != 0) {
            if (vol_read(v, sec, 1, v->fat_cache) != 0) return 0x0FFFFFFF;
            fw_memset(v->fat_cache + SECTOR, 0, SECTOR);
        }
        v->fat_cached_lba = sec;
    }
    uint32_t off = (uint32_t)(byte_off % SECTOR);
    const uint8_t *p = v->fat_cache + off;

    switch (v->fat_bits) {
    case 12: {
        uint16_t raw = (uint16_t)(p[0] | (p[1] << 8));
        uint32_t val = (clus & 1) ? (raw >> 4) : (raw & 0x0FFF);
        return (val >= 0x0FF8) ? 0x0FFFFFFF : val;
    }
    case 16: {
        uint32_t val = rd16(p);
        return (val >= 0xFFF8) ? 0x0FFFFFFF : val;
    }
    default: {
        uint32_t val = rd32(p) & 0x0FFFFFFF;
        return (val >= 0x0FFFFFF8) ? 0x0FFFFFFF : val;
    }
    }
}

static uint64_t clus_lba(struct fw_volume *v, uint32_t clus) {
    return v->first_data_sec + (uint64_t)(clus - 2) * v->sec_per_clus;
}

/* -- directory scan -------------------------------------------------- */

typedef struct {
    uint32_t cluster;
    uint32_t size;
    uint8_t  attr;
    int      found;
} fat_entry;

static void name83(const uint8_t *de, char *out) {
    int n = 0;
    for (int i = 0; i < 8 && de[i] != ' '; i++) out[n++] = (char)de[i];
    if (de[8] != ' ') {
        out[n++] = '.';
        for (int i = 8; i < 11 && de[i] != ' '; i++) out[n++] = (char)de[i];
    }
    out[n] = 0;
}

static char upc(char c) { return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c; }

static int name_eq(const char *a, const char *b) {
    while (*a && *b) { if (upc(*a) != upc(*b)) return 0; a++; b++; }
    return *a == 0 && *b == 0;
}

/* Search one directory (cluster chain, or FAT12/16 fixed root when
 * dir_cluster == 0) for `want`. Matches 8.3 and LFN. */
static int dir_find(struct fw_volume *v, uint32_t dir_cluster, const char *want, fat_entry *out) {
    uint8_t sec[SECTOR];
    char lfn[260];
    int  lfn_len = 0;
    lfn[0] = 0;

    uint32_t clus = dir_cluster;
    int fixed_root = (dir_cluster == 0 && v->fat_bits != 32);
    uint32_t root_sec_count = ((v->root_entries * 32) + SECTOR - 1) / SECTOR;
    uint64_t fixed_lba = v->reserved_sec + v->num_fats * v->fat_size;

    for (;;) {
        uint32_t nsec = fixed_root ? root_sec_count : v->sec_per_clus;
        for (uint32_t s = 0; s < nsec; s++) {
            uint64_t lba = fixed_root ? (fixed_lba + s) : (clus_lba(v, clus) + s);
            if (vol_read(v, lba, 1, sec) != 0) return -1;

            for (int e = 0; e < SECTOR / 32; e++) {
                const uint8_t *de = sec + e * 32;
                if (de[0] == 0x00) return 0;              /* end of dir */
                if (de[0] == 0xE5) { lfn_len = 0; lfn[0] = 0; continue; }

                if (de[11] == 0x0F) {                     /* LFN slot */
                    int ord = de[0] & 0x3F;
                    if (ord < 1 || ord > 20) { lfn_len = 0; continue; }
                    int base = (ord - 1) * 13;
                    static const int idx[13] = {1,3,5,7,9,14,16,18,20,22,24,28,30};
                    for (int i = 0; i < 13; i++) {
                        uint16_t ch = rd16(de + idx[i]);
                        if (ch == 0xFFFF || ch == 0) { if (base + i < 259) lfn[base + i] = 0; continue; }
                        if (base + i < 259) lfn[base + i] = (ch < 128) ? (char)ch : '_';
                    }
                    if (base + 13 > lfn_len) lfn_len = base + 13;
                    if (lfn_len < 260) lfn[lfn_len < 259 ? lfn_len : 259] = 0;
                    continue;
                }

                if (de[11] & 0x08) { lfn_len = 0; lfn[0] = 0; continue; }  /* volume label */

                char shortname[13];
                name83(de, shortname);
                int hit = name_eq(shortname, want) || (lfn_len && name_eq(lfn, want));
                lfn_len = 0; lfn[0] = 0;
                if (!hit) continue;

                out->cluster = ((uint32_t)rd16(de + 20) << 16) | rd16(de + 26);
                out->size    = rd32(de + 28);
                out->attr    = de[11];
                out->found   = 1;
                return 0;
            }
        }
        if (fixed_root) return 0;
        clus = fat_next(v, clus);
        if (clus < 2 || clus >= 0x0FFFFFF8) return 0;
    }
}

/* -- path walk ------------------------------------------------------- */

static int path_resolve(struct fw_volume *v, const char *path, fat_entry *out) {
    if (!v->valid) return -1;
    uint32_t dir = (v->fat_bits == 32) ? v->root_cluster : 0;
    char comp[256];
    const char *p = path;

    fw_memset(out, 0, sizeof(*out));
    while (*p == '\\' || *p == '/') p++;

    if (!*p) {   /* root itself */
        out->cluster = dir;
        out->attr = 0x10;
        out->found = 1;
        return 0;
    }

    while (*p) {
        size_t n = 0;
        while (p[n] && p[n] != '\\' && p[n] != '/') {
            if (n >= sizeof(comp) - 1) return -1;
            comp[n] = p[n]; n++;
        }
        comp[n] = 0;
        p += n;
        while (*p == '\\' || *p == '/') p++;

        fat_entry e = {0};
        if (dir_find(v, dir, comp, &e) != 0 || !e.found) return -1;

        if (*p) {                                   /* must be a directory */
            if (!(e.attr & 0x10)) return -1;
            dir = e.cluster;
            if (dir == 0 && v->fat_bits == 32) dir = v->root_cluster;
        } else {
            *out = e;
            return 0;
        }
    }
    return -1;
}

int fw_vol_stat(fw_volume *v, const char *path, uint64_t *size, uint32_t *attr) {
    fat_entry e;
    if (path_resolve(v, path, &e) != 0 || !e.found) return -1;
    if (size) *size = e.size;
    if (attr) *attr = e.attr;
    return 0;
}

int fw_vol_read_file(fw_volume *v, const char *path, void **out, uint64_t *size) {
    fat_entry e;
    if (!out) return -1;
    if (path_resolve(v, path, &e) != 0 || !e.found) return -1;
    if (e.attr & 0x10) return -1;

    uint64_t bytes = e.size;
    size_t pages = (size_t)((bytes + EFI_PAGE_SIZE - 1) / EFI_PAGE_SIZE);
    if (pages == 0) pages = 1;
    uint8_t *buf = fw_alloc_pages(pages, EfiLoaderData);
    if (!buf) return -1;

    uint32_t clus = e.cluster;
    uint64_t done = 0;
    uint32_t csize = v->sec_per_clus * SECTOR;
    uint8_t  tmp[SECTOR];

    while (done < bytes && clus >= 2 && clus < 0x0FFFFFF8) {
        uint64_t lba = clus_lba(v, clus);
        uint64_t left = bytes - done;
        uint32_t want = (left >= csize) ? csize : (uint32_t)left;
        uint32_t whole = want / SECTOR;

        if (whole && vol_read(v, lba, whole, buf + done) != 0) { fw_free_pages(buf, pages); return -1; }
        done += (uint64_t)whole * SECTOR;
        uint32_t rem = want - whole * SECTOR;
        if (rem) {
            if (vol_read(v, lba + whole, 1, tmp) != 0) { fw_free_pages(buf, pages); return -1; }
            fw_memcpy(buf + done, tmp, rem);
            done += rem;
        }
        clus = fat_next(v, clus);
    }
    if (done < bytes) { fw_free_pages(buf, pages); return -1; }

    *out = buf;
    if (size) *size = bytes;
    return 0;
}

/* -- partition discovery --------------------------------------------- */

static void add_volume(int blk, uint64_t start, uint64_t count) {
    if (g_nvol >= MAX_VOLUMES) return;
    struct fw_volume *v = &g_vol[g_nvol];
    fw_memset(v, 0, sizeof(*v));
    v->blk = blk;
    v->lba_start = start;
    v->lba_count = count;
    if (fat_mount(v) == 0) {
        fw_printf("[media] vol%d: FAT%d  lba=%lu  %u clusters  %u sec/clus\n",
                  g_nvol, v->fat_bits, start, v->cluster_count, v->sec_per_clus);
        g_nvol++;
    }
}

static void scan_gpt(int blk) {
    uint8_t hdr[SECTOR];
    if (fw_block_read(blk, 1, 1, hdr) != 0) return;
    if (fw_memcmp(hdr, "EFI PART", 8) != 0) return;

    uint64_t part_lba = *(const uint64_t *)(hdr + 72);
    uint32_t nparts   = *(const uint32_t *)(hdr + 80);
    uint32_t psize    = *(const uint32_t *)(hdr + 84);
    if (psize < 128 || psize > SECTOR) return;
    if (nparts > 128) nparts = 128;

    uint8_t sec[SECTOR];
    uint32_t per_sec = SECTOR / psize;
    for (uint32_t i = 0; i < nparts; i++) {
        if (i % per_sec == 0) {
            if (fw_block_read(blk, part_lba + i / per_sec, 1, sec) != 0) return;
        }
        const uint8_t *pe = sec + (i % per_sec) * psize;
        int zero = 1;
        for (int b = 0; b < 16; b++) if (pe[b]) { zero = 0; break; }
        if (zero) continue;
        uint64_t first = *(const uint64_t *)(pe + 32);
        uint64_t last  = *(const uint64_t *)(pe + 40);
        if (last < first) continue;
        add_volume(blk, first, last - first + 1);
    }
}

static void scan_mbr(int blk) {
    uint8_t mbr[SECTOR];
    if (fw_block_read(blk, 0, 1, mbr) != 0) return;
    if (rd16(mbr + 510) != 0xAA55) return;

    int any = 0;
    for (int i = 0; i < 4; i++) {
        const uint8_t *pe = mbr + 446 + i * 16;
        uint8_t type = pe[4];
        if (type == 0 || type == 0xEE) continue;         /* 0xEE = protective */
        uint32_t first = rd32(pe + 8);
        uint32_t count = rd32(pe + 12);
        if (!count) continue;
        add_volume(blk, first, count);
        any = 1;
    }
    /* Superfloppy: FAT directly at LBA 0, no partition table semantics. */
    if (!any) add_volume(blk, 0, fw_block_get(blk)->sectors);
}

int fw_media_init(void) {
    g_nvol = 0;
    /* Every registered block device is scanned, so IDE, AHCI and NVMe all
     * contribute volumes through the same path. */
    int n = fw_block_count();
    for (int i = 0; i < n; i++) {
        uint8_t mbr[SECTOR];
        if (fw_block_read(i, 0, 1, mbr) != 0) continue;
        int protective = 0;
        if (rd16(mbr + 510) == 0xAA55) {
            for (int p = 0; p < 4; p++) if (mbr[446 + p * 16 + 4] == 0xEE) protective = 1;
        }
        if (protective) scan_gpt(i);
        else            scan_mbr(i);
    }
    return g_nvol;
}

int fw_media_count(void) { return g_nvol; }

fw_volume *fw_media_get(int i) {
    if (i < 0 || i >= g_nvol) return NULL;
    return &g_vol[i];
}

/* -- directory iteration for the shell ---------------------------- */

/* Minimal scan of the root directory (or a given dir cluster) for the
 * shell's `ls`. FAT32 root is a cluster chain; FAT12/16 root is fixed. */
static uint32_t g_scan_clus;
static uint32_t g_scan_idx;

const char *fw_volume_label(int vol) {
    static char buf[12];
    struct fw_volume *v = &g_vol[vol];
    if (!v->valid) return NULL;
    fw_memcpy(buf, v->label, sizeof(v->label));
    buf[sizeof(buf)-1] = 0;
    return buf;
}

void fw_volume_reset(int vol) {
    struct fw_volume *v = &g_vol[vol];
    if (!v->valid) { g_scan_clus = 0; return; }
    g_scan_clus = (v->fat_bits == 32) ? v->root_cluster
                                      : v->reserved_sec + v->num_fats * v->fat_size;
    g_scan_idx = 0;
}

/* Read one directory entry from the current scan position. */
static int read_dir_entry(struct fw_volume *v, uint32_t *clus, uint32_t *idx, fw_dirent *out) {
    static uint8_t sec[SECTOR];
    for (;;) {
        uint32_t sec_in_clus = (*idx) / (SECTOR / 32);
        uint32_t lba;
        if (v->fat_bits == 32) {
            if (*clus < 2) return 0;
            lba = clus_lba(v, *clus) + sec_in_clus;
        } else {
            uint32_t root_sec = v->reserved_sec + v->num_fats * v->fat_size;
            if (*idx * 32 >= v->root_entries * 32) return 0;
            lba = root_sec + (*idx) / (SECTOR / 32);
        }
        if (vol_read(v, lba, 1, sec) != 0) return 0;
        uint32_t e = (*idx) % (SECTOR / 32);
        uint8_t *de = sec + e * 32;
        (*idx)++;
        if (de[0] == 0) return 0;                 /* end of dir */
        if (de[0] == 0xE5) continue;              /* deleted */
        if (de[11] & 0x0F) continue;              /* LFN */
        if (de[11] & 0x08) continue;              /* volume label entry */
        /* Build the 8.3 name. */
        int k = 0;
        for (int i = 0; i < 8; i++) { char c = de[i]; if (c == ' ') break; out->name[k++] = c; }
        if (de[8] != ' ') {
            out->name[k++] = '.';
            for (int i = 8; i < 11; i++) { char c = de[i]; if (c == ' ' ) continue; out->name[k++] = c; }
        }
        out->name[k] = 0;
        out->attr = de[11];
        out->is_dir = (de[11] & 0x10) != 0;
        out->size = rd32(de + 28);
        out->valid = 1;
        return 1;
    }
}

fw_dirent *fw_volume_next(int vol) {
    static fw_dirent e;
    struct fw_volume *v = &g_vol[vol];
    if (!v->valid) return NULL;
    if (read_dir_entry(v, &g_scan_clus, &g_scan_idx, &e) != 1) return NULL;
    return &e;
}

/* -- open a file by path (shell + boot) --------------------------- */

/* blocking char read for the shell (polls the keyboard/serial) */
int fw_getc(void) {
    for (;;) { int c = fw_getc_nb(); if (c >= 0) return c; __asm__ volatile("":::"memory"); }
}

fw_openfile *fw_volume_open(struct fw_volume *v, const char *path) {
    fat_entry fe;
    if (path_resolve(v, path, &fe) != 0 || !fe.found) return NULL;
    fw_openfile *f = fw_pool_alloc(sizeof(fw_openfile), EfiBootServicesData);
    if (!f) return NULL;
    f->vol = v;
    f->first = fe.cluster;
    f->size = fe.size;
    f->pos = 0;
    return f;
}

/* Read up to `count` bytes at file offset `off` (cluster-walk), returns n. */
uint32_t fw_openfile_pread(fw_openfile *f, uint64_t off, uint32_t count, void *buf) {
    if (off >= f->size) return 0;
    if (off + count > f->size) count = (uint32_t)(f->size - off);
    struct fw_volume *v = f->vol;
    uint32_t clus = f->first;
    uint32_t per = v->sec_per_clus * v->bytes_per_sec;
    uint32_t skip = (uint32_t)(off / per);
    for (uint32_t i = 0; i < skip && clus >= 2; i++) clus = fat_next(v, clus);
    uint32_t done = 0;
    uint32_t in_off = (uint32_t)(off % per);
    static uint8_t sec[SECTOR];
    while (done < count && clus >= 2) {
        uint64_t lba = clus_lba(v, clus) + in_off / v->bytes_per_sec;
        uint32_t sec_off = in_off % v->bytes_per_sec;
        if (vol_read(v, lba, 1, sec) != 0) break;
        uint32_t take = v->bytes_per_sec - sec_off;
        if (take > count - done) take = count - done;
        fw_memcpy((uint8_t*)buf + done, sec + sec_off, take);
        done += take;
        in_off += take;
        if (in_off >= per) { clus = fat_next(v, clus); in_off = 0; }
    }
    return done;
}

uint64_t fw_openfile_size(fw_openfile *f) { return f ? f->size : 0; }
