/*
 * fat32.h  --  My Seed FAT32 Filesystem
 *
 * Clean C11 reimplementation of ZealOS FAT32 design.
 * Reference: ZealOS/src/Kernel/BlkDev/FileSysFAT.ZC
 *
 * Provides:
 *   - Boot sector parsing
 *   - FAT table chain walking
 *   - Cluster allocation / deallocation
 *   - Directory entry read/write (8.3 + LFN)
 *   - File create/open/read/write/close
 *
 * Block device I/O is abstracted via fat32_blk_ops  -- 
 * real kernel will use AHCI/IDE; hosted tests use a RAM disk.
 */

#ifndef MYSEED_FAT32_H
#define MYSEED_FAT32_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* -- Constants ---------------------------------------------------- */

#define FAT32_SECTOR_SIZE      512
#define FAT32_CLUSTER_EOC      0x0FFFFFF8   /* End-of-chain marker */
#define FAT32_CLUSTER_BAD      0x0FFFFFF7   /* Bad cluster */
#define FAT32_CLUSTER_FREE     0x00000000   /* Free cluster */
#define FAT32_ROOT_DIR_LBA     2            /* Root dir starts at cluster 2 */
#define FAT32_DIR_ENTRY_SIZE   32
#define FAT32_ENTRIES_PER_SEC  (FAT32_SECTOR_SIZE / FAT32_DIR_ENTRY_SIZE) /* 16 */
#define FAT32_LFN_CHARS       13           /* 5+6+2 chars per LFN entry */
#define FAT32_MAX_PATH        260
#define FAT32_MAX_NAME        255
#define FAT32_ATTR_READ_ONLY  0x01
#define FAT32_ATTR_HIDDEN     0x02
#define FAT32_ATTR_SYSTEM     0x04
#define FAT32_ATTR_VOLUME_ID  0x08
#define FAT32_ATTR_DIRECTORY  0x10
#define FAT32_ATTR_ARCHIVE    0x20
#define FAT32_ATTR_LFN        0x0F         /* Long name entry */

/* -- Block Device I/O Interface ----------------------------------- */

/*
 * Abstract block device  --  lets FAT32 work on real disk or RAM disk.
 * read:  read `n_sectors` from `lba` into `buf`
 * write: write `n_sectors` from `buf` to `lba`
 */
typedef struct {
    int  (*read)(void *ctx, uint64_t lba, uint32_t n_sectors, void *buf);
    int  (*write)(void *ctx, uint64_t lba, uint32_t n_sectors, const void *buf);
    void  *ctx;        /* Device context (e.g. RAM disk pointer, AHCI port) */
    uint64_t n_sectors; /* Total sectors on device */
} fat32_blk_ops;

/* -- FAT32 Boot Sector (first 512 bytes of volume) --------------- */

typedef struct __attribute__((packed)) {
    uint8_t  jump[3];             /* Boot jump + NOP */
    char     oem_name[8];         /* "MSWIN4.1" */
    uint16_t bytes_per_sector;    /* 512 */
    uint8_t  sectors_per_cluster; /* 1,2,4,8,16,32,64 */
    uint16_t reserved_sectors;    /* Usually 32 */
    uint8_t  num_fats;            /* 2 */
    uint16_t root_entry_count;    /* 0 for FAT32 */
    uint16_t total_sectors_16;    /* 0 for FAT32 */
    uint8_t  media_descriptor;    /* 0xF8 = hard disk */
    uint16_t sectors_per_fat_16;  /* 0 for FAT32 */
    uint16_t sectors_per_track;
    uint16_t num_heads;
    uint32_t hidden_sectors;
    uint32_t total_sectors_32;
    /* FAT32 extended */
    uint32_t sectors_per_fat;
    uint16_t fat_flags;
    uint16_t version;            /* 0 */
    uint32_t root_cluster;       /* Usually 2 */
    uint16_t fs_info_sector;     /* Usually 1 */
    uint16_t backup_boot_sector; /* Usually 6 */
    uint8_t  reserved[12];
    uint8_t  drive_number;       /* 0x80 */
    uint8_t  reserved1;
    uint8_t  boot_sig;           /* 0x29 */
    uint32_t volume_serial;
    char     volume_label[11];
    char     fs_type[8];         /* "FAT32   " */
    uint8_t  boot_code[420];
    uint16_t signature;          /* 0xAA55 */
} fat32_boot_sector;

/* -- Directory Entry (32 bytes) ----------------------------------- */

typedef struct __attribute__((packed)) {
    char     name[8];            /* 8.3 name (space-padded) */
    char     ext[3];             /* Extension (space-padded) */
    uint8_t  attr;               /* File attributes */
    uint8_t  nt_res;             /* NT reserved */
    uint8_t  create_time_tenth;  /* Create time, 10ms units */
    uint16_t create_time;
    uint16_t create_date;
    uint16_t access_date;
    uint16_t cluster_hi;        /* First cluster, high 16 bits */
    uint16_t write_time;
    uint16_t write_date;
    uint16_t cluster_lo;        /* First cluster, low 16 bits */
    uint32_t file_size;
} fat32_dir_entry;

/* -- Long Filename Entry (32 bytes) ------------------------------- */

typedef struct __attribute__((packed)) {
    uint8_t  ord;                /* Sequence number (0x41 = last) */
    uint16_t name1[5];          /* Characters 1-5 */
    uint8_t  attr;              /* FAT32_ATTR_LFN */
    uint8_t  type;              /* 0 */
    uint8_t  checksum;          /* Checksum of 8.3 name */
    uint16_t name2[6];          /* Characters 6-11 */
    uint16_t first_cluster;     /* 0 */
    uint16_t name3[2];          /* Characters 12-13 */
} fat32_lfn_entry;

