/*
 * wubu_lfn.c  --  FAT32 VFAT Long File Name codec (self-contained)
 *
 * Gap A16. VFAT LFN layout per entry (32 bytes):
 *   [0]    sequence/ordinal: bit 6 = last entry, low 6 bits = index
 *   [1..10] chars 1-5  (UTF-16LE, 0x0000 ends, 0xFFFF pads)
 *   [11]   attribute = 0x0F
 *   [12]   type = 0 (reserved)
 *   [13]   checksum of the 8.3 name
 *   [14..25] chars 6-11 (UTF-16LE)
 *   [26..27] cluster = 0
 *   [28..31] chars 12-13 (UTF-16LE)
 * The chain is stored on disk in reverse order (the LAST chunk is the
 * FIRST entry, immediately before the 8.3 entry). Freestanding C11,
 * no heap, opaque-friendly.
 */
#include "wubu_lfn.h"
#include <string.h>

/* ---- UTF-16 helpers ------------------------------------------------ */

static void put_u16le(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)(v >> 8);
}

static uint16_t get_u16le(const uint8_t *p)
{
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

/* ---- codec --------------------------------------------------------- */

void wubu_lfn_encode_chunk(uint8_t entry[WUBU_LFN_ENTRY_SZ],
                           const uint16_t *chunk, int nchars,
                           int offset, int total_entries,
                           uint8_t checksum)
{
    if (!entry) return;
    memset(entry, 0, WUBU_LFN_ENTRY_SZ);

    uint8_t ordinal = (uint8_t)(offset + 1);
    if (offset == total_entries - 1) ordinal |= 0x40;  /* last entry */
    entry[0] = ordinal;
    entry[11] = WUBU_LFN_ATTR;
    entry[13] = checksum;

    int base[3] = { 1, 14, 28 };          /* char field bases */
    int nfield[3] = { 5, 6, 2 };
    int i = 0;                            /* flat char index */
    for (int f = 0; f < 3; f++) {
        for (int k = 0; k < nfield[f]; k++, i++) {
            if (i < nchars)
                put_u16le(entry + base[f] + 2 * k, chunk[i]);
            else if (i == nchars)
                put_u16le(entry + base[f] + 2 * k, 0x0000); /* terminator */
            else
                put_u16le(entry + base[f] + 2 * k, 0xFFFF); /* padding */
        }
    }
}

int wubu_lfn_decode_chunk(const uint8_t entry[WUBU_LFN_ENTRY_SZ],
                          uint16_t out[WUBU_LFN_CHARS_PER_ENTRY])
{
    if (!entry || entry[11] != WUBU_LFN_ATTR) return -1;
    if (!out) return -1;

    int base[3] = { 1, 14, 28 };
    int nfield[3] = { 5, 6, 2 };
    int n = 0;
    for (int f = 0; f < 3; f++) {
        for (int k = 0; k < nfield[f]; k++) {
            uint16_t c = get_u16le(entry + base[f] + 2 * k);
            if (c == 0x0000 || c == 0xFFFF) return n;  /* terminator/pad */
            if (n < WUBU_LFN_CHARS_PER_ENTRY) out[n++] = c;
        }
    }
    return n;
}

/* ---- chain build / reconstruct ------------------------------------- */

int wubu_lfn_build_entries(const char *name,
                           uint8_t entries[][WUBU_LFN_ENTRY_SZ],
                           int max_entries)
{
    if (!name || !entries || max_entries <= 0) return 0;

    size_t len = strlen(name);
    /* an 8.3-compatible name needs no LFN entries (the dir layer keeps
     * its plain 8.3 path); anything longer does */
    if (len == 0 || len > WUBU_LFN_MAX_NAME) return 0;

    /* decide: does this need LFN? names with lowercase or > 8.3 */
    int needs = (len > 12);   /* "12345678.123" is the longest 8.3 */
    if (!needs) {
        /* check for chars that don't survive the 8.3 mapping */
        for (size_t i = 0; i < len; i++) {
            char c = name[i];
            if (c >= 'a' && c <= 'z') { needs = 1; break; }
        }
    }
    if (!needs) return 0;

    /* build the UTF-16 code unit array */
    uint16_t units[WUBU_LFN_MAX_NAME];
    int nunits = 0;
    for (size_t i = 0; i < len && nunits < WUBU_LFN_MAX_NAME; i++)
        units[nunits++] = (uint16_t)(unsigned char)name[i];  /* ASCII->UTF16 */

    int total = (nunits + WUBU_LFN_CHARS_PER_ENTRY - 1) /
                WUBU_LFN_CHARS_PER_ENTRY;
    if (total > max_entries) total = max_entries;

    /* the 8.3 checksum for the chain (caller keeps the 8.3 in the
     * short entry; here we checksum the 8.3 built from the name) */
    char n83[11];
    memset(n83, ' ', 11);
    size_t i = 0;
    int w = 0;
    while (i < len && name[i] != '.' && w < 8) {
        char c = name[i++];
        if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
        n83[w++] = c;
    }
    if (i < len && name[i] == '.') {
        i++;
        w = 8;
        while (i < len && w < 11) {
            char c = name[i++];
            if (c >= 'a' && c <= 'z') c = (char)(c - 'a' + 'A');
            n83[w++] = c;
        }
    }
    uint8_t csum = wubu_lfn_checksum(n83);

    /* on disk the LAST chunk is the FIRST entry; we fill the array so
     * entry[0] is the on-disk-first (== the last chunk) */
    for (int e = 0; e < total; e++) {
        int chunk_index = total - 1 - e;      /* reverse order */
        int start = chunk_index * WUBU_LFN_CHARS_PER_ENTRY;
        int nchunk = nunits - start;
        if (nchunk > WUBU_LFN_CHARS_PER_ENTRY) nchunk = WUBU_LFN_CHARS_PER_ENTRY;
        if (nchunk < 0) nchunk = 0;
        wubu_lfn_encode_chunk(entries[e], units + start, nchunk,
                              chunk_index, total, csum);
    }
    return total;
}

int wubu_lfn_reconstruct(const uint8_t entries[][WUBU_LFN_ENTRY_SZ],
                         int n_entries, char *out, size_t outsz)
{
    if (!entries || !out || outsz == 0 || n_entries <= 0) return -1;

    /* the on-disk chain is reversed: entry[0] is the last chunk, so the
     * name is built by walking backwards */
    uint16_t units[WUBU_LFN_MAX_NAME];
    int nunits = 0;
    for (int e = n_entries - 1; e >= 0 && nunits < WUBU_LFN_MAX_NAME; e--) {
        uint16_t chunk[WUBU_LFN_CHARS_PER_ENTRY];
        int n = wubu_lfn_decode_chunk(entries[e], chunk);
        if (n < 0) return -1;
        for (int i = 0; i < n && nunits < WUBU_LFN_MAX_NAME; i++)
            units[nunits++] = chunk[i];
    }
    size_t k = 0;
    for (int i = 0; i < nunits && k + 1 < outsz; i++) {
        if (units[i] > 0xFF) units[i] = '?';   /* non-ASCII: placeholder */
        out[k++] = (char)units[i];
    }
    out[k] = '\0';
    return 0;
}

uint8_t wubu_lfn_checksum(const char name83[11])
{
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++) {
        sum = (uint8_t)(((sum & 1) ? 0x80 : 0) + (sum >> 1) +
                        (uint8_t)name83[i]);
    }
    return sum;
}
