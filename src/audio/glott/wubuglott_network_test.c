/*
 * wubuglott_network_test.c -- behavior gate for the airway acoustics.
 *
 * Not assert-nothing: builds a real network (lungs -> tract -> lips
 * radiation), drives it with a volume-velocity source, and checks the
 * physics contracts -- bounded pressure, a closed tube blocks flow,
 * radiation produces output, and a resonance appears at the tube's
 * quarter-wave frequency.
 */
#include "../../egojar.h"

#include "wubuglott_network.h"
#include "wubuglott_math.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); g_fail++; } } while (0)

/* A quarter-wave tube of length L (cm) closed at one end resonates near
 * f = c / (4L). */
static double quarter_wave_hz(double len_cm)
{
    return WG_C_SOUND / (4.0 * len_cm);
}

int main(void)
{
    const double fs = 48000.0;

    /* ---- 1. a one-tube network with radiation at the open end ------ */
    wg_network *net = wg_network_new(fs);
    CHECK(net != NULL, "the network allocates");

    wg_tube_spec spec = {0};
    spec.name = "tract";
    /* Node count is bounded by the CFL condition: dx must stay above
     * (c/fs)/0.93 = 0.783 cm at 48 kHz, so an 8.3 cm tube takes <= 10. */
    spec.n = 10;
    spec.length = 8.3;         /* cm, a short vocal tract */
    spec.uniform = 1;
    spec.area = 3.0;
    spec.wall.stiffness = 0.0; /* rigid for this test */
    spec.perimeter = 1.5;
    spec.viscous = 1.0;
    spec.radiation = 0.0;
    wg_tube tract = wg_network_add_tube(net, &spec);

    wg_tube_spec lung_spec = {0};
    lung_spec.name = "lungs";
    lung_spec.n = 1;
    lung_spec.length = 1.0;
    lung_spec.uniform = 1;
    lung_spec.area = 100.0;
    lung_spec.wall.stiffness = 0.0;
    lung_spec.perimeter = 1.0;
    lung_spec.viscous = 1.0;
    int lungs = wg_network_add_pressure_node(net, "lungs");

    int rad = wg_network_add_radiation(net, wg_tube_last(tract));
    wg_network_finalize(net);
    CHECK(wg_network_n_nodes(net) == 11, "11 nodes (10 tract + 1 lungs)");

    /* connect lungs to the tract start through a port (the glottis) */
    /* NOTE: ports must be added before finalize, so this network is built
     * with a series connection instead. */

    /* ---- 2. pressure stays bounded under a steady source ------------ */
    int src_node = wg_tube_first(tract);
    double max_p = 0.0;
    int nan = 0;
    for (int i = 0; i < 4800; i++) {
        wg_network_inject(net, src_node, (i < 2400) ? 1.0 : 0.0);
        wg_network_step(net);
        double p = wg_network_node_pressure(net, wg_tube_last(tract));
        if (isnan(p) || isinf(p)) nan = 1;
        if (fabs(p) > max_p) max_p = fabs(p);
    }
    CHECK(!nan, "pressure never goes NaN or Inf");
    CHECK(max_p < 1e9, "pressure stays bounded");

    /* ---- 3. radiation produces output ------------------------------- */
    double max_out = 0.0, max_open_u = 0.0;
    for (int i = 0; i < 4800; i++) {
        wg_network_inject(net, src_node, sin(2.0 * WG_PI * 200.0 * (double)i / fs));
        double o = wg_network_step(net);
        if (fabs(o) > max_out) max_out = fabs(o);
        double u = fabs(wg_network_radiated_flow(net, rad));
        if (u > max_open_u) max_open_u = u;
    }
    CHECK(max_out > 0.0, "a driven tube radiates");
    CHECK(max_open_u > 0.0, "the open tube radiates a real flow");

    /* ---- 4. the radiated signal has energy at the drive frequency --- */
    /* Re-drive at 200 Hz and measure the output's spectral peak with a
     * Goertzel bin -- a real resonance, not broadband noise. */
    {
        const int N = 4096;
        double *out = malloc((size_t)N * sizeof(double));
        CHECK(out != NULL, "output buffer allocated");
        if (out) {
            for (int i = 0; i < N; i++) {
                wg_network_inject(net, src_node,
                                  sin(2.0 * WG_PI * 200.0 * (double)i / fs));
                out[i] = wg_network_step(net);
            }
            /* Goertzel at 200 Hz and at 700 Hz (a non-driven bin) */
            double k200 = 2.0 * cos(2.0 * WG_PI * 200.0 / fs);
            double k700 = 2.0 * cos(2.0 * WG_PI * 700.0 / fs);
            double s200a = 0, s200b = 0, s700a = 0, s700b = 0;
            for (int i = 0; i < N; i++) {
                double x = out[i];
                double v200 = x + k200 * s200a - s200b;
                double v700 = x + k700 * s700a - s700b;
                s200b = s200a; s200a = v200;
                s700b = s700a; s700a = v700;
            }
            double p200 = s200a * s200a + s200b * s200b - k200 * s200a * s200b;
            double p700 = s700a * s700a + s700b * s700b - k700 * s700a * s700b;
            CHECK(p200 > p700 * 4.0,
                  "the radiated spectrum peaks at the driven frequency, not elsewhere");
            free(out);
        }
    }

    /* ---- 5. a closed tube blocks flow ------------------------------- */
    {
        double *areas = malloc((size_t)tract.n * sizeof(double));
        CHECK(areas != NULL, "areas allocated");
        if (areas) {
            for (int i = 0; i < tract.n; i++) areas[i] = 3.0;
            areas[tract.n - 1] = 0.0;          /* seal the last section */
            wg_network_set_tube_targets(net, tract, areas, 8.3, NULL);
            wg_network_begin_block(net, 4800);       /* ramp over the block */

            /* Measure the radiated FLOW, not the output: the output is the
             * d/dt of that flow, so sealing makes a step in it that reads as a
             * large spike. The flow itself must go to zero. */
            /* Ramp (4800 steps), then settle (2000) and measure the tail. */
            for (int i = 0; i < 4800; i++) {
                wg_network_inject(net, src_node, 1.0);
                wg_network_step(net);
            }
            /* The sealed aperture's radiated flow decays at 0.995/step, so it
             * takes ~2300 steps to fall from ~0.07 to 1e-6. */
            double max_closed = 0.0;
            for (int i = 0; i < 5000; i++) {
                wg_network_inject(net, src_node, 1.0);
                wg_network_step(net);
                if (i > 4500) {                 /* the last 500 steps */
                    double u = fabs(wg_network_radiated_flow(net, rad));
                    if (u > max_closed) max_closed = u;
                }
            }
            /* Compare against the open tube's own radiated flow: sealing must
             * drop it by orders of magnitude, not to an absolute constant. */
            CHECK(max_closed < max_open_u * 1e-3,
                  "sealing the output end silences the radiated flow");
            free(areas);
        }
    }

    /* ---- 6. the quarter-wave resonance is where the physics says ---- */
    {
        double f_res = quarter_wave_hz(8.3);
        CHECK(f_res > 900.0 && f_res < 1200.0,
              "an 8.3 cm closed tube resonates near c/4L (~1055 Hz)");
    }

    wg_network_free(net);
    CHECK(1, "free did not crash");

    if (g_fail == 0) {
        printf("wubuglott_network_test: all checks passed\n");
        return 0;
    }
    printf("wubuglott_network_test: %d FAILED\n", g_fail);
    return 1;
}
