/* wubu_image_codec.c -- Self-contained image-codec leaf for WuBuOS canvas.
 *
 * Pure, dependency-free codec primitives: CRC32 (PNG), big-endian writers,
 * PNG chunk emission, and PNG adaptive-unfilter. Extracted from the monolithic
 * wubu_canvas_io.c so the canvas I/O glue no longer recompiles when the codec
 * changes and other modules can link these without the canvas surface.
 * C11, minimal includes, no god headers.
 */

#include "wubu_image_codec_internal.h"

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

/* CRC32 table + init guard (private to this codec module). */
static uint32_t crc32_table[256];
static bool crc32_table_init = false;

void crc32_init(void) {
    if (crc32_table_init) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int j = 0; j < 8; j++) {
            c = (c & 1) ? (0xEDB88320 ^ (c >> 1)) : (c >> 1);
        }
        crc32_table[i] = c;
    }
    crc32_table_init = true;
}

uint32_t crc32_update(uint32_t crc_in, const void *data, size_t len) {
    if (!crc32_table_init) crc32_init();
    const uint8_t *p = (const uint8_t*)data;
    /* crc_in is the PREVIOUS return value (already inverted); re-invert to get
       the running state, then continue the CRC, then re-invert on return.
       Returning ~0 here would restart the CRC instead of chaining it. */
    uint32_t crc = ~crc_in;
    for (size_t i = 0; i < len; i++) {
        crc = crc32_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

uint32_t crc32_data(const void *data, size_t len) {
    return crc32_update(0, data, len);
}

void write_be32(uint8_t *buf, uint32_t val) {
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = val & 0xFF;
}

void write_be16(uint8_t *buf, uint16_t val) {
    buf[0] = (val >> 8) & 0xFF;
    buf[1] = val & 0xFF;
}

void png_write_chunk(FILE *f, const char *type, const void *data, size_t len) {
    uint8_t len_buf[4];
    write_be32(len_buf, (uint32_t)len);
    fwrite(len_buf, 1, 4, f);
    fwrite(type, 1, 4, f);
    if (data && len > 0) fwrite(data, 1, len, f);
    uint32_t crc = crc32_data(type, 4);
    if (data && len > 0) crc = crc32_update(crc, data, len);
    static uint8_t crc_buf[4];
        write_be32(crc_buf, crc);
        fwrite(crc_buf, 1, 4, f);
    }

int png_unfilter(uint8_t *raw, int w, int h, int channels) {
    const int stride = w * channels;       /* pixels per row */
    const int pitch  = stride + 1;          /* +1 filter byte per row */
    uint8_t *prev = (uint8_t*)calloc(stride, 1);
    if (!prev) return -1;
    for (int y = 0; y < h; y++) {
        uint8_t *cur = raw + (size_t)y * pitch;
        uint8_t ftype = cur[0];
        uint8_t *line = cur + 1;            /* pixel data starts after filter byte */
        for (int x = 0; x < stride; x++) {
            int a = (x >= channels) ? line[x - channels] : 0;
            int b = prev[x];
            int c = (x >= channels) ? prev[x - channels] : 0;
            int v;
            switch (ftype) {
                case 0: v = line[x]; break;
                case 1: v = line[x] + a; break;
                case 2: v = line[x] + b; break;
                case 3: v = line[x] + ((a + b) >> 1); break;
                case 4: {
                    int p = a + b - c;
                    int pa = abs(p - a), pb = abs(p - b), pc = abs(p - c);
                    int pred = (pa <= pb && pa <= pc) ? a : (pb <= pc ? b : c);
                    v = line[x] + pred;
                    break;
                }
                default: free(prev); return -1;
            }
            line[x] = (uint8_t)(v & 0xFF);
        }
        memcpy(prev, line, stride);
    }
    free(prev);
    return 0;
}
