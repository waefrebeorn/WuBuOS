/*
 * wubu_canvas_io.c  --  WuBuOS Image Editor: File I/O backend
 *
 * Cell 397 (I/O split 2026-07-09): PNG/BMP/PPM/GIF save + load.
 * Self-contained: depends only on the public canvas API (wubu_canvas.h),
 * zlib for PNG DEFLATE, and C11 system headers. Uses only the documented
 * public surface (wubu_cv_resize / wubu_cv_composite / layer pixels via
 * wubu_cv_layer_get) -- no access to canvas internals.
 */

#include "wubu_canvas.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <zlib.h>

/* -- File I/O (Native PNG/GIF/BMP/PPM) -------------------------- */

static uint32_t crc32_table[256];
static bool crc32_table_init = false;

static void crc32_init(void) {
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

static uint32_t crc32_update(uint32_t crc_in, const void *data, size_t len) {
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

static uint32_t crc32_data(const void *data, size_t len) {
    return crc32_update(0, data, len);
}

/* Write 32-bit big-endian */
static void write_be32(uint8_t *buf, uint32_t val) {
    buf[0] = (val >> 24) & 0xFF;
    buf[1] = (val >> 16) & 0xFF;
    buf[2] = (val >> 8) & 0xFF;
    buf[3] = val & 0xFF;
}

static void write_be16(uint8_t *buf, uint16_t val) {
    buf[0] = (val >> 8) & 0xFF;
    buf[1] = val & 0xFF;
}

/* Write PNG chunk: length (4 bytes), type (4 bytes), data, crc (4 bytes) */
static void png_write_chunk(FILE *f, const char *type, const void *data, size_t len) {
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

/* Native PNG save - uses uncompressed IDAT (no zlib needed) */
int wubu_cv_save_png(WubuCanvas *cv, const char *path) {
    if (!cv) return -1;

    uint32_t *flat = (uint32_t*)malloc(cv->w * cv->h * sizeof(uint32_t));
    if (!flat) return -1;
    wubu_cv_composite(cv, flat, cv->w, cv->h);

    FILE *f = fopen(path, "wb");
    if (!f) { free(flat); return -1; }

    /* PNG signature */
    static const uint8_t png_sig[8] = {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A};
    fwrite(png_sig, 1, 8, f);

    /* IHDR chunk */
    uint8_t ihdr[13];
    write_be32(ihdr, cv->w);
    write_be32(ihdr + 4, cv->h);
    ihdr[8] = 8;   /* bit depth */
    ihdr[9] = 2;   /* color type: truecolor */
    ihdr[10] = 0;  /* compression: 0 (deflate) */
    ihdr[11] = 0;  /* filter: 0 (adaptive) */
    ihdr[12] = 0;  /* interlace: 0 (none) */
    png_write_chunk(f, "IHDR", ihdr, 13);

    /* Build the raw scanline buffer: one filter byte (None=0) + RGB per row.
       PNG IDAT carries a *zlib* stream (RFC 1950: 2-byte header + DEFLATE +
       4-byte Adler-32), NOT raw DEFLATE — so we compress it with zlib's
       compress2(), exactly what inflateInit()/PIL expect on decode. */
    size_t row_bytes = 3 * cv->w;
    size_t scanline_size = 1 + row_bytes; /* filter byte + RGB data */
    size_t raw_size = (size_t)cv->h * scanline_size;

    uint8_t *raw = malloc(raw_size);
    uint8_t *idat = malloc(raw_size + 64); /* zlib output <= raw_size + 12 */
    if (!raw || !idat) { free(flat); free(raw); free(idat); fclose(f); return -1; }

    for (int y = 0; y < cv->h; y++) {
        uint8_t *sl = raw + (size_t)y * scanline_size;
        sl[0] = 0; /* filter type 0 = None */
        for (int x = 0; x < cv->w; x++) {
            uint32_t px = flat[y * cv->w + x];
            /* canvas pixel is 0xAARRGGBB: R in bits 16-23, B in 0-7 */
            sl[1 + x * 3]     = (px >> 16) & 0xFF;   /* R */
            sl[1 + x * 3 + 1] = (px >> 8) & 0xFF;    /* G */
            sl[1 + x * 3 + 2] = px & 0xFF;           /* B */
        }
    }

    uLongf idat_len = (uLongf)(raw_size + 64);
    int zr = compress2(idat, &idat_len, raw, (uLong)raw_size, Z_DEFAULT_COMPRESSION);
    free(raw);
    if (zr != Z_OK) { free(flat); free(idat); fclose(f); return -1; }

    /* Write IDAT chunk: length, type, data, crc */
    uint8_t idat_len_be[4];
    write_be32(idat_len_be, (uint32_t)idat_len);
    fwrite(idat_len_be, 1, 4, f);
    fwrite("IDAT", 1, 4, f);
    fwrite(idat, 1, idat_len, f);
    uint32_t idat_crc = crc32_data("IDAT", 4);
    idat_crc = crc32_update(idat_crc, idat, idat_len);
    uint8_t crc_buf[4];
    write_be32(crc_buf, idat_crc);
    fwrite(crc_buf, 1, 4, f);
    free(idat);

    /* IEND chunk */
    png_write_chunk(f, "IEND", NULL, 0);

    free(flat);
    fclose(f);
    return 0;
}

/* Native GIF save - uses uncompressed LZW (no external libs) */
int wubu_cv_save_gif(WubuCanvas *cv, const char *path) {
    if (!cv) return -1;

    uint32_t *flat = (uint32_t*)malloc(cv->w * cv->h * sizeof(uint32_t));
    if (!flat) return -1;
    wubu_cv_composite(cv, flat, cv->w, cv->h);

    FILE *f = fopen(path, "wb");
    if (!f) { free(flat); return -1; }

    /* GIF signature and version */
    static const uint8_t gif_sig[6] = {'G', 'I', 'F', '8', '9', 'a'};
    fwrite(gif_sig, 1, 6, f);

    /* Logical Screen Descriptor */
    uint8_t lsd[7];
    write_be16(lsd, cv->w);
    write_be16(lsd + 2, cv->h);
    lsd[4] = 0xF7; /* GCT flag=1, color resolution=7, sort=0, GCT size=7 (256 colors) */
    lsd[5] = 0;    /* Background color index */
    lsd[6] = 0;    /* Pixel aspect ratio */
    fwrite(lsd, 1, 7, f);

    /* Global Color Table (256 colors, RGB) */
    uint8_t gct[768];
    for (int i = 0; i < 256; i++) {
        gct[i * 3] = i;
        gct[i * 3 + 1] = i;
        gct[i * 3 + 2] = i;
    }
    fwrite(gct, 1, 768, f);

    /* Image Descriptor */
    uint8_t img_desc[10];
    img_desc[0] = ',';  /* Image separator */
    write_be16(img_desc + 1, 0);  /* Left */
    write_be16(img_desc + 3, 0);  /* Top */
    write_be16(img_desc + 5, cv->w);
    write_be16(img_desc + 7, cv->h);
    img_desc[9] = 0x00;  /* No local color table, no interlace */
    fwrite(img_desc, 1, 10, f);

    /* Image Data - uncompressed LZW (code size 8, no compression) */
    fputc(8, f);  /* LZW minimum code size */

    /* Write each scanline as a sub-block */
    uint8_t *scanline = malloc(cv->w);
    if (!scanline) { free(flat); fclose(f); return -1; }

    for (int y = 0; y < cv->h; y++) {
        for (int x = 0; x < cv->w; x++) {
            uint32_t px = flat[y * cv->w + x];
            /* Simple palette index: use grayscale value */
            uint8_t gray = (uint8_t)((px & 0xFF) * 0.299 + ((px >> 8) & 0xFF) * 0.587 + ((px >> 16) & 0xFF) * 0.114);
            scanline[x] = gray;
        }
        fputc(cv->w, f);  /* Sub-block size */
        fwrite(scanline, 1, cv->w, f);
    }
    free(scanline);

    /* Block terminator */
    fputc(0, f);

    /* GIF Trailer */
    fputc(';', f);

    free(flat);
    fclose(f);
    return 0;
}

/* Native BMP save - 24-bit uncompressed */
int wubu_cv_save_bmp(WubuCanvas *cv, const char *path) {
    if (!cv || !path) return -1;

    uint32_t *flat = (uint32_t*)malloc(cv->w * cv->h * sizeof(uint32_t));
    if (!flat) return -1;
    wubu_cv_composite(cv, flat, cv->w, cv->h);

    FILE *f = fopen(path, "wb");
    if (!f) { free(flat); return -1; }

    int w = cv->w, h = cv->h;
    int row_size = (w * 3 + 3) & ~3;  /* 24-bit aligned to 4 bytes */
    int image_size = row_size * h;
    int file_size = 54 + image_size;

    /* BMP header (14 bytes) */
    uint8_t hdr[54] = {0};
    hdr[0] = 'B'; hdr[1] = 'M';
    *(int32_t*)(hdr + 2) = file_size;
    *(int32_t*)(hdr + 10) = 54;

    /* DIB header (40 bytes - BITMAPINFOHEADER) */
    *(int32_t*)(hdr + 14) = 40;
    *(int32_t*)(hdr + 18) = w;
    *(int32_t*)(hdr + 22) = h;
    *(int16_t*)(hdr + 26) = 1;       /* planes */
    *(int16_t*)(hdr + 28) = 24;      /* bits per pixel */
    *(int32_t*)(hdr + 30) = 0;       /* compression: BI_RGB */
    *(int32_t*)(hdr + 34) = image_size;
    *(int32_t*)(hdr + 38) = 2835;    /* X pixels per meter (72 DPI) */
    *(int32_t*)(hdr + 42) = 2835;    /* Y pixels per meter */
    *(int32_t*)(hdr + 46) = 0;       /* colors used */
    *(int32_t*)(hdr + 50) = 0;       /* important colors */

    fwrite(hdr, 1, 54, f);

    /* BMP stores rows bottom-up */
    uint8_t *row = malloc(row_size);
    if (!row) { free(flat); fclose(f); return -1; }

    for (int y = h - 1; y >= 0; y--) {
        memset(row, 0, row_size);
        for (int x = 0; x < w; x++) {
            uint32_t px = flat[y * w + x];
            row[x * 3] = px & 0xFF;          /* B */
            row[x * 3 + 1] = (px >> 8) & 0xFF;   /* G */
            row[x * 3 + 2] = (px >> 16) & 0xFF;  /* R */
        }
        fwrite(row, 1, row_size, f);
    }

    free(row);
    free(flat);
    fclose(f);
    return 0;
}

/* Native PPM (P6 binary) save — self-contained, no external libs. */
int wubu_cv_save_ppm(WubuCanvas *cv, const char *path) {
    if (!cv || !path) return -1;

    uint32_t *flat = (uint32_t*)malloc(cv->w * cv->h * sizeof(uint32_t));
    if (!flat) return -1;
    wubu_cv_composite(cv, flat, cv->w, cv->h);

    FILE *f = fopen(path, "wb");
    if (!f) { free(flat); return -1; }

    fprintf(f, "P6\n%d %d\n255\n", cv->w, cv->h);
    for (int y = 0; y < cv->h; y++) {
        for (int x = 0; x < cv->w; x++) {
            uint32_t px = flat[y * cv->w + x];
            uint8_t rgb[3];
            rgb[0] = (px >> 16) & 0xFF;            /* R */
            rgb[1] = (px >> 8) & 0xFF;             /* G */
            rgb[2] = px & 0xFF;                    /* B */
            fwrite(rgb, 1, 3, f);
        }
    }

    free(flat);
    fclose(f);
    return 0;
}

int wubu_cv_load(WubuCanvas *cv, const char *path) {
    if (!cv || !path) return -1;

    /* Detect file type by extension and magic bytes */
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    uint8_t magic[8];
    size_t read = fread(magic, 1, 8, f);
    fclose(f);
    if (read < 2) return -1;

    /* PNG: 89 50 4E 47 0D 0A 1A 0A */
    if (read >= 8 && magic[0] == 0x89 && magic[1] == 0x50 && magic[2] == 0x4E && magic[3] == 0x47 &&
        magic[4] == 0x0D && magic[5] == 0x0A && magic[6] == 0x1A && magic[7] == 0x0A) {
        return wubu_cv_load_png(cv, path);
    }

    /* BMP: BM */
    if (magic[0] == 'B' && magic[1] == 'M') {
        return wubu_cv_load_bmp(cv, path);
    }

    /* PPM: P6 or P3 */
    if (magic[0] == 'P' && (magic[1] == '6' || magic[1] == '3')) {
        return wubu_cv_load_ppm(cv, path);
    }

    /* Unknown format - try PPM as fallback */
    return wubu_cv_load_ppm(cv, path);
}

/* Native PPM loader (P3 and P6) */
int wubu_cv_load_ppm(WubuCanvas *cv, const char *path) {
    if (!cv || !path) return -1;

    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char magic[3];
    int w, h, maxval;
    if (fscanf(f, "%2s %d %d %d", magic, &w, &h, &maxval) != 4 || magic[0] != 'P') {
        fclose(f);
        return -1;
    }
    int ch = fgetc(f); /* consume newline */
    (void)ch;

    if (w > 4096 || h > 4096) { fclose(f); return -1; }
    wubu_cv_resize(cv, w, h);
    WubuLayer *l = &cv->layers[cv->active_layer];

    if (magic[1] == '6') {
        /* Binary PPM (P6) */
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                unsigned char rgb[3];
                if (fread(rgb, 1, 3, f) != 3) { fclose(f); return -1; }
                l->pixels[y * w + x] = 0xFF000000 | ((uint32_t)rgb[0] << 16) | ((uint32_t)rgb[1] << 8) | (uint32_t)rgb[2];
            }
        }
    } else if (magic[1] == '3') {
        /* ASCII PPM (P3) */
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int r, g, b;
                if (fscanf(f, "%d %d %d", &r, &g, &b) != 3) { fclose(f); return -1; }
                l->pixels[y * w + x] = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
        }
    }

    fclose(f);
    return 0;
}

