/*
 * wubuglott_decimator.h -- Kaiser-windowed sinc FIR decimator.
 *
 * Integer factor M. Push one input sample at a time; when push() returns
 * true, `value` holds the next output sample.
 *
 * Slermed from chimera-vocal-synth, crates/chimera/src/airway/decimator.rs.
 * C11. Allocates at init only; push() allocates nothing.
 */
#ifndef WUBUGLOTT_DECIMATOR_H
#define WUBUGLOTT_DECIMATOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int    factor;
    int    taps;
    double *h;          /* taps coefficients */
    double *buf;        /* 2*taps, the circular history mirrored */
    int    pos;
    int    count;
    double value;
} wg_decimator;

/* factor 1 is a pass-through. */
void wg_decimator_init(wg_decimator *d, int factor);
void wg_decimator_init_taps(wg_decimator *d, int factor, int taps_per_phase);
void wg_decimator_free(wg_decimator *d);

/* Push one input sample; returns 1 when an output sample is ready. */
int wg_decimator_push(wg_decimator *d, double x);

#ifdef __cplusplus
}
#endif

#endif /* WUBUGLOTT_DECIMATOR_H */
