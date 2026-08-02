/*
 * wubu_ladder.h -- virtual-analog Moog-ladder lowpass (WT-B). C11.
 * The classic 4-pole transistor-ladder: four one-pole stages in a
 * cascade with the resonance feedback + the tanh saturation. The
 * simplified-but-real variant (the "Moog ladder" family).
 */
#ifndef WUBU_LADDER_H
#define WUBU_LADDER_H

typedef struct {
    float z[4];        /* the four stage states */
    float cutoff;      /* normalized 0..1 */
    float res;         /* resonance 0..1 */
    float drive;       /* input drive 1..4 */
    float sample_rate;
} wubu_ladder_t;

/* Init with a sample rate; cutoff 0..1, res 0..1. */
int wubu_ladder_init(wubu_ladder_t *f, float sample_rate,
                     float cutoff, float res);

/* Process one sample; returns the filtered output. */
float wubu_ladder_process(wubu_ladder_t *f, float in);

/* Process a buffer in place. */
int wubu_ladder_process_buf(wubu_ladder_t *f, float *buf, int n);

/* The measured cutoff: the -3dB point of the filter's magnitude
 * response at a given frequency (the calibration helper). */
float wubu_ladder_gain_at(const wubu_ladder_t *f, float freq_hz);

#endif
