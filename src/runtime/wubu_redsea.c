/*
 * wubu_redsea.c -- RedSea: the TempleOS filesystem (ISO-9660 derived).
 *
 * See wubu_redsea.h.
 */
#include "wubu_redsea.h"

#include <string.h>

/* A little-endian u32. */
static uint32_t rd_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* A big-endian u32. */
static uint32_t rd_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

bool wubu_redsea_both_endian_u32(const uint8_t *p, uint32_t *out)
{
    if (!p || !out) return false;
    uint32_t le = rd_le32(p);
    uint32_t be = rd_be32(p + 4);
    /* The two halves are REQUIRED to agree.  A disagreement is a corrupt or
     * hostile volume, and picking one half would silently accept it. */
    if (le != be) return false;
    *out = le;
    return true;
}

const char *wubu_redsea_check_pvd(const uint8_t *sector, wubu_redsea_pvd_t *out)
{
    if (out) memset(out, 0, sizeof(*out));
    if (!sector) return "no sector";
    if (!out) return "no output";

    /* The magic sits at offset 1, not 0: byte 0 is the descriptor type. */
    if (memcmp(sector + WUBU_REDSEA_MAGIC_OFF, WUBU_REDSEA_MAGIC, 5) != 0)
        return "a bad magic (not a RedSea/ISO volume)";

    uint8_t type = sector[0];
    if (type != WUBU_REDSEA_DESC_PRIMARY &&
        type != WUBU_REDSEA_DESC_SUPPLEMENTARY &&
        type != WUBU_REDSEA_DESC_TERMINATOR)
        return "an unknown volume descriptor type";

    /* The volume size at offset 80 is a BOTH-ENDIAN field. */
    if (!wubu_redsea_both_endian_u32(sector + 80, &out->volume_space_size))
        return "the volume size's two halves disagree (a corrupt volume)";

    /* The logical block size at offset 128 is a both-endian UINT16 -- TWO bytes
     * little-endian then TWO big-endian, not four and four.  Reading it as a
     * u32 consumes the path table size as well and overlaps the two fields. */
    {
        uint16_t le = (uint16_t)sector[128] | ((uint16_t)sector[129] << 8);
        uint16_t be = (uint16_t)((uint16_t)sector[130] << 8) | (uint16_t)sector[131];
        if (le != be) return "the block size's two halves disagree";
        out->block_size = le;
    }
    /* The path table size at offset 132. */
    if (!wubu_redsea_both_endian_u32(sector + 132, &out->path_table_size))
        return "the path table size's two halves disagree";

    /* The root directory record is at offset 156 and is at least 34 bytes. */
    const uint8_t *root = sector + 156;
    if (root[0] < WUBU_REDSEA_REC_HEADER + 1)
        return "a truncated root directory record";
    if (!wubu_redsea_both_endian_u32(root + 2, &out->root_extent))
        return "the root extent's two halves disagree";
    if (!wubu_redsea_both_endian_u32(root + 10, &out->root_size))
        return "the root size's two halves disagree";

    out->type = type;
    memcpy(out->magic, sector + WUBU_REDSEA_MAGIC_OFF, 5);
    out->valid = true;
    return NULL;
}

const char *wubu_redsea_check_dirent(const uint8_t *rec, size_t rec_len,
                                     wubu_redsea_dirent_t *out)
{
    if (out) memset(out, 0, sizeof(*out));
    if (!rec) return "no record";
    if (!out) return "no output";

    /* The record length is its OWN length: the reader must not trust the
     * caller's buffer size. */
    uint8_t len = rec[0];
    if (len < WUBU_REDSEA_REC_HEADER)
        return "a directory record shorter than its fixed header";
    if ((size_t)len > rec_len)
        return "a directory record that runs past the sector";
    /* A record must not straddle a sector boundary. */
    if (len > WUBU_REDSEA_SECTOR)
        return "a directory record larger than a sector";

    uint8_t name_len = rec[32];
    if (name_len == 0) return "a directory record with a zero-length name";
    if ((size_t)(WUBU_REDSEA_REC_HEADER + name_len) > (size_t)len)
        return "a name that runs past the end of the record";

    if (!wubu_redsea_both_endian_u32(rec + 2, &out->extent))
        return "the extent's two halves disagree";
    if (!wubu_redsea_both_endian_u32(rec + 10, &out->size))
        return "the size's two halves disagree";

    out->flags = rec[25];
    out->is_dir = (out->flags & 0x02) != 0;

    /* "." and ".." are SINGLE BYTES 0x00 and 0x01, not the ASCII characters.
     * Reading them as text produces a file named "." in every listing. */
    const uint8_t *nm = rec + WUBU_REDSEA_REC_HEADER;
    if (name_len == 1 && nm[0] == 0x00) {
        out->is_self = true;
        out->name[0] = '.';
        out->name[1] = '\0';
        out->name_len = 1;
        return NULL;
    }
    if (name_len == 1 && nm[0] == 0x01) {
        out->is_parent = true;
        out->name[0] = '.';
        out->name[1] = '.';
        out->name[2] = '\0';
        out->name_len = 2;
        return NULL;
    }

    /* A name may not contain '/' -- that is a path separator, and a name that
     * embeds one would escape the directory it is listed in. */
    for (uint8_t i = 0; i < name_len; i++) {
        if (nm[i] == '/') return "a name containing '/'";
    }

    /* Strip the ";" VERSION SUFFIX: "FILE.;1" is named "FILE.". */
    uint8_t copy = name_len;
    for (uint8_t i = 0; i < name_len; i++) {
        if (nm[i] == ';') { copy = i; break; }
    }
    if (copy >= WUBU_REDSEA_NAME_MAX) return "a name that is too long";
    memcpy(out->name, nm, copy);
    out->name[copy] = '\0';
    out->name_len = copy;
    return NULL;
}

bool wubu_redsea_extent_fits(uint32_t extent, uint32_t size,
                             uint32_t volume_sectors)
{
    /* An extent at or beyond the volume is outside it, and a size that would
     * run past the end is a corrupt record.  Unsigned arithmetic makes the
     * overflow check necessary rather than decorative. */
    if (extent >= volume_sectors) return false;
    uint64_t start = (uint64_t)extent * WUBU_REDSEA_SECTOR;
    uint64_t end = start + size;
    return end <= (uint64_t)volume_sectors * WUBU_REDSEA_SECTOR;
}
