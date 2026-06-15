/*
 * iso9660.h  --  WuBuOS ISO 9660 Filesystem Structures
 *
 * Cell 060: ISO 9660 filesystem builder for creating bootable ISOs.
 * Implements the core ECMA-119 / ISO 9660 structures needed to
 * create a valid bootable ISO image:
 *   - Primary Volume Descriptor
 *   - Boot Record Volume Descriptor (El Torito)
 *   - Volume Descriptor Set Terminator
 *   - Directory records
 *   - Path Table (L+M)
 *
 * All C11, no external deps.
 */

#ifndef WUBU_ISO9660_H
#define WUBU_ISO9660_H

#include <stdint.h>
#include <stddef.h>

/* -- ISO 9660 Constants -------------------------------------- */

#define ISO_SECTOR_SIZE     2048
#define ISO_MAX_SECTORS     500000   /* ~1GB max ISO */
#define ISO_VOL_ID_LEN     32
#define ISO_PUB_ID_LEN     128
#define ISO_DATE_LEN       17       /* ASCII date/time */
#define ISO_DIR_ID_LEN     31       /* Max directory identifier */
#define ISO_MAX_FILES      4096
#define ISO_MAX_DEPTH      8

/* Volume Descriptor types */
#define ISO_VD_BOOT        0
#define ISO_VD_PRIMARY     1
#define ISO_VD_SUPPLEMENT  2
#define ISO_VD_TERMINATOR  255

/* Directory record flags */
#define ISO_FLAG_HIDDEN    0x01
#define ISO_FLAG_DIR       0x02
#define ISO_FLAG_ASSOC     0x04
#define ISO_FLAG_RECORD    0x08
#define ISO_FLAG_PROTECT   0x10
#define ISO_FLAG_MULTIEXT  0x80

/* El Torito Boot Catalog signature */
#define EL_TORITO_SIG      0xAA55

/* -- ISO 9660 Structures ------------------------------------- */

#pragma pack(push, 1)

/* Both-endian 16-bit value */
typedef struct { uint16_t le; uint16_t be; } iso_both16_t;
/* Both-endian 32-bit value */
typedef struct { uint32_t le; uint32_t be; } iso_both32_t;

/* Dec-binary date/time (17 bytes ASCII) */
typedef struct {
    char year[4];     /* "2026" */
    char month[2];    /* "06" */
    char day[2];      /* "07" */
    char hour[2];     /* "12" */
    char minute[2];   /* "00" */
    char second[2];   /* "00" */
    char hundredth[2];/* "00" */
    char timezone;    /* 0 = GMT */
} iso_date_t;

/* Primary Volume Descriptor (sector 16) */
typedef struct {
    uint8_t  type;                        /* 1 = Primary */
    char     id[5];                       /* "CD001" */
    uint8_t  version;                     /* 1 */
    uint8_t  unused1;
    char     system_id[32];               /* System identifier */
    char     volume_id[32];               /* Volume identifier */
    uint8_t  unused2[8];
    iso_both32_t volume_space_size;       /* Total sectors */
    uint8_t  unused3[32];
    iso_both16_t volume_set_size;         /* 1 */
    iso_both16_t volume_seq_number;      /* 1 */
    iso_both16_t logical_block_size;      /* 2048 */
    iso_both32_t path_table_size;         /* Path table bytes */
    uint32_t l_path_table;               /* L-path table sector */
    uint32_t l_path_table_opt;           /* Optional L-path table */
    uint32_t m_path_table;               /* M-path table sector */
    uint32_t m_path_table_opt;           /* Optional M-path table */
    uint8_t  root_dir_record[34];        /* Root directory record */
    char     volume_set_id[128];
    char     publisher_id[128];
    char     data_preparer_id[128];
    char     application_id[128];
    char     copyright_file_id[37];
    char     abstract_file_id[37];
    char     bibliographic_file_id[37];
    iso_date_t creation_date;
    iso_date_t modification_date;
    iso_date_t expiration_date;
    iso_date_t effective_date;
    uint8_t  file_structure_version;      /* 1 */
    uint8_t  unused4;
    uint8_t  application_use[512];
    uint8_t  unused5[653];
} iso_primary_vd_t;

/* Boot Record Volume Descriptor (sector 17 for El Torito) */
typedef struct {
    uint8_t  type;                        /* 0 = Boot Record */
    char     id[5];                       /* "CD001" */
    uint8_t  version;                     /* 1 */
    uint8_t  unused1;
    char     boot_system_id[32];          /* "EL TORITO SPECIFICATION" */
    char     boot_id[32];                 /* Unused */
    uint8_t  unused2[40];
    uint32_t boot_catalog_lba;            /* LBA of boot catalog */
    uint8_t  unused3[1973];
} iso_boot_vd_t;

/* Volume Descriptor Set Terminator (sector 18+) */
typedef struct {
    uint8_t  type;                        /* 255 = Terminator */
    char     id[5];                       /* "CD001" */
    uint8_t  version;                     /* 1 */
    uint8_t  unused[2041];
} iso_terminator_vd_t;

