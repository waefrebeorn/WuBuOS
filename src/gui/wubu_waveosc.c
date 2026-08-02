/*
 * wubu_waveosc.c -- bandlimited wavetable oscillator (WT-A). C11.
 * The tables are precomputed by additive harmonic sums; the read path
 * is a phase accumulator with linear interpolation.
 */
#include "wubu_waveosc.h"
#include <math.h>
#include <string.h>

static void build_table(float *tbl, int kind, int bandlimit_harmonics)
{
    const double twopi = 6.283185307179586;
    for (int i = 0; i < WUBU_OSC_TABLE_LEN; i++) {
        double th = twopi * (double)i / (double)WUBU_OSC_TABLE_LEN;
        double v = 0;
        if (kind == WUBU_OSC_SINE) {
            v = sin(th);
        } else if (kind == WUBU_OSC_TRIANGLE) {
            /* odd harmonics, 1/k^2 amplitudes (the bandlimited tri) */
            for (int k = 1; k <= bandlimit_harmonics; k += 2)
                v += (8.0 / (3.14159265 * 3.14159265)) *
                     (1.0 / (double)(k * k)) * sin(k * th);
        } else if (kind == WUBU_OSC_SQUARE) {
            for (int k = 1; k <= bandlimit_harmonics; k += 2)
                v += (4.0 / 3.14159265) * (1.0 / (double)k) * sin(k * th);
        } else { /* saw */
            for (int k = 1; k <= bandlimit_harmonics; k++)
                v += (2.0 / 3.14159265) * (1.0 / (double)k) * sin(k * th);
        }
        /* normalize to a healthy peak ~0.9 */
        tbl[i] = (float)(v * 0.9);
    }
}

int wubu_waveosc_init(wubu_waveosc_t *o)
{
    if (!o) return -1;
    memset(o, 0, sizeof(*o));
    /* the table layout: kind-major, two bandlimits each for morphing */
    int k = 0;
    for (int kind = 0; kind < WUBU_OSC_LAST && k < WUBU_OSC_NTABLES; kind++) {
        /* the "soft" bandlimit (fewer harmonics) + the "bright" one */
        build_table(o->tables[k], kind, 24);
        k++;
        build_table(o->tables[k], kind, 96);
        k++;
    }
    o->table_count = k;
    return 0;
}

float wubu_waveosc_read(const wubu_waveosc_t *o, int table, float pos)
{
    if (!o || table < 0 || table >= o->table_count) return 0;
    if (pos < 0) pos = 0;
    if (pos >= 1) pos -= (float)(int)pos;
    float f = pos * (WUBU_OSC_TABLE_LEN - 1);
    int i = (int)f;
    float frac = f - i;
    const float *t = o->tables[table];
    float a = t[i];
    float b = t[i + 1];
    return a + (b - a) * frac;
}

int wubu_waveosc_render(wubu_waveosc_t *o, float freq, float sample_rate,
                        float m, float *out, int n)
{
    if (!o || !out || n <= 0 || sample_rate <= 0) return 0;
    if (freq < 0) freq = 0;
    if (m < 0) m = 0;
    if (m > 1) m = 1;
    if (freq > sample_rate * 0.45f) freq = sample_rate * 0.45f; /* Nyquist */
    /* the morph pairs: (0,1) sine, (2,3) tri, (4,5) square, (6,7) saw */
    int tA = ((int)(m * 4)) * 2;
    if (tA > 6) tA = 6;
    int tB = tA + 1;
    float ffrac = m * 4 - (int)(m * 4);
    if (tA >= 6) { tA = 6; tB = 7; ffrac = 1; }
    const float *A = o->tables[tA];
    const float *B = o->tables[tB];
    float phase = o->phase;
    for (int i = 0; i < n; i++) {
        if (phase >= 1) phase -= (float)(int)phase;
        float f = phase * (WUBU_OSC_TABLE_LEN - 1);
        int idx = (int)f;
        float frac = f - idx;
        float a = A[idx] + (A[idx + 1] - A[idx]) * frac;
        float b = B[idx] + (B[idx + 1] - B[idx]) * frac;
        out[i] = a + (b - a) * ffrac;
        phase += freq / sample_rate;
    }
    o->phase = phase;
    return n;
}

void wubu_waveosc_reset(wubu_waveosc_t *o)
{
    if (o) o->phase = 0;
}
