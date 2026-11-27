/* wubu_utf8.c -- see wubu_utf8.h. Table-free; ranges are checked, not guessed. */
#include "wubu_utf8.h"

size_t wubu_utf8_decode(const char *s, size_t len, unsigned *cp) {
    if (!s || len == 0) { if (cp) *cp = WUBU_UTF8_REPLACEMENT; return 0; }
    const unsigned char *p = (const unsigned char *)s;
    unsigned b0 = p[0];
    if (b0 < 0x80) { if (cp) *cp = b0; return 1; }

    size_t need;
    unsigned v, min;
    if ((b0 & 0xE0) == 0xC0)      { need = 1; v = b0 & 0x1Fu; min = 0x80; }
    else if ((b0 & 0xF0) == 0xE0) { need = 2; v = b0 & 0x0Fu; min = 0x800; }
    else if ((b0 & 0xF8) == 0xF0) { need = 3; v = b0 & 0x07u; min = 0x10000; }
    else { if (cp) *cp = WUBU_UTF8_REPLACEMENT; return 0; }   /* continuation or 0xF8+ */

    if (len <= need) { if (cp) *cp = WUBU_UTF8_REPLACEMENT; return 0; }
    for (size_t i = 1; i <= need; i++) {
        if ((p[i] & 0xC0) != 0x80) { if (cp) *cp = WUBU_UTF8_REPLACEMENT; return 0; }
        v = (v << 6) | (p[i] & 0x3Fu);
    }
    if (v < min) { if (cp) *cp = WUBU_UTF8_REPLACEMENT; return 0; }             /* overlong */
    if (v > 0x10FFFFu) { if (cp) *cp = WUBU_UTF8_REPLACEMENT; return 0; }       /* beyond Unicode */
    if (v >= 0xD800u && v <= 0xDFFFu) { if (cp) *cp = WUBU_UTF8_REPLACEMENT; return 0; } /* surrogate */
    if (cp) *cp = v;
    return need + 1;
}

size_t wubu_utf8_encode(unsigned cp, char out[4]) {
    if (cp >= 0xD800u && cp <= 0xDFFFu) return 0;
    if (cp > 0x10FFFFu) return 0;
    if (cp < 0x80u) { out[0] = (char)cp; return 1; }
    if (cp < 0x800u) {
        out[0] = (char)(0xC0u | (cp >> 6));
        out[1] = (char)(0x80u | (cp & 0x3Fu));
        return 2;
    }
    if (cp < 0x10000u) {
        out[0] = (char)(0xE0u | (cp >> 12));
        out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
        out[2] = (char)(0x80u | (cp & 0x3Fu));
        return 3;
    }
    out[0] = (char)(0xF0u | (cp >> 18));
    out[1] = (char)(0x80u | ((cp >> 12) & 0x3Fu));
    out[2] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
    out[3] = (char)(0x80u | (cp & 0x3Fu));
    return 4;
}

int wubu_utf8_valid(const char *s, size_t len) {
    size_t i = 0;
    while (i < len) {
        size_t n = wubu_utf8_decode(s + i, len - i, NULL);
        if (n == 0) return 0;
        i += n;
    }
    return 1;
}

size_t wubu_utf8_count(const char *s, size_t len) {
    size_t i = 0, n = 0;
    while (i < len) {
        size_t k = wubu_utf8_decode(s + i, len - i, NULL);
        i += k ? k : 1;
        n++;
    }
    return n;
}

