/* wubu_x11_setup_reply.c -- see wubu_x11_setup_reply.h. */
#include "wubu_x11_setup_reply.h"

#include <string.h>

static const char k_vendor[] = "WuBuOS X11 bridge";

typedef struct w {
    uint8_t *p; size_t cap, at; int lsb; int overflow;
} w;

static void put8(w *b, uint8_t v) {
    if (b->at < b->cap) b->p[b->at++] = v; else { b->overflow = 1; b->at++; }
}
static void put16(w *b, uint16_t v) {
    if (b->lsb) { put8(b, (uint8_t)(v & 0xff)); put8(b, (uint8_t)(v >> 8)); }
    else        { put8(b, (uint8_t)(v >> 8));    put8(b, (uint8_t)(v & 0xff)); }
}
static void put32(w *b, uint32_t v) {
    if (b->lsb) { put16(b, (uint16_t)(v & 0xffff)); put16(b, (uint16_t)(v >> 16)); }
    else        { put16(b, (uint16_t)(v >> 16));    put16(b, (uint16_t)(v & 0xffff)); }
}
/* pad to a multiple of `align` from the START of the message */
static void pad(w *b, size_t align) {
    while (b->at % align) put8(b, 0);
}
static size_t padded(size_t n, size_t align) { return (n + align - 1) & ~(align - 1); }

int wubu_x11_build_setup_reply(uint8_t *out, size_t cap,
                               const WubuX11ScreenCfg *cfg, int lsb_first) {
    if (!out || !cfg) return -1;
    size_t vendor_len = strlen(k_vendor);
    size_t vendor_pad = padded(vendor_len, 4);
    /* one pixmap format per depth offered: 1-bit and 24/32-bit */
    const size_t n_formats = 2;
    const size_t screen_bytes = 40 + 8 + 24;      /* header + one depth + one visual */
    size_t additional = 32 + vendor_pad + n_formats * 8 + screen_bytes;
    if (additional % 4) return -2;                 /* the length field is in words */

    w b = { out, cap, 0, lsb_first, 0 };

    /* --- the 8-byte reply prefix ------------------------------------------ */
    put8(&b, 1);                                   /* Success */
    put8(&b, 0);
    put16(&b, 11);                                 /* protocol 11.0 */
    put16(&b, 0);
    put16(&b, (uint16_t)(additional / 4));          /* length in 4-byte units */

    /* --- fixed part of the additional data -------------------------------- */
    put32(&b, 1);                                   /* release number */
    put32(&b, 0x00400000);                          /* resource id base */
    put32(&b, 0x001fffff);                          /* resource id mask */
    /* The layout here is xcb_setup_t's, not the prose order in the spec: the
     * 4-byte field after the id mask is motion-buffer-size, vendor length is a
     * CARD16, and maximum-request-length is a CARD16 too. Two CARD32s where the
     * wire has CARD16s moved every following byte by four -- a real client
     * answered "unable to open display" and nothing else. */
    put32(&b, 256);                                 /* motion buffer size */
    put16(&b, (uint16_t)vendor_len);                /* vendor length: CARD16 */
    put16(&b, 65535);                               /* max request length: CARD16 */
    put8(&b, 1);                                    /* screens */
    put8(&b, (uint8_t)n_formats);                   /* pixmap formats */
    put8(&b, lsb_first ? 0 : 1);                    /* image byte order: 0=LSBFirst */
    put8(&b, lsb_first ? 0 : 1);                    /* bitmap bit order */
    put8(&b, 32);                                   /* scanline unit */
    put8(&b, 32);                                   /* scanline pad */
    put8(&b, 8);                                    /* min keycode */
    put8(&b, 255);                                  /* max keycode */
    put32(&b, 0);                                   /* unused */

    /* --- vendor string ---------------------------------------------------- */
    for (size_t i = 0; i < vendor_len; i++) put8(&b, (uint8_t)k_vendor[i]);
    pad(&b, 4);

    /* --- pixmap formats --------------------------------------------------- */
    put8(&b, 1);  put8(&b, 1);  put8(&b, 32); put8(&b, 0); put32(&b, 0);
    put8(&b, cfg->depth); put8(&b, 32); put8(&b, 32); put8(&b, 0); put32(&b, 0);

    /* --- one screen -------------------------------------------------------- */
    put32(&b, cfg->root_window);
    put32(&b, cfg->default_colormap);
    put32(&b, 0xffffff);                            /* white pixel */
    put32(&b, 0x000000);                            /* black pixel */
    put32(&b, 0);                                   /* current input masks */
    put16(&b, cfg->width);
    put16(&b, cfg->height);
    put16(&b, cfg->width_mm);
    put16(&b, cfg->height_mm);
    put16(&b, 1);                                   /* min installed maps */
    put16(&b, 1);                                   /* max installed maps */
    put32(&b, cfg->root_visual);
    put8(&b, 0);                                    /* backing stores: never */
    put8(&b, 0);                                    /* save unders */
    put8(&b, cfg->depth);                           /* root depth */
    put8(&b, 1);                                    /* allowed depths */

    /* --- the one depth ---------------------------------------------------- */
    put8(&b, cfg->depth);
    put8(&b, 0);
    put16(&b, 1);                                   /* visuals at this depth */
    put32(&b, 0);                                   /* unused */

    /* --- the one visual: TrueColor, 8/8/8 --------------------------------- */
    put32(&b, cfg->root_visual);
    put8(&b, 4);                                    /* class: TrueColor */
    put8(&b, 8);                                    /* bits per rgb */
    put16(&b, 256);                                 /* colormap entries */
    put32(&b, 0xff0000);
    put32(&b, 0x00ff00);
    put32(&b, 0x0000ff);
    put32(&b, 0);                                   /* unused */

    if (b.overflow || b.at != 8 + additional) return -3;
    return (int)b.at;
}
