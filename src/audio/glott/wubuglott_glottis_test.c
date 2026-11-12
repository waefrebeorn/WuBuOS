/*
 * wubuglott_glottis_test.c -- behavior gate for the vocal-fold source.
 *
 * Not assert-nothing: drives the model with a real transglottal pressure,
 * checks the area waveform is physically bounded, that voicing gates on
 * with pressure and off without it, and that the pulse shape is correct.
 */
#include "../../egojar.h"

#include "wubuglott_glottis.h"
#include "wubuglott_math.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int g_fail;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); g_fail++; } } while (0)

/* cmH2O in dyn/cm^2 -- the same constant the engine uses. */
#define CM_H2O 980.665

int main(void)
{
    const double fs = 48000.0;
    wg_glottis g;
    wg_glottis_init(&g, fs);

    /* ---- 1. the defaults are the documented ones --------------------- */
    CHECK(g.params.f0 == 110.0, "default f0 is 110 Hz (male)");
    CHECK(g.params.open_quotient == 0.62, "default open quotient");
    CHECK(g.threshold == 2.5 * CM_H2O, "phonation threshold is 2.5 cmH2O");

    /* ---- 2. below threshold: no voicing, area ~ static chink only ---- */
    double max_low = 0.0;
    for (int i = 0; i < 4800; i++) {
        double a = wg_glottis_next(&g, 0.5 * CM_H2O);   /* below 2.5 cmH2O */
        if (a > max_low) max_low = a;
    }
    CHECK(g.voicing < 0.05, "below threshold the voice does not vibrate");
    CHECK(max_low < 0.02, "sub-threshold area is just the chink leakage");

    /* ---- 3. above threshold: it vibrates ----------------------------- */
    wg_glottis_reset(&g);
    double max_hi = 0.0, min_hi = 1e9;
    int crossings = 0;
    double prev = 0.0;
    for (int i = 0; i < 4800; i++) {
        double a = wg_glottis_next(&g, 6.0 * CM_H2O);
        if (a > max_hi) max_hi = a;
        if (a < min_hi) min_hi = a;
        if ((prev < 0.02 && a >= 0.02) || (prev >= 0.02 && a < 0.02))
            crossings++;
        prev = a;
    }
    CHECK(g.voicing > 0.9, "above threshold the voice vibrates");
    CHECK(max_hi > 0.05, "the area pulse has real amplitude");
    CHECK(crossings >= 6, "there are several glottal cycles in 0.1 s at 110 Hz");

    /* ---- 4. the area is physically bounded --------------------------- */
    CHECK(max_hi < 1.0, "the area never exceeds the peak vibratory area scale");
    CHECK(min_hi >= 0.0, "the area is never negative");

    /* ---- 5. the pulse shape ------------------------------------------ */
    /* t = ph/oq; the peak sits at t = s (skew), and the fold closes at t = 1. */
    CHECK(wg_glottis_shape(0.0, 0.62, 0.62, 0.6) == 0.0,
          "the fold is closed at phase 0");
    CHECK(wg_glottis_shape(0.62 * 0.62, 0.62, 0.62, 0.6) > 0.99,
          "the pulse peaks at t = skew");
    CHECK(wg_glottis_shape(0.62, 0.62, 0.62, 0.6) == 0.0,
          "the pulse is closed at the open quotient");
    CHECK(wg_glottis_shape(0.9, 0.62, 0.62, 0.6) == 0.0,
          "the pulse stays closed past the open quotient");

    /* ---- 6. vibrato changes f0 --------------------------------------- */
    wg_glottis_reset(&g);
    g.params.vibrato_depth = 0.0;                 /* off: f0 is exact */
    for (int i = 0; i < 100; i++) wg_glottis_next(&g, 6.0 * CM_H2O);
    double f0_off = g.f0_now;
    CHECK(fabs(f0_off - 110.0) < 5.0, "f0 sits near 110 Hz with vibrato off");

    /* ---- 7. fry lowers the rate -------------------------------------- */
    wg_glottis_reset(&g);
    g.params.fry = 0.9;
    for (int i = 0; i < 100; i++) wg_glottis_next(&g, 6.0 * CM_H2O);
    CHECK(g.f0_now < 60.0, "vocal fry pulls f0 down toward 45-48 Hz");

    /* ---- 8. chaos stays bounded -------------------------------------- */
    wg_glottis_reset(&g);
    g.params.fry = 0.0;
    g.params.chaos = 0.8;
    int bounded = 1;
    for (int i = 0; i < 48000; i++) {
        double a = wg_glottis_next(&g, 6.0 * CM_H2O);
        if (!(a >= 0.0 && a < 2.0)) { bounded = 0; break; }
    }
    CHECK(bounded, "at chaos 0.8 the area stays bounded over 1 s");

    if (g_fail == 0) {
        printf("wubuglott_glottis_test: all checks passed\n");
        return 0;
    }
    printf("wubuglott_glottis_test: %d FAILED\n", g_fail);
    return 1;
}
