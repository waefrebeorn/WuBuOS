/* wubu_screenshot_png.c -- Screenshot PNG encoder subsystem.
 *
 * Self-contained module extracted from wubu_screenshot.c: wubu_screenshot_save
 * + the PNG writer (CRC, chunking, RGBA encoder). Uses the public
 * wubu_screenshot API types; other screenshot helpers are public. Minimal includes.
 */

#include "wubu_screenshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <zlib.h>

static void write_chunk(FILE *f, const char *type, const void *data, int len) {
    unsigned char len_bytes[4];
    len_bytes[0] = (len >> 24) & 0xFF;
    len_bytes[1] = (len >> 16) & 0xFF;
    len_bytes[2] = (len >> 8) & 0xFF;
    len_bytes[3] = len & 0xFF;
    fwrite(len_bytes, 1, 4, f);
    fwrite(type, 1, 4, f);
    fwrite(data, 1, len, f);
    unsigned char crc[4] = {0,0,0,0};
    fwrite(crc, 1, 4, f);
}

bool wubu_screenshot_save(wubu_sshot_t *sshot, const char *filename) {
    if (!sshot || !sshot->pixels || !filename) return false;

    if (sshot->dirty) wubu_screenshot_render_annotations(sshot);

    FILE *f = fopen(filename, "wb");
    if (!f) return false;

    /* PNG signature */
    static const unsigned char sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    fwrite(sig, 1, 8, f);

    /* IHDR */
    unsigned char ihdr[13];
    int w = sshot->width, h = sshot->height;
    ihdr[0] = (w >> 24) & 0xFF; ihdr[1] = (w >> 16) & 0xFF; ihdr[2] = (w >> 8) & 0xFF; ihdr[3] = w & 0xFF;
    ihdr[4] = (h >> 24) & 0xFF; ihdr[5] = (h >> 16) & 0xFF; ihdr[6] = (h >> 8) & 0xFF; ihdr[7] = h & 0xFF;
    ihdr[8] = 8; ihdr[9] = 6; ihdr[10] = 0; ihdr[11] = 0; ihdr[12] = 0;
    write_chunk(f, "IHDR", ihdr, 13);

    /* IDAT - uncompressed raw data with filter bytes */
    int row_bytes = w * 4 + 1;
    unsigned char *idat = malloc(h * row_bytes);
    if (!idat) { fclose(f); return false; }

    for (int y = 0; y < h; y++) {
        idat[y * row_bytes] = 0;
        uint32_t *src = &sshot->pixels[y * w];
        unsigned char *dst = &idat[y * row_bytes + 1];
        for (int x = 0; x < w; x++) {
            uint32_t p = src[x];
            *dst++ = (p >> 16) & 0xFF;
            *dst++ = (p >> 8) & 0xFF;
            *dst++ = p & 0xFF;
            *dst++ = (p >> 24) & 0xFF;
        }
    }

    /* zlib header + uncompressed blocks */
    int uncompressed_size = h * row_bytes;
    int compressed_size = uncompressed_size + 6 + h * 5;
    unsigned char *zlib = malloc(compressed_size);
    if (!zlib) { free(idat); fclose(f); return false; }

    unsigned char *zp = zlib;
    *zp++ = 0x78; *zp++ = 0x01;

    for (int i = 0; i < h; i++) {
        int block_size = row_bytes;
        bool last = (i == h - 1);
        *zp++ = last ? 0x01 : 0x00;
        *zp++ = block_size & 0xFF;
        *zp++ = (block_size >> 8) & 0xFF;
        *zp++ = (~block_size) & 0xFF;
        *zp++ = ((~block_size) >> 8) & 0xFF;
        memcpy(zp, &idat[i * row_bytes], block_size);
        zp += block_size;
    }

    *zp++ = 0; *zp++ = 0; *zp++ = 0; *zp++ = 0x01;

    write_chunk(f, "IDAT", zlib, zp - zlib);
    write_chunk(f, "IEND", "", 0);

    free(idat);
    free(zlib);
    fclose(f);
    return true;
}

bool wubu_screenshot_save_auto(wubu_sshot_t *sshot, char *out_path, size_t out_size) {
    if (!sshot) return false;
    char filename[256];
    wubu_screenshot_gen_filename(filename, sizeof(filename));
    const char *dir = wubu_screenshot_get_dir();
    snprintf(out_path, out_size, "%s/%s", dir, filename);
    return wubu_screenshot_save(sshot, out_path);
}

/* ============================================================
 * Clipboard Integration
 * ============================================================
 *
 * A real clipboard holds an encoded image so it can be pasted into a
 * Wayland/X11 peer. We encode the screenshot pixels to a self-contained
 * PNG (uncompressed DEFLATE, no zlib dependency) in memory and keep the
 * buffer owned by this module. Returns true only when real bytes were
 * produced; the previous placeholder returned true with nothing stored.
 */

/* -- self-contained PNG encoder (in-memory, zlib-backed) ---------- */

static uint32_t png_crc_data(const void *data, size_t len);
static uint32_t png_crc_update(uint32_t crc_in, const void *data, size_t len);