int wubu_utf8_width(unsigned cp) {
    if (cp < 0x20u || cp == 0x7Fu) return 0;                       /* control */
    if ((cp >= 0x0300u && cp <= 0x036Fu) ||                        /* combining diacritics */
        (cp >= 0x0483u && cp <= 0x0489u) ||
        (cp >= 0x0591u && cp <= 0x05BDu) ||
        (cp >= 0x0610u && cp <= 0x061Au) ||
        (cp >= 0x064Bu && cp <= 0x065Fu) ||
        (cp >= 0x0670u && cp <= 0x0670u) ||
        (cp >= 0x06D6u && cp <= 0x06DCu) ||
        (cp >= 0x20D0u && cp <= 0x20F0u) ||
        (cp >= 0xFE20u && cp <= 0xFE2Fu)) return 0;                /* zero width */
    if ((cp >= 0x1100u && cp <= 0x115Fu) ||                        /* Hangul Jamo */
        (cp >= 0x2E80u && cp <= 0x303Eu) ||                        /* CJK radicals, punctuation */
        (cp >= 0x3041u && cp <= 0x33FFu) ||                        /* kana, CJK compat */
        (cp >= 0x3400u && cp <= 0x4DBFu) ||                        /* CJK ext A */
        (cp >= 0x4E00u && cp <= 0x9FFFu) ||                        /* CJK unified */
        (cp >= 0xA000u && cp <= 0xA4CFu) ||                        /* Yi */
        (cp >= 0xAC00u && cp <= 0xD7A3u) ||                        /* Hangul syllables */
        (cp >= 0xF900u && cp <= 0xFAFFu) ||                        /* CJK compat ideographs */
        (cp >= 0xFE30u && cp <= 0xFE4Fu) ||                        /* CJK compat forms */
        (cp >= 0xFF00u && cp <= 0xFF60u) ||                        /* fullwidth forms */
        (cp >= 0xFFE0u && cp <= 0xFFE6u) ||
        (cp >= 0x1F300u && cp <= 0x1F64Fu) ||                      /* emoji */
        (cp >= 0x1F900u && cp <= 0x1F9FFu) ||
        (cp >= 0x20000u && cp <= 0x3FFFDu)) return 2;              /* wide */
    return 1;
}

size_t wubu_utf8_fit(const char *s, size_t len, int max_cols) {
    if (!s || len == 0 || max_cols <= 0) return 0;
    size_t i = 0;
    int cols = 0, first = 1;
    while (i < len) {
        unsigned cp;
        size_t n = wubu_utf8_decode(s + i, len - i, &cp);
        int w;
        if (n == 0) { n = 1; cp = WUBU_UTF8_REPLACEMENT; w = 1; }
        else w = wubu_utf8_width(cp);
        if (!first && cols + w > max_cols) break;
        cols += w;
        i += n;
        first = 0;
    }
    return i;
}

unsigned wubu_utf8_tolower(unsigned cp) {
    if (cp >= 'A' && cp <= 'Z') return cp + 32;
    if (cp >= 0xC0u && cp <= 0xDEu && cp != 0xD7u) return cp + 32;         /* Latin-1 */
    if (cp >= 0x100u && cp <= 0x137u && (cp % 2) == 0) return cp + 1;      /* Latin Ext-A even */
    if (cp >= 0x139u && cp <= 0x148u && (cp % 2) == 1) return cp + 1;
    if (cp >= 0x14Au && cp <= 0x177u && (cp % 2) == 0) return cp + 1;
    if (cp == 0x178u) return 0xFFu;
    if (cp >= 0x179u && cp <= 0x17Eu && (cp % 2) == 1) return cp + 1;
    if (cp >= 0x391u && cp <= 0x3ABu && cp != 0x3A2u) return cp + 32;      /* Greek */
    if (cp >= 0x410u && cp <= 0x42Fu) return cp + 32;                      /* Cyrillic */
    if (cp >= 0x400u && cp <= 0x40Fu) return cp + 80;
    return cp;
}

unsigned wubu_utf8_toupper(unsigned cp) {
    if (cp >= 'a' && cp <= 'z') return cp - 32;
    if (cp >= 0xE0u && cp <= 0xFEu && cp != 0xF7u) return cp - 32;
    if (cp == 0xFFu) return 0x178u;
    if (cp >= 0x100u && cp <= 0x137u && (cp % 2) == 1) return cp - 1;
    if (cp >= 0x139u && cp <= 0x148u && (cp % 2) == 0) return cp - 1;
    if (cp >= 0x14Au && cp <= 0x177u && (cp % 2) == 1) return cp - 1;
    if (cp >= 0x179u && cp <= 0x17Eu && (cp % 2) == 0) return cp - 1;
    if (cp >= 0x3B1u && cp <= 0x3CBu && cp != 0x3C2u) return cp - 32;
    if (cp == 0x3C2u) return 0x3A3u;
    if (cp >= 0x430u && cp <= 0x44Fu) return cp - 32;
    if (cp >= 0x450u && cp <= 0x45Fu) return cp - 80;
    return cp;
}
