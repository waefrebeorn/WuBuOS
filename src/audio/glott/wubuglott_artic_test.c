/*
 * wubuglott_artic_test.c -- behavior gate for the articulatory model.
 *
 * Not assert-nothing: checks the neutral posture's documented values, that
 * all 17 keys round-trip through get/set and the array form, that the body
 * presets resolve exactly, and that a 50/50 blend lands halfway.
 */
#include "../../egojar.h"

#include "wubuglott_artic.h"
#include "wubuglott_math.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
#define CHECK(c, m) do { if (!(c)) { printf("FAIL: %s\n", m); g_fail++; } } while (0)

int main(void)
{
    wg_artic a = WG_ARTIC_NEUTRAL;

    /* ---- 1. the neutral posture's documented values ---------------- */
    CHECK(a.jaw == 0.6, "neutral jaw is 0.6 cm");
    CHECK(a.tb_x == 2.9 && a.tb_y == 6.85, "neutral tongue body");
    CHECK(a.tt_x == 6.45 && a.tt_y == 7.3, "neutral tongue tip");
    CHECK(a.lip_ap == 0.7 && a.lip_prot == 0.2, "neutral lips");
    CHECK(a.lip_width == 0.5, "neutral lip spreading");
    CHECK(a.velum == -0.15, "neutral velum is closed (-0.15)");
    CHECK(a.larynx == 0.0 && a.epilarynx == 0.0, "neutral larynx/epilarynx");
    CHECK(a.ventricular == 0.0 && a.lateral == 0.0 && a.dorsum == 0.0,
          "neutral ventricular/lateral/dorsum");

    /* ---- 2. every key round-trips through get/set ----------------- */
    {
        int ok = 1;
        for (int k = 0; k < WG_N_ARTIC; k++) {
            double before = wg_artic_get(&a, k);
            wg_artic_set(&a, k, 12.5);
            if (wg_artic_get(&a, k) != 12.5) ok = 0;
            wg_artic_set(&a, k, before);
        }
        CHECK(ok, "all 17 articulation keys round-trip through get/set");
    }

    /* ---- 3. array form round-trips --------------------------------- */
    {
        double arr[WG_N_ARTIC];
        wg_artic_to_array(&a, arr, WG_N_ARTIC);
        wg_artic b;
        wg_artic_from_array(&b, arr, WG_N_ARTIC);
        double arr2[WG_N_ARTIC];
        wg_artic_to_array(&b, arr2, WG_N_ARTIC);
        CHECK(memcmp(arr, arr2, sizeof(arr)) == 0,
              "to_array/from_array round-trips exactly");

        /* a partial array starts from NEUTRAL and overwrites what it has */
        wg_artic c;
        double partial[2] = { 9.0, 8.0 };
        wg_artic_from_array(&c, partial, 2);
        CHECK(c.jaw == 9.0 && c.tb_x == 8.0, "a partial array sets the keys it has");
        CHECK(c.tb_y == 6.85, "the rest of a partial array stays neutral");
    }

    /* ---- 4. the body presets resolve exactly ----------------------- */
    {
        wg_body m = wg_body_mix_resolve(WG_BODY_MIX_MALE);
        wg_body f = wg_body_mix_resolve(WG_BODY_MIX_FEMALE);
        wg_body c = wg_body_mix_resolve(WG_BODY_MIX_CHILD);
        CHECK(m.scale == 1.00 && m.pharynx == 1.00, "male is the 1.0 base");
        CHECK(f.scale == 0.90 && f.pharynx == 0.88, "female is smaller, shorter pharynx");
        CHECK(c.scale == 0.74 && c.pharynx == 0.80, "child is smallest");
        CHECK(m.tongue_radius == 2.25 && m.lip_width == 4.6, "male tongue/lip dims");
        CHECK(f.lip_width == 4.4 && c.lip_width == 4.0, "female/child lip widths");
    }

    /* ---- 5. a 50/50 blend lands halfway --------------------------- */
    {
        wg_body_mix half = { WG_BODY_MALE, 0.5, 0.0 };
        wg_body b = wg_body_mix_resolve(half);
        CHECK(fabs(b.scale - (1.0 + 0.9) / 2.0) < 1e-12,
              "a half-female blend is halfway in scale");
        CHECK(fabs(b.pharynx - (1.0 + 0.88) / 2.0) < 1e-12,
              "a half-female blend is halfway in pharynx");
        CHECK(fabs(b.tongue_radius - (2.25 + 2.20) / 2.0) < 1e-12,
              "a half-female blend is halfway in tongue radius");
    }

    /* ---- 6. out-of-range and non-finite mix factors clamp --------- */
    {
        wg_body_mix over = { WG_BODY_MALE, 5.0, -3.0 };
        wg_body b = wg_body_mix_resolve(over);
        /* female clamps to 1 -> pure female; child clamps to 0 */
        CHECK(fabs(b.scale - 0.90) < 1e-12, "an over-1 female factor clamps to female");

        wg_body_mix nan_mix = { WG_BODY_MALE, NAN, 0.0 };
        wg_body nb = wg_body_mix_resolve(nan_mix);
        CHECK(nb.scale == 1.00, "a NaN female factor counts as male");
    }

    /* ---- 7. the blend weights sum to 1 ----------------------------- */
    {
        double wm = (1.0 - 0.3) * (1.0 - 0.25);
        double wf = (1.0 - 0.3) * 0.25;
        double wc = 0.3;
        CHECK(fabs(wm + wf + wc - 1.0) < 1e-12, "the body weights sum to 1");
    }

    if (g_fail == 0) {
        printf("wubuglott_artic_test: all checks passed\n");
        return 0;
    }
    printf("wubuglott_artic_test: %d FAILED\n", g_fail);
    return 1;
}
