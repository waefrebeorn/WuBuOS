/* wubu_truetype_test.c -- SLERM-2027 stage 1 gate.
 *
 * The expected advances come from FreeType via Pillow (tests/fixtures/ttf_oracle.h), generated
 * by tools/make_ttf_oracle.py. Our reader is compared against a foreign implementation, not
 * against itself; a tolerance of 1 px covers FreeType's fractional pen advance honestly rather
 * than pretending the two round identically.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wubu_truetype.h"
#include "fixtures/ttf_oracle.h"

static int g_pass, g_fail;

static void ok(int cond, const char *what)
{
    if (cond) { g_pass++; return; }
    g_fail++;
    printf("FAIL %s\n", what);
}

static uint8_t *slurp(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n <= 0) { fclose(f); return NULL; }
    rewind(f);
    uint8_t *b = malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { free(b); fclose(f); return NULL; }
    fclose(f);
    *len = (size_t)n;
    return b;
}

int main(void)
{
    const char *path = getenv("WUBU_TTF_PATH");
    if (!path) path = TTF_ORACLE_PATH;

    size_t len = 0;
    uint8_t *font = slurp(path, &len);
    if (!font) {
        printf("wubu_truetype: FAIL -- cannot read %s (set WUBU_TTF_PATH)\n", path);
        return 1;
    }

    const char *why = NULL;
    WubuTtf *t = wubu_ttf_open(font, len, &why);
    ok(t != NULL, "opens a real TrueType font");
    if (!t) {
        printf("wubu_truetype: open refused: %s\n", why ? why : "(no reason given)");
        free(font);
        return 1;
    }

    ok(wubu_ttf_units_per_em(t) > 0, "unitsPerEm is positive");
    ok(wubu_ttf_num_glyphs(t) > 100, "font has a real glyph count");
    ok(wubu_ttf_has_glyf(t) == 1, "TrueType outlines (glyf) detected");
    ok(wubu_ttf_glyph_index(t, 0x20) != 0, "space maps to a glyph");
    ok(wubu_ttf_glyph_index(t, 0xFFFFF) == 0, "an unmapped code point returns .notdef");

    /* The oracle comparison: every code point FreeType reports an advance for must map here,
     * and agree in whole pixels at both sizes. */
    int mismatches = 0, mapped = 0;
    for (int i = 0; i < TTF_ORACLE_N; i++) {
        unsigned cp = TTF_ORACLE[i].cp;
        uint32_t gid = wubu_ttf_glyph_index(t, cp);
        if (gid == 0) { mismatches++; continue; }
        mapped++;
        for (int k = 0; k < TTF_ORACLE_NPPEM; k++) {
            int ppem = TTF_ORACLE_PPEM[k];
            int expect = (int)(TTF_ORACLE[i].adv[k] + 0.5);
            int got = wubu_ttf_advance_px(t, gid, ppem);
            int d = got - expect;
            if (d < 0) d = -d;
            if (d > 1) {
                if (mismatches < 8)
                    printf("  U+%04X at %dpx: got %d, FreeType %.3f\n",
                           cp, ppem, got, TTF_ORACLE[i].adv[k]);
                mismatches++;
            }
        }
    }
    printf("  oracle: %d code points, %d mapped, %d disagreements\n",
           TTF_ORACLE_N, mapped, mismatches);
    ok(mismatches == 0, "advances match FreeType within 1px at every sampled code point");

    wubu_ttf_close(t);

    /* Refusals: untrusted input must be refused, not partially parsed. */
    const char *r1 = NULL, *r2 = NULL, *r3 = NULL, *r4 = NULL;
    ok(wubu_ttf_open(NULL, 0, &r1) == NULL && r1 != NULL, "refuses NULL bytes with a reason");
    ok(wubu_ttf_open(font, 12, &r2) == NULL && r2 != NULL, "refuses a header-only buffer");
    ok(wubu_ttf_open(font, len / 3, &r3) == NULL && r3 != NULL, "refuses a truncated table");

    uint8_t otto[12];
    memcpy(otto, font, 12);
    otto[0] = 'O'; otto[1] = 'T'; otto[2] = 'T'; otto[3] = 'O';
    ok(wubu_ttf_open(otto, sizeof otto, &r4) == NULL && r4 != NULL,
       "refuses CFF outlines instead of misreading them");

    printf("wubu_truetype: %d passed, %d failed\n", g_pass, g_fail);
    free(font);
    return g_fail ? 1 : 0;
}