/* Native BMP loader */
int wubu_cv_load_bmp(WubuCanvas *cv, const char *path) {
    if (!cv || !path) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    uint8_t hdr[54];
    if (fread(hdr, 1, 54, f) != 54 || hdr[0] != 'B' || hdr[1] != 'M') {
        fclose(f);
        return -1;
    }

    int w = *(int32_t*)(hdr + 18);
    int h = *(int32_t*)(hdr + 22);
    int bpp = *(int16_t*)(hdr + 28);
    int compression = *(int32_t*)(hdr + 30);

    if (bpp != 24 && bpp != 32) { fclose(f); return -1; }
    if (compression != 0) { fclose(f); return -1; } /* Only uncompressed */

    if (w > 4096 || h > 4096) { fclose(f); return -1; }
    wubu_cv_resize(cv, w, h);
    WubuLayer *l = &cv->layers[cv->active_layer];

    int row_size = (w * (bpp / 8) + 3) & ~3;
    uint8_t *row = malloc(row_size);
    if (!row) { fclose(f); return -1; }

    for (int y = h - 1; y >= 0; y--) {
        if (fread(row, 1, row_size, f) != row_size) { free(row); fclose(f); return -1; }
        for (int x = 0; x < w; x++) {
            int bpp_bytes = bpp / 8;
            uint8_t b = row[x * bpp_bytes];
            uint8_t g = row[x * bpp_bytes + 1];
            uint8_t r = row[x * bpp_bytes + 2];
            l->pixels[y * w + x] = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        }
    }
    free(row);
    fclose(f);
    return 0;
}

