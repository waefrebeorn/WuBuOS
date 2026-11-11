/*
 * wubu_dbus_test.c -- the D-Bus wire format marshaller.
 *
 * The load-bearing rules: every type has an ALIGNMENT and padding is inserted
 * to reach it; a STRING's UINT32 is the byte count EXCLUDING the NUL; and an
 * ARRAY's UINT32 is the element count IN BYTES, not an element count.
 */
#include "wubu_dbus.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (!(c)) { printf("  FAIL: %s\n", m); failures++; } } while (0)

int main(void)
{
    printf("=== test_wubu_dbus ===\n");
    wb_dbus_buf_t b;
    wb_dbus_buf_init(&b);

    /* -- alignment of each type code -- */
    CHECK(wb_dbus_align_of('y') == 1, "BYTE aligns to 1");
    CHECK(wb_dbus_align_of('u') == 4, "UINT32 aligns to 4");
    CHECK(wb_dbus_align_of('s') == 4, "STRING aligns to 4");
    CHECK(wb_dbus_align_of('g') == 4, "SIGNATURE aligns to 4 (the length is a BYTE)");
    CHECK(wb_dbus_align_of('q') == 2, "UINT16 aligns to 2");
    CHECK(wb_dbus_align_of('t') == 8, "UINT64 aligns to 8");
    CHECK(wb_dbus_align_of('d') == 8, "DOUBLE aligns to 8");
    CHECK(wb_dbus_align_of('a') == 4, "ARRAY aligns to 4");
    CHECK(wb_dbus_align_of('(') == 8, "STRUCT aligns to 8");
    CHECK(wb_dbus_align_of('v') == 8, "VARIANT aligns to 8");

    /* -- padding is inserted to reach the alignment -- */
    wb_dbus_buf_reset(&b);
    CHECK(wb_dbus_append_u8(&b, 0xAA) == 0, "a byte is appended");
    CHECK(b.len == 1, "and takes 1 byte");
    CHECK(wb_dbus_append_u32(&b, 0x11223344u) == 0, "a u32 after a byte");
    CHECK(b.len == 8, "pads to 4 then writes 4 (1 + 3 + 4)");
    CHECK(b.buf[0] == 0xAA && b.buf[1] == 0 && b.buf[2] == 0 && b.buf[3] == 0,
          "the padding is zero and BEFORE the value");
    CHECK(b.buf[4] == 0x44 && b.buf[5] == 0x33 && b.buf[6] == 0x22 && b.buf[7] == 0x11,
          "the u32 is little-endian");

    /* -- u64 needs 8-byte alignment -- */
    wb_dbus_buf_reset(&b);
    wb_dbus_append_u32(&b, 1);
    wb_dbus_append_u64(&b, 0x0102030405060708ULL);
    CHECK(b.len == 16, "a u32 then a u64: 4 + 4 pad + 8");
    CHECK(b.buf[4] == 0 && b.buf[5] == 0 && b.buf[6] == 0 && b.buf[7] == 0,
          "the padding lands before the u64");

    /* -- strings: UINT32 length (excluding NUL), bytes, NUL -- */
    wb_dbus_buf_reset(&b);
    CHECK(wb_dbus_append_string(&b, "hi") == 0, "a string is appended");
    CHECK(b.len == 4 + 2 + 1, "4 length + 2 bytes + 1 NUL");
    CHECK(b.buf[0] == 2, "the length EXCLUDES the NUL");
    CHECK(b.buf[4] == 'h' && b.buf[5] == 'i' && b.buf[6] == '\0',
          "the bytes are followed by a NUL");
    CHECK(wb_dbus_append_string(&b, "") == 0, "an empty string is appended");
    CHECK(b.buf[7] == 0, "with a zero length");

    /* -- signatures: BYTE length, chars, NUL -- */
    wb_dbus_buf_reset(&b);
    CHECK(wb_dbus_append_signature(&b, "ss") == 0, "a signature is appended");
    CHECK(b.len == 1 + 2 + 1, "1 length + 2 chars + 1 NUL");
    CHECK(b.buf[0] == 2, "the length is a BYTE and excludes the NUL");
    CHECK(b.buf[1] == 's' && b.buf[2] == 's' && b.buf[3] == '\0', "the chars then a NUL");

    /* -- a struct is 8-byte aligned even when its first member needs less -- */
    wb_dbus_buf_reset(&b);
    wb_dbus_append_u8(&b, 1);
    CHECK(wb_dbus_struct_begin(&b) == 0, "a struct begins");
    CHECK(b.len == 8, "and pads to 8 for a first member that needs only 1");

    /* -- an array's count is in BYTES, not elements -- */
    wb_dbus_buf_reset(&b);
    CHECK(wb_dbus_array_begin(&b) == 0, "an array begins");
    CHECK(b.len == 4, "reserving the 4 count bytes");
    wb_dbus_append_u32(&b, 10);
    wb_dbus_append_u32(&b, 20);
    CHECK(wb_dbus_array_end(&b) == 0, "the array ends");
    CHECK(b.len == 12, "4 count + 8 elements");
    CHECK(b.buf[0] == 8 && b.buf[1] == 0 && b.buf[2] == 0 && b.buf[3] == 0,
          "the count is 8 BYTES (two u32s), NOT 2 ELEMENTS");
    CHECK(b.buf[4] == 10 && b.buf[8] == 20, "and the elements follow the count");

    /* -- an empty array has a zero count -- */
    wb_dbus_buf_reset(&b);
    wb_dbus_array_begin(&b);
    wb_dbus_array_end(&b);
    CHECK(b.len == 4 && b.buf[0] == 0, "an empty array is a zero count");

    /* -- a bool is a u32 -- */
    wb_dbus_buf_reset(&b);
    wb_dbus_append_bool(&b, true);
    CHECK(b.len == 4 && b.buf[0] == 1, "true is the u32 1");
    wb_dbus_buf_reset(&b);
    wb_dbus_append_bool(&b, false);
    CHECK(b.len == 4 && b.buf[0] == 0, "false is the u32 0");

    /* -- reading back: the same alignment rules -- */
    {
        wb_dbus_buf_reset(&b);
        wb_dbus_append_u8(&b, 0xEE);
        wb_dbus_append_u32(&b, 0xDEADBEEF);
        wb_dbus_append_string(&b, "ok");

        wb_dbus_cursor_t c = { b.buf, b.len, 0 };
        uint8_t u8; uint32_t u32; char str[64];
        CHECK(wb_dbus_read_u8(&c, &u8) == 0 && u8 == 0xEE, "read the byte back");
        CHECK(wb_dbus_read_u32(&c, &u32) == 0 && u32 == 0xDEADBEEFu,
              "read the u32 back (alignment enforced on read too)");
        CHECK(c.pos == 8, "the cursor skipped the padding");
        CHECK(wb_dbus_read_string(&c, str, sizeof(str)) == 0 && strcmp(str, "ok") == 0,
              "and the string, including its NUL");
        CHECK(c.pos == b.len, "the cursor is at the end");
    }

    /* -- a read past the end is refused -- */
    {
        wb_dbus_cursor_t c = { b.buf, 1, 0 };
        uint32_t u32;
        CHECK(wb_dbus_read_u32(&c, &u32) != 0, "a truncated read is refused");
        wb_dbus_cursor_t c2 = { NULL, 0, 0 };
        uint8_t u8;
        CHECK(wb_dbus_read_u8(&c2, &u8) != 0, "a null buffer is refused");
        CHECK(wb_dbus_read_u64(&c2, NULL) != 0, "a null out pointer is refused");
        CHECK(wb_dbus_read_string(&c2, NULL, 0) != 0, "a null string out is refused");
    }

    /* -- bad arguments -- */
    CHECK(wb_dbus_align(NULL, 4) != 0, "a null buffer is refused");
    CHECK(wb_dbus_append_u8(NULL, 1) != 0, "a null append is refused");
    CHECK(wb_dbus_append_string(&b, NULL) != 0, "a null string is refused");
    CHECK(wb_dbus_append_signature(&b, NULL) != 0, "a null signature is refused");
    CHECK(wb_dbus_append_u32(&b, 1) != -1, "sanity: appends still work");

    wb_dbus_buf_free(&b);
    wb_dbus_buf_free(NULL);

    if (failures == 0) printf("ALL WUBU_DBUS TESTS PASSED\n");
    else printf("%d WUBU_DBUS FAILURES\n", failures);
    return failures ? 1 : 0;
}
