/*
 * dosgui_cp_theme.c -- the Control Panel Theme applet (the SteamOS
 * Personalization, in our code — the Win98/XP display-properties
 * vibes).
 *
 * The header declared dosgui_cp_create_theme_applet() for ages; the
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
#include "dosgui_controlpanel.h"
#include "wubu_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    int pending_theme;   /* -1 = none */
} ThemeAppletData;

static ThemeAppletData g_thm = { -1 };

/* set the pending theme (the test hook). */
void dosgui_cp_theme_set_pending(int theme_id)
{
    g_thm.pending_theme = theme_id;
    CpApplet *a = dosgui_controlpanel_get_applet(CP_APPLET_THEME);
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
                         int x, int y, int w, int h)
{
    (void)fb; (void)x; (void)y; (void)w; (void)h;
    (void)applet;
}

/* the test hook: the theme line the applet would show. */
int dosgui_cp_theme_test_line(char *out, size_t cap)
{
    if (!out || cap == 0) return -1;
    CpApplet *a = dosgui_controlpanel_get_applet(CP_APPLET_THEME);
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

CpApplet dosgui_cp_create_theme_applet(void)
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
