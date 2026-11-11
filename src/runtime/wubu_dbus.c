/*
 * wubu_dbus.c -- the D-Bus wire format marshaller.
 *
 * See wubu_dbus.h.  All integers are appended LITTLE-ENDIAN (the byte order
 * flag in the header is 'l' for every machine this tree targets); a real
 * marshaller checks the header and swaps, and a test would cover both.
 */
#include "wubu_dbus.h"

#include <stdlib.h>
#include <string.h>

void wb_dbus_buf_init(wb_dbus_buf_t *b)
{
    if (!b) return;
    b->buf = NULL;
    b->len = 0;
    b->cap = 0;
}

void wb_dbus_buf_free(wb_dbus_buf_t *b)
{
    if (!b) return;
    free(b->buf);
    b->buf = NULL;
    b->len = b->cap = 0;
}

void wb_dbus_buf_reset(wb_dbus_buf_t *b)
{
    if (b) b->len = 0;
}

static int buf_reserve(wb_dbus_buf_t *b, size_t need)
{
    if (!b) return -1;
    if (b->len + need <= b->cap) return 0;
    size_t cap = b->cap ? b->cap * 2 : 64;
    while (cap < b->len + need) cap *= 2;
    uint8_t *nb = (uint8_t *)realloc(b->buf, cap);
    if (!nb) return -1;
    b->buf = nb;
    b->cap = cap;
    return 0;
}

static void put_bytes(wb_dbus_buf_t *b, const void *p, size_t n)
{
    if (!b || !p) return;
    if (b->len + n <= b->cap) {
        memcpy(b->buf + b->len, p, n);
        b->len += n;
    }
}

static void put_u8(wb_dbus_buf_t *b, uint8_t v)
{
    put_bytes(b, &v, 1);
}

uint32_t wb_dbus_align_of(char t)
{
    switch (t) {
    case 'y': return 1;                       /* BYTE */
    case 'b': case 'i': case 'u': case 'h': return 4;  /* BOOL, INT32, UINT32, FD */
    case 'n': case 'q': return 2;             /* INT16, UINT16 */
    case 'x': case 't': case 'd': return 8;   /* INT64, UINT64, DOUBLE */
    case 's': case 'o': case 'g': return 4;   /* STRING, OBJECT_PATH, SIGNATURE */
    case 'a': return 4;                       /* ARRAY */
    case 'r': case 'e': case '(': case '{': return 8;  /* STRUCT, DICT_ENTRY */
    case 'v': return 8;                       /* VARIANT */
    default:  return 1;
    }
}

int wb_dbus_align(wb_dbus_buf_t *b, uint32_t alignment)
{
    if (!b || alignment == 0) return -1;
    uint32_t pad = (alignment - (b->len % alignment)) % alignment;
    if (pad == 0) return 0;
    if (buf_reserve(b, pad) != 0) return -1;
    memset(b->buf + b->len, 0, pad);
    b->len += pad;
    return 0;
}

static int append_u16(wb_dbus_buf_t *b, uint16_t v)
{
    if (wb_dbus_align(b, 2) != 0) return -1;
    if (buf_reserve(b, 2) != 0) return -1;
    b->buf[b->len++] = (uint8_t)(v & 0xFF);
    b->buf[b->len++] = (uint8_t)(v >> 8);
    return 0;
}

int wb_dbus_append_u8(wb_dbus_buf_t *b, uint8_t v)
{
    if (!b) return -1;
    if (buf_reserve(b, 1) != 0) return -1;
    b->buf[b->len++] = v;
    return 0;
}

int wb_dbus_append_u16(wb_dbus_buf_t *b, uint16_t v)
{
    return append_u16(b, v);
}

int wb_dbus_append_u32(wb_dbus_buf_t *b, uint32_t v)
{
    if (!b) return -1;
    if (wb_dbus_align(b, 4) != 0) return -1;
    if (buf_reserve(b, 4) != 0) return -1;
    b->buf[b->len++] = (uint8_t)(v & 0xFF);
    b->buf[b->len++] = (uint8_t)((v >> 8) & 0xFF);
    b->buf[b->len++] = (uint8_t)((v >> 16) & 0xFF);
    b->buf[b->len++] = (uint8_t)(v >> 24);
    return 0;
}

int wb_dbus_append_u64(wb_dbus_buf_t *b, uint64_t v)
{
    if (!b) return -1;
    if (wb_dbus_align(b, 8) != 0) return -1;
    if (buf_reserve(b, 8) != 0) return -1;
    for (int i = 0; i < 8; i++)
        b->buf[b->len++] = (uint8_t)(v >> (i * 8));
    return 0;
}

