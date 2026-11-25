/* wubu_x11_bridge_test.c -- the parse, against bytes a real client sends. */
#include "wubu_x11_bridge.h"

#include <stdio.h>
#include <string.h>

static int failures;

static void put16(uint8_t *p, uint16_t v, int lsb) {
    if (lsb) { p[0] = (uint8_t)(v & 0xff); p[1] = (uint8_t)(v >> 8); }
    else     { p[0] = (uint8_t)(v >> 8);    p[1] = (uint8_t)(v & 0xff); }
}

/* Build a setup block the way a client would: order byte, version, auth lengths,
 * then the name padded to four bytes and the data padded to four bytes. */
static size_t build(uint8_t *buf, int lsb, uint16_t major, uint16_t minor,
                    const char *name, const uint8_t *data, uint16_t data_len) {
    size_t name_len = strlen(name);
    memset(buf, 0, 64);
    buf[0] = lsb ? 'l' : 'B';
    put16(buf + 2, major, lsb);
    put16(buf + 4, minor, lsb);
    put16(buf + 6, (uint16_t)name_len, lsb);
    put16(buf + 8, data_len, lsb);
    memcpy(buf + 12, name, name_len);
    size_t off = 12 + ((name_len + 3) & ~3u);
    if (data_len) memcpy(buf + off, data, data_len);
    return off + ((data_len + 3) & ~3u);
}

#define CHECK(cond, msg) do { if (cond) printf("  [PASS] %s\n", msg); \
    else { printf("  [FAIL] %s\n", msg); failures++; } } while (0)

int main(void) {
    printf("x11 bridge: connection setup\n");
    uint8_t buf[64];
    WubuX11Setup s;

    /* 1. little-endian client, MIT-MAGIC-COOKIE-1, 16 bytes of auth data */
    const char *name = "MIT-MAGIC-COOKIE-1";
    uint8_t cookie[16];
    for (int i = 0; i < 16; i++) cookie[i] = (uint8_t)(i * 7);
    size_t n = build(buf, 1, 11, 0, name, cookie, 16);
    memset(&s, 0, sizeof s);
    CHECK(wubu_x11_parse_setup(buf, n, &s) == 0, "little-endian setup parses");
    CHECK(s.lsb_first == 1, "byte order recorded as little-endian");
    CHECK(s.major == 11 && s.minor == 0, "protocol 11.0");
    CHECK(strcmp(s.auth_name, name) == 0, "auth name preserved");
    CHECK(s.total_len == n, "consumed length matches the padded block");

    /* 2. big-endian client: the SAME bytes read the other way must give the same
     *    version -- this is the bug the parser exists to prevent */
    n = build(buf, 0, 11, 0, name, cookie, 16);
    memset(&s, 0, sizeof s);
    CHECK(wubu_x11_parse_setup(buf, n, &s) == 0, "big-endian setup parses");
    CHECK(s.lsb_first == 0, "byte order recorded as big-endian");
    CHECK(s.major == 11, "big-endian version is still 11");

    /* 3. truncated block: a half-delivered setup must be refused, not guessed */
    memset(&s, 0, sizeof s);
    CHECK(wubu_x11_parse_setup(buf, 8, &s) < 0, "truncated setup refused");
    CHECK(wubu_x11_parse_setup(buf, n - 4, &s) < 0, "short auth data refused");

    /* 4. nonsense order byte */
    uint8_t bad[16]; memset(bad, 0, sizeof bad); bad[0] = 'q';
    CHECK(wubu_x11_parse_setup(bad, sizeof bad, &s) < 0, "bad byte order refused");

    /* 5. empty auth name: not a client */
    n = build(buf, 1, 11, 0, "", NULL, 0);
    CHECK(wubu_x11_parse_setup(buf, n > 12 ? n : 12, &s) < 0, "empty auth name refused");

    printf("x11 bridge: %s (%d failure(s))\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