/* Directory Record (variable length, min 33) */
typedef struct {
    uint8_t  length;                      /* Record length */
    uint8_t  ext_attr_length;             /* Extended attribute record length */
    iso_both32_t extent;                  /* Location of extent (LBA) */
    iso_both32_t data_length;             /* Data length */
    iso_date_t recording_date;            /* Recording date and time */
    uint8_t  flags;                       /* File flags */
    uint8_t  file_unit_size;              /* File unit size (interleave) */
    uint8_t  interleave_gap;              /* Interleave gap size */
    iso_both16_t volume_seq;             /* Volume sequence number */
    uint8_t  id_len;                     /* Identifier length */
    /* Identifier follows (id_len bytes), then padding */
} iso_dir_record_t;

/* El Torito Validation Entry */
typedef struct {
    uint8_t  header_id;                   /* 0x01 */
    char     platform_id;                 /* 0 = x86 */
    uint16_t reserved1;
    char     id_string[24];               /* "WuBuOS Boot" */
    uint16_t checksum;
    uint16_t signature;                   /* 0xAA55 */
} el_torito_validation_t;

/* El Torito Initial/Default Entry */
typedef struct {
    uint8_t  boot_indicator;              /* 0x88 = bootable */
    char     boot_media_type;             /* 0 = no emulation */
    uint16_t load_segment;                /* 0x0000 for flat */
    uint8_t  system_type;
    uint8_t  unused1;
    uint16_t sector_count;                /* Sectors to load */
    uint32_t load_rba;                    /* LBA of boot image */
    uint8_t  unused2[20];
} el_torito_entry_t;

/* El Torito Boot Catalog (validation + entry) */
typedef struct {
    el_torito_validation_t validation;
    el_torito_entry_t      entry;
    uint8_t                padding[ISO_SECTOR_SIZE - sizeof(el_torito_validation_t) - sizeof(el_torito_entry_t)];
} el_torito_catalog_t;

#pragma pack(pop)

/* -- ISO Builder API ----------------------------------------- */

/* File entry for ISO */
typedef struct {
    char     name[ISO_DIR_ID_LEN + 1];   /* Filename (8.3 or LFN) */
    uint32_t lba;                         /* LBA of file data */
    uint32_t size;                        /* File size in bytes */
    uint8_t  flags;                       /* File flags (ISO_FLAG_DIR, etc.) */
    uint8_t  is_boot;                     /* 1 = this is the boot image */
} iso_file_entry_t;

/* ISO builder state */
typedef struct {
    uint8_t         *image;               /* ISO image buffer */
    uint32_t         image_size;          /* Total image size in bytes */
    uint32_t        total_sectors;        /* Total sectors */

    /* Volume info */
    char            volume_id[ISO_VOL_ID_LEN + 1];

    /* Files */
    iso_file_entry_t files[ISO_MAX_FILES];
    int              num_files;

    /* Boot info */
    uint32_t        boot_catalog_lba;     /* LBA of boot catalog */
    uint32_t        boot_image_lba;       /* LBA of boot image */
    uint32_t        boot_image_size;      /* Boot image size */
    uint8_t        *boot_image_data;      /* Boot image data */
    int              has_boot;            /* 1 = bootable */

    /* Layout (sector numbers) */
    uint32_t        primary_vd_lba;       /* 16 */
    uint32_t        boot_vd_lba;          /* 17 (if bootable) */
    uint32_t        terminator_vd_lba;    /* 18 or 17 */
    uint32_t        l_path_table_lba;     /* L-path table */
    uint32_t        m_path_table_lba;     /* M-path table */
    uint32_t        root_dir_lba;         /* Root directory */
    uint32_t        next_data_lba;        /* Next available data sector */

    /* Stats */
    uint64_t        bytes_written;
} iso_builder_t;

/* -- Builder Lifecycle --------------------------------------- */

/* Initialize ISO builder. reserves ISO_RESERVE_SECTORS (16) + VDs. */
int  iso_builder_init(iso_builder_t *b, const char *volume_id);

/* Shutdown and free resources */
void iso_builder_shutdown(iso_builder_t *b);

/* -- Boot Configuration -------------------------------------- */

/* Set boot image data. This makes the ISO El Torito bootable. */
int  iso_builder_set_boot(iso_builder_t *b, const uint8_t *boot_data,
                          uint32_t boot_size);

/* -- File Management ----------------------------------------- */

/* Add a file to the ISO */
int  iso_builder_add_file(iso_builder_t *b, const char *name,
                           const uint8_t *data, uint32_t size);

/* Add a directory */
int  iso_builder_add_dir(iso_builder_t *b, const char *name);

/* -- ISO Assembly -------------------------------------------- */

/* Build the complete ISO image. Writes all volume descriptors,
 * path tables, directory records, and file data.
 * Returns total image size, or 0 on error. */
uint32_t iso_builder_build(iso_builder_t *b);

/* -- Output -------------------------------------------------- */

/* Write ISO image to a file */
int  iso_builder_write_file(iso_builder_t *b, const char *path);

/* Get pointer to the ISO image data */
const uint8_t *iso_builder_image(const iso_builder_t *b);

/* Get image size */
uint32_t iso_builder_image_size(const iso_builder_t *b);

/* -- Validation ---------------------------------------------- */

/* Check if the built ISO has valid primary volume descriptor */
int  iso_builder_validate(const iso_builder_t *b);

/* Check for El Torito boot signature */
int  iso_builder_is_bootable(const iso_builder_t *b);

#endif /* WUBU_ISO9660_H */
