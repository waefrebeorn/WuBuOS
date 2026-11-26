/*
 * open_with_dialog.c -- see open_with_dialog.h.
 *
 * One geometry owner: open_with_rect() decides where the card is, and the row / checkbox / button rects
 * are derived from it by pure functions that the renderer and the hit-test both call. Explorer shipped a
 * dialog whose drawing and hit-test disagreed by 28 px; this one cannot, because there is one expression
 * per rect.
 *
 * C11.
 */
#include "open_with_dialog.h"
#include "../wubu_theme.h"
#include "../../kernel/vbe.h"
#include "../wubu_mime.h"
#include "wubu_gui_metrics.h"

#include <stdio.h>
#include <string.h>

static OpenWithState g_ow;
static char g_last_path[OPEN_WITH_PATH_MAX];
static char g_last_handler[128];

static void lower(char *s)
{
    for (; *s; s++)
        if (*s >= 'A' && *s <= 'Z') *s = (char)(*s - 'A' + 'a');
}

static bool already(const OpenWithApp *apps, int n, const char *id)
{
    for (int i = 0; i < n; i++)
        if (strcmp(apps[i].id, id) == 0) return true;
    return false;
}

int open_with_candidates(const char *path, OpenWithApp *out, int cap)
{
    if (!path || !out || cap <= 0) return 0;
    memset(out, 0, sizeof(OpenWithApp) * (size_t)cap);

    const char *mime = wubu_mime_guess_type(path);
    const MimeSystem *ms = wubu_mime_state();
    int n = 0;

    /* 1. The type's registered default comes first: it is the answer the OS would use anyway. */
    const MimeTypeEntry *te = wubu_mime_lookup_by_extension(path);
    if (te && te->default_handler[0] && n < cap) {
        const DesktopEntry *de = wubu_mime_get_desktop_entry(te->default_handler);
        snprintf(out[n].id, sizeof out[n].id, "%.127s", te->default_handler);
        snprintf(out[n].name, sizeof out[n].name, "%.127s",
                 (de && de->name[0]) ? de->name : te->default_handler);
        n++;
    }

    /* 2. Every desktop entry that declares it can open this type. */
    if (ms && mime) {
        char want[128];
        snprintf(want, sizeof want, "%s", mime);
        lower(want);
        for (int i = 0; i < ms->desktop_entry_count && n < cap; i++) {
            const DesktopEntry *de = &ms->desktop_entries[i];
            if (!de->id[0] || already(out, n, de->id)) continue;

            char types[512];
            snprintf(types, sizeof types, "%s", de->mime_types);
            lower(types);
            if (!strstr(types, want)) continue;

            snprintf(out[n].id, sizeof out[n].id, "%s", de->id);
            snprintf(out[n].name, sizeof out[n].name, "%s", de->name[0] ? de->name : de->id);
            n++;
        }
    }
    return n;
}

bool open_with_open(const char *path)
{
    if (!path || !path[0]) return false;

    memset(&g_ow, 0, sizeof g_ow);
    snprintf(g_ow.path, sizeof g_ow.path, "%s", path);
    snprintf(g_ow.mime, sizeof g_ow.mime, "%s", wubu_mime_guess_type(path));

    const char *dot = strrchr(path, '.');
    if (dot && strlen(dot) < sizeof g_ow.ext) snprintf(g_ow.ext, sizeof g_ow.ext, "%s", dot);

    g_ow.count = open_with_candidates(path, g_ow.apps, OPEN_WITH_MAX_APPS);
    if (g_ow.count <= 0) {
        memset(&g_ow, 0, sizeof g_ow);
        return false;                 /* nothing to offer: the caller keeps its default behaviour */
    }
    g_ow.open = true;
    g_ow.selected = 0;
    /* A user who has to answer this question every time has not answered it. Pre-tick when the type has
     * no association yet -- the first choice SHOULD stick. */
    g_ow.remember = true;
    return true;
}

