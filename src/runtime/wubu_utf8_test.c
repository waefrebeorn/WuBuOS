/* wubu_utf8_test.c -- the slerm gate for SLERM-2011 (UTF-8).
 *
 * Two halves, and the second is the one that matters: every INVALID encoding of a
 * codepoint must be refused by name -- overlong, surrogate, beyond U+10FFFF,
 * stray continuation, truncated. A decoder that accepts overlong forms is how the
 * same character gets two spellings, one of which bypasses a filter.
 */
#include "wubu_utf8.h"
#include <stdio.h>
#include <string.h>

static int pass, fail;

typedef struct { const char *name; unsigned char bytes[4]; size_t avail; size_t consumed; unsigned cp; } Dec;
typedef struct { unsigned char bytes[4]; size_t avail; } Inv;

static const Dec DECODES[] = {
    { "A", { 0x41 }, 1, 1, 0x41 },
    { "é", { 0xc3, 0xa9 }, 2, 2, 0xe9 },
    { "€", { 0xe2, 0x82, 0xac }, 3, 3, 0x20ac },
    { "𝄞", { 0xf0, 0x9d, 0x84, 0x9e }, 4, 4, 0x1d11e },
    { "😀", { 0xf0, 0x9f, 0x98, 0x80 }, 4, 4, 0x1f600 },
};

static const Inv INVALID[] = {
    { { 0xc0, 0x80 }, 2 },
    { { 0xe0, 0x80, 0xaf }, 3 },
    { { 0xed, 0xa0, 0x80 }, 3 },
    { { 0xf4, 0x90, 0x80, 0x80 }, 4 },
    { { 0x80 }, 1 },
    { { 0xc3 }, 1 },
    { { 0xf0, 0x9d, 0x84 }, 3 },
};

static void decodes(void) {
    for (size_t i = 0; i < sizeof DECODES / sizeof DECODES[0]; i++) {
        const Dec *d = &DECODES[i];
        unsigned cp = 0;
        size_t n = wubu_utf8_decode((const char *)d->bytes, d->avail, &cp);
        if (n == d->consumed && cp == d->cp) pass++;
        else { printf("  [FAIL] decode %-6s got n=%zu cp=U+%04X want n=%zu cp=U+%04X\n",
                       d->name, n, cp, d->consumed, d->cp); fail++; }
    }
}

static void refuses(void) {
    for (size_t i = 0; i < sizeof INVALID / sizeof INVALID[0]; i++) {
        unsigned cp = 0;
        size_t n = wubu_utf8_decode((const char *)INVALID[i].bytes, INVALID[i].avail, &cp);
        if (n == 0 && cp == WUBU_UTF8_REPLACEMENT) pass++;
        else { printf("  [FAIL] invalid seq #%zu accepted: n=%zu cp=U+%04X\n", i, n, cp); fail++; }
    }
}

int main(void) {
    printf("=== wubu_utf8 (slerm gate, SLERM-2011) ===\n");
    decodes();
    refuses();

    /* round trip: encode then decode must return the same codepoint */
    { static const unsigned cps[] = { 0x41, 0xE9, 0x20AC, 0x1D11E, 0x10FFFF, 0x7FF };
      for (size_t i = 0; i < sizeof cps / sizeof cps[0]; i++) {
          char buf[4]; unsigned back = 0;
          size_t n = wubu_utf8_encode(cps[i], buf);
          size_t m = wubu_utf8_decode(buf, n, &back);
          if (n && m == n && back == cps[i]) pass++;
          else { printf("  [FAIL] roundtrip U+%04X\n", cps[i]); fail++; }
      } }

    /* surrogates and beyond-Unicode cannot be encoded at all */
    { char buf[4];
      if (wubu_utf8_encode(0xD800, buf) == 0 && wubu_utf8_encode(0x110000, buf) == 0) pass++;
      else { printf("  [FAIL] encoder accepted a surrogate or >U+10FFFF\n"); fail++; } }

    /* validity of whole buffers */
    { const char *bad = "ab\xC3" "(" "cd";   /* split so \x28 is not read as \x28c */
      if (wubu_utf8_valid("héllo", strlen("héllo")) && !wubu_utf8_valid(bad, 6)) pass++;
      else { printf("  [FAIL] whole-buffer validity\n"); fail++; } }

    /* counting never lies low: an invalid byte counts as one character */
    { if (wubu_utf8_count("héllo", 6) == 5 && wubu_utf8_count("\x80\x80", 2) == 2) pass++;
      else { printf("  [FAIL] count (%zu, %zu)\n", wubu_utf8_count("héllo", 6), wubu_utf8_count("\x80\x80", 2)); fail++; } }

    /* width: combining 0, CJK and emoji 2, ASCII 1, control 0 */
    { if (wubu_utf8_width(0x0301) == 0 && wubu_utf8_width(0x4E16) == 2 &&
          wubu_utf8_width(0x1F600) == 2 && wubu_utf8_width('a') == 1 &&
          wubu_utf8_width(0x0A) == 0) pass++;
      else { printf("  [FAIL] width table\n"); fail++; } }

    /* fit: never split a character, never exceed the cell budget */
    { size_t a = wubu_utf8_fit("世界你好", strlen("世界你好"), 5);   /* 2+2 fits, 6 does not */
      size_t b = wubu_utf8_fit("héllo", 6, 3);                      /* h + é + l = 3 cells, 4 bytes */
      size_t c = wubu_utf8_fit("世界", strlen("世界"), 1);           /* must still progress */
      if (a == 6 && b == 4 && c == 3) pass++;
      else { printf("  [FAIL] fit a=%zu b=%zu c=%zu\n", a, b, c); fail++; } }

    /* case mapping, including the pairs that are not +-32 */
    { if (wubu_utf8_tolower('A') == 'a' && wubu_utf8_tolower(0xC9) == 0xE9 &&
          wubu_utf8_tolower(0x3A3) == 0x3C3 && wubu_utf8_tolower(0x416) == 0x436 &&
          wubu_utf8_toupper(0xFF) == 0x178 && wubu_utf8_toupper(0x3C2) == 0x3A3 &&
          wubu_utf8_tolower(0x1F600) == 0x1F600) pass++;
      else { printf("  [FAIL] case mapping\n"); fail++; } }

    printf("\n=== %d passed, %d failed ===\n", pass, fail);
    return fail ? 1 : 0;
}
