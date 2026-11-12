/*
 * wubuglott_decimator.c -- Kaiser-windowed sinc FIR decimator.
 * See wubuglott_decimator.h. Slermed from chimera's airway/decimator.rs.
 */
#include "wubuglott_decimator.h"
#include "wubuglott_math.h"

#include <math.h>
#include <stdlib.h>

/* Modified Bessel function of the first kind, order 0. */
static double bessel_i0(double x)
{
    double sum = 1.0;
    double term = 1.0;
    for (int k = 1; k < 30; k++) {
        double q = x / (2.0 * (double)k);
        term *= q * q;
        sum += term;
        if (term < 1e-12 * sum)
            break;
    }
    return sum;
}

/* Kaiser-windowed sinc low-pass; `cutoff` is a fraction of the input rate. */
static void design_lowpass(double *h, int taps, double cutoff, double beta)
{
    double mid = (double)(taps - 1) / 2.0;
    double i0b = bessel_i0(beta);
    double sum = 0.0;
    for (int i = 0; i < taps; i++) {
        double x = (double)i - mid;
        double sinc = (x == 0.0)
            ? 2.0 * cutoff
            : sin(2.0 * WG_PI * cutoff * x) / (WG_PI * x);
        double r = (2.0 * (double)i) / (double)(taps - 1) - 1.0;
        double w = bessel_i0(beta * sqrt(wg_js_max(0.0, 1.0 - r * r))) / i0b;
        h[i] = sinc * w;
        sum += h[i];
    }
    for (int i = 0; i < taps; i++)
        h[i] /= sum;
}

void wg_decimator_init_taps(wg_decimator *d, int factor, int taps_per_phase)
{
    if (factor < 1) factor = 1;
    d->factor = factor;
    d->pos = 0;
    d->count = 0;
    d->value = 0.0;
    d->taps = (factor == 1) ? 1 : factor * taps_per_phase + 1;
    d->h = calloc((size_t)d->taps, sizeof(double));
    d->buf = calloc((size_t)d->taps * 2, sizeof(double));
    if (!d->h || !d->buf) {
        free(d->h); free(d->buf);
        d->h = NULL; d->buf = NULL; d->taps = 0;
        return;
    }
    if (factor == 1) {
        d->h[0] = 1.0;
    } else {
        design_lowpass(d->h, d->taps, 0.46 / (double)factor, 8.5);
    }
}

void wg_decimator_init(wg_decimator *d, int factor)
{
    wg_decimator_init_taps(d, factor, 24);
}

void wg_decimator_free(wg_decimator *d)
{
    if (!d) return;
    free(d->h);
    free(d->buf);
    d->h = NULL; d->buf = NULL;
    d->taps = 0;
}

int wg_decimator_push(wg_decimator *d, double x)
{
    int n = d->taps;
    if (n <= 0 || !d->h || !d->buf)
        return 0;

    d->buf[d->pos] = x;
    d->buf[d->pos + n] = x;
    d->pos = (d->pos + 1 == n) ? 0 : d->pos + 1;
    d->count++;

    if (d->count < d->factor)
        return 0;

    d->count = 0;
    int start = d->pos;                 /* oldest sample */
    const double *b = d->buf + start;
    double acc = 0.0;
    for (int i = 0; i < n; i++)
        acc += d->h[n - 1 - i] * b[i];
    d->value = acc;
    return 1;
}
