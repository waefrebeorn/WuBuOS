/*
 * edr_dash.c -- EDR Activity Dashboard (the disclosure surface)
 *
 * This is the user-facing half of WuBuOS's security model: DISCLOSURE.
 * The OS does not sandbox the AGI; it makes everything the OS AND the AGI do
 * visible, searchable, and replayable. This window tails the live EDR ring
 * (every syscall + every agent input) and renders it as a scrollable,
 * filterable timeline. The "giant toggle" (analytics master switch) lives
 * here too, so the user can flip transparency off -- forfeiting debug-report
 * eligibility, by policy -- and watch the state change in real time.
 *
 * No god headers: only the public EDR API, the WM window API, the theme, and
 * vbe drawing primitives.
 */

#include "wubu_edr.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_wm_internal.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* Maximum events we snapshot per render. The ring is 64k; we only show the
 * most recent window plus whatever the user scrolls back over. */
#define EDR_DASH_MAX   512
#define EDR_DASH_ROW_H  16

typedef enum {
    FILTER_ALL = 0,
    FILTER_AGENT,
    FILTER_PROC,
    FILTER_NET,
    FILTER_FILE,
    FILTER_ALERT,
    FILTER_COUNT
} EdrDashFilter;

typedef struct {
    EdrEventView *buf;        /* reused snapshot buffer */
    int           count;      /* events currently shown */
    int           scroll;     /* first row offset (0 = newest at bottom) */
    EdrDashFilter filter;
    int           auto_tail;  /* 1 = follow newest */
    int           frame;      /* increments each render for live updates */
} EdrDashState;

static const char *g_filter_names[FILTER_COUNT] = {
    "All", "Agent", "Proc", "Net", "File", "Alerts"
};

/* Map a filter to a (min_type, max_type) window over the event taxonomy. */
static void filter_window(EdrDashFilter f, int *min_t, int *max_t) {
    *min_t = 0; *max_t = 0;
    switch (f) {
        case FILTER_AGENT: *min_t = 26; *max_t = 26; break;
        case FILTER_PROC:  *min_t = 1;  *max_t = 4;  break;
        case FILTER_NET:   *min_t = 13; *max_t = 14; break;
        case FILTER_FILE:  *min_t = 5;  *max_t = 8;  break;
        /* Alerts are a separate buffer; handled in draw. */
        case FILTER_ALERT: *min_t = -2; *max_t = -2; break;
        default: break;
    }
}

static EdrDashState *g_dash = NULL;

static EdrDashState *edr_dash_state(void) {
    if (!g_dash) {
        g_dash = (EdrDashState *)calloc(1, sizeof(EdrDashState));
        if (g_dash) { g_dash->buf = (EdrEventView *)calloc(EDR_DASH_MAX, sizeof(EdrEventView)); }
    }
    return g_dash;
}

static void edr_dash_refresh(EdrDashState *s) {
    if (!s || !s->buf) return;
    int min_t, max_t;
    filter_window(s->filter, &min_t, &max_t);
    if (s->filter == FILTER_ALERT) {
        /* Alerts live in a separate buffer and are drawn directly in
         * edr_dash_draw(); nothing to snapshot into the event ring. */
        s->count = 0;
    } else {
        s->count = edr_recent_events(s->buf, EDR_DASH_MAX, min_t, max_t);
    }
    if (s->auto_tail) s->scroll = 0;
}

