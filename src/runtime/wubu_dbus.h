/*
 * wubu_dbus.h -- the D-Bus wire format marshaller.
 *
 * AT-SPI2 (the accessibility protocol) rides on D-Bus, and D-Bus has a binary
 * wire format whose rules are easy to get subtly wrong.  The bugs are all
 * ALIGNMENT and LENGTH:
 *
 *   * Every type has an alignment: BYTE 1, BOOLEAN/STRING/OBJECT_PATH 4,
 *     UINT16/INT16 2, UINT32/INT32/UNIX_FD 4, UINT64/INT64/DOUBLE 8,
 *     SIGNATURE 1, STRUCT/DICT_ENTRY 8, ARRAY 4, VARIANT 8.
 *   * Padding is inserted to reach the alignment of the type being appended.
 *     Omitting it makes the peer read garbage -- or reject the message.
 *   * A STRING is a UINT32 byte count (EXCLUDING the NUL) followed by the bytes
 *     and a NUL.  Counting the NUL, or forgetting it, desynchronises the rest
 *     of the message.
 *   * A SIGNATURE is a BYTE length (excluding the NUL), the chars, and a NUL.
 *   * An ARRAY is 4-byte aligned and preceded by a UINT32 ELEMENT COUNT IN
 *     BYTES -- not an element count.  This is the single most-misimplemented
 *     field in the format.
 *   * A STRUCT and a DICT_ENTRY are 8-byte aligned even when their first
 *     member needs less.
 *   * A message body's FIRST field is aligned to 8, not to 4.
 *
 * Opaque structs, C11, minimal includes.
 */
#ifndef WUBU_DBUS_H
#define WUBU_DBUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *buf;
    size_t   len;
    size_t   cap;
    size_t   array_count_pos;   /* where the reserved array byte-count lives */
} wb_dbus_buf_t;

void wb_dbus_buf_init(wb_dbus_buf_t *b);
void wb_dbus_buf_free(wb_dbus_buf_t *b);

/* Reset the write position, keeping the allocation. */
void wb_dbus_buf_reset(wb_dbus_buf_t *b);

/* The alignment of a D-Bus type code. */
uint32_t wb_dbus_align_of(char type_code);

/* Append raw bytes with a required alignment (padding inserted). */
int wb_dbus_align(wb_dbus_buf_t *b, uint32_t alignment);

int wb_dbus_append_u8(wb_dbus_buf_t *b, uint8_t v);
int wb_dbus_append_u16(wb_dbus_buf_t *b, uint16_t v);
int wb_dbus_append_u32(wb_dbus_buf_t *b, uint32_t v);
int wb_dbus_append_u64(wb_dbus_buf_t *b, uint64_t v);
int wb_dbus_append_bool(wb_dbus_buf_t *b, bool v);

/* A string: UINT32 length (excluding the NUL), bytes, NUL. */
int wb_dbus_append_string(wb_dbus_buf_t *b, const char *s);

/* A signature: BYTE length (excluding the NUL), chars, NUL. */
int wb_dbus_append_signature(wb_dbus_buf_t *b, const char *sig);

/* An array: 4-byte align, UINT32 byte count, then the elements.  The caller
 * appends the elements between wb_dbus_array_begin() and
 * wb_dbus_array_end(), which writes the byte count. */
int wb_dbus_array_begin(wb_dbus_buf_t *b);
int wb_dbus_array_end(wb_dbus_buf_t *b);

/* A struct or dict entry: 8-byte align. */
int wb_dbus_struct_begin(wb_dbus_buf_t *b);

/* Read side: a cursor that enforces the same alignment on the way in. */
typedef struct {
    const uint8_t *buf;
    size_t   len;
    size_t   pos;
} wb_dbus_cursor_t;

int wb_dbus_read_u8(wb_dbus_cursor_t *c, uint8_t *out);
int wb_dbus_read_u32(wb_dbus_cursor_t *c, uint32_t *out);
int wb_dbus_read_u64(wb_dbus_cursor_t *c, uint64_t *out);
int wb_dbus_read_string(wb_dbus_cursor_t *c, char *out, size_t n);

#endif /* WUBU_DBUS_H */
