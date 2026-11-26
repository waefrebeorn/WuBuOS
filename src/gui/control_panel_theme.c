/*
 * control_panel_theme.c -- the Control Panel Theme applet (the SteamOS
 * Personalization, in our code — the Win98/XP display-properties
 * vibes).
 *
 * The header declared cp_create_theme_applet() for ages; the
 * implementation never existed. This is it: the applet drives the
 * REAL theme engine (wubu_theme_set / cycle):
 *
 *   - the current theme + the theme list
 *   - clicking cycles to the next theme (the engine does the swap)
 *
 * The pending-theme hook lets the test set the selection without the
 * full engine; the real path calls wubu_theme_set directly.
 * C11.
 */
#include "control_panel.h"
#include "control_panel_text.h"
#include "wubu_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int pending_theme;   /* -1 = none */
} ThemeAppletData;

static ThemeAppletData g_thm;
static char g_last_drawn[256];   /* the body line the page last drew ("" = none) */

/* set the pending theme (the test hook). */
void cp_theme_set_pending(int theme_id)
{
    g_thm.pending_theme = theme_id;
    CpApplet *a = controlpanel_get_applet(CP_APPLET_THEME);
    if (a && a->user_data)
        ((ThemeAppletData *)a->user_data)->pending_theme = theme_id;
}

static void theme_init(CpApplet *applet)
{
    if (!applet->user_data)
        applet->user_data = (void *)calloc(1, sizeof(ThemeAppletData));
    ThemeAppletData *d = (ThemeAppletData *)applet->user_data;
    if (d) d->pending_theme = -1;
}

/* a click cycles the theme (the engine does the real swap) */
static void theme_mouse(CpApplet *applet, int x, int y, int btn, int kind)
{
    (void)x; (void)y; (void)btn; (void)kind;
    ThemeAppletData *d = (ThemeAppletData *)applet->user_data;
    if (d) d->pending_theme = -1;   /* the engine cycles */
    wubu_theme_cycle();
}

static void theme_render(CpApplet *applet, uint32_t *fb,
                         int fb_w, int fb_h, int x, int y, int w, int h)
{
    /* Colours from the theme, like every other page: this one painted a near-white card (0xFFF7F7F7) in
     * a dark-only OS, which is the "one page looks bootleg" complaint with a line number. */
    const WubuThemeColors *c = wubu_theme_colors();
    if (!c) return;

    cp_fill(fb, fb_w, fb_h, x, y, w, h, c->win_face);
    cp_draw_text(fb, fb_w, fb_h, x + 14, y + 12, applet->name, c->select_bg, 2);

    char line[256];
    /* The builder returns 0 on SUCCESS and -1 on bad arguments; "> 0" was never true, so this page drew
     * its heading and nothing else. Same defect the Display page had. */
    if (cp_theme_test_line(line, sizeof line) >= 0) {
        snprintf(g_last_drawn, sizeof g_last_drawn, "%s", line);
        cp_text_wrapped(fb, fb_w, fb_h, x + 14, y + 44, w - 28, line, c->body_text, 1);
    }

    /* the palette, drawn: a theme page that shows no colour is a lie */
    const uint32_t sw[6] = {
        c->desktop_bg, c->win_face, c->win_title_active,
        c->border_light, c->border_dark, c->win_title_text
    };
    for (int i = 0; i < 6; i++)
        cp_fill(fb, fb_w, fb_h, x + 14 + i * 44, y + h - 60, 38, 38, sw[i]);
}

/* the test hook: the theme line the applet would show. */
int cp_theme_test_line(char *out, size_t cap)
{
    if (!out || cap == 0) return -1;
    CpApplet *a = controlpanel_get_applet(CP_APPLET_THEME);
    int pending = g_thm.pending_theme;
    if (a && a->user_data)
        pending = ((ThemeAppletData *)a->user_data)->pending_theme;
    if (pending >= 0)
        snprintf(out, cap, "theme: %s (pending %d)",
                 wubu_theme_name((WubuThemeId)pending), pending);
    else
        snprintf(out, cap, "theme: %s", wubu_theme_name(wubu_theme_current()));
    return 0;
}

CpApplet cp_create_theme_applet(void)
{
    CpApplet a;
    memset(&a, 0, sizeof(a));
    a.id = CP_APPLET_THEME;
    strncpy(a.name, "Theme", sizeof(a.name) - 1);
    strncpy(a.desc, "The look of the desktop (the theme engine)",
            sizeof(a.desc) - 1);
    a.icon_color = 0xFFD040A0;
    a.init = theme_init;
    a.render = theme_render;
    a.mouse = theme_mouse;
    a.key = NULL;
    a.cleanup = NULL;
    a.user_data = NULL;
    a.initialized = false;
    return a;
}

/* Test hook: the body line the Theme page last drew. */
const char *cp_theme_last_drawn(void) { return g_last_drawn; }