static void edr_dash_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h, void *user) {
    EdrDashState *s = (EdrDashState *)user;
    (void)fb_w; (void)fb_h; (void)fb;
    if (!win || !s) return;
    int bw  = border_width();
    int tbh = title_bar_height();
    int cx = win->x + bw;
    int cy = win->y + tbh;
    int cw = win->w - 2 * bw;
    int ch = win->h - tbh - bw;

    /* Content background. */
    uint32_t bg = theme()->Luna_start_button ? 0x00F0F0F0 : 0x00C0C0C0;
    vbe_fill_rect(cx, cy, cw, ch, bg);

    /* -- Toolbar (filters + analytics toggle) -- */
    int ty = cy + 4;
    int bx = cx + 8;
    for (int i = 0; i < FILTER_COUNT; i++) {
        int w = 56, h = 18;
        uint32_t col = (i == (int)s->filter) ? 0x0039A0EC : (theme()->Luna_start_button ? 0x00FFFFFF : 0x00D0D0D0);
        vbe_fill_rect(bx, ty, w, h, col);
        vbe_draw_text(bx + 6, ty + 4, g_filter_names[i], 0x00000000, 1);
        bx += w + 6;
    }
    /* Analytics giant toggle (right side of toolbar). */
    int toggle_x = cx + cw - 132, toggle_y = ty, tw = 124, th = 18;
    int on = edr_analytics_enabled();
    vbe_fill_rect(toggle_x, toggle_y, tw, th, on ? 0x002E7D32 : 0x007A1F1F);
    vbe_draw_text(toggle_x + 8, toggle_y + 4, on ? "Analytics: ON" : "Analytics: OFF",
                  0x00FFFFFF, 1);

    /* Divider. */
    int list_y = ty + 24;
    vbe_fill_rect(cx, list_y - 2, cw, 1, 0x00808080);

    /* -- Event list (auto-tail: newest at bottom) -- */
    int rows = (ch - (list_y - cy) - 6) / EDR_DASH_ROW_H;
    int start = 0, end = s->count;
    if (rows > 0 && s->count > 0) {
        if (s->auto_tail) {
            start = (s->count > rows) ? s->count - rows : 0;
            end   = s->count;
        } else {
            start = s->scroll;
            end   = (s->scroll + rows < s->count) ? s->scroll + rows : s->count;
        }
    }
    int ry = list_y + 2;
    for (int i = start; i < end; i++) {
        EdrEventView *v = &s->buf[i];
        /* Timestamp -> simple HH:MM:SS from ns timestamp. */
        uint64_t sec = v->ts_ns / 1000000000ULL;
        int hh = (int)(sec / 3600) % 24, mm = (int)(sec / 60) % 60, ss = (int)(sec % 60);
        char line[200];
        const char *tname = edr_event_type_name(v->type);
        if (v->type == EDR_EV_AGENT_ACTION)
            snprintf(line, sizeof(line), "%02d:%02d:%02d  [AGENT %s] pid=%u  %s",
                     hh, mm, ss, edr_agent_action_name(v->u32), v->pid, v->detail);
        else
            snprintf(line, sizeof(line), "%02d:%02d:%02d  [%s] pid=%u  %s",
                     hh, mm, ss, tname, v->pid, v->detail);
        uint32_t col = (v->type == EDR_EV_AGENT_ACTION) ? 0x000000AA : 0x00000000;
        vbe_draw_text(cx + 8, ry, line, col, 1);
        ry += EDR_DASH_ROW_H;
    }

    /* -- Alerts panel (if filter == Alerts) -- */
    if (s->filter == FILTER_ALERT) {
        EdrAlert alerts[64];
        int na = edr_get_alerts(alerts, 64);
        ry = list_y + 2;
        for (int i = 0; i < na && ry < cy + ch - EDR_DASH_ROW_H; i++) {
            char line[200];
            snprintf(line, sizeof(line), "[%s] %s  pid=%u  %s",
                     alerts[i].severity, alerts[i].rule_name, alerts[i].pid,
                     alerts[i].description);
            vbe_draw_text(cx + 8, ry, line, 0x00AA0000, 1);
            ry += EDR_DASH_ROW_H;
        }
        if (na == 0)
            vbe_draw_text(cx + 8, list_y + 2, "No alerts.", 0x00606060, 1);
    }

    /* Footer: totals. */
    char foot[120];
    snprintf(foot, sizeof(foot), "showing %d events | agent events logged: %llu | toggle=%s",
             s->count, (unsigned long long)edr_agent_events_logged(),
             edr_analytics_enabled() ? "ON" : "OFF");
    vbe_draw_text(cx + 8, cy + ch - 14, foot, 0x00606060, 1);
}

static void edr_dash_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    (void)btn; (void)kind;
    EdrDashState *s = edr_dash_state();
    if (!win || !s) return;
    int bw  = border_width();
    int tbh = title_bar_height();
    int cx = win->x + bw;
    int cy = win->y + tbh;
    int cw = win->w - 2 * bw;

    /* Click on a filter chip? */
    int ty = cy + 4;
    int bx = cx + 8;
    for (int i = 0; i < FILTER_COUNT; i++) {
        int w = 56, h = 18;
        if (x >= bx && x <= bx + w && y >= ty && y <= ty + h) {
            s->filter = (EdrDashFilter)i;
            s->auto_tail = 1; s->scroll = 0;
            edr_dash_refresh(s);
            return;
        }
        bx += w + 6;
    }
    /* Click on the analytics toggle? */
    int toggle_x = cx + cw - 132, toggle_y = ty, tw = 124, th = 18;
    if (x >= toggle_x && x <= toggle_x + tw && y >= toggle_y && y <= toggle_y + th) {
        edr_analytics_set_enabled(!edr_analytics_enabled());
        return;
    }
    /* Wheel / drag scroll on the list (kind==2 = up, btn==2 = right btn->pgdown). */
    /* kind==0 moves don't scroll; we use key for scroll. */
}

static void edr_dash_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)win; (void)mods;
    EdrDashState *s = edr_dash_state();
    if (!s) return;
    if (key == 0xE048) {            /* Up arrow */ s->auto_tail = 0; if (s->scroll > 0) s->scroll--; }
    else if (key == 0xE050) {       /* Down arrow */ s->auto_tail = 0; if (s->scroll < s->count - 1) s->scroll++; }
    else if (key == 'a' || key == 'A') edr_analytics_set_enabled(!edr_analytics_enabled());
    else if (key == 't' || key == 'T') s->auto_tail = !s->auto_tail;
}

DosGuiWindow *edr_dash_launch(void) {
    EdrDashState *s = edr_dash_state();
    if (!s) return NULL;
    edr_dash_refresh(s);
    DosGuiWindow *win = dosgui_wm_create(140, 60, 760, 520, "EDR Activity");
    if (win) {
        win->on_draw  = edr_dash_draw;
        win->on_mouse = edr_dash_mouse;
        win->on_key   = edr_dash_key;
        win->user_data = s;          /* keep state referenced by the window */
    }
    return win;
}
