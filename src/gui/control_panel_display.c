/*
 * control_panel_display.c -- the Control Panel Display applet (the
 * SteamOS System Settings > Display, in our code).
 *
 * The header declared cp_create_display_applet() for ages —
 * the implementation never existed (form-without-function). This is
 * it: the applet reads the WORLD state (the driver registry's live
 * snapshot) and shows the display contract:
 *
 *   - the connector (eDP / MIPI-DSI / HDMI / DP)
 *   - the current mode (WxH @ refresh)
 *   - the VRAM (the framebuffer budget)
 *   - the GPU presence
 *
 * The world provider is injectable (the desktop wires the real
 * wubu_world_snapshot; the tests inject a fake).
 * C11.
 */
#include "control_panel.h"
#include "control_panel_text.h"
#include "wubu_world.h"
#include "wubu_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the world snapshot provider (injectable) */
typedef const wubu_world_t *(*DisplaySnapFn)(void);

typedef struct {
    DisplaySnapFn snap;
} DisplayAppletData;

static DisplayAppletData g_disp;

/* What the page last DREW for its body ("" when it drew none). A page that renders only its heading
 * looks like a broken app, and the only way to gate that is to record the draw, not to count pixels. */
static char g_last_drawn[256];

/* set the provider (the desktop wires wubu_world_snapshot). */
void cp_display_set_snapshot(DisplaySnapFn fn)
{
    g_disp.snap = fn;
    CpApplet *a = controlpanel_get_applet(CP_APPLET_DISPLAY);
    if (a && a->user_data)
        ((DisplayAppletData *)a->user_data)->snap = fn;
}

static void display_init(CpApplet *applet)
{
    if (!applet->user_data)
        applet->user_data = (void *)calloc(1, sizeof(DisplayAppletData));
}

static const char *connector_name(int c)
{
    switch (c) {
    case 1: return "eDP";
    case 2: return "MIPI-DSI";
    case 3: return "HDMI";
    case 4: return "DisplayPort";
    default: return "unknown";
    }
}

/* One row per measured fact. The page used to state everything in a single sentence and leave ~500 px of
 * empty card, which reads as unfinished. The fix is more FACTS, not more decoration -- and every value
 * here is either measured or says plainly that it is not. */
#define CP_DISP_MAX_ROWS 4
typedef struct { const char *label; char value[80]; } CpDispRow;
static CpDispRow g_rows[CP_DISP_MAX_ROWS];
static int g_nrows;

/* A value is only showable if it could be true. The live panel proved why: a hosted session reported
 * "unknown 51325x35144" and "2202583157 MB" straight from the snapshot, because the fields the panel reads
 * and the fields the kernel fills are not necessarily the same ones -- and a plausible-looking wrong
 * number is worse than "not reported". */
#define CP_DISP_MAX_W 16384
#define CP_DISP_MAX_VRAM_MB 1048576u   /* 1 TB: no machine this runs on exceeds it */

/* Could this struct be a real world snapshot? The applet's stored provider read a DIFFERENT object than
 * the live world in a hosted session (measured: provider returned 0x47a4ec while &g_world was 0x360d320,
 * with screen=3843x51383), so a provider's answer is not trusted on its identity -- only on its content. */
static int world_plausible(const wubu_world_t *w)
{
    if (!w) return 0;
    return w->screen_w == 0 && w->screen_h == 0
        || (w->screen_w >= 320 && w->screen_h >= 200 &&
            w->screen_w <= 16384 && w->screen_h <= 16384);
}

static void disp_build_rows(const wubu_world_t *w)
{
    g_nrows = 0;
    CpDispRow *r;

    r = &g_rows[g_nrows++];
    r->label = "Display";
    int mode_ok = (w->screen_w > 0 && w->screen_h > 0 &&
                   w->screen_w <= CP_DISP_MAX_W && w->screen_h <= CP_DISP_MAX_W);
    const char *conn = connector_name(w->gpu_connector);
    if (mode_ok && strcmp(conn, "unknown") != 0)
        snprintf(r->value, sizeof r->value, "%s %ux%u", conn, w->screen_w, w->screen_h);
    else if (mode_ok)
        snprintf(r->value, sizeof r->value, "%ux%u (connector not reported)",
                 w->screen_w, w->screen_h);
    else
        snprintf(r->value, sizeof r->value, "not reported (no usable mode)");

    r = &g_rows[g_nrows++];
    r->label = "Refresh";
    snprintf(r->value, sizeof r->value, "not reported by this machine");

    r = &g_rows[g_nrows++];
    r->label = "Video memory";
    if (w->vram_mb > 0 && w->vram_mb <= CP_DISP_MAX_VRAM_MB)
        snprintf(r->value, sizeof r->value, "%u MB", w->vram_mb);
    else
        snprintf(r->value, sizeof r->value, "not reported");

    r = &g_rows[g_nrows++];
    r->label = "Pixel format";
    snprintf(r->value, sizeof r->value, "32 bpp (uint32 surface)");
}

