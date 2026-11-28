/* wubu_truetype_selawik_test.c -- acceptance probe for the vendored reference face.
 *
 * The vendored-fill rule says every third-party artifact ships with its own probe, and this is
 * Selawik's: our reader must open the file we actually ship (not DejaVu, not a sample) and agree
 * with FreeType on its advances and its ink.
 *
 * This probe is also the reason the face was worth vendoring at all: Selawik uses the SHORT hmtx
 * form (numberOfHMetrics < numGlyphs, with a trailing 2-byte lsb array), which the reader's
 * original bounds check refused outright. DejaVu carries a metric for every glyph, so nothing we
 * had could have caught it.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wubu_truetype.h"
#include "fixtures/selawik_oracle.h"

static int passed, failed;

static void ok(int cond, const char *what)
{
    if (cond) passed++;
    else { failed++; printf("  FAIL %s\n", what); }
}

static uint8_t *slurp(const char *path, size_t *n)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    rewind(f);
    if (sz <= 0) { fclose(f); return NULL; }
    uint8_t *b = malloc((size_t)sz);
    if (!b) { fclose(f); return NULL; }
    size_t got = fread(b, 1, (size_t)sz, f);
    fclose(f);
    if (got != (size_t)sz) { free(b); return NULL; }
    *n = got;
    return b;
}

int main(void)
{
    size_t n = 0;
    uint8_t *data = slurp(TTF_ORACLE_PATH, &n);
    if (!data) { printf("  FAIL cannot read %s\n", TTF_ORACLE_PATH); return 1; }

    const char *why = NULL;
    WubuTtf *t = wubu_ttf_open(data, n, &why);
    ok(t != NULL, "opens the shipped Selawik");
    if (!t) { printf("  reason: %s\n", why ? why : "(none)"); free(data); return 1; }

    unsigned upem = wubu_ttf_units_per_em(t);
    unsigned ng = wubu_ttf_num_glyphs(t);
    ok(upem >= 1000 && upem <= 4096, "unitsPerEm is in a sane range");
    ok(ng > 200, "has a real glyph repertoire");

    int missing = 0;
    for (unsigned c = 0x20; c < 0x7F; c++)
        if (!wubu_ttf_glyph_index(t, c)) missing++;
    ok(missing == 0, "maps all 95 printable ASCII code points");

    /* Advances against FreeType, exactly as the DejaVu fixture does. */
    int bad_adv = 0, adv_rows = 0;
    for (int i = 0; i < TTF_ORACLE_N; i++) {
        const TtfOracleRow *r = &TTF_ORACLE[i];
        uint32_t gid = wubu_ttf_glyph_index(t, r->cp);
        if (!gid) continue;
        for (int k = 0; k < TTF_ORACLE_NPPEM; k++) {
            int got = wubu_ttf_advance_px(t, gid, TTF_ORACLE_PPEM[k]);
            int want = (int)(r->adv[k] + 0.5);
            adv_rows++;
            if (got < want - 1 || got > want + 1) bad_adv++;
        }
    }
    printf("  advances: %d glyph/size pairs, %d outside 1px\n", adv_rows, bad_adv);
    ok(adv_rows > 200 && bad_adv == 0, "advances agree with FreeType within 1px");

    /* Ink against FreeType at the sizes the shell will actually use. */
    static WubuTtfPoint pts[WUBU_TTF_MAX_POINTS];
    static uint16_t ends[WUBU_TTF_MAX_CONTOURS];
    static uint8_t cov[256 * 256];
    int bad_ink = 0, ink_rows = 0;
    double lo = 9.0, hi = 0.0;
    for (int i = 0; i < TTF_RASTER_N; i++) {
        const TtfRasterRow *r = &TTF_RASTER[i];
        int np = 0, nc = 0, w = 0, h = 0;
        int ok_out = wubu_ttf_outline(t, wubu_ttf_glyph_index(t, r->cp), pts, WUBU_TTF_MAX_POINTS,
                                      &np, ends, WUBU_TTF_MAX_CONTOURS, &nc);
        int ok_ras = ok_out && wubu_ttf_raster(pts, np, ends, nc, upem, r->ppem, cov, 256, 256, &w, &h);
        ink_rows++;
        if (!ok_ras) { bad_ink++; printf("  U+%04X %dpx: no coverage\n", r->cp, r->ppem); continue; }
        long m = 0;
        for (int k = 0; k < w * h; k++) m += cov[k];
        double ratio = r->mass ? (double)m / (double)r->mass : 0.0;
        if (ratio < lo) lo = ratio;
        if (ratio > hi) hi = ratio;
        double band = (r->ppem >= 40) ? 0.12 : 0.20;
        if (ratio < 1.0 - band || ratio > 1.0 + band) {
            bad_ink++;
            printf("  U+%04X %dpx: ink %ld vs FreeType %ld (%.2f)\n", r->cp, r->ppem, m, r->mass, ratio);
        }
    }
    printf("  coverage: %d glyph/size pairs, ink ratio %.2f..%.2f of FreeType\n", ink_rows, lo, hi);
    ok(ink_rows > 0 && bad_ink == 0, "coverage matches FreeType within the measured band");

    /* A truncated face must be refused, not half-parsed: the shipped file minus its tail. */
    const char *why2 = NULL;
    WubuTtf *cut = wubu_ttf_open(data, n / 3, &why2);
    /* Either outcome is acceptable, silence is not: a truncated face may still be usable for
     * metrics, but it must SAY it has no outlines rather than drawing nothing later. */
    if (!cut) {
        ok(1, "refuses a truncated copy of the same face");
    } else {
        ok(wubu_ttf_has_outlines(cut) == 0, "truncated face reports no outlines");
        ok(wubu_ttf_outline(cut, 1, (WubuTtfPoint[8]){{0}}, 8, &(int){0}, (uint16_t[2]){0}, 2,
                            &(int){0}) == 0,
           "truncated face cannot produce an outline");
        wubu_ttf_close(cut);
    }

    wubu_ttf_close(t);
    free(data);
    printf("wubu_truetype_selawik: %d passed, %d failed\n", passed, failed);
    return failed ? 1 : 0;
}
