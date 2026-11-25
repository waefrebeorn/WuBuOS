/* wubu_x11_request.c -- see wubu_x11_request.h. */
#include "wubu_x11_request.h"

#include <string.h>

typedef struct w { uint8_t *p; size_t cap, at; int lsb; } w;
static void put8(w *b, uint8_t v) { if (b->at < b->cap) b->p[b->at] = v; b->at++; }
static void put16(w *b, uint16_t v) {
    if (b->lsb) { put8(b, (uint8_t)v); put8(b, (uint8_t)(v >> 8)); }
    else        { put8(b, (uint8_t)(v >> 8)); put8(b, (uint8_t)v); }
}
static void put32(w *b, uint32_t v) {
    if (b->lsb) { put16(b, (uint16_t)v); put16(b, (uint16_t)(v >> 16)); }
    else        { put16(b, (uint16_t)(v >> 16)); put16(b, (uint16_t)v); }
}
static int done(w *b) { return b->at <= b->cap ? (int)b->at : -1; }

int wubu_x11_parse_request(const uint8_t *buf, size_t n, WubuX11Request *out,
                           size_t *need, int lsb_first) {
    if (!buf || !out || n < 4) return -1;
    out->opcode = buf[0];
    out->data = buf[1];
    out->length = lsb_first ? (uint16_t)(buf[2] | (buf[3] << 8))
                            : (uint16_t)((buf[2] << 8) | buf[3]);
    if (out->length == 0) return -2;              /* a zero-length request is not a request */
    out->body = buf + 4;
    if (need) *need = (size_t)out->length * 4u;
    return 0;
}

int wubu_x11_reply_query_extension(uint8_t *out, size_t cap, uint16_t seq,
                                   int present, int lsb) {
    w b = { out, cap, 0, lsb };
    put8(&b, 1); put8(&b, (uint8_t)present); put16(&b, seq); put32(&b, 0);
    put8(&b, (uint8_t)present); put8(&b, 0); put16(&b, 0); put8(&b, 0);
    while (b.at < 32) put8(&b, 0);
    return done(&b);
}

int wubu_x11_reply_list_extensions(uint8_t *out, size_t cap, uint16_t seq,
                                   const char *const *names, int count, int lsb) {
    size_t total = 1 + (size_t)count * 8;
    for (int i = 0; i < count; i++) {
        size_t l = strlen(names[i]);
        total += (l + 3) & ~(size_t)3;
    }
    w b = { out, cap, 0, lsb };
    put8(&b, 1); put8(&b, (uint8_t)count); put16(&b, seq); put32(&b, (uint32_t)(total / 4));
    /* 24 unused bytes: the names list starts at byte 32, not at 8. Writing the
     * names immediately after the header put every name in the wrong place and
     * a client would read garbage lengths. */
    while (b.at < 32) put8(&b, 0);
    for (int i = 0; i < count; i++) {
        size_t l = strlen(names[i]);
        put8(&b, (uint8_t)l); put8(&b, 0);
        for (size_t k = 0; k < l; k++) put8(&b, (uint8_t)names[i][k]);
        for (size_t k = l; k & 3; k++) put8(&b, 0);
    }
    return done(&b);
}

int wubu_x11_reply_atom(uint8_t *out, size_t cap, uint16_t seq, uint32_t atom,
                        int lsb) {
    w b = { out, cap, 0, lsb };
    put8(&b, 1); put8(&b, 0); put16(&b, seq); put32(&b, 0);
    put32(&b, atom);
    while (b.at < 32) put8(&b, 0);
    return done(&b);
}

int wubu_x11_reply_atom_name(uint8_t *out, size_t cap, uint16_t seq,
                             const char *name, int lsb) {
    size_t l = strlen(name), pad = (l + 3) & ~(size_t)3;
    w b = { out, cap, 0, lsb };
    put8(&b, 1); put8(&b, 0); put16(&b, seq); put32(&b, (uint32_t)(pad / 4));
    put16(&b, (uint16_t)l);
    while (b.at < 32) put8(&b, 0);
    for (size_t i = 0; i < l; i++) put8(&b, (uint8_t)name[i]);
    while (b.at < (size_t)32 + pad) put8(&b, 0);
    return done(&b);
}

int wubu_x11_reply_geometry(uint8_t *out, size_t cap, uint16_t seq,
                            uint32_t root, int16_t x, int16_t y,
                            uint16_t w_, uint16_t h_, uint16_t bw, int lsb) {
    w b = { out, cap, 0, lsb };
    put8(&b, 1); put8(&b, 24); put16(&b, seq); put32(&b, 0);   /* depth 24 */
    put32(&b, root);
    put16(&b, (uint16_t)x); put16(&b, (uint16_t)y);
    put16(&b, w_); put16(&b, h_);
    put16(&b, bw);
    while (b.at < 32) put8(&b, 0);
    return done(&b);
}

