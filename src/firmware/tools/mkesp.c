/*
 * mkesp.c  --  WuBuOS FAT32 ESP image builder (C11, self-contained).
 *
 * Creates a GPT-partitioned disk image with a single EFI System Partition
 * formatted FAT32, and copies files into it at given paths. No mkfs, no
 * mtools: the on-disk structures are written directly so the format is
 * exactly what our firmware's FAT reader is tested against.
 *
 * usage: mkesp <out.img> <size_mb> [<src_file> <dest\path>]...
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define SECTOR       512
/* 512B clusters: FAT32 is only legal at >= 65525 clusters, so small ESP
 * images must use 1 sector per cluster to stay out of FAT16 territory. */
#define SEC_PER_CLUS 1
#define RESERVED     32
#define NUM_FATS     2

static uint8_t *disk;
static uint64_t disk_sectors;

static uint64_t part_start;         /* ESP first LBA */
static uint64_t part_sectors;
static uint32_t fat_size;           /* sectors per FAT */
static uint32_t first_data_sec;     /* relative to partition */
static uint32_t root_cluster = 2;
static uint32_t next_free_cluster = 3;

static uint8_t *sec_ptr(uint64_t lba) { return disk + lba * SECTOR; }
static uint8_t *psec(uint64_t rel)    { return sec_ptr(part_start + rel); }

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8*i)); }
static void put64(uint8_t *p, uint64_t v) { for (int i = 0; i < 8; i++) p[i] = (uint8_t)(v >> (8*i)); }