/* -- File Info (our internal representation) ---------------------- */

typedef struct {
    char     name[FAT32_MAX_NAME + 1];  /* Null-terminated long name */
    uint32_t first_cluster;
    uint32_t file_size;
    uint8_t  attr;
    uint32_t dir_cluster;              /* Cluster containing this entry */
    uint32_t dir_offset;               /* Entry offset within cluster */
} fat32_file_info;

/* -- FAT32 Volume State ------------------------------------------
 * OPAQUE: the concrete layout lives in fat32_internal.h (self-contained
 * modules access it; public API consumers see only the forward decl). This
 * enforces the "no god headers / minimal includes" rule -- callers depend on
 * behavior, not on the volume's field layout. */
typedef struct fat32_volume fat32_volume;

/* -- Open File Handle -------------------------------------------- */

typedef struct {
    fat32_volume  *vol;
    uint32_t       first_cluster;
    uint32_t       current_cluster;
    uint32_t       file_size;
    uint8_t        attr;
    uint64_t       pos;            /* Current file position */
    bool           write_mode;     /* Open for writing */
    uint32_t       dir_cluster;    /* Cluster containing dir entry (O(1) close) */
    uint32_t       dir_offset;     /* Entry offset within cluster */
} fat32_file;

/* -- API: Volume Management -------------------------------------- */

/*
 * Mount a FAT32 volume using the given block device.
 * Reads boot sector, validates, initializes volume state.
 * Returns 0 on success, -1 on error.
 */
int  fat32_mount(fat32_volume *vol, const fat32_blk_ops *blk);

/*
 * Unmount  --  flush any cached data, release resources.
 */
void fat32_unmount(fat32_volume *vol);

/*
 * Format a block device as FAT32.
 * Creates boot sector, FAT tables, root directory.
 * sector_count: total sectors on device
 * vol_name: volume label (max 11 chars)
 * Returns 0 on success.
 */
int  fat32_format(const fat32_blk_ops *blk, uint64_t sector_count,
                  const char *vol_name);

/* -- API: Cluster Operations ------------------------------------- */

/*
 * Get next cluster in chain. Returns 0 on error or EOC.
 */
uint32_t fat32_next_cluster(fat32_volume *vol, uint32_t cluster);

/*
 * Allocate `count` contiguous clusters, starting from `hint` (0 = auto).
 * Returns first cluster, or 0 on failure.
 */
uint32_t fat32_alloc_clusters(fat32_volume *vol, uint32_t count, uint32_t hint);

/*
 * Free cluster chain starting at `cluster`.
 */
void fat32_free_chain(fat32_volume *vol, uint32_t cluster);

/*
 * Count free clusters on volume (caches result).
 */
uint32_t fat32_count_free(fat32_volume *vol);

/* -- API: Cluster ↔ LBA Conversion ------------------------------- */

/* Convert cluster number to LBA of its first sector. */
uint64_t fat32_cluster_to_lba(fat32_volume *vol, uint32_t cluster);

/* Convert LBA to cluster number. */
uint32_t fat32_lba_to_cluster(fat32_volume *vol, uint64_t lba);

/* -- API: Directory Operations ------------------------------------ */

/*
 * Open directory at given cluster (0 = root).
 * Calls callback for each entry. Returns number of entries found.
 * Callback returns true to continue, false to stop.
 */
typedef bool (*fat32_dir_callback)(const fat32_file_info *info, void *ctx);

int fat32_dir_read(fat32_volume *vol, uint32_t dir_cluster,
                   fat32_dir_callback cb, void *ctx);

/*
 * Find file in directory by name.
 * dir_cluster: cluster of parent dir (0 = root)
 * name: filename (case-insensitive)
 * Returns 0 if found, -1 if not found.
 */
int fat32_find(fat32_volume *vol, uint32_t dir_cluster,
               const char *name, fat32_file_info *out);

/*
 * Create file or directory in parent.
 * dir_cluster: cluster of parent dir (0 = root)
 * name: filename
 * attr: file attributes (FAT32_ATTR_DIRECTORY for dir)
 * Returns 0 on success.
 */
int fat32_create(fat32_volume *vol, uint32_t dir_cluster,
                 const char *name, uint8_t attr, fat32_file_info *out);

/*
 * Delete file or directory.
 * Returns 0 on success.
 */
int fat32_delete(fat32_volume *vol, uint32_t dir_cluster, const char *name);

/* -- API: File I/O ------------------------------------------------ */

/*
 * Open file. mode: "r", "w", "a", "rw".
 * Returns 0 on success.
 */
int  fat32_open(fat32_volume *vol, uint32_t dir_cluster,
                const char *name, const char *mode, fat32_file *fp);

/*
 * Close file. Flushes writes.
 */
void fat32_close(fat32_file *fp);

/*
 * Read `n` bytes from file at current position.
 * Returns bytes actually read.
 */
size_t fat32_read(fat32_file *fp, void *buf, size_t n);

/*
 * Write `n` bytes to file at current position.
 * Returns bytes actually written.
 */
size_t fat32_write(fat32_file *fp, const void *buf, size_t n);

/*
 * Seek to position. whence: SEEK_SET, SEEK_CUR, SEEK_END.
 * Returns new position, or -1 on error.
 */
int64_t fat32_seek(fat32_file *fp, int64_t offset, int whence);

/* -- API: Diagnostics --------------------------------------------- */

/*
 * Validate filesystem integrity. Returns 0 if OK.
 */
int fat32_validate(fat32_volume *vol);

/*
 * Get volume info string (for display).
 */
void fat32_info(fat32_volume *vol, char *buf, size_t buf_size);

#endif /* MYSEED_FAT32_H */
