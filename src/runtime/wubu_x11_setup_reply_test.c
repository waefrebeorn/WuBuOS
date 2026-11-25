/* wubu_x11_setup_reply_test.c -- the reply, read back with an INDEPENDENT reader.
 *
 * The test deliberately does not reuse the encoder's writer: a test that shares
 * the code under test can only prove the code agrees with itself. Every offset
 * below comes from the protocol layout, not from the implementation. */
#include "wubu_x11_setup_reply.h"

#include <stdio.h>
#include <string.h>

static int failures;

static uint16_t r16(const uint8_t *p, int lsb) {
    return lsb ? (uint16_t)(p[0] | (p[1] << 8)) : (uint16_t)((p[0] << 8) | p[1]);
}
static uint32_t r32(const uint8_t *p, int lsb) {
    return lsb ? (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24))
               : (uint32_t)((p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3]);
}
#define CHECK(c, m) do { if (c) printf("  [PASS] %s\n", m); else { printf("  [FAIL] %s\n", m); failures++; } } while (0)

static void check_reply(const uint8_t *p, int n, int lsb, const char *label) {
    char msg[128];
    printf("  -- %s --\n", label);
    CHECK(n > 0, "encoder produced bytes");
    if (n <= 0) return;
    CHECK(p[0] == 1, "status is Success");
    CHECK(r16(p + 2, lsb) == 11 && r16(p + 4, lsb) == 0, "protocol 11.0");
    uint16_t words = r16(p + 6, lsb);
    CHECK((size_t)n == 8 + (size_t)words * 4, "declared length matches the bytes written");

    const uint8_t *a = p + 8;
    CHECK(r32(a + 0, lsb) == 1, "release number 1");
    CHECK(r32(a + 4, lsb) == 0x00400000, "resource id base");
    CHECK(r32(a + 12, lsb) == 256, "motion buffer size (CARD32)");
    uint32_t vendor_len = r16(a + 16, lsb);          /* CARD16, not CARD32 */
    CHECK(vendor_len == strlen("WuBuOS X11 bridge"), "vendor length is a CARD16");
    CHECK(r16(a + 18, lsb) == 65535, "max request length is a CARD16");
    CHECK(a[20] == 1, "one screen");
    CHECK(a[21] == 2, "two pixmap formats");
    CHECK(a[22] == (lsb ? 0 : 1), "image byte order follows the client");
    CHECK(a[24] == 32 && a[25] == 32, "bitmap scanline unit/pad are 32");
    CHECK(a[26] == 8 && a[27] == 255, "keycode range 8..255");

    /* vendor is padded to 4 from the START of the message: 8 + 32 = 40 */
    const char *v = (const char *)(p + 40);
    snprintf(msg, sizeof msg, "vendor string reads \"%.*s\"", (int)vendor_len, v);
    CHECK(strncmp(v, "WuBuOS X11 bridge", vendor_len) == 0, msg);

    /* pixmap formats start at 40 + padded(vendor) */
    size_t pf = 40 + ((vendor_len + 3) & ~3u);
    CHECK(p[pf + 0] == 1 && p[pf + 1] == 1, "pixmap format 1: depth 1, bpp 1");
    CHECK(p[pf + 8 + 0] == 24 && p[pf + 8 + 1] == 32, "pixmap format 2: depth 24, bpp 32");

    const uint8_t *s = p + pf + 16;
    CHECK(r32(s + 0, lsb) == 0x200001, "root window id");
    CHECK(r16(s + 20, lsb) == 1024 && r16(s + 22, lsb) == 768, "screen is 1024x768");
    CHECK(r32(s + 32, lsb) == 0x21, "root visual id");
    CHECK(s[38] == 24, "root depth 24");
    CHECK(s[39] == 1, "one allowed depth");
    /* depth block then the visual */
    CHECK(s[40] == 24, "depth entry is 24");
    CHECK(r16(s + 42, lsb) == 1, "one visual at that depth");
    const uint8_t *vis = s + 48;
    CHECK(r32(vis + 0, lsb) == 0x21, "visual id matches the root visual");
    CHECK(vis[4] == 4, "visual class TrueColor");
    CHECK(vis[5] == 8, "8 bits per rgb");
    CHECK(r32(vis + 8, lsb) == 0xff0000 && r32(vis + 12, lsb) == 0x00ff00 &&
          r32(vis + 16, lsb) == 0x0000ff, "RGB masks are r,g,b");
}

int main(void) {
    printf("x11 bridge: setup reply\n");
    WubuX11ScreenCfg cfg = { 1024, 768, 271, 203, 24, 0x200001, 0x21, 0x20 };
    uint8_t buf[WUBU_X11_REPLY_MAX];

    int n = wubu_x11_build_setup_reply(buf, sizeof buf, &cfg, 1);
    check_reply(buf, n, 1, "little-endian client");

    int m = wubu_x11_build_setup_reply(buf, sizeof buf, &cfg, 0);
    check_reply(buf, m, 0, "big-endian client");
    CHECK(n == m, "same size in both byte orders");

    uint8_t tiny[16];
    CHECK(wubu_x11_build_setup_reply(tiny, sizeof tiny, &cfg, 1) < 0,
          "a buffer too small is refused, not overflowed");

    printf("x11 setup reply: %s (%d failure(s))\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