void open_with_close(void)
{
    memset(&g_ow, 0, sizeof g_ow);
}

bool open_with_is_open(void)
{
    return g_ow.open;
}

const OpenWithState *open_with_state(void)
{
    return &g_ow;
}

const char *open_with_move(int delta)
{
    if (!g_ow.open || g_ow.count <= 0) return NULL;
    int s = (g_ow.selected + delta) % g_ow.count;
    if (s < 0) s += g_ow.count;
    g_ow.selected = s;
    return g_ow.apps[s].id;
}

void open_with_toggle_remember(void)
{
    if (g_ow.open) g_ow.remember = !g_ow.remember;
}

const char *open_with_accept(void)
{
    static char chosen[128];
    chosen[0] = '\0';
    if (!g_ow.open || g_ow.count <= 0) return NULL;

    const OpenWithApp *app = &g_ow.apps[g_ow.selected];
    snprintf(chosen, sizeof chosen, "%s", app->id);
    /* Keep path+handler together: the caller asks this module to launch, it does not rebuild the pair. */
    snprintf(g_last_path, sizeof g_last_path, "%s", g_ow.path);
    snprintf(g_last_handler, sizeof g_last_handler, "%s", app->id);

    if (g_ow.remember) {
        if (g_ow.ext[0]) wubu_mime_associate(g_ow.ext, app->id, MIME_ACTION_OPEN);
        if (g_ow.mime[0]) wubu_mime_set_default(g_ow.mime, app->id);
    }
    g_ow.open = false;
    return chosen;
}

int open_with_launch_last(void)
{
    if (!g_last_path[0] || !g_last_handler[0]) return -1;
    return wubu_mime_launch(g_last_path, g_last_handler);
}

/* -- Geometry ---------------------------------------------------------- */

#define OW_W 460
#define OW_ROW_H 26
#define OW_HEAD 74
#define OW_FOOT 52

bool open_with_rect(int fb_w, int fb_h, int *x, int *y, int *w, int *h)
{
    int n = g_ow.count > 0 ? g_ow.count : 1;
    int hh = OW_HEAD + n * OW_ROW_H + OW_FOOT;
    if (fb_w < OW_W + 40 || fb_h < hh + 40) return false;
    if (x) *x = (fb_w - OW_W) / 2;
    if (y) *y = (fb_h - hh) / 2;
    if (w) *w = OW_W;
    if (h) *h = hh;
    return true;
}

/* The ONE expression for "where is row i", given the card's origin. The renderer and the hit-test both
 * call it; a gate that wants to click a row asks the same function rather than recomputing the pitch. */
static void row_rect(int x, int y, int i, int *rx, int *ry, int *rw, int *rh)
{
    if (rx) *rx = x + 16;
    if (ry) *ry = y + OW_HEAD + i * OW_ROW_H;
    if (rw) *rw = OW_W - 32;
    if (rh) *rh = OW_ROW_H;
}

bool open_with_row_rect(int fb_w, int fb_h, int i, int *rx, int *ry, int *rw, int *rh)
{
    int cx, cy, cw, ch;
    if (i < 0 || i >= g_ow.count) return false;
    if (!open_with_rect(fb_w, fb_h, &cx, &cy, &cw, &ch)) return false;
    row_rect(cx, cy, i, rx, ry, rw, rh);
    return true;
}

static void foot_rect(int x, int y, int h, int *cbx, int *cby, int *okx, int *cax)
{
    if (cbx) *cbx = x + 16;
    if (cby) *cby = y + h - OW_FOOT + 6;
    if (okx) *okx = x + OW_W - 16 - 88 - 8 - 88;
    if (cax) *cax = x + OW_W - 16 - 88;
}

