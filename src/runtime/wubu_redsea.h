/*
 * wubu_redsea.h -- RedSea: the TempleOS filesystem (ISO-9660 derived).
 *
 * RedSea is TempleOS's filesystem, derived from ISO-9660.  Its on-disk rules
 * are the ones that bite:
 *
 *   * A VOLUME DESCRIPTOR has a type (1 = primary, 2 = supplementary, 255 =
 *     terminator) and the magic "CD001" at offset 1.  A descriptor without the
 *     magic is not a RedSea/ISO volume.
 *   * Both-endian fields: ISO-9660 stores a 32-bit value as FOUR BYTES LITTLE
 *     ENDIAN followed by FOUR BYTES BIG ENDIAN.  Reading only one half is the
 *     single most common parser bug, and the two halves are REQUIRED to agree
 *     -- a disagreement means a corrupt or hostile volume.
 *   * A DIRECTORY RECORD has a fixed 33-byte header, then the name, then
 *     padding, then the system-use area.  Its length must be at least 33 and
 *     must not exceed the sector.
 *   * The NAME has an explicit length byte, may not contain '/', and carries a
 *     ";" VERSION SUFFIX ("FILE.;1") that is not part of the name.
 *   * "." and ".." are single-byte names (0x00 and 0x01), not the ASCII
 *     characters -- reading them as text produces a file named "." in every
 *     listing.
 *   * The extent and size must lie INSIDE the volume.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_REDSEA_H
#define WUBU_REDSEA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WUBU_REDSEA_SECTOR      2048u
#define WUBU_REDSEA_MAGIC       "CD001"
#define WUBU_REDSEA_MAGIC_OFF   1u
#define WUBU_REDSEA_DESC_SIZE   2048u
#define WUBU_REDSEA_REC_HEADER  33u
#define WUBU_REDSEA_NAME_MAX    256u

typedef enum {
    WUBU_REDSEA_DESC_BOOT = 0,
    WUBU_REDSEA_DESC_PRIMARY = 1,
    WUBU_REDSEA_DESC_SUPPLEMENTARY = 2,
    WUBU_REDSEA_DESC_PARTITION = 3,
    WUBU_REDSEA_DESC_TERMINATOR = 255
} wubu_redsea_desc_type_t;

typedef struct {
    uint8_t              type;
    char                 magic[5];
    uint32_t             volume_space_size;   /* in sectors */
    uint32_t             block_size;
    uint32_t             path_table_size;
    uint32_t             root_extent;         /* root directory's first sector */
    uint32_t             root_size;
    bool                 valid;
} wubu_redsea_pvd_t;

/* Validate a primary volume descriptor read from `sector`.  Returns NULL on
 * success, else a static reason. */
const char *wubu_redsea_check_pvd(const uint8_t *sector, wubu_redsea_pvd_t *out);

typedef struct {
    uint32_t extent;          /* first sector */
    uint32_t size;            /* in bytes */
    uint32_t flags;
    uint8_t  name_len;
    char     name[WUBU_REDSEA_NAME_MAX];
    bool     is_dir;
    bool     is_self;         /* the "." entry */
    bool     is_parent;       /* the ".." entry */
} wubu_redsea_dirent_t;

/* Parse one directory record.  Returns NULL on success, else a static reason. */
const char *wubu_redsea_check_dirent(const uint8_t *rec, size_t rec_len,
                                     wubu_redsea_dirent_t *out);

/* Read a both-endian u32 (4 LE then 4 BE).  Returns false when the two halves
 * DISAGREE, which is a corrupt or hostile volume. */
bool wubu_redsea_both_endian_u32(const uint8_t *p, uint32_t *out);

/* Is this extent/size pair inside a volume of `volume_sectors`? */
bool wubu_redsea_extent_fits(uint32_t extent, uint32_t size,
                             uint32_t volume_sectors);

#endif /* WUBU_REDSEA_H */