int wubu_x11_reply_property_missing(uint8_t *out, size_t cap, uint16_t seq, int lsb) {
    w b = { out, cap, 0, lsb };
    put8(&b, 1); put8(&b, 0); put16(&b, seq); put32(&b, 0);   /* format 0 = absent */
    put32(&b, 0); put32(&b, 0); put32(&b, 0);                 /* type, bytes-after, value-len */
    while (b.at < 32) put8(&b, 0);
    return done(&b);
}

int wubu_x11_reply_window_attrs(uint8_t *out, size_t cap, uint16_t seq,
                                uint32_t visual, uint32_t colormap, int lsb) {
    w b = { out, cap, 0, lsb };
    put8(&b, 1); put8(&b, 0); put16(&b, seq); put32(&b, 3);   /* length: 3 words */
    put32(&b, visual);
    put16(&b, 1);                                             /* class: InputOutput */
    put8(&b, 0); put8(&b, 0);                                 /* bit/win gravity */
    put32(&b, 0); put32(&b, 0);                               /* backing planes/pixel */
    put8(&b, 0); put8(&b, 0);                                 /* save-under, map-installed */
    put8(&b, 2);                                              /* map state: Viewable */
    put8(&b, 0);                                              /* override-redirect */
    put32(&b, colormap);
    put32(&b, 0); put32(&b, 0);                               /* all / your event masks */
    put16(&b, 0); put16(&b, 0);                               /* do-not-propagate, unused */
    while (b.at < 44) put8(&b, 0);
    return done(&b);
}

int wubu_x11_reply_keymap(uint8_t *out, size_t cap, uint16_t seq,
                          uint8_t keysyms_per, int count, int lsb) {
    size_t bytes = (size_t)keysyms_per * (size_t)count * 4u;
    w b = { out, cap, 0, lsb };
    put8(&b, 1); put8(&b, keysyms_per); put16(&b, seq); put32(&b, (uint32_t)(bytes / 4));
    while (b.at < 32) put8(&b, 0);
    while (b.at < 32 + bytes) put8(&b, 0);
    return done(&b);
}

/* Small fixed replies. Each one is 32 bytes and echoes the sequence number; the
 * body fields are the defaults a plain screen would report. */
static int reply_fixed(uint8_t *out, size_t cap, uint16_t seq, int data_byte,
                       int lsb, const uint16_t *u16s, int n16) {
    w b = { out, cap, 0, lsb };
    put8(&b, 1); put8(&b, (uint8_t)data_byte); put16(&b, seq); put32(&b, 0);
    for (int i = 0; i < n16; i++) put16(&b, u16s[i]);
    while (b.at < 32) put8(&b, 0);
    return done(&b);
}

int wubu_x11_reply_best_size(uint8_t *out, size_t cap, uint16_t seq,
                             uint16_t w_, uint16_t h_, int lsb) {
    uint16_t f[2] = { w_, h_ };
    return reply_fixed(out, cap, seq, 0, lsb, f, 2);
}

int wubu_x11_reply_screen_saver(uint8_t *out, size_t cap, uint16_t seq, int lsb) {
    uint16_t f[2] = { 600, 600 };             /* timeout, interval */
    return reply_fixed(out, cap, seq, 1, lsb, f, 2);
}

int wubu_x11_reply_list_hosts(uint8_t *out, size_t cap, uint16_t seq, int lsb) {
    return reply_fixed(out, cap, seq, 0, lsb, NULL, 0);   /* mode Disable, 0 hosts */
}

int wubu_x11_reply_pointer_control(uint8_t *out, size_t cap, uint16_t seq, int lsb) {
    uint16_t f[3] = { 2, 1, 4 };              /* accel num/den, threshold */
    return reply_fixed(out, cap, seq, 0, lsb, f, 3);
}

int wubu_x11_op_expects_reply(uint8_t opcode) {
    /* Core X11 opcodes whose reply the client waits for. Everything else in the
     * core range mutates state and is answered with silence. */
    switch (opcode) {
    case 3: case 14: case 15: case 16: case 17: case 20: case 21: case 23:
    case 26: case 31: case 36: case 38: case 42: case 43: case 47: case 48:
    case 49: case 50: case 52: case 84: case 85: case 86: case 87: case 88:
    case 91: case 97: case 98: case 99: case 101: case 102: case 103: case 104:
    case 106: case 108: case 109: case 110: case 113: case 114: case 115:
    case 116: case 117: case 119: case 120: case 121: case 122: case 123:
    case 124: case 125: case 126:
        return 1;
    default:
        return 0;
    }
}

int wubu_x11_reply_input_focus(uint8_t *out, size_t cap, uint16_t seq,
                               uint32_t focus, int lsb) {
    w b = { out, cap, 0, lsb };
    put8(&b, 1); put8(&b, 1); put16(&b, seq); put32(&b, 0);
    put32(&b, focus);
    while (b.at < 32) put8(&b, 0);
    return done(&b);
}