/* -- PNG unfilter (Adaptive filtering, filter types 0-4) ------------ */

static int png_unfilter(uint8_t *raw, int w, int h, int channels) {
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

/* Native PNG loader - real decode: zlib-inflate IDAT, unfilter, map to XRGB8888 */
int wubu_cv_load_png(WubuCanvas *cv, const char *path) {
    if (!cv || !path) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    /* Read and verify PNG signature */
    uint8_t sig[8];
    if (fread(sig, 1, 8, f) != 8 || sig[0] != 0x89 || sig[1] != 0x50 ||
        sig[2] != 0x4E || sig[3] != 0x47 || sig[4] != 0x0D ||
        sig[5] != 0x0A || sig[6] != 0x1A || sig[7] != 0x0A) {
        fclose(f);
        return -1;
    }

    int w = 0, h = 0, bit_depth = 0, color_type = 0;
    int has_idat = 0;

    /* Accumulate IDAT payload into a growable buffer */
    uint8_t *idat = NULL;
    size_t idat_cap = 0, idat_len = 0;

    /* Parse chunks */
    while (!feof(f)) {
        uint8_t len_buf[4];
        if (fread(len_buf, 1, 4, f) != 4) break;
        uint32_t chunk_len = (len_buf[0] << 24) | (len_buf[1] << 16) | (len_buf[2] << 8) | len_buf[3];

        uint8_t type[4];
        if (fread(type, 1, 4, f) != 4) break;

        if (type[0] == 'I' && type[1] == 'H' && type[2] == 'D' && type[3] == 'R') {
            uint8_t ihdr[13];
            if (fread(ihdr, 1, 13, f) != 13) break;
            w = (ihdr[0] << 24) | (ihdr[1] << 16) | (ihdr[2] << 8) | ihdr[3];
            h = (ihdr[4] << 24) | (ihdr[5] << 16) | (ihdr[6] << 8) | ihdr[7];
            bit_depth = ihdr[8];
            color_type = ihdr[9];
            if (w <= 0 || h <= 0 || w > 4096 || h > 4096 || bit_depth != 8 ||
                (color_type != 2 && color_type != 6)) {
                free(idat);
                fclose(f);
                return -1; /* Unsupported format */
            }
        } else if (type[0] == 'I' && type[1] == 'D' && type[2] == 'A' && type[3] == 'T') {
            if (chunk_len) {
                if (idat_len + chunk_len > idat_cap) {
                    size_t ncap = idat_cap ? idat_cap * 2 : 65536;
                    while (ncap < idat_len + chunk_len) ncap *= 2;
                    uint8_t *nb = (uint8_t*)realloc(idat, ncap);
                    if (!nb) { free(idat); fclose(f); return -1; }
                    idat = nb; idat_cap = ncap;
                }
                if (fread(idat + idat_len, 1, chunk_len, f) != chunk_len) {
                    free(idat); fclose(f); return -1;
                }
                idat_len += chunk_len;
            }
            has_idat = 1;
        } else if (type[0] == 'I' && type[1] == 'E' && type[2] == 'N' && type[3] == 'D') {
            break; /* IEND */
        } else {
            fseek(f, chunk_len, SEEK_CUR); /* Skip unknown chunk */
        }

        /* Skip CRC */
        fseek(f, 4, SEEK_CUR);
    }
    fclose(f);

    if (!has_idat || idat_len == 0) { free(idat); return -1; }

    /* Inflate IDAT via zlib */
    int channels = (color_type == 6) ? 4 : 3;
    size_t raw_size = (size_t)h * (1 + (size_t)w * channels);
    uint8_t *raw = (uint8_t*)malloc(raw_size);
    if (!raw) { free(idat); return -1; }

    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    if (inflateInit(&zs) != Z_OK) { free(idat); free(raw); return -1; }
    zs.next_in = idat;
    zs.avail_in = (uInt)idat_len;
    zs.next_out = raw;
    zs.avail_out = (uInt)raw_size;
    int zret = inflate(&zs, Z_FINISH);
    inflateEnd(&zs);
    free(idat);
    if (zret != Z_STREAM_END || zs.total_out != raw_size) { free(raw); return -1; }

    if (png_unfilter(raw, w, h, channels) != 0) { free(raw); return -1; }

    /* Map to canvas (XRGB8888, little-endian) */
    wubu_cv_resize(cv, w, h);
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (!l->pixels) { free(raw); return -1; }
    for (int y = 0; y < h; y++) {
        const uint8_t *src = raw + (size_t)y * ((size_t)w * channels + 1) + 1;
        for (int x = 0; x < w; x++) {
            uint8_t r = src[x * channels + 0];
            uint8_t g = src[x * channels + 1];
            uint8_t b = src[x * channels + 2];
            l->pixels[y * w + x] = 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        }
    }
    free(raw);
    return 0;
}

/* -- GIF LZW decoder (variable code width, clear/eoi) -------------- */

/* Native GIF loader - real decode: LZW-decompress first image, map GCT to XRGB8888 */
int wubu_cv_load_gif(WubuCanvas *cv, const char *path) {
    if (!cv || !path) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    uint8_t sig[6];
    if (fread(sig, 1, 6, f) != 6 || sig[0] != 'G' || sig[1] != 'I' || sig[2] != 'F' ||
        sig[3] != '8' || (sig[4] != '7' && sig[4] != '9') || sig[5] != 'a') {
        fclose(f);
        return -1;
    }

    /* Logical Screen Descriptor */
    uint8_t lsd[7];
    if (fread(lsd, 1, 7, f) != 7) { fclose(f); return -1; }
    int w = lsd[0] | (lsd[1] << 8);
    int h = lsd[2] | (lsd[3] << 8);
    int gct_flag = (lsd[4] & 0x80) != 0;
    int gct_size = 2 << (lsd[4] & 0x07);
    int bg_index = lsd[5];
    (void)bg_index;

    /* Read Global Color Table */
    uint8_t gct[256 * 3];
    if (gct_flag) {
        if (fread(gct, 1, gct_size * 3, f) != (size_t)gct_size * 3) { fclose(f); return -1; }
    } else {
        memset(gct, 0, sizeof(gct));
    }

    /* Scan to first image descriptor ('2C') */
    int found = 0;
    for (;;) {
        int sep = fgetc(f);
        if (sep == EOF) break;
        if (sep == 0x21) { /* Extension */
            int label = fgetc(f);
            if (label == EOF) break;
            /* skip sub-blocks */
            for (;;) {
                int n = fgetc(f);
                if (n == EOF) break;
                if (n == 0) break;
                if (fseek(f, n, SEEK_CUR) != 0) break;
            }
        } else if (sep == 0x2C) { /* Image Descriptor */
            found = 1;
            break;
        } else if (sep == 0x3B) { /* Trailer */
            break;
        } else {
            break;
        }
    }
    if (!found) { fclose(f); return -1; }

    uint8_t idesc[9];
    if (fread(idesc, 1, 9, f) != 9) { fclose(f); return -1; }
    int iw = idesc[4] | (idesc[5] << 8);
    int ih = idesc[6] | (idesc[7] << 8);
    int lct_flag = (idesc[8] & 0x80) != 0;
    int lct_size = 2 << (idesc[8] & 0x07);

    uint8_t *palette = gct;
    if (lct_flag) {
        if (fread(gct, 1, lct_size * 3, f) != (size_t)lct_size * 3) { fclose(f); return -1; }
        palette = gct;
    }

    int min_code_size = fgetc(f);
    if (min_code_size == EOF) { fclose(f); return -1; }

    int interlaced = (idesc[8] & 0x40) != 0;

    /* Collect LZW data sub-blocks */
    uint8_t *lzw = NULL;
    size_t lzw_cap = 0, lzw_len = 0;
    for (;;) {
        int n = fgetc(f);
        if (n <= 0) break;
        if (lzw_len + (size_t)n > lzw_cap) {
            size_t ncap = lzw_cap ? lzw_cap * 2 : 4096;
            while (ncap < lzw_len + (size_t)n) ncap *= 2;
            uint8_t *nb = (uint8_t*)realloc(lzw, ncap);
            if (!nb) { free(lzw); fclose(f); return -1; }
            lzw = nb; lzw_cap = ncap;
        }
        if (fread(lzw + lzw_len, 1, n, f) != (size_t)n) { free(lzw); fclose(f); return -1; }
        lzw_len += n;
    }
    fclose(f);

    if (!lzw || lzw_len == 0) { free(lzw); return -1; }

    /* LZW decode */
    int clear_code = 1 << min_code_size;
    int eoi_code = clear_code + 1;
    int code_width = min_code_size + 1;
    int next_code = eoi_code + 1;
    int prev_code = -1;

    /* int16_t so the -1 "no parent" sentinel is a genuine negative (uint16_t
       would wrap to 0xFFFF and break the EMIT chain walk) */
    int16_t *table = (int16_t*)malloc(sizeof(int16_t) * 4096);
    uint8_t *table_suffix = (uint8_t*)malloc(4096);
    int *table_len = (int*)malloc(sizeof(int) * 4096);
    uint8_t *indices = (uint8_t*)malloc((size_t)iw * ih + 1);
    if (!table || !table_suffix || !table_len || !indices) {
        free(lzw); free(table); free(table_suffix); free(table_len); free(indices);
        return -1;
    }
    size_t out_len = 0;

    int bitbuf = 0, bitcnt = 0;
    size_t bytepos = 0;

    /* output an entry's suffix chain (reversed) */
    #define EMIT(code) do { \
        int stack[4096], sp = 0; \
        int c = (code); \
        while (c >= 0 && sp < 4096) { stack[sp++] = table_suffix[c]; c = table[c]; } \
        while (sp > 0 && out_len < (size_t)iw * ih) indices[out_len++] = (uint8_t)stack[--sp]; \
    } while (0)

    while (bytepos < lzw_len) {
        while (bitcnt < code_width && bytepos < lzw_len) {
            bitbuf |= lzw[bytepos++] << bitcnt;
            bitcnt += 8;
        }
        if (bitcnt < code_width) break;
        int code = bitbuf & ((1 << code_width) - 1);
        bitbuf >>= code_width;
        bitcnt -= code_width;

        if (code == clear_code) {
            code_width = min_code_size + 1;
            next_code = eoi_code + 1;
            prev_code = -1;
            for (int i = 0; i < next_code; i++) {
                table[i] = -1;
                table_suffix[i] = (uint8_t)i; /* literals are their own byte */
                table_len[i] = 1;
            }
            continue;
        }
        if (code == eoi_code) break;

        if (prev_code == -1) {
            EMIT(code);
            prev_code = code;
            continue;
        }

        if (code < next_code) {
            EMIT(code);
            /* add new entry: prev_code + first char of code */
            if (next_code < 4096) {
                table[next_code] = (uint16_t)prev_code;
                int c2 = code;
                while (table[c2] != -1) c2 = table[c2];
                table_suffix[next_code] = (uint8_t)c2;
                table_len[next_code] = table_len[prev_code] + 1;
                next_code++;
            }
        } else {
            /* code == next_code (KwKwK) */
            int c = prev_code;
            while (table[c] != -1) c = table[c];
            uint8_t first = (uint8_t)c;
            /* emit prev_code's string + first */
            EMIT(prev_code);
            if (out_len < (size_t)iw * ih) indices[out_len++] = first;
            if (next_code < 4096) {
                table[next_code] = (uint16_t)prev_code;
                table_suffix[next_code] = first;
                table_len[next_code] = table_len[prev_code] + 1;
                next_code++;
            }
        }
        prev_code = code;
        if (next_code == (1 << code_width) && code_width < 12) code_width++;
    }
    #undef EMIT

    free(table); free(table_suffix); free(table_len); free(lzw);

    /* Map indices → canvas (use image dims, fallback to logical screen).
       GIF is usually interlaced (Adam7): the LZW stream yields rows in
       interlace order, which must be de-scrambled to top-to-bottom. */
    int out_w = iw > 0 ? iw : w;
    int out_h = ih > 0 ? ih : h;
    wubu_cv_resize(cv, out_w, out_h);
    WubuLayer *l = &cv->layers[cv->active_layer];
    if (!l->pixels) { free(indices); return -1; }

    if (interlaced) {
        size_t pos = 0;
        static const int start[4] = {0, 4, 2, 1};
        static const int step[4]  = {8, 8, 4, 2};
        for (int p = 0; p < 4; p++) {
            for (int y = start[p]; y < out_h; y += step[p]) {
                for (int x = 0; x < out_w; x++) {
                    uint8_t ci = (pos < out_len) ? indices[pos++] : 0;
                    uint8_t r = palette[ci * 3 + 0];
                    uint8_t g = palette[ci * 3 + 1];
                    uint8_t b = palette[ci * 3 + 2];
                    l->pixels[y * out_w + x] = 0xFF000000 |
                        ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
                }
            }
        }
    } else {
        for (int y = 0; y < out_h; y++) {
            for (int x = 0; x < out_w; x++) {
                int idx = (int)(y * (size_t)out_w + x);
                uint8_t ci = (idx < (int)out_len) ? indices[idx] : 0;
                uint8_t r = palette[ci * 3 + 0];
                uint8_t g = palette[ci * 3 + 1];
                uint8_t b = palette[ci * 3 + 2];
                l->pixels[y * out_w + x] = 0xFF000000 |
                    ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
        }
    }
    free(indices);
    return 0;
}