int wb_dbus_append_bool(wb_dbus_buf_t *b, bool v)
{
    return wb_dbus_append_u32(b, v ? 1u : 0u);
}

int wb_dbus_append_string(wb_dbus_buf_t *b, const char *s)
{
    if (!b || !s) return -1;
    size_t l = strlen(s);
    if (l > 0x7FFFFFFFu) return -1;
    if (wb_dbus_append_u32(b, (uint32_t)l) != 0) return -1;   /* 4-aligned */
    if (buf_reserve(b, l + 1) != 0) return -1;
    memcpy(b->buf + b->len, s, l);
    b->len += l;
    b->buf[b->len++] = '\0';
    return 0;
}

int wb_dbus_append_signature(wb_dbus_buf_t *b, const char *sig)
{
    if (!b || !sig) return -1;
    size_t l = strlen(sig);
    if (l > 255) return -1;
    /* a signature is BYTE-aligned: no padding, and its length is a BYTE */
    if (buf_reserve(b, l + 2) != 0) return -1;
    b->buf[b->len++] = (uint8_t)l;
    memcpy(b->buf + b->len, sig, l);
    b->len += l;
    b->buf[b->len++] = '\0';
    return 0;
}

int wb_dbus_array_begin(wb_dbus_buf_t *b)
{
    /* An array is 4-byte aligned and its element count is a UINT32 in BYTES --
     * not an element count, which is the most-misimplemented field in the
     * format.  The 4 count bytes are reserved now and filled at the end, so
     * the caller appends elements between begin() and end(). */
    if (!b) return -1;
    if (wb_dbus_align(b, 4) != 0) return -1;
    if (buf_reserve(b, 4) != 0) return -1;
    b->array_count_pos = b->len;
    b->len += 4;
    return 0;
}

int wb_dbus_array_end(wb_dbus_buf_t *b)
{
    if (!b) return -1;
    size_t count_pos = b->array_count_pos;
    /* the count covers the bytes AFTER the reserved count itself */
    size_t bytes = b->len - (count_pos + 4);
    if (bytes > 0xFFFFFFFFu) return -1;
    b->buf[count_pos + 0] = (uint8_t)(bytes & 0xFF);
    b->buf[count_pos + 1] = (uint8_t)((bytes >> 8) & 0xFF);
    b->buf[count_pos + 2] = (uint8_t)((bytes >> 16) & 0xFF);
    b->buf[count_pos + 3] = (uint8_t)(bytes >> 24);
    return 0;
}

int wb_dbus_struct_begin(wb_dbus_buf_t *b)
{
    /* A struct is 8-byte aligned regardless of its first member. */
    return wb_dbus_align(b, 8);
}

/* -- reading -------------------------------------------------------------- */

int wb_dbus_read_u8(wb_dbus_cursor_t *c, uint8_t *out)
{
    if (!c || !out || c->pos + 1 > c->len) return -1;
    *out = c->buf[c->pos++];
    return 0;
}

int wb_dbus_read_u32(wb_dbus_cursor_t *c, uint32_t *out)
{
    if (!c || !out) return -1;
    size_t pad = (4 - (c->pos % 4)) % 4;
    c->pos += pad;
    if (c->pos + 4 > c->len) return -1;
    *out = (uint32_t)c->buf[c->pos] |
           ((uint32_t)c->buf[c->pos + 1] << 8) |
           ((uint32_t)c->buf[c->pos + 2] << 16) |
           ((uint32_t)c->buf[c->pos + 3] << 24);
    c->pos += 4;
    return 0;
}

int wb_dbus_read_u64(wb_dbus_cursor_t *c, uint64_t *out)
{
    if (!c || !out) return -1;
    size_t pad = (8 - (c->pos % 8)) % 8;
    c->pos += pad;
    if (c->pos + 8 > c->len) return -1;
    uint64_t v = 0;
    for (int i = 0; i < 8; i++) v |= (uint64_t)c->buf[c->pos + i] << (i * 8);
    c->pos += 8;
    *out = v;
    return 0;
}

int wb_dbus_read_string(wb_dbus_cursor_t *c, char *out, size_t n)
{
    if (!c || !out || n == 0) return -1;
    uint32_t l;
    if (wb_dbus_read_u32(c, &l) != 0) return -1;
    if (c->pos + l + 1 > c->len) return -1;      /* bytes + NUL */
    if (l + 1 > n) return -1;
    memcpy(out, c->buf + c->pos, l);
    out[l] = '\0';
    c->pos += l + 1;                             /* skip the NUL too */
    return 0;
}
