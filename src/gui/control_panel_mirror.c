/*
 * control_panel_mirror.c -- the "Software sources" applet: the package mirrorlist, and the ranking
 * reflector would do.
 *
 * The panel does NOT probe on every frame. Opening it reads the file (cheap); F5 ranks the first
 * MIRROR_PANEL_PROBE mirrors with a short deadline each, so the worst case is bounded by
 * MIRROR_PANEL_PROBE * MIRROR_PANEL_BUDGET_MS and the user can see why a mirror moved.
 *
 * Parity: reflector (CachyOS). When there is no mirrorlist the page says so instead of pretending.
 *
 * C11.
 */
#include "control_panel.h"
#include "wubu_tokens.h"
#include "control_panel_text.h"
#include "../runtime/wubu_mirror.h"
#include "wubu_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIRROR_PANEL_ROWS        6
#define MIRROR_PANEL_PROBE       6
#define MIRROR_PANEL_BUDGET_MS 250

typedef struct {
    WubuMirrorList list;
    bool           loaded;
    bool           ranked;
    char           path[256];
} MirrorAppletData;

static void mirror_load(MirrorAppletData *d)
{
    if (!d) return;
    snprintf(d->path, sizeof d->path, "%s", wubu_mirror_default_path());
    if (wubu_mirror_read(d->path, &d->list) < 0) {
        memset(&d->list, 0, sizeof d->list);
        d->ranked = false;
    }
    d->loaded = true;
}

static void mirror_rank(MirrorAppletData *d)
{
    if (!d || d->list.count == 0) return;
    /* Only the mirrors the panel shows are probed: the panel must never stall on a 64-mirror list. */
    int cap = d->list.count < MIRROR_PANEL_PROBE ? d->list.count : MIRROR_PANEL_PROBE;
    int saved = d->list.count;
    d->list.count = cap;
    wubu_mirror_rank(&d->list, MIRROR_PANEL_BUDGET_MS);
    d->list.count = saved;
    d->ranked = true;
}

static void mirror_init(CpApplet *applet)
{
    if (!applet->user_data)
        applet->user_data = calloc(1, sizeof(MirrorAppletData));
    mirror_load((MirrorAppletData *)applet->user_data);
}

static void mirror_render(CpApplet *applet, uint32_t *fb,
                          int fb_w, int fb_h, int x, int y, int w, int h)
{
    const WubuThemeColors *c = wubu_theme_colors();
    MirrorAppletData *d = (MirrorAppletData *)applet->user_data;
    if (!d) return;
    if (!d->loaded) mirror_load(d);

    cp_fill(fb, fb_w, fb_h, x, y, w, h, c->win_face);
    cp_page_title(fb, fb_w, fb_h, x + 10, y + 6, "Software sources", c->select_bg);
    cp_draw_text(fb, fb_w, fb_h, x + 10, y + 24,
                 "The mirrors the package manager will use. F5 measures them and reorders the list.",
                 c->body_text_ina, 1);

    int ry = y + 50;
    const int step = 20;

    if (d->list.count == 0) {
        char msg[320];
        snprintf(msg, sizeof msg, "No mirrorlist at %s", d->path);
        cp_draw_text(fb, fb_w, fb_h, x + 10, ry, msg, c->body_text, 1);
        cp_draw_text(fb, fb_w, fb_h, x + 10, ry + step,
                     "CachyOS ships /etc/pacman.d/mirrorlist; nothing here is simulated.",
                     c->body_text_ina, 1);
    } else {
        char head[128];
        snprintf(head, sizeof head, "%d servers%s", d->list.count,
                 d->ranked ? "  (measured just now)" : "  (file order; press F5 to measure)");
        cp_draw_text(fb, fb_w, fb_h, x + 10, ry, head, c->body_text, 1);
        ry += step + 4;

        int rows = d->list.count < MIRROR_PANEL_ROWS ? d->list.count : MIRROR_PANEL_ROWS;
        for (int i = 0; i < rows; i++) {
            const WubuMirror *m = &d->list.m[i];
            char line[WUBU_MIRROR_URL_MAX + 48];
            if (d->ranked && m->reachable)
                snprintf(line, sizeof line, "%d. %d ms  %s", i + 1, m->latency_ms, m->url);
            else if (d->ranked)
                snprintf(line, sizeof line, "%d. no answer  %s", i + 1, m->url);
            else
                snprintf(line, sizeof line, "%d. %s", i + 1, m->url);
            cp_draw_text(fb, fb_w, fb_h, x + 10, ry, line,
                         d->ranked && !m->reachable ? c->body_text_ina : c->body_text, 1);
            ry += step;
        }
        if (d->list.count > rows) {
            char more[48];
            snprintf(more, sizeof more, "+%d more", d->list.count - rows);
            cp_draw_text(fb, fb_w, fb_h, x + 10, ry, more, c->body_text_ina, 1);
        }
    }

    cp_draw_text(fb, fb_w, fb_h, x + 10, y + h - cp_text_line_h() - WUBU_SPACE_SM,
                 "Read from the mirrorlist file; latency measured by TCP connect.",
                 c->body_text_ina, 1);
}

static void mirror_mouse(CpApplet *applet, int x, int y, int btn, int kind)
{
    (void)x; (void)y; (void)btn; (void)kind;
    if (applet->user_data) mirror_load((MirrorAppletData *)applet->user_data);
}

static void mirror_key(CpApplet *applet, uint32_t key, uint32_t mods)
{
    (void)mods;
    if (key == 0x3F && applet->user_data)      /* F5 ranks */
        mirror_rank((MirrorAppletData *)applet->user_data);
}

static void mirror_cleanup(CpApplet *applet)
{
    if (applet->user_data) {
        free(applet->user_data);
        applet->user_data = NULL;
    }
}

CpApplet cp_create_mirror_applet(void)
{
    CpApplet a;
    memset(&a, 0, sizeof(a));
    a.id = CP_APPLET_MIRROR;
    snprintf(a.name, sizeof(a.name), "Software sources");
    snprintf(a.desc, sizeof(a.desc), "The package mirrorlist, ranked by measured latency");
    a.icon_color = 0xFF00A050;
    a.init = mirror_init;
    a.render = mirror_render;
    a.mouse = mirror_mouse;
    a.key = mirror_key;
    a.cleanup = mirror_cleanup;
    return a;
}

/* Test hook: how many servers the applet read, and the first one's URL. */
int cp_mirror_test_rows(char *first_url, size_t cap)
{
    CpApplet *a = controlpanel_get_applet(CP_APPLET_MIRROR);
    if (!a || !a->user_data) return -1;
    MirrorAppletData *d = (MirrorAppletData *)a->user_data;
    if (first_url && cap)
        snprintf(first_url, cap, "%s", d->list.count ? d->list.m[0].url : "");
    return d->list.count;
}
