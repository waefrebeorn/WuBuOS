/*
 * wubuglott_decimator_test.c -- behavior gate for the FIR decimator.
 *
 * Not assert-nothing: checks the filter's DC gain is exactly 1, that a
 * pass-through (factor 1) is exact, that the output rate is the input rate
 * divided by the factor, and that images above the new Nyquist are
 * attenuated -- which is the whole point of the filter.
 */
#include "../../egojar.h"

#include "wubuglott_decimator.h"
#include "wubuglott_math.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int g_fail;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); g_fail++; } } while (0)

int main(void)
{
    /* ---- 1. factor 1 is an exact pass-through --------------------- */
    {
        wg_decimator d;
        wg_decimator_init(&d, 1);
        CHECK(d.taps == 1, "factor 1 has a single tap");
        int outs = 0;
        for (int i = 0; i < 64; i++)
            if (wg_decimator_push(&d, (double)i))
                outs++;
        CHECK(outs == 64, "factor 1 emits one output per input");
        CHECK(d.value == 63.0, "factor 1 passes the sample through unchanged");
        wg_decimator_free(&d);
    }

    /* ---- 2. DC gain is exactly 1 ---------------------------------- */
    {
        wg_decimator d;
        wg_decimator_init(&d, 4);
        CHECK(d.taps == 4 * 24 + 1, "taps = factor*24 + 1");
        double sum = 0.0;
        for (int i = 0; i < d.taps; i++) sum += d.h[i];
        CHECK(fabs(sum - 1.0) < 1e-12, "the coefficients sum to 1 (unity DC gain)");

        /* push a constant and check the output is that constant */
        int outs = 0;
        double last = 0.0;
        for (int i = 0; i < 4 * 200; i++) {
            if (wg_decimator_push(&d, 0.5)) {
                last = d.value;
                outs++;
            }
        }
        CHECK(outs == 200, "exactly one output per 4 inputs");
        CHECK(fabs(last - 0.5) < 1e-12, "a DC input comes out unchanged");
        wg_decimator_free(&d);
    }

    /* ---- 3. the output rate is the input rate / factor ------------ */
    {
        wg_decimator d;
        wg_decimator_init(&d, 6);
        int outs = 0;
        for (int i = 0; i < 6000; i++)
            if (wg_decimator_push(&d, (double)(i & 7)))
                outs++;
        CHECK(outs == 1000, "6000 inputs at factor 6 give 1000 outputs");
        wg_decimator_free(&d);
    }

    /* ---- 4. images above the new Nyquist are attenuated ----------- */
    {
        /* Input at 48 kHz, factor 4 -> output at 12 kHz, Nyquist 6 kHz.
         * A 5 kHz tone is below Nyquist and must survive; a 9 kHz tone is an
         * image and must be strongly attenuated. */
        const double fs = 48000.0;
        const int factor = 4;
        const int n = 8000;
        double *out = malloc((size_t)(n / factor) * sizeof(double));
        CHECK(out != NULL, "output buffer allocated");
        if (out) {
            for (int pass = 0; pass < 2; pass++) {
                double hz = (pass == 0) ? 5000.0 : 9000.0;
                wg_decimator d;
                wg_decimator_init(&d, factor);
                int k = 0;
                for (int i = 0; i < n && k < n / factor; i++) {
                    double x = sin(2.0 * WG_PI * hz * (double)i / fs);
                    if (wg_decimator_push(&d, x))
                        out[k++] = d.value;
                }
                /* measure the output amplitude in the settled tail */
                double peak = 0.0;
                for (int i = k / 2; i < k; i++)
                    if (fabs(out[i]) > peak) peak = fabs(out[i]);
                if (pass == 0) {
                    CHECK(peak > 0.8, "a tone below the new Nyquist survives");
                    printf("  5 kHz (in-band) peak=%.3f\n", peak);
                } else {
                    CHECK(peak < 0.05, "an image above the new Nyquist is attenuated");
                    printf("  9 kHz (image)  peak=%.3f\n", peak);
                }
                wg_decimator_free(&d);
            }
            free(out);
        }
    }

    /* ---- 5. free is idempotent ------------------------------------ */
    {
        wg_decimator d;
        wg_decimator_init(&d, 2);
        wg_decimator_free(&d);
        wg_decimator_free(&d);
        CHECK(1, "double free did not crash");
    }

    if (g_fail == 0) {
        printf("wubuglott_decimator_test: all checks passed\n");
        return 0;
    }
    printf("wubuglott_decimator_test: %d FAILED\n", g_fail);
    return 1;
}
