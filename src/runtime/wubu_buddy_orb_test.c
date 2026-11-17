/* wubu_buddy_orb_test.c -- gates for the live metaball orb.
 *
 * Asserts the orb is well-formed and actually LIVING: deterministic in t,
 * contained in its render box, and visibly reshaped between two instants.
 */
#include "wubu_buddy_orb.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static int g_pass, g_fail;

static void check(int cond, const char *what)
{
    if (cond) {
        ++g_pass;
    } else {
        ++g_fail;
        printf("[wubu_buddy_orb] FAIL: %s\n", what);
    }
}

int main(void)
{
    wubu_buddy_orb_t *o = wubu_buddy_orb_create(8);
    check(o != NULL, "create returns an orb");
    check(wubu_buddy_orb_blob_count(o) == 8, "blob count honoured");

    wubu_buddy_orb_t *big = wubu_buddy_orb_create(99);
    check(wubu_buddy_orb_blob_count(big) == WUBU_BUDDY_ORB_MAX_BLOBS,
          "blob count clamped to MAX");
    wubu_buddy_orb_destroy(big);

    /* out-of-range access is safe and zeroed */
    float c[3] = {1, 1, 1};
    float r = -1;
    wubu_buddy_orb_get_blob(o, 999, c, &r);
    check(c[0] == 0 && c[1] == 0 && c[2] == 0 && r == 0,
          "out-of-range blob reads zeros");

    /* deterministic: same t -> same body */
    wubu_buddy_orb_set_time(o, 0.0f);
    float a[3], ar, b[3], br;
    wubu_buddy_orb_get_blob(o, 3, a, &ar);
    float f0 = wubu_buddy_orb_field(o, 0.1f, 0.2f, 0.3f);
    wubu_buddy_orb_set_time(o, 0.0f);
    wubu_buddy_orb_get_blob(o, 3, b, &br);
    check(a[0] == b[0] && a[1] == b[1] && a[2] == b[2] && ar == br,
          "blob state is a pure function of t");
    check(isfinite(f0), "field value is finite");

    /* living form: the body must change over time */
    wubu_buddy_orb_set_time(o, 1.37f);
    float d[3];
    wubu_buddy_orb_get_blob(o, 3, d, NULL);
    float moved = fabsf(d[0] - a[0]) + fabsf(d[1] - a[1]) + fabsf(d[2] - a[2]);
    check(moved > 1e-4f, "form changes over time");

    /* each blob centre is inside the surface, and inside the render box */
    float lo[3], hi[3];
    wubu_buddy_orb_render_box(o, lo, hi);
    int inside = 1, boxed = 1;
    for (int i = 0; i < wubu_buddy_orb_blob_count(o); ++i) {
        float cc[3], rr;
        wubu_buddy_orb_get_blob(o, i, cc, &rr);
        if (!(wubu_buddy_orb_field(o, cc[0], cc[1], cc[2]) > WUBU_BUDDY_ISO))
            inside = 0;
        for (int k = 0; k < 3; ++k) {
            if (cc[k] < lo[k] - 1e-3f || cc[k] > hi[k] + 1e-3f)
                boxed = 0;
        }
    }
    check(inside, "every blob centre lies inside the surface");
    check(boxed, "render box contains every blob");
    check(lo[0] < hi[0] && lo[1] < hi[1] && lo[2] < hi[2],
          "render box has positive extent");

    wubu_buddy_orb_destroy(o);
    wubu_buddy_orb_destroy(NULL);      /* must be a safe no-op */

    printf("[wubu_buddy_orb] %d pass, %d fail\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
