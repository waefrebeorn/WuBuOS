/*
 * control_panel_programs.c -- "Programs": the packaged apps this machine has registered.
 *
 * CP_APPLET_PROGRAMS was declared in the header and never implemented (form without function, the same way
 * cp_create_display_applet sat declared for ages). It is also the natural caller for wubu_pkgx, so the
 * packaged-app runtime has a surface instead of being a library nobody opens.
 *
 * Rows, like the other pages: how many packages, then each one with its version, then the root so a user
 * can find the directory they are installing into. F5 re-reads.
 */
#include "control_panel.h"
#include "control_panel_text.h"
#include "wubu_theme.h"
#include "../runtime/wubu_pkgx.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static WubuPkgRegistry g_reg;
static char g_last_drawn[512];

#define PROGRAMS_ROWS 7

static void programs_reload(void) { wubu_pkgx_list(&g_reg); }

static void programs_applet_init(CpApplet *applet)
{
    if (!applet->user_data)
        applet->user_data = (void *)calloc(1, sizeof(WubuPkgRegistry));
    programs_reload();
}

static void programs_applet_key(CpApplet *applet, unsigned key, unsigned mods)
{
    (void)applet; (void)mods;
    if (key == 0xE034) programs_reload();          /* F5 */
}

static void programs_render(CpApplet *applet, uint32_t *fb,
                            int fb_w, int fb_h, int x, int y, int w, int h)
{
    const WubuThemeColors *c = wubu_theme_colors();
    if (!c) return;

    cp_fill(fb, fb_w, fb_h, x, y, w, h, c->win_face);
    cp_draw_text(fb, fb_w, fb_h, x + 14, y + 12, applet->name, c->select_bg, 2);

    char count[64], ver[PROGRAMS_ROWS][2][128];
    if (g_reg.count == 1) snprintf(count, sizeof count, "1 package");
    else                  snprintf(count, sizeof count, "%d packages", g_reg.count);

    const char *label[PROGRAMS_ROWS];
    const char *value[PROGRAMS_ROWS];
    int rows = 0;
    label[rows] = "Installed"; value[rows] = count; rows++;
    for (int i = 0; i < g_reg.count && rows < PROGRAMS_ROWS - 1; i++) {
        snprintf(ver[i][0], sizeof ver[i][0], "%s", g_reg.a[i].name);
        snprintf(ver[i][1], sizeof ver[i][1], "%s", g_reg.a[i].version);
        label[rows] = ver[i][0];
        value[rows] = ver[i][1];
        rows++;
    }
    if (g_reg.count == 0) {
        label[rows] = "Store"; value[rows] = "no packaged apps installed"; rows++;
    }
    label[rows] = "Root"; value[rows] = wubu_pkgx_default_root(); rows++;

    size_t off = 0;
    g_last_drawn[0] = 0;
    for (int i = 0; i < rows; i++) {
        int n = snprintf(g_last_drawn + off, sizeof g_last_drawn - off, "%s%s: %s",
                         i ? "; " : "", label[i], value[i]);
        if (n < 0) break;
        off += (size_t)n;
        int ry = y + 44 + i * 26;
        if (ry + 10 > y + h) break;
        cp_draw_text(fb, fb_w, fb_h, x + 14, ry, label[i], c->body_text_ina, 1);
        cp_draw_text(fb, fb_w, fb_h, x + 150, ry, value[i], c->body_text, 1);
    }
}

CpApplet cp_create_programs_applet(void)
{
    CpApplet a;
    memset(&a, 0, sizeof(a));
    a.id = CP_APPLET_PROGRAMS;
    strncpy(a.name, "Programs", sizeof(a.name) - 1);
    strncpy(a.desc, "Packaged apps registered on this machine",
            sizeof(a.desc) - 1);
    a.icon_color = 0xFF4CA06A;
    a.init = programs_applet_init;
    a.render = programs_render;
    a.mouse = NULL;
    a.key = programs_applet_key;
    a.cleanup = NULL;
    a.user_data = NULL;
    a.initialized = false;
    return a;
}

/* Test hooks. */
const char *cp_programs_last_drawn(void) { return g_last_drawn; }
void cp_programs_test_reload(void) { programs_reload(); }
