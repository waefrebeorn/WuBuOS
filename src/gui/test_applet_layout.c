/*
 * test_applet_layout.c -- "the margins are bad" as a number, not an opinion.
 *
 * Every Control Panel applet is rendered into a real surface at several window sizes, and the bounding
 * box of everything it painted is measured against the rect it was given:
 *
 *   CLIPPING   no painted pixel may sit in the outermost 2 px of the rect -- content that runs off the
 *              edge is the "cutoff text" class, and it is a defect at every size.
 *   PADDING    content must keep at least MIN_PAD px from the left/right/bottom edges. An applet whose
 *              first glyph starts at x+1 looks broken even when nothing is missing.
 *   BOTTOM     the last 4 px must be the panel's own background: text sitting on the bottom edge reads
 *              as a crushed layout.
 *
 * The surfaces are real (vbe), the colours come from the theme, and the applet is called exactly the way
 * the Control Panel calls it. What is NOT measured here is taste -- but "the right edge of the text is
 * inside the panel" is not taste.
 *
 * C11.
 */
#include "control_panel.h"
#include "wubu_theme.h"
#include "../kernel/vbe.h"
#include "wubu_gui_metrics.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* The sound applet hands samples to the sink; this gate measures LAYOUT, not audio, so the sink is a
 * stub rather than a reason to drag the whole audio engine into a pixel audit. */
void wubu_audio_sink_int16(const int16_t *pcm, int n) { (void)pcm; (void)n; }

#define MIN_PAD 6
#define EDGE    2

static int g_pass, g_fail;

static const char *g_where = "";
#define CHECK(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  \u274c [%s] %s\n", g_where, msg); } \
} while (0)

/* The colour most of the rect is painted with: the applet's own background. Counting is bounded to a
 * small table, because this is an audit, not a histogram. */
#define MODE_SLOTS 16
static uint32_t mode_color(const uint32_t *fb, int stride, int x, int y, int w, int h, int *out_count)
{
    uint32_t col[MODE_SLOTS];
    int cnt[MODE_SLOTS];
    int n = 0;

    for (int i = 0; i < MODE_SLOTS; i++) { col[i] = 0; cnt[i] = 0; }

    /* Sample on a lattice: a 1920x1080 scan for every applet × size is a wasted second per case. */
    int step_x = w > 320 ? w / 160 : 1;
    int step_y = h > 240 ? h / 120 : 1;
    for (int yy = y; yy < y + h; yy += step_y) {
        const uint32_t *row = fb + (size_t)yy * stride;
        for (int xx = x; xx < x + w; xx += step_x) {
            uint32_t p = row[xx];
            int k;
            for (k = 0; k < n; k++) if (col[k] == p) { cnt[k]++; break; }
            if (k == n && n < MODE_SLOTS) { col[n] = p; cnt[n] = 1; n++; }
        }
    }
    int best = 0;
    for (int i = 1; i < n; i++) if (cnt[i] > cnt[best]) best = i;
    if (out_count) *out_count = n ? cnt[best] : 0;
    return n ? col[best] : 0;
}

typedef struct {
    int min_x, min_y, max_x, max_y;   /* bounding box of non-background pixels, rect-relative */
    int painted;                      /* count of non-background pixels sampled */
} ContentBox;

static ContentBox content_box(const uint32_t *fb, int stride, int x, int y, int w, int h, uint32_t bg)
{
    ContentBox b = { w, h, -1, -1, 0 };
    for (int yy = y; yy < y + h; yy++) {
        const uint32_t *row = fb + (size_t)yy * stride;
        for (int xx = x; xx < x + w; xx++) {
            if (row[xx] == bg) continue;
            int rx = xx - x, ry = yy - y;
            if (rx < b.min_x) b.min_x = rx;
            if (ry < b.min_y) b.min_y = ry;
            if (rx > b.max_x) b.max_x = rx;
            if (ry > b.max_y) b.max_y = ry;
            b.painted++;
        }
    }
    return b;
}

