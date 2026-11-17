/* wubu_buddy_orb.c -- wubu_buddy: live metaball orb (C11, opaque).
 *
 * The blob choreography is deliberately simple and readable: each blob orbits
 * the origin on its own tilted ring while its radius breathes.  That gives a
 * creature-like silhouette that never repeats exactly, without any asset.
 */
#include "wubu_buddy_orb.h"

#include <math.h>
#include <stdlib.h>

struct wubu_buddy_orb {
    int n;
    float t;
};

float wubu_buddy_hash01(uint32_t seed)
{
    uint32_t x = seed * 2654435761u + 2246822519u;
    x ^= x >> 15;
    x *= 2246822519u;
    x ^= x >> 13;
    return (float)(x & 0xFFFFFFu) / (float)0xFFFFFFu;
}

wubu_buddy_orb_t *wubu_buddy_orb_create(int n)
{
    if (n <= 0)
        return NULL;
    if (n > WUBU_BUDDY_ORB_MAX_BLOBS)
        n = WUBU_BUDDY_ORB_MAX_BLOBS;
    wubu_buddy_orb_t *o = (wubu_buddy_orb_t *)malloc(sizeof(*o));
    if (!o)
        return NULL;
    o->n = n;
    o->t = 0.0f;
    return o;
}

void wubu_buddy_orb_destroy(wubu_buddy_orb_t *orb)
{
    free(orb);
}

void wubu_buddy_orb_set_time(wubu_buddy_orb_t *orb, float t)
{
    if (orb)
        orb->t = t;
}

int wubu_buddy_orb_blob_count(const wubu_buddy_orb_t *orb)
{
    return orb ? orb->n : 0;
}

void wubu_buddy_orb_get_blob(const wubu_buddy_orb_t *orb, int i,
                             float out_xyz[3], float *out_r)
{
    if (!orb || i < 0 || i >= orb->n) {
        if (out_xyz) {
            out_xyz[0] = out_xyz[1] = out_xyz[2] = 0.0f;
        }
        if (out_r)
            *out_r = 0.0f;
        return;
    }
    float t = orb->t;
    float ph = wubu_buddy_hash01((uint32_t)(i + 1)) * 6.2831853f;

    /* Per-blob orbit: radius, speed and vertical bob all differ per blob, so
     * the aggregate silhouette keeps reshaping instead of looping. */
    float ring = 0.30f + 0.45f * (0.5f + 0.5f * sinf(ph + 1.7f));
    float spd = 0.55f + 0.35f * (0.5f + 0.5f * cosf(ph * 1.3f));
    float ang = t * spd + ph;
    float tilt = 0.35f * sinf(ph * 2.1f);

    float x = ring * cosf(ang);
    float z = ring * sinf(ang) * cosf(tilt);
    float y = 0.55f * (0.5f + 0.5f * sinf(t * 0.7f + ph * 1.9f))
            + 0.18f * sinf(ang * 1.7f + ph);

    float r = 0.20f + 0.13f * (0.5f + 0.5f * sinf(t * 1.1f + ph * 2.3f));

    if (out_xyz) {
        out_xyz[0] = x;
        out_xyz[1] = y;
        out_xyz[2] = z;
    }
    if (out_r)
        *out_r = r;
}

float wubu_buddy_orb_field(const wubu_buddy_orb_t *orb, float x, float y, float z)
{
    if (!orb)
        return 0.0f;
    float f = 0.0f;
    for (int i = 0; i < orb->n; ++i) {
        float c[3], r;
        wubu_buddy_orb_get_blob(orb, i, c, &r);
        float dx = x - c[0], dy = y - c[1], dz = z - c[2];
        float d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < 1e-6f)
            d2 = 1e-6f;
        f += (r * r) / d2;
    }
    return f;
}

void wubu_buddy_orb_render_box(const wubu_buddy_orb_t *orb,
                               float out_min[3], float out_max[3])
{
    float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
    if (orb) {
        for (int i = 0; i < orb->n; ++i) {
            float c[3], r;
            wubu_buddy_orb_get_blob(orb, i, c, &r);
            for (int k = 0; k < 3; ++k) {
                if (c[k] - r < lo[k])
                    lo[k] = c[k] - r;
                if (c[k] + r > hi[k])
                    hi[k] = c[k] + r;
            }
        }
    }
    const float margin = 0.25f;
    for (int k = 0; k < 3; ++k) {
        out_min[k] = lo[k] - margin;
        out_max[k] = hi[k] + margin;
    }
}