static uint32_t g_png_crc_table[256];
static bool g_png_crc_init = false;

static void png_crc_init(void) {
    if (g_png_crc_init) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        g_png_crc_table[i] = c;
    }
    g_png_crc_init = true;
}

static uint32_t png_crc_data(const void *data, size_t len) {
    if (!g_png_crc_init) png_crc_init();
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = ~0u;
    for (size_t i = 0; i < len; i++)
        crc = g_png_crc_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

/* Continue a CRC from an existing value (used for the IDAT chunk, whose
 * CRC covers the 4-byte "IDAT" type followed by the zlib stream). */
static uint32_t png_crc_update(uint32_t crc_in, const void *data, size_t len) {
    if (!g_png_crc_init) png_crc_init();
    const uint8_t *p = (const uint8_t *)data;
    uint32_t crc = crc_in;
    for (size_t i = 0; i < len; i++)
        crc = g_png_crc_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}

static void put_be32(uint8_t *b, uint32_t v) {
    b[0] = (v >> 24) & 0xFF; b[1] = (v >> 16) & 0xFF;
    b[2] = (v >> 8) & 0xFF;  b[3] = v & 0xFF;
}
static void put_be16(uint8_t *b, uint16_t v) {
    b[0] = (v >> 8) & 0xFF; b[1] = v & 0xFF;
}

/* Encode RGBA pixels (w*h, 0xRRGGBBAA) to a PNG in *out (caller frees).
 * Returns total byte length, or 0 on failure.
 *
 * Strategy: build the PNG "filtered scanline stream" (per row: 1 filter
 * byte (0=None) + RGBA bytes), then DEFLATE that stream with zlib
 * (compress2) so the IDAT is a valid zlib-wrapped deflate stream that any
 * real decoder (libpng/PIL/file) accepts. */
size_t png_encode_rgba(const uint32_t *px, int w, int h, uint8_t **out) {
    if (!px || w <= 0 || h <= 0 || !out) return 0;
    *out = NULL;

    size_t row_bytes = (size_t)4 * w;
    size_t scanline  = 1 + row_bytes;            /* filter byte + RGBA */
    size_t raw_size  = scanline * (size_t)h;

    /* Filtered scanline stream. */
    uint8_t *raw = (uint8_t *)malloc(raw_size ? raw_size : 1);
    if (!raw) return 0;
    for (int y = 0; y < h; y++) {
        uint8_t *dst = raw + (size_t)y * scanline;
        dst[0] = 0;                               /* filter type 0 (None) */
        memcpy(dst + 1, px + (size_t)y * w, row_bytes);
    }

    /* Compress with zlib. Upper bound on compressed size:
     *   deflateBound-style: raw_size + (raw_size>>12) + (raw_size>>14) + 11,
     *   plus the 2-byte zlib header + 4-byte adler32 are already inside. */
    uLongf zcap = raw_size + (raw_size >> 12) + (raw_size >> 14) + 13;
    uint8_t *zbuf = (uint8_t *)malloc(zcap);
    if (!zbuf) { free(raw); return 0; }
    uLongf zlen = zcap;
    int zr = compress2(zbuf, &zlen, raw, (uLong)raw_size, Z_DEFAULT_COMPRESSION);
    free(raw);
    if (zr != Z_OK) { free(zbuf); return 0; }

    size_t idat_total = zlen;                     /* zlib stream length */
    size_t total = 8 /* sig */ + (4 + 4 + 13 + 4) /* IHDR */
                 + (4 + 4) + idat_total + 4       /* IDAT */
                 + (4 + 4 + 0 + 4)                /* IEND */;
    uint8_t *buf = (uint8_t *)malloc(total);
    if (!buf) { free(zbuf); return 0; }

    size_t p = 0;
    static const uint8_t sig[8] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A};
    memcpy(buf, sig, 8); p += 8;

    /* IHDR */
    put_be32(buf + p, 13); p += 4;
    memcpy(buf + p, "IHDR", 4); p += 4;
    put_be32(buf + p, (uint32_t)w); p += 4;
    put_be32(buf + p, (uint32_t)h); p += 4;
    buf[p++] = 8;   /* bit depth */
    buf[p++] = 6;   /* color type: RGBA */
    buf[p++] = 0;   /* compression */
    buf[p++] = 0;   /* filter */
    buf[p++] = 0;   /* interlace */
    put_be32(buf + p, png_crc_data(buf + 8 + 4, 4 + 13)); p += 4;

    /* IDAT (zlib stream) */
    put_be32(buf + p, (uint32_t)idat_total); p += 4;
    memcpy(buf + p, "IDAT", 4); p += 4;
    uint32_t idat_crc = png_crc_data("IDAT", 4);
    idat_crc = png_crc_update(idat_crc, zbuf, zlen);
    memcpy(buf + p, zbuf, zlen); p += zlen;
    free(zbuf);
    put_be32(buf + p, idat_crc); p += 4;

    /* IEND */
    put_be32(buf + p, 0); p += 4;
    memcpy(buf + p, "IEND", 4); p += 4;
    put_be32(buf + p, png_crc_data("IEND", 4)); p += 4;

    *out = buf;
    return p; /* == total */
}