static void audit_applet(const char *name, CpApplet *a, int W, int H)
{
    if (!a || !a->render) return;

    uint32_t *fb = calloc((size_t)W * H, sizeof(uint32_t));
    if (!fb) { CHECK(false, "surface allocation"); return; }

    /* A sentinel everywhere: a pixel the applet leaves alone is DETECTABLY untouched. */
    for (size_t i = 0; i < (size_t)W * H; i++) fb[i] = 0xFF00FF01u;

    if (a->init) a->init(a);
    a->render(a, fb, W, H, 0, 0, W, H);

    int mode_n = 0;
    uint32_t bg = mode_color(fb, W, 0, 0, W, H, &mode_n);
    ContentBox b = content_box(fb, W, 0, 0, W, H, bg);

    static char where[160];
    snprintf(where, sizeof where, "%s @ %dx%d", name, W, H);
    g_where = where;
    const char *tag = where;

    if (b.painted == 0) {
        printf("  %-28s nothing painted (bg=%08x)\n", tag, bg);
        CHECK(false, "the applet paints something");
    } else {
        printf("  %-28s content x[%d..%d] y[%d..%d] of %dx%d\n",
               tag, b.min_x, b.max_x, b.min_y, b.max_y, W, H);
    }

    /* CLIPPING: nothing in the outermost EDGE pixels. */
    CHECK(b.min_x >= EDGE || b.painted == 0, "content clears the left edge");
    CHECK(b.max_x <= W - 1 - EDGE || b.painted == 0, "content clears the right edge");
    CHECK(b.min_y >= EDGE || b.painted == 0, "content clears the top edge");
    CHECK(b.max_y <= H - 1 - EDGE || b.painted == 0, "content clears the bottom edge");

    /* PADDING: the readable content keeps MIN_PAD. */
    if (b.painted > 0 && W >= 2 * MIN_PAD + 40) {
        CHECK(b.min_x >= MIN_PAD, "content keeps a left margin");
        CHECK(b.max_x <= W - 1 - MIN_PAD, "content keeps a right margin");
        CHECK(b.max_y <= H - 1 - MIN_PAD, "content keeps a bottom margin");
    }

    if (a->cleanup) a->cleanup(a);
    free(fb);
}

static void audit_control_panel(int W, int H)
{
    /* The panel is a WINDOW, not an applet: its surface is its client area, the caption belongs to the
     * WM, and the edge/padding rules above are about content laid out INSIDE a page. What this case can
     * honestly assert is that the panel paints at all -- the applet cases carry the layout rules, and
     * test_cp_applets carries the pixel contract. */
    uint32_t *fb = calloc((size_t)W * H, sizeof(uint32_t));
    if (!fb) return;
    for (size_t i = 0; i < (size_t)W * H; i++) fb[i] = 0xFF00FF01u;

    controlpanel_init();
    controlpanel_render(fb, W, H);

    size_t painted = 0;
    for (size_t i = 0; i < (size_t)W * H; i++) if (fb[i] != 0xFF00FF01u) painted++;

    static char where[64];
    snprintf(where, sizeof where, "Control Panel @ %dx%d", W, H);
    g_where = where;
    printf("  %-28s painted=%zu of %d (%.0f%%)\n", where, painted, W * H,
           100.0 * (double)painted / (double)(W * H));
    CHECK(painted > (size_t)W * H / 2, "the panel paints most of its surface");

    controlpanel_shutdown();
    free(fb);
}

int main(void)
{
    printf("╔══════════════════════════════════════════════════════════╗\n");
    printf("║  Control Panel applet layout audit                        ║\n");
    printf("╚══════════════════════════════════════════════════════════╝\n");

    const int sizes[][2] = {
        { 800, 560 },      /* the smallest window the panel is allowed to be */
        { 1024, 768 },
        { 1600, 900 },
    };

    struct { const char *name; CpApplet (*make)(void); } apps[] = {
        { "display",  cp_create_display_applet },
        { "network",  cp_create_network_applet },
        { "sound",    cp_create_sound_applet },
        { "theme",    cp_create_theme_applet },
        { "hardware", cp_create_hardware_applet },
        { "firmware", cp_create_firmware_applet },
        { "mirror",   cp_create_mirror_applet },
        { "account",  cp_create_account_applet },
        { "limits",   cp_create_limits_applet },
        { "signin",   cp_create_signin_applet },
        { "ai",       cp_create_ai_applet },
    };

    for (unsigned s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++) {
        int W = sizes[s][0], H = sizes[s][1];
        printf("\n== %dx%d ==\n", W, H);
        vbe_init(W, H);
        vbe_reset_clip();
        wubu_theme_set(THEME_DARK);

        for (unsigned i = 0; i < sizeof(apps) / sizeof(apps[0]); i++) {
            CpApplet a = apps[i].make();
            audit_applet(apps[i].name, &a, W, H);
        }
        audit_control_panel(W, H);
    }

    printf("\n  applet_layout: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail ? 1 : 0;
}
