/* wubu_x11_request_test.c -- the request header and the replies, byte by byte. */
#include "wubu_x11_request.h"

#include <stdio.h>
#include <string.h>

static int failures;
#define CHECK(c, m) do { if (c) printf("  [PASS] %s\n", m); else { printf("  [FAIL] %s\n", m); failures++; } } while (0)

int main(void) {
    printf("x11 requests\n");
    uint8_t buf[64]; memset(buf, 0, sizeof buf);
    WubuX11Request r; size_t need = 0;

    /* a real QueryExtension: opcode 98, data 0, length 5 words (20 bytes) */
    buf[0] = 98; buf[1] = 0; buf[2] = 5; buf[3] = 0;
    CHECK(wubu_x11_parse_request(buf, 4, &r, &need, 1) == 0, "header parses");
    CHECK(r.opcode == 98, "opcode is QueryExtension");
    CHECK(need == 20, "length is in 4-byte words (5 -> 20 bytes)");

    /* the same header read big-endian must give the same length */
    buf[2] = 0; buf[3] = 5;
    CHECK(wubu_x11_parse_request(buf, 4, &r, &need, 0) == 0 && need == 20,
          "big-endian header gives the same length");

    /* a zero-length request is malformed, not empty */
    buf[2] = 0; buf[3] = 0;
    CHECK(wubu_x11_parse_request(buf, 4, &r, &need, 1) < 0, "zero-length request refused");
    CHECK(wubu_x11_parse_request(buf, 3, &r, &need, 1) < 0, "truncated header refused");

    uint8_t out[128];

    int n = wubu_x11_reply_query_extension(out, sizeof out, 0x1234, 0, 1);
    CHECK(n == 32, "QueryExtension reply is 32 bytes");
    CHECK(out[0] == 1, "reply byte 0 is 1");
    CHECK(out[1] == 0, "present flag clear for an unsupported extension");
    CHECK(out[2] == 0x34 && out[3] == 0x12, "sequence echoed little-endian");

    const char *names[] = { "BIG-REQUESTS", "XKEYBOARD" };
    n = wubu_x11_reply_list_extensions(out, sizeof out, 7, names, 2, 1);
    CHECK(n > 0, "ListExtensions encodes");
    CHECK(out[1] == 2, "two extensions");
    CHECK(out[32] == 12, "first name length is 12, at byte 32 (after 24 unused)");
    CHECK(memcmp(out + 34, "BIG-REQUESTS", 12) == 0, "first name is BIG-REQUESTS");
    CHECK(n % 4 == 0, "reply length is a multiple of four");

    n = wubu_x11_reply_atom(out, sizeof out, 9, 42, 1);
    CHECK(n == 32 && out[8] == 42, "InternAtom reply carries the atom id");

    n = wubu_x11_reply_atom_name(out, sizeof out, 9, "WM_NAME", 1);
    CHECK(n == 40, "GetAtomName reply is header + padded name");
    CHECK(out[8] == 7 && out[9] == 0, "name length is a CARD16");
    CHECK(memcmp(out + 32, "WM_NAME", 7) == 0, "name follows the fixed header");

    n = wubu_x11_reply_geometry(out, sizeof out, 3, 0x00400001, 0, 0, 1024, 768, 0, 1);
    CHECK(n == 32, "GetGeometry reply is 32 bytes");
    CHECK(out[1] == 24, "depth is reported in byte 1");
    CHECK(out[8] == 0x01 && out[9] == 0x00 && out[10] == 0x40 && out[11] == 0x00,
          "root window id starts at byte 8 (after the 8-byte header)");

    n = wubu_x11_reply_input_focus(out, sizeof out, 5, 1, 1);
    CHECK(n == 32 && out[1] == 1, "GetInputFocus reports PointerRoot");
    CHECK(out[8] == 1, "focus id in the reply body");

    /* the small fixed replies: 32 bytes each, sequence echoed, values present */
    n = wubu_x11_reply_best_size(out, sizeof out, 11, 1024, 768, 1);
    CHECK(n == 32 && out[8] == 0x00 && out[9] == 0x04, "QueryBestSize returns the size asked");
    n = wubu_x11_reply_screen_saver(out, sizeof out, 12, 1);
    CHECK(n == 32 && out[1] == 1, "GetScreenSaver reports blanking preferred");
    n = wubu_x11_reply_list_hosts(out, sizeof out, 13, 1);
    CHECK(n == 32 && out[1] == 0, "ListHosts: no hosts, mode Disable");
    n = wubu_x11_reply_pointer_control(out, sizeof out, 14, 1);
    CHECK(n == 32 && out[8] == 2 && out[9] == 0, "GetPointerControl accel num/den");

    /* the reply/no-reply table: answering a no-reply request with an error aborts
     * a client that did nothing wrong */
    CHECK(wubu_x11_op_expects_reply(98) == 1, "QueryExtension expects a reply");
    CHECK(wubu_x11_op_expects_reply(20) == 1, "GetProperty expects a reply");
    CHECK(wubu_x11_op_expects_reply(55) == 0, "CreateGC expects NO reply");
    CHECK(wubu_x11_op_expects_reply(8) == 0, "MapWindow expects NO reply");

    printf("x11 requests: %s (%d failure(s))\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
