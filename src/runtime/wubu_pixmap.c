/* wubu_pixmap.c -- see wubu_pixmap.h. PNG uses wubu_inflate; nothing is allocated here. */
#include "wubu_pixmap.h"
#include "wubu_inflate.h"
#include <string.h>

static unsigned be32(const unsigned char *p) {
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3];
}
static unsigned le32(const unsigned char *p) {
    return (unsigned)p[0] | ((unsigned)p[1] << 8) | ((unsigned)p[2] << 16) | ((unsigned)p[3] << 24);
}
static unsigned le16(const unsigned char *p) { return (unsigned)p[0] | ((unsigned)p[1] << 8); }

/* PNG CRC-32 (the IEEE polynomial), computed without a 256-entry table: a 8 KB table
 * for a decoder is a cache cost we do not need to pay. */
static unsigned crc32_png(const unsigned char *p, size_t n) {
    unsigned crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        crc ^= p[i];
        for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (unsigned)(-(int)(crc & 1)));
    }
    return crc ^ 0xFFFFFFFFu;
}

static int png_channels(int color_type, int *ch) {
    switch (color_type) {
        case 0: *ch = 1; return 0;                 /* grey */
        case 2: *ch = 3; return 0;                 /* RGB */
        case 3: *ch = 3; return 0;                 /* palette -> expanded to RGB */
        case 6: *ch = 4; return 0;                 /* RGBA */
        case 4: return WUBU_PIXMAP_UNSUPPORTED;     /* grey+alpha, named as absent */
        default: return WUBU_PIXMAP_CORRUPT;
    }
}

typedef struct {
    unsigned w, h;
    int depth, color, interlace;
    int channels;        /* channels in the DECODED output */
    int raw_ch;          /* bytes per pixel in the filtered stream (palette = 1) */
    const unsigned char *plte;
    size_t plte_len;
} Png;

static int png_parse_ihdr(const unsigned char *d, size_t len, Png *p) {
    if (len < 13) return WUBU_PIXMAP_TRUNCATED;
    p->w = be32(d); p->h = be32(d + 4);
    p->depth = d[8]; p->color = d[9];
    if (d[10] != 0 || d[11] != 0) return WUBU_PIXMAP_UNSUPPORTED;   /* compression/filter method */
    p->interlace = d[12];
    if (p->w == 0 || p->h == 0) return WUBU_PIXMAP_CORRUPT;
    if (p->depth != 8) return WUBU_PIXMAP_UNSUPPORTED;              /* 1/2/4/16 bit not yet */
    if (p->interlace != 0) return WUBU_PIXMAP_UNSUPPORTED;          /* Adam7 not yet */
    int rc = png_channels(p->color, &p->channels);
    p->raw_ch = (p->color == 3) ? 1 : p->channels;
    return rc;
}