int open_with_hit(int mx, int my, int fb_w, int fb_h)
{
    int x, y, w, h;
    if (!g_ow.open || !open_with_rect(fb_w, fb_h, &x, &y, &w, &h)) return -1;
    if (mx < x || mx >= x + w || my < y || my >= y + h) return -1;

    for (int i = 0; i < g_ow.count; i++) {
        int rx, ry, rw, rh;
        row_rect(x, y, i, &rx, &ry, &rw, &rh);
        if (mx >= rx && mx < rx + rw && my >= ry && my < ry + rh) return i;
    }
    int cbx, cby, okx, cax;
    foot_rect(x, y, h, &cbx, &cby, &okx, &cax);
    int fy = y + h - OW_FOOT + 6;
    if (my >= fy && my < fy + 22) {
        if (mx >= cbx && mx < cbx + 240) return -2;
        if (mx >= cax && mx < cax + 88) return -3;
        if (mx >= okx && mx < okx + 88) return -4;
    }
    return -1;
}

int open_with_click(int mx, int my, int fb_w, int fb_h)
{
    int hit = open_with_hit(mx, my, fb_w, fb_h);
    if (hit >= 0) {
        g_ow.selected = hit;
        return 0;
    }
    if (hit == -2) { open_with_toggle_remember(); return 0; }
    if (hit == -3) { open_with_close(); return 2; }
    if (hit == -4) { open_with_accept(); return 1; }
    return 0;
}

/* -- Render ------------------------------------------------------------ */

void open_with_render(uint32_t *fb, int fb_w, int fb_h)
{
    int x, y, w, h;
    if (!g_ow.open || !open_with_rect(fb_w, fb_h, &x, &y, &w, &h)) return;
    const WubuThemeColors *c = wubu_theme_colors();

    /* The scrim: the rest of the desktop is not clickable while this is up, so it must not look live. */
    for (int yy = 0; yy < fb_h; yy += 2)
        vbe_fill_rect(0, yy, fb_w, 1, c->win_shadow);

    vbe_fill_rect_rounded(x, y, w, h, WUBU_METRIC_RADIUS_WINDOW, c->win_face);
    vbe_rect_rounded(x, y, w, h, WUBU_METRIC_RADIUS_WINDOW, c->border_dark);

    vbe_draw_text(x + 16, y + 14, "How do you want to open this file?", c->win_title_text, 1);
    const char *name = strrchr(g_ow.path, '/');
    vbe_draw_text(x + 16, y + 36, name ? name + 1 : g_ow.path, c->body_text_ina, 1);
    vbe_draw_text(x + 16, y + 54, g_ow.mime, c->body_text_ina, 1);

    for (int i = 0; i < g_ow.count; i++) {
        int rx, ry, rw, rh;
        row_rect(x, y, i, &rx, &ry, &rw, &rh);
        bool sel = (i == g_ow.selected);
        if (sel) vbe_fill_rect_rounded(rx, ry, rw, rh, WUBU_METRIC_RADIUS_CONTROL, c->select_bg);
        vbe_draw_text(rx + 10, ry + 6, g_ow.apps[i].name, sel ? c->select_text : c->body_text, 1);
    }

    int cbx, cby, okx, cax;
    foot_rect(x, y, h, &cbx, &cby, &okx, &cax);
    int fy = y + h - OW_FOOT + 6;
    vbe_rect_rounded(cbx, fy + 3, 12, 12, 2, c->border_dark);
    if (g_ow.remember)
        vbe_fill_rect_rounded(cbx + 3, fy + 6, 6, 6, 1, c->select_bg);
    vbe_draw_text(cbx + 20, fy + 2, "Always use this app", c->body_text, 1);

    vbe_fill_rect_rounded(okx, fy, 88, 22, WUBU_METRIC_RADIUS_CONTROL, c->select_bg);
    vbe_draw_text(okx + 8, fy + 6, "Open", c->select_text, 1);
    vbe_fill_rect_rounded(cax, fy, 88, 22, WUBU_METRIC_RADIUS_CONTROL, c->btn_face);
    vbe_draw_text(cax + 8, fy + 6, "Cancel", c->btn_text, 1);
}
