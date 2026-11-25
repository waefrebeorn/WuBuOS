/* buddy_orb_draw: the mascot as a shaded metaball, not as a PNG sprite.
 *
 * This lives in its OWN translation unit on purpose. It first went into wubu_bonzi.c,
 * which is the sprite blitter -- so every link graph that compiled the sprite suddenly
 * needed the orb unit too, and test_wm broke with five undefined references to
 * wubu_buddy_orb_*(). A draw path and a sprite blitter are two concerns; keeping them
 * in one file makes a dependency of one into a dependency of the other.
 *
 * The shape is a pure function of time, so any frame is reproducible from t alone.
 */
#include <stdint.h>
#include <stddef.h>   /* NULL */
#include "wubu_buddy_orb.h"

/* smoothstep half-width across the iso surface: this IS the antialiasing. A hard iso
 * cut is what "blocky" looks like on a curved body. */
#define ORB_EDGE 40.0f
#define ORB_BOX  112          /* on-screen size of the mascot */
#define ORB_MARGIN_R 24
#define ORB_MARGIN_B 72

/* ---- shader-based buddy: the metaball orb, not the PNG sprite ----------------
 *
 * The PNG mascot came back because the desktop still called wubu_bonzi_draw(); the
 * orb module (runtime/wubu_buddy_orb) was only used by its own test. This renders the
 * orb: a field, a smoothstep threshold for ANTIALIASED edges (a hard iso cut is the
 * "blocky" look), and a normal from finite differences so the body is shaded rather
 * than flat.
 *
 * The shape is a pure function of time, so a frame is reproducible from t alone.
 */

void wubu_bonzi_draw_orb(uint32_t *fb, int fb_w, int fb_h) {
    static wubu_buddy_orb_t *orb = NULL;
    static float t = 0.0f;
    if (!fb || fb_w <= 0 || fb_h <= 0) return;
    if (!orb) { orb = wubu_buddy_orb_create(9); if (!orb) return; }
    t += 1.0f / 60.0f;
    wubu_buddy_orb_set_time(orb, t);

    const int box = 112;                       /* on-screen size */
    int x0 = fb_w - box - 24, y0 = fb_h - box - 72;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;

    float mn[3], mx[3];
    wubu_buddy_orb_render_box(orb, mn, mx);
    float span_x = mx[0] - mn[0], span_y = mx[1] - mn[1], span_z = mx[2] - mn[2];
    if (span_x <= 0 || span_y <= 0 || span_z <= 0) return;
    float cz = mn[2] + span_z * 0.5f;

    for (int py = 0; py < box; py++) {
        int sy = y0 + py;
        if (sy < 0 || sy >= fb_h) continue;
        float wy = mn[1] + span_y * (1.0f - (float)py / (float)box);
        for (int px = 0; px < box; px++) {
            int sx = x0 + px;
            if (sx < 0 || sx >= fb_w) continue;
            float wx = mn[0] + span_x * ((float)px / (float)box);
            float f = wubu_buddy_orb_field(orb, wx, wy, cz);
            /* smoothstep across the iso surface: this IS the antialiasing */
            float d = (f - WUBU_BUDDY_ISO);
            float a = d * 40.0f + 0.5f;          /* ~1px of transition */
            if (a <= 0.0f) continue;
            if (a > 1.0f) a = 1.0f;
            /* normal from the field gradient, so the blob is lit, not flat */
            float eps = 0.01f;
            float gx = wubu_buddy_orb_field(orb, wx + eps, wy, cz) - wubu_buddy_orb_field(orb, wx - eps, wy, cz);
            float gy = wubu_buddy_orb_field(orb, wx, wy + eps, cz) - wubu_buddy_orb_field(orb, wx, wy - eps, cz);
            float nz = 1.0f / (1.0f + (gx < 0 ? -gx : gx) * 6.0f + (gy < 0 ? -gy : gy) * 6.0f);
            int shade = (int)(60.0f + 150.0f * nz);
            if (shade > 255) shade = 255;
            int r = shade, g = (int)(shade * 0.72f), b = (int)(shade * 0.55f);
            uint32_t *dst = &fb[sy * fb_w + sx];
            int dr = (int)((*dst >> 16) & 0xff), dg = (int)((*dst >> 8) & 0xff), db = (int)(*dst & 0xff);
            r = (int)(r * a + dr * (1.0f - a));
            g = (int)(g * a + dg * (1.0f - a));
            b = (int)(b * a + db * (1.0f - a));
            *dst = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
        }
    }
}