/* Walk chunks once. mode 0 = info only, 1 = gather IDAT into scratch. */
static int png_walk(const unsigned char *d, size_t len, Png *p,
                    unsigned char *scratch, size_t scratch_cap, size_t *idat_total) {
    static const unsigned char sig[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    if (len < 8 || memcmp(d, sig, 8)) return WUBU_PIXMAP_FORMAT;
    size_t off = 8, used = 0;
    int saw_ihdr = 0, saw_iend = 0;
    while (off + 8 <= len) {
        unsigned clen = be32(d + off);
        if (clen > len || off + 12 + clen > len) return WUBU_PIXMAP_TRUNCATED;
        const unsigned char *type = d + off + 4;
        const unsigned char *body = d + off + 8;
        if (crc32_png(type, (size_t)clen + 4) != be32(body + clen)) return WUBU_PIXMAP_CORRUPT;
        if (!memcmp(type, "IHDR", 4)) {
            int rc = png_parse_ihdr(body, clen, p);
            if (rc) return rc;
            saw_ihdr = 1;
        } else if (!memcmp(type, "PLTE", 4)) {
            p->plte = body; p->plte_len = clen;
        } else if (!memcmp(type, "IDAT", 4)) {
            if (!saw_ihdr) return WUBU_PIXMAP_CORRUPT;
            if (scratch) {
                if (used + clen > scratch_cap) return WUBU_PIXMAP_SCRATCH;
                memcpy(scratch + used, body, clen);
            }
            used += clen;
        } else if (!memcmp(type, "IEND", 4)) {
            saw_iend = 1;
            break;
        }
        off += 12 + clen;
    }
    if (!saw_ihdr) return WUBU_PIXMAP_CORRUPT;
    if (!saw_iend) return WUBU_PIXMAP_TRUNCATED;
    if (idat_total) *idat_total = used;
    if (used == 0) return WUBU_PIXMAP_CORRUPT;
    return 0;
}

static unsigned char paeth(int a, int b, int c) {
    int p = a + b - c;
    int pa = p > a ? p - a : a - p;
    int pb = p > b ? p - b : b - p;
    int pc = p > c ? p - c : c - p;
    if (pa <= pb && pa <= pc) return (unsigned char)a;
    if (pb <= pc) return (unsigned char)b;
    return (unsigned char)c;
}

static int png_unfilter(unsigned char *raw, size_t stride, unsigned h, int bpp) {
    for (unsigned y = 0; y < h; y++) {
        unsigned char *row = raw + (size_t)y * (stride + 1);
        int f = row[0];
        unsigned char *cur = row + 1;
        const unsigned char *prev = y ? (raw + (size_t)(y - 1) * (stride + 1) + 1) : NULL;
        switch (f) {
            case 0: break;
            case 1: for (size_t i = (size_t)bpp; i < stride; i++) cur[i] = (unsigned char)(cur[i] + cur[i - bpp]); break;
            case 2: if (prev) for (size_t i = 0; i < stride; i++) cur[i] = (unsigned char)(cur[i] + prev[i]); break;
            case 3:
                for (size_t i = 0; i < stride; i++) {
                    int a = i >= (size_t)bpp ? cur[i - bpp] : 0;
                    int b = prev ? prev[i] : 0;
                    cur[i] = (unsigned char)(cur[i] + ((a + b) >> 1));
                }
                break;
            case 4:
                for (size_t i = 0; i < stride; i++) {
                    int a = i >= (size_t)bpp ? cur[i - bpp] : 0;
                    int b = prev ? prev[i] : 0;
                    int c = (prev && i >= (size_t)bpp) ? prev[i - bpp] : 0;
                    cur[i] = (unsigned char)(cur[i] + paeth(a, b, c));
                }
                break;
            default: return WUBU_PIXMAP_CORRUPT;
        }
    }
    return 0;
}

static int png_decode(const unsigned char *d, size_t len, void *scratch, size_t scratch_cap,
                      unsigned char *out, size_t outcap, WubuPixmap *img) {
    Png p = { 0 };
    size_t idat_total = 0;
    int rc = png_walk(d, len, &p, NULL, 0, &idat_total);
    if (rc) return rc;
    if (!scratch || scratch_cap < idat_total) return WUBU_PIXMAP_SCRATCH;
    rc = png_walk(d, len, &p, (unsigned char *)scratch, scratch_cap, &idat_total);
    if (rc) return rc;

    size_t stride = (size_t)p.w * (size_t)p.raw_ch;
    size_t need = (stride + 1) * (size_t)p.h;
    if (outcap < stride * (size_t)p.h) return WUBU_PIXMAP_OVERFLOW;
    /* Unfilter in place over the decoded scanlines, then expand palette in place. */
    if (need > outcap) return WUBU_PIXMAP_OVERFLOW;   /* need room for the filter byte per row */
    int ierr = 0;
    long n = wubu_inflate((const unsigned char *)scratch, idat_total, out, outcap, &ierr);
    if (n < 0) return WUBU_PIXMAP_CORRUPT;
    if ((size_t)n != need) return WUBU_PIXMAP_CORRUPT;
    rc = png_unfilter(out, stride, p.h, p.raw_ch);
    if (rc) return rc;

    /* Compact the rows over the filter bytes: output becomes w*h*channels bytes. */
    /* Row 0 moves too: its data starts at byte 1, so skipping y==0 left every row
     * shifted by one filter byte and the pixels "differed" in every format. */
    for (unsigned y = 0; y < p.h; y++)
        memmove(out + (size_t)y * stride, out + (size_t)y * (stride + 1) + 1, stride);

    if (p.color == 3) {
        if (!p.plte || p.plte_len < 3) return WUBU_PIXMAP_CORRUPT;
        size_t px = (size_t)p.w * p.h;
        if (outcap < px * 3) return WUBU_PIXMAP_OVERFLOW;
        for (size_t i = 0; i < px; i++)
            if ((size_t)out[i] * 3 + 2 >= p.plte_len) return WUBU_PIXMAP_CORRUPT;
        for (size_t i = px; i-- > 0; ) {
            unsigned idx = out[i];
            out[i * 3 + 0] = p.plte[idx * 3 + 0];
            out[i * 3 + 1] = p.plte[idx * 3 + 1];
            out[i * 3 + 2] = p.plte[idx * 3 + 2];
        }
    }
    img->width = (int)p.w; img->height = (int)p.h; img->channels = p.channels;
    return 0;
}

static int bmp_decode(const unsigned char *d, size_t len, unsigned char *out, size_t outcap, WubuPixmap *img) {
    if (len < 54 || d[0] != 'B' || d[1] != 'M') return WUBU_PIXMAP_FORMAT;
    unsigned data_off = le32(d + 10);
    unsigned hdr = le32(d + 14);
    if (hdr < 40) return WUBU_PIXMAP_UNSUPPORTED;
    int w = (int)le32(d + 18), h = (int)le32(d + 22);
    int bpp = (int)le16(d + 28);
    unsigned compression = le32(d + 30);
    if (w <= 0 || h == 0) return WUBU_PIXMAP_CORRUPT;
    if (compression != 0) return WUBU_PIXMAP_UNSUPPORTED;
    if (bpp != 24 && bpp != 32) return WUBU_PIXMAP_UNSUPPORTED;
    int ch = bpp / 8;
    int bottom_up = h > 0;
    unsigned height = (unsigned)(bottom_up ? h : -h);
    size_t stride = ((size_t)w * (size_t)ch + 3u) & ~(size_t)3u;
    if (data_off > len || (size_t)data_off + stride * height > len) return WUBU_PIXMAP_TRUNCATED;
    if (outcap < (size_t)w * height * (size_t)ch) return WUBU_PIXMAP_OVERFLOW;
    for (unsigned y = 0; y < height; y++) {
        unsigned src_row = bottom_up ? height - 1 - y : y;
        const unsigned char *s = d + data_off + (size_t)src_row * stride;
        unsigned char *o = out + (size_t)y * (size_t)w * (size_t)ch;
        for (int x = 0; x < w; x++) {
            o[x * ch + 0] = s[x * ch + 2];      /* BMP stores BGR(A) */
            o[x * ch + 1] = s[x * ch + 1];
            o[x * ch + 2] = s[x * ch + 0];
            if (ch == 4) o[x * ch + 3] = s[x * ch + 3];
        }
    }
    img->width = w; img->height = (int)height; img->channels = ch;
    return 0;
}

static int sniff(const unsigned char *d, size_t len) {
    if (len >= 8 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G') return 1;
    if (len >= 2 && d[0] == 'B' && d[1] == 'M') return 2;
    return 0;
}

int wubu_pixmap_info(const void *data, size_t len, WubuPixmap *img, int *err) {
    if (err) *err = WUBU_PIXMAP_OK;
    if (!data || !img || len < 8) { if (err) *err = WUBU_PIXMAP_ARGS; return -1; }
    const unsigned char *d = (const unsigned char *)data;
    int kind = sniff(d, len);
    int rc;
    if (kind == 1) {
        Png p = { 0 };
        rc = png_walk(d, len, &p, NULL, 0, NULL);
        if (rc) { if (err) *err = rc; return -1; }
        img->width = (int)p.w; img->height = (int)p.h; img->channels = p.channels;
        return 0;
    }
    if (kind == 2) {
        if (len < 54) { if (err) *err = WUBU_PIXMAP_TRUNCATED; return -1; }
        int w = (int)le32(d + 18), h = (int)le32(d + 22), bpp = (int)le16(d + 28);
        if (w <= 0 || h == 0 || (bpp != 24 && bpp != 32)) { if (err) *err = WUBU_PIXMAP_CORRUPT; return -1; }
        img->width = w; img->height = h < 0 ? -h : h; img->channels = bpp / 8;
        return 0;
    }
    if (err) *err = WUBU_PIXMAP_FORMAT;
    return -1;
}

int wubu_pixmap_decode(const void *data, size_t len, void *scratch, size_t scratch_cap,
                      unsigned char *out, size_t outcap, WubuPixmap *img, int *err) {
    if (err) *err = WUBU_PIXMAP_OK;
    if (!data || !out || !img || len < 8) { if (err) *err = WUBU_PIXMAP_ARGS; return -1; }
    const unsigned char *d = (const unsigned char *)data;
    int rc;
    switch (sniff(d, len)) {
        case 1: rc = png_decode(d, len, scratch, scratch_cap, out, outcap, img); break;
        case 2: rc = bmp_decode(d, len, out, outcap, img); break;
        default: rc = WUBU_PIXMAP_FORMAT; break;
    }
    if (rc) { if (err) *err = rc; return -1; }
    return 0;
}

const char *wubu_pixmap_reason(int err) {
    switch (err) {
        case WUBU_PIXMAP_OK:          return "ok";
        case WUBU_PIXMAP_ARGS:        return "bad arguments";
        case WUBU_PIXMAP_FORMAT:      return "not an image format we decode";
        case WUBU_PIXMAP_TRUNCATED:   return "that image file is truncated";
        case WUBU_PIXMAP_UNSUPPORTED: return "that image variant is not supported yet";
        case WUBU_PIXMAP_CORRUPT:     return "that image is damaged";
        case WUBU_PIXMAP_SCRATCH:     return "scratch buffer too small for the image data";
        case WUBU_PIXMAP_OVERFLOW:    return "pixel buffer too small for that image size";
        default:                     return "invalid";
    }
}
