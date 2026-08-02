/*
 * wubu_waveosc.h -- bandlimited wavetable oscillator (WT-A). C11.
 * The engine's real synthesis core (beyond the simple wave() in
 * wubu_sound). Bandlimited tables are precomputed by additive harmonic
 * sums (harmonics up to Nyquist); the read path is phase-accumulator +
 * interpolation; morphing crossfades between two tables.
 */
#ifndef WUBU_WAVEOSC_H
#define WUBU_WAVEOSC_H

#include <stdint.h>

#define WUBU_OSC_TABLE_LEN 2048
#define WUBU_OSC_NTABLES  8

enum {
    WUBU_OSC_SINE = 0,
    WUBU_OSC_TRIANGLE,
    WUBU_OSC_SQUARE,
    WUBU_OSC_SAW,
    WUBU_OSC_LAST
};

typedef struct {
    float tables[WUBU_OSC_NTABLES][WUBU_OSC_TABLE_LEN];
    float phase;        /* 0..1 accumulator */
    int   table_count;
} wubu_waveosc_t;

/* Precompute the bandlimited tables (sine/triangle/square/saw, each at
 * two bandlimits for the morph pairs). Returns 0. */
int wubu_waveosc_init(wubu_waveosc_t *o);

/* Render n mono samples at freq Hz (sample_rate Hz), morphing between
 * the two tables by m (0..1). Returns the samples written. */
int wubu_waveosc_render(wubu_waveosc_t *o, float freq, float sample_rate,
                        float m, float *out, int n);

/* One interpolated table read at a 0..1 position (linear). */
float wubu_waveosc_read(const wubu_waveosc_t *o, int table, float pos);

/* Reset the phase (note-on). */
void wubu_waveosc_reset(wubu_waveosc_t *o);

#endif
