/*
 * wubu_9pwire.h  --  the 9P2000 wire vocabulary, in ONE place.
 *
 * The kernel speaks 9P in both roles: wubu_9pc is the CLIENT (it fetches
 * /apps over COM2) and wubu_9psrv is the SERVER (it publishes /n/kv from the
 * live KV tensor). Both encode the same little-endian fields, so the codec
 * lives here as static inline helpers and NEITHER side keeps its own copy.
 *
 * No libc beyond memcpy: this compiles under -ffreestanding -nostdlib with
 * -mcmodel=kernel, and in a hosted test that includes it.
 */
#ifndef WUBU_9PWIRE_H
#define WUBU_9PWIRE_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/* ---- message types (9P2000, the subset both roles use) ---- */
#define W9_TVERSION 100
#define W9_RVERSION 101
#define W9_TATTACH  104
#define W9_RATTACH  105
#define W9_RERROR   107
#define W9_TWALK    110
#define W9_RWALK    111
#define W9_TOPEN    112
#define W9_ROPEN    113
#define W9_TREAD    116
#define W9_RREAD    117
#define W9_TCLUNK   120
#define W9_RCLUNK   121
#define W9_TSTAT    124
#define W9_RSTAT    125

/* ---- qid ---- */
#define W9_QTDIR   0x80u
#define W9_QTFILE  0x00u

/* header: size[4] type[1] tag[2] */
#define W9_HDR 7u

static inline void w9w_put16(uint8_t *p, uint16_t v) { memcpy(p, &v, 2); }
static inline void w9w_put32(uint8_t *p, uint32_t v) { memcpy(p, &v, 4); }
static inline void w9w_put64(uint8_t *p, uint64_t v) { memcpy(p, &v, 8); }
static inline uint16_t w9w_get16(const uint8_t *p) { uint16_t v; memcpy(&v, p, 2); return v; }
static inline uint32_t w9w_get32(const uint8_t *p) { uint32_t v; memcpy(&v, p, 4); return v; }
static inline uint64_t w9w_get64(const uint8_t *p) { uint64_t v; memcpy(&v, p, 8); return v; }

/* string: len[2] bytes. Returns bytes written (2 + len). */
static inline uint32_t w9w_putstr(uint8_t *p, const char *s)
{
    size_t l = 0;
    if (s) while (s[l]) l++;
    w9w_put16(p, (uint16_t)l);
    if (l) memcpy(p + 2, s, l);
    return (uint32_t)(2 + l);
}

/* string: read len[2] then copy at most cap-1 bytes into out (NUL-terminated).
 * Returns the field size on the wire (2 + len), or 0 if the field would run
 * past `end`. A too-long name is truncated in `out` but still consumed. */
static inline uint32_t w9w_getstr(const uint8_t *p, const uint8_t *end,
                                  char *out, size_t cap)
{
    if (p + 2 > end) return 0;
    uint16_t l = w9w_get16(p);
    if (p + 2 + l > end) return 0;
    if (out && cap) {
        size_t n = (size_t)l < cap - 1 ? (size_t)l : cap - 1;
        memcpy(out, p + 2, n);
        out[n] = '\0';
    }
    return (uint32_t)(2 + l);
}

/* stamp the size/type/tag header once the body is written */
static inline void w9w_header(uint8_t *b, uint32_t size, uint8_t type, uint16_t tag)
{
    w9w_put32(b, size);
    b[4] = type;
    w9w_put16(b + 5, tag);
}

#endif /* WUBU_9PWIRE_H */
