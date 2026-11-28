/* Gate for the runtime text face: composed strings against an FreeType-composed oracle.
 *
 * Per-glyph tests (test_wubu_truetype) can be green while a line of text is laid out wrongly --
 * advances rounded per glyph, bearings dropped, pen not carried. These checks are on whole strings:
 * total advance and total ink, at several ppem, against Pillow/FreeType.
 *
 * Ink is a SUM with a band, not a per-pixel map: FreeType grid-fits stems and we do not, so pixel
 * equality is unattainable. The band below is the spread actually observed at these sizes; see
 * test_wubu_text_cache for the per-glyph measurements it came from.
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "wubu_text_face.h"
#include "../../tests/fixtures/selawik_strings.h"

#define FB_W 1024   /* a 44-char line at 28px is ~556px wide: a smaller canvas clips the ink and
                     * under-reports mass, which reads as a rendering deficit */
#define FB_H 128

static int g_pass, g_fail;
static uint32_t g_fb[FB_W * FB_H];

static void ok(int cond, const char *what)
{
    if (cond) {
        g_pass++;
    } else {
        g_fail++;
        printf("  FAIL %s\n", what);
    }
}

/* Advance: observed worst 1.9 px on a 20-glyph run and 0.9 px on a 44-character line, because the
 * pen accumulates in quarter pixels. Rounding each glyph to whole pixels instead drifts to 5.5 px and
 * fails this check -- the mutation is recorded in the commit that added this gate. */
static int advance_ok(double ours, double theirs)
{
    double d = ours - theirs;
    return d <= 2.0 && d >= -2.0;
}

/* Ink: ratio of summed coverage over 24 strings at 13/20/28 px. Observed 0.91..1.02; the band is
 * 0.89..1.05, i.e. 2% outside the observed ends. The deficit is FreeType's hinting thickening stems,
 * which we decline to do; anything below 0.89 means coverage is being lost, not merely unhinted. */
static int ink_ok(double ours, double theirs)
{
    if (theirs <= 0) return ours == 0;
    double r = ours / theirs;
    return r >= 0.89 && r <= 1.05;
}

static long ink_mass(void)
{
    long m = 0;
    for (int i = 0; i < FB_W * FB_H; i++) m += (long)(g_fb[i] & 0xFF);
    return m;
}

int main(void)
{
    const char *why = NULL;
    WubuTextFace *f = wubu_text_face_open_file(TTF_STRINGS_PATH, &why);
    if (!f) {
        printf("  FAIL open %s: %s\n", TTF_STRINGS_PATH, why ? why : "?");
        return 1;
    }

    printf("  %-14s %3s %8s %8s %5s %9s %9s %6s\n",
           "text", "px", "advance", "oracle", "diff", "ink", "oracle", "ratio");

    double worst_adv = 0.0, lo_ratio = 9.0, hi_ratio = 0.0;
    for (int i = 0; i < TTF_STRINGS_COUNT; i++) {
        const TtfStringRow *r = &g_ttf_strings[i];
        int adv = wubu_text_face_measure(f, r->text, r->ppem);
        ok(adv >= 0, "measure succeeds");

        memset(g_fb, 0, sizeof(g_fb));
        int drawn = wubu_text_face_draw(f, g_fb, FB_W, FB_H, 8, 60, r->text, r->ppem, 0xFFFFFF);
        ok(drawn == adv, "draw returns the same advance as measure");

        long mass = ink_mass();
        double ratio = r->mass > 0 ? (double)mass / (double)r->mass : 0.0;
        double adiff = (double)adv - r->advance;
        if (adiff < 0) adiff = -adiff;
        if (adiff > worst_adv) worst_adv = adiff;
        if (r->mass > 0) {
            if (ratio < lo_ratio) lo_ratio = ratio;
            if (ratio > hi_ratio) hi_ratio = ratio;
        }
        printf("  %-14s %3d %8d %8.1f %5.1f %9ld %9d %6.2f\n",
               r->text, r->ppem, adv, r->advance, (double)adv - r->advance, mass, r->mass, ratio);
        ok(advance_ok((double)adv, r->advance), "composed advance matches FreeType");
        ok(ink_ok((double)mass, (double)r->mass), "composed ink matches within the hinting band");
    }
    printf("  worst advance delta %.1f px; ink ratio %.2f..%.2f\n", worst_adv, lo_ratio, hi_ratio);

    /* The cache must be doing the work: a second pass over every string rasterises nothing new. */
    unsigned hits1, miss1, g1, hits2, miss2, g2;
    wubu_text_face_stats(f, &hits1, &miss1, &g1);
    for (int i = 0; i < TTF_STRINGS_COUNT; i++)
        wubu_text_face_draw(f, g_fb, FB_W, FB_H, 8, 60, g_ttf_strings[i].text, g_ttf_strings[i].ppem, 0xFFFFFF);
    wubu_text_face_stats(f, &hits2, &miss2, &g2);
    ok(miss2 == miss1, "redrawing every string rasterises no new glyph");
    ok(hits2 > hits1, "redrawing every string is served from cache");

    /* Wrong bytes must be refused, not rendered as garbage: a truncated sequence returns -1. */
    ok(wubu_text_face_draw(f, g_fb, FB_W, FB_H, 0, 10, "\xFF\xFE", 20, 0xFFFFFF) < 0,
       "malformed UTF-8 is refused");

    wubu_text_face_close(f);
    printf("wubu_text_face: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
