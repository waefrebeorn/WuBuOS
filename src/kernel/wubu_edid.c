/*
 * wubu_edid.c -- see wubu_edid.h.  Pure base-block parsing, no I/O.
 *
 * Layout reference (EDID 1.x, 128 bytes):
 *   [0..7]    header 00 FF FF FF FF FF FF 00
 *   [8..9]    manufacturer ID, three 5-bit letters packed big-endian
 *   [10..11]  product code (LE)
 *   [12..15]  serial number (LE)
 *   [16]      manufacture week, [17] year-1990
 *   [18]      EDID version, [19] revision
 *   [21]      max horizontal image size in cm, [22] max vertical in cm
 *   [54..71]  detailed timing descriptor 1 (the preferred mode)
 *   [126]     extension count, [127] checksum
 */
#include "wubu_edid.h"

#include <stddef.h>
#include <string.h>

/* Three-letter manufacturer ID: five bits per letter, +64 gives A-Z. */
static void decode_manuf(uint16_t v, char out[4])
{
    out[0] = (char)(((v >> 10) & 0x1F) + 'A' - 1);
    out[1] = (char)(((v >>  5) & 0x1F) + 'A' - 1);
    out[2] = (char)(( v        & 0x1F) + 'A' - 1);
    out[3] = '\0';
}

/* A monitor-name descriptor lives in the four 18-byte descriptor slots at 54, 72,
 * 90 and 108.  Byte 0..2 must be 00 00 00, byte 3 == 0xFC for a name.  Text starts
 * at byte 5 and runs to a 0x0A terminator. */
static void find_name(const uint8_t *d, char out[16])
{
    out[0] = '\0';

    static const int slots[4] = { 54, 72, 90, 108 };

    /* Many panels put their MODEL STRING in an 0xFE ("unspecified text") descriptor
     * rather than the 0xFC ("monitor name") one -- this machine's eDP-1 does exactly
     * that, with "N156HRA-EA1".  Accepting only 0xFC would report no name on real
     * hardware.  0xFC wins when present; otherwise fall back to 0xFE, then 0xFF. */
    static const uint8_t types[3] = { 0xFC, 0xFE, 0xFF };

    for (int k = 0; k < 3 && out[0] == '\0'; k++) {
        for (int s = 0; s < 4; s++) {
            const uint8_t *p = d + slots[s];
            if (!(p[0] == 0x00 && p[1] == 0x00 && p[2] == 0x00)) continue;
            if (p[3] != types[k]) continue;

            int n = 0;
            for (int i = 5; i < 18 && n < 15; i++) {
                if (p[i] == 0x0A) break;          /* terminator, not part of the text */
                if (p[i] < 0x20 || p[i] > 0x7E) break;   /* junk: keep what we have */
                out[n++] = (char)p[i];
            }
            out[n] = '\0';
            break;
        }
    }
}

/* Preferred timing from detailed timing descriptor 1 (bytes 54..71).
 *
 * The packing matters and is easy to get wrong: active-low and blank-low each get a
 * full byte, while the HIGH bits of active and blank SHARE one byte each -- horizontal
 * pair at offset 4, vertical pair at offset 7.  Treating them as separate bytes (as a
 * synthetic reading of the spec does) yields plausible-looking nonsense; parsing this
 * machine's real panel caught it immediately. */
static void decode_timing(const uint8_t *d, int *pw, int *ph, int *refresh_mhz)
{
    const uint8_t *t = d + 54;

    uint32_t clock10k = (uint32_t)t[0] | ((uint32_t)t[1] << 8);
    if (clock10k == 0) { *pw = *ph = *refresh_mhz = 0; return; }

    int h_active = t[2] | (((unsigned)t[4] & 0xF0) << 4);
    int h_blank  = t[3] | (((unsigned)t[4] & 0x0F) << 8);
    int v_active = t[5] | (((unsigned)t[7] & 0xF0) << 4);
    int v_blank  = t[6] | (((unsigned)t[7] & 0x0F) << 8);

    *pw = h_active;
    *ph = v_active;

    long h_total = (long)h_active + h_blank;
    long v_total = (long)v_active + v_blank;
    if (h_total <= 0 || v_total <= 0) { *refresh_mhz = 0; return; }

    double hz = (double)(clock10k * 10000L) / ((double)h_total * (double)v_total);
    *refresh_mhz = (int)(hz + 0.5);          /* nearest Hz */
}

int wubu_edid_parse(const uint8_t *data, size_t len, wubu_edid_t *out)
{
    if (!out) return -1;
    memset(out, 0, sizeof(*out));              /* fail closed: zeroed even on refusal */

    if (!data || len < 128) return -1;

    static const uint8_t hdr[8] = { 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00 };
    if (memcmp(data, hdr, 8) != 0) return -1;

    /* Version 1 only: the base-block layout this parser reads was replaced by CEA-861
     * in 2.0, and guessing would produce plausible-looking nonsense. */
    if (data[18] != 1) return -1;

    /* Checksum over all 128 bytes must be 0 mod 256.  A corrupted blob must never be
     * reported as a real monitor. */
    unsigned sum = 0;
    for (int i = 0; i < 128; i++) sum += data[i];
    if ((sum & 0xFF) != 0) return -1;

    out->manuf[0] = '\0';
    decode_manuf((uint16_t)((data[8] << 8) | data[9]), out->manuf);

    out->product = (uint16_t)(data[10] | (data[11] << 8));
    out->serial  = (uint32_t)data[12] | ((uint32_t)data[13] << 8) |
                   ((uint32_t)data[14] << 16) | ((uint32_t)data[15] << 24);

    /* Week 0 means "not specified" in EDID 1.x; the byte is still raw here. */
    out->week = data[16];
    out->year = 1990 + data[17];

    /* Image size is in centimetres; 0 means unknown.  Convert rather than make the
     * caller remember the unit. */
    out->mm_w = data[21] ? data[21] * 10 : 0;
    out->mm_h = data[22] ? data[22] * 10 : 0;

    find_name(data, out->name);
    decode_timing(data, &out->pref_w, &out->pref_h, &out->pref_refresh_mhz);

    out->valid = true;
    return 0;
}