/* ---- CRC32 (GPT) ---- */
static uint32_t crc32_buf(const void *data, size_t n) {
    static uint32_t tbl[256];
    static int init = 0;
    if (!init) {
        for (uint32_t i = 0; i < 256; i++) {
            uint32_t c = i;
            for (int k = 0; k < 8; k++) c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            tbl[i] = c;
        }
        init = 1;
    }
    uint32_t crc = 0xFFFFFFFFu;
    const uint8_t *p = data;
    for (size_t i = 0; i < n; i++) crc = tbl[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

/* ---- FAT table access ---- */
static void fat_set(uint32_t clus, uint32_t val) {
    for (int f = 0; f < NUM_FATS; f++) {
        uint64_t off = (uint64_t)RESERVED * SECTOR + (uint64_t)f * fat_size * SECTOR + clus * 4ull;
        put32(psec(0) + off, val & 0x0FFFFFFF);
    }
}

static uint32_t clus_alloc(void) {
    uint32_t c = next_free_cluster++;
    fat_set(c, 0x0FFFFFFF);
    return c;
}

static uint8_t *clus_ptr(uint32_t clus) {
    uint64_t rel = first_data_sec + (uint64_t)(clus - 2) * SEC_PER_CLUS;
    return psec(rel);
}

/* ---- 8.3 name ---- */
static void make83(const char *name, uint8_t out[11]) {
    memset(out, ' ', 11);
    int i = 0, j = 0;
    for (; name[i] && name[i] != '.' && j < 8; i++) {
        char c = name[i];
        if (c >= 'a' && c <= 'z') c = (char)(c - 32);
        out[j++] = (uint8_t)c;
    }
    while (name[i] && name[i] != '.') i++;
    if (name[i] == '.') {
        i++;
        for (int k = 0; name[i] && k < 3; i++, k++) {
            char c = name[i];
            if (c >= 'a' && c <= 'z') c = (char)(c - 32);
            out[8 + k] = (uint8_t)c;
        }
    }
}

/* Find or create a subdirectory `name` in directory cluster `dir`. */
static uint32_t dir_mkdir(uint32_t dir, const char *name) {
    uint8_t n83[11];
    make83(name, n83);
    uint8_t *d = clus_ptr(dir);
    int slots = SEC_PER_CLUS * SECTOR / 32;

    for (int i = 0; i < slots; i++) {
        uint8_t *e = d + i * 32;
        if (e[0] == 0) {                              /* free: create */
            memcpy(e, n83, 11);
            e[11] = 0x10;                             /* directory */
            uint32_t nc = clus_alloc();
            put16(e + 20, (uint16_t)(nc >> 16));
            put16(e + 26, (uint16_t)(nc & 0xFFFF));
            put32(e + 28, 0);

            uint8_t *nd = clus_ptr(nc);
            memset(nd, 0, SEC_PER_CLUS * SECTOR);
            memset(nd, ' ', 11); nd[0] = '.';  nd[11] = 0x10;
            put16(nd + 20, (uint16_t)(nc >> 16));
            put16(nd + 26, (uint16_t)(nc & 0xFFFF));
            memset(nd + 32, ' ', 11); nd[32] = '.'; nd[33] = '.'; nd[32 + 11] = 0x10;
            uint32_t pc = (dir == root_cluster) ? 0 : dir;
            put16(nd + 32 + 20, (uint16_t)(pc >> 16));
            put16(nd + 32 + 26, (uint16_t)(pc & 0xFFFF));
            return nc;
        }
        if (e[11] == 0x0F) continue;
        if (memcmp(e, n83, 11) == 0 && (e[11] & 0x10))
            return ((uint32_t)(e[21] << 8 | e[20]) << 16) | (uint32_t)(e[27] << 8 | e[26]);
    }
    fprintf(stderr, "mkesp: directory full\n");
    exit(1);
}

static void dir_addfile(uint32_t dir, const char *name, const uint8_t *data, uint32_t size) {
    uint8_t n83[11];
    make83(name, n83);
    uint8_t *d = clus_ptr(dir);
    int slots = SEC_PER_CLUS * SECTOR / 32;

    uint32_t clus_bytes = SEC_PER_CLUS * SECTOR;
    uint32_t nclus = size ? (size + clus_bytes - 1) / clus_bytes : 1;
    uint32_t first = 0, prev = 0;
    for (uint32_t i = 0; i < nclus; i++) {
        uint32_t c = clus_alloc();
        if (!first) first = c;
        if (prev) fat_set(prev, c);
        uint32_t off = i * clus_bytes;
        uint32_t n = (size - off > clus_bytes) ? clus_bytes : (size - off);
        if (size > off) memcpy(clus_ptr(c), data + off, n);
        prev = c;
    }

    for (int i = 0; i < slots; i++) {
        uint8_t *e = d + i * 32;
        if (e[0] != 0) continue;
        memcpy(e, n83, 11);
        e[11] = 0x20;                                /* archive */
        put16(e + 20, (uint16_t)(first >> 16));
        put16(e + 26, (uint16_t)(first & 0xFFFF));
        put32(e + 28, size);
        return;
    }
    fprintf(stderr, "mkesp: directory full\n");
    exit(1);
}

/* Copy `src` into the image at backslash-separated `dest`. */
static void add_path(const char *src, const char *dest) {
    FILE *f = fopen(src, "rb");
    if (!f) { perror(src); exit(1); }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *buf = malloc((size_t)(n ? n : 1));
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) { fprintf(stderr, "read %s\n", src); exit(1); }
    fclose(f);

    char path[512];
    snprintf(path, sizeof(path), "%s", dest);
    for (char *p = path; *p; p++) if (*p == '/') *p = '\\';

    char *cur = path;
    while (*cur == '\\') cur++;
    uint32_t dir = root_cluster;
    for (;;) {
        char *slash = strchr(cur, '\\');
        if (!slash) break;
        *slash = 0;
        dir = dir_mkdir(dir, cur);
        cur = slash + 1;
    }
    dir_addfile(dir, cur, buf, (uint32_t)n);
    printf("mkesp: + %s -> %s (%ld bytes)\n", src, dest, n);
    free(buf);
}

/* ---- format ---- */
static void format_fat32(void) {
    uint32_t clusters_est = (uint32_t)(part_sectors / SEC_PER_CLUS);
    fat_size = ((clusters_est * 4u) + SECTOR - 1) / SECTOR + 8;
    first_data_sec = RESERVED + NUM_FATS * fat_size;

    uint8_t *bs = psec(0);
    bs[0] = 0xEB; bs[1] = 0x58; bs[2] = 0x90;
    memcpy(bs + 3, "WUBUFW  ", 8);
    put16(bs + 11, SECTOR);
    bs[13] = SEC_PER_CLUS;
    put16(bs + 14, RESERVED);
    bs[16] = NUM_FATS;
    put16(bs + 17, 0);                     /* root entries: 0 on FAT32 */
    put16(bs + 19, 0);
    bs[21] = 0xF8;
    put16(bs + 22, 0);                     /* FATSz16 = 0 */
    put16(bs + 24, 32);                    /* sectors per track */
    put16(bs + 26, 8);                     /* heads */
    put32(bs + 28, (uint32_t)part_start);  /* hidden sectors */
    put32(bs + 32, (uint32_t)part_sectors);
    put32(bs + 36, fat_size);
    put16(bs + 40, 0);
    put16(bs + 42, 0);
    put32(bs + 44, root_cluster);
    put16(bs + 48, 1);                     /* FSInfo sector */
    put16(bs + 50, 6);                     /* backup boot sector */
    bs[64] = 0x80;
    bs[66] = 0x29;
    put32(bs + 67, 0x57554255);
    memcpy(bs + 71, "WUBU ESP   ", 11);
    memcpy(bs + 82, "FAT32   ", 8);
    put16(bs + 510, 0xAA55);

    memcpy(psec(6), bs, SECTOR);           /* backup boot sector */

    uint8_t *fsi = psec(1);
    put32(fsi + 0, 0x41615252);
    put32(fsi + 484, 0x61417272);
    put32(fsi + 488, 0xFFFFFFFF);
    put32(fsi + 492, 0xFFFFFFFF);
    put16(fsi + 510, 0xAA55);

    fat_set(0, 0x0FFFFFF8);
    fat_set(1, 0x0FFFFFFF);
    fat_set(root_cluster, 0x0FFFFFFF);
    memset(clus_ptr(root_cluster), 0, SEC_PER_CLUS * SECTOR);
}

/* ---- GPT ---- */
static void write_gpt(void) {
    /* Protective MBR */
    uint8_t *mbr = sec_ptr(0);
    mbr[446 + 0] = 0x00;
    mbr[446 + 4] = 0xEE;
    put32(mbr + 446 + 8, 1);
    uint64_t pm = disk_sectors - 1;
    put32(mbr + 446 + 12, (uint32_t)(pm > 0xFFFFFFFFull ? 0xFFFFFFFFull : pm));
    put16(mbr + 510, 0xAA55);

    static const uint8_t esp_type[16] = {
        0x28,0x73,0x2A,0xC1, 0x1F,0xF8, 0xD2,0x11, 0xBA,0x4B, 0x00,0xA0,0xC9,0x3E,0xC9,0x3B
    };
    static const uint8_t part_uuid[16] = {
        0x11,0x22,0x33,0x44,0x55,0x66,0x77,0x88,0x99,0xAA,0xBB,0xCC,0xDD,0xEE,0xFF,0x01
    };
    static const uint8_t disk_uuid[16] = {
        0xA1,0xB2,0xC3,0xD4,0xE5,0xF6,0x07,0x18,0x29,0x3A,0x4B,0x5C,0x6D,0x7E,0x8F,0x90
    };

    uint64_t pent_lba = 2;
    uint32_t nents = 128, entsz = 128;
    uint32_t pent_sectors = (nents * entsz) / SECTOR;

    uint8_t *ent = sec_ptr(pent_lba);
    memcpy(ent, esp_type, 16);
    memcpy(ent + 16, part_uuid, 16);
    put64(ent + 32, part_start);
    put64(ent + 40, part_start + part_sectors - 1);
    put64(ent + 48, 0);
    static const char label[] = "EFI System";
    for (int i = 0; label[i]; i++) put16(ent + 56 + i * 2, (uint16_t)label[i]);

    uint32_t pcrc = crc32_buf(ent, nents * entsz);

    uint8_t *hdr = sec_ptr(1);
    memcpy(hdr, "EFI PART", 8);
    put32(hdr + 8, 0x00010000);
    put32(hdr + 12, 92);
    put32(hdr + 16, 0);                    /* CRC placeholder */
    put32(hdr + 20, 0);
    put64(hdr + 24, 1);                    /* this header LBA   */
    put64(hdr + 32, disk_sectors - 1);     /* backup header LBA */
    put64(hdr + 40, pent_lba + pent_sectors);
    put64(hdr + 48, disk_sectors - 1 - pent_sectors - 1);
    memcpy(hdr + 56, disk_uuid, 16);
    put64(hdr + 72, pent_lba);
    put32(hdr + 80, nents);
    put32(hdr + 84, entsz);
    put32(hdr + 88, pcrc);
    put32(hdr + 16, crc32_buf(hdr, 92));

    /* Backup: entries then header at the last sector. */
    uint64_t bak_ent = disk_sectors - 1 - pent_sectors;
    memcpy(sec_ptr(bak_ent), ent, nents * entsz);
    uint8_t *bhdr = sec_ptr(disk_sectors - 1);
    memcpy(bhdr, hdr, 92);
    put32(bhdr + 16, 0);
    put64(bhdr + 24, disk_sectors - 1);
    put64(bhdr + 32, 1);
    put64(bhdr + 72, bak_ent);
    put32(bhdr + 16, crc32_buf(bhdr, 92));
}

int main(int argc, char **argv) {
    if (argc < 3 || (argc - 3) % 2 != 0) {
        fprintf(stderr, "usage: %s <out.img> <size_mb> [<src> <dest\\path>]...\n", argv[0]);
        return 2;
    }
    const char *out = argv[1];
    uint64_t mb = strtoull(argv[2], NULL, 10);
    if (mb < 8) mb = 8;

    disk_sectors = mb * 1024 * 1024 / SECTOR;
    disk = calloc(1, (size_t)(disk_sectors * SECTOR));
    if (!disk) { fprintf(stderr, "oom\n"); return 1; }

    part_start   = 2048;
    part_sectors = disk_sectors - part_start - 34;

    format_fat32();
    for (int i = 3; i + 1 < argc; i += 2) add_path(argv[i], argv[i + 1]);
    write_gpt();

    FILE *f = fopen(out, "wb");
    if (!f) { perror(out); return 1; }
    fwrite(disk, 1, (size_t)(disk_sectors * SECTOR), f);
    fclose(f);

    printf("mkesp: %s  %llu MB  ESP lba=%llu sectors=%llu  FATsz=%u  data@%u\n",
           out, (unsigned long long)mb, (unsigned long long)part_start,
           (unsigned long long)part_sectors, fat_size, first_data_sec);
    return 0;
}