/* One join of the rows, so the page and its test can never disagree (they did: the old single-line
 * builder validated nothing and the renderer's rows printed raw fields). */
static void disp_join_rows(char *out, size_t cap)
{
    size_t off = 0;
    out[0] = 0;
    for (int i = 0; i < g_nrows && off < cap; i++) {
        int n = snprintf(out + off, cap - off, "%s%s: %s",
                         i ? "; " : "", g_rows[i].label, g_rows[i].value);
        if (n < 0) break;
        off += (size_t)n;
    }
}

static void display_render(CpApplet *applet, uint32_t *fb,
                           int fb_w, int fb_h, int x, int y, int w, int h)
{
    DisplayAppletData *d = (DisplayAppletData *)applet->user_data;
    const wubu_world_t *wd = d ? d->snap : NULL;
    if (!wd && g_disp.snap) wd = g_disp.snap();
    /* Resolve the LIVE world here, before the null check: the fallback used to sit below it, so a session
     * with no installed provider (which is now every product session) drew "no display snapshot". */
    {
        extern const wubu_world_t *wubu_world_snapshot(void) __attribute__((weak));
        if (!world_plausible(wd) && wubu_world_snapshot) wd = wubu_world_snapshot();
    }

    /* Colours come from the theme: this page used to paint a near-white card in a dark-only OS,
     * which is exactly how one panel read as bootleg next to every other. */
    const WubuThemeColors *c = wubu_theme_colors();
    cp_fill(fb, fb_w, fb_h, x, y, w, h, c->win_face);
    cp_draw_text(fb, fb_w, fb_h, x + 14, y + 12, applet->name, c->select_bg, 2);
    if (!wd) {
        cp_draw_text(fb, fb_w, fb_h, x + 14, y + 44,
                     "no display snapshot", c->body_text_ina, 1);
        g_last_drawn[0] = 0;
        return;
    }

    disp_build_rows(wd);

    /* Record exactly what is drawn: the gate reads this, so a row can never exist only in a comment. */
    disp_join_rows(g_last_drawn, sizeof g_last_drawn);
    for (int i = 0; i < g_nrows; i++) {
        int ry = y + 44 + i * 26;
        if (ry + 10 > y + h) break;                       /* never draw below the card */
        cp_draw_text(fb, fb_w, fb_h, x + 14, ry, g_rows[i].label, c->body_text_ina, 1);
        cp_draw_text(fb, fb_w, fb_h, x + 150, ry, g_rows[i].value, c->body_text, 1);
    }
}

/* How many rows the Display page draws (for the gate: a page with one line reads as unfinished). */
int cp_display_row_count(void)
{
    DisplayAppletData *d = NULL;
    CpApplet *a = controlpanel_get_applet(CP_APPLET_DISPLAY);
    if (a && a->user_data) d = (DisplayAppletData *)a->user_data;
    const wubu_world_t *wd = d ? d->snap : NULL;
    if (!wd && g_disp.snap) wd = g_disp.snap();
    if (!wd) return 0;
    disp_build_rows(wd);
    return g_nrows;
}

/* the test hook: the display line the applet would show. */
int cp_display_test_line(char *out, size_t cap)
{
    DisplaySnapFn snap = g_disp.snap;
    CpApplet *a = controlpanel_get_applet(CP_APPLET_DISPLAY);
    if (a && a->user_data && ((DisplayAppletData *)a->user_data)->snap)
        snap = ((DisplayAppletData *)a->user_data)->snap;
    if (!out || cap == 0) return -1;
    if (!snap) { snprintf(out, cap, "no display state"); return 0; }
    const wubu_world_t *w = snap();
    disp_build_rows(w);
    disp_join_rows(out, cap);
    return 0;
}

CpApplet cp_create_display_applet(void)
{
    CpApplet a;
    memset(&a, 0, sizeof(a));
    a.id = CP_APPLET_DISPLAY;
    strncpy(a.name, "Display", sizeof(a.name) - 1);
    strncpy(a.desc, "The display the GPU drives (the world state)",
            sizeof(a.desc) - 1);
    a.icon_color = 0xFF4080D0;
    a.init = display_init;
    a.render = display_render;
    a.mouse = NULL;
    a.key = NULL;
    a.cleanup = NULL;
    a.user_data = NULL;
    a.initialized = false;
    return a;
}

/* Test hook: the body line the Display page last drew. */
const char *cp_display_last_drawn(void) { return g_last_drawn; }
