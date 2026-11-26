/*
 * wm_toast.c -- the transient notification card the reference desktop shows on arrival.
 *
 * This OS had a notification CENTRE and no toast: a notification was invisible unless the user opened
 * the centre (Win+N) inside its ten-second lifetime. Windows shows the card, then leaves the item in
 * the centre -- so this renders the newest few active notifications above the taskbar for as long as
 * the model keeps them, and clicking one opens the centre for the rest.
 *
 * The model is not duplicated here: notifications come from wubu_notify_get()/active_count(), the
 * geometry from the NOTIF tokens the centre already uses, and the colours from the theme.
 */
#include "../wm_internal.h"
#include "../wubu_tokens.h"
#include "wubu_notify.h"

/* Windows stacks up to three cards before collapsing into the centre. */
#define TOAST_MAX_VISIBLE 3

void wm_toast_rect(int index, int fb_w, int fb_h, int *x, int *y, int *w, int *h)
{
    int task_h = taskbar_height_dynamic();
    int card_w = WUBU_TOAST_W;
    int card_h = WUBU_TOAST_H;
    if (x) *x = fb_w - card_w - WUBU_SPACE_SM;
    if (y) *y = fb_h - task_h - WUBU_SPACE_SM - (index + 1) * (card_h + WUBU_SPACE_XS);
    if (w) *w = card_w;
    if (h) *h = card_h;
}

/* Truncate with an ellipsis rather than letting text run out of the card. */
static void draw_clipped(int x, int y, const char *s, uint32_t color, int max_px)
{
    char buf[128];
    size_t n = strlen(s);
    if (n >= sizeof buf) n = sizeof buf - 1;
    memcpy(buf, s, n);
    buf[n] = '\0';
    /* Rough proportional trim: the 8x8 font averages ~6 px per glyph at scale 1. */
    int budget = max_px / 6;
    if (budget < 3) budget = 3;
    if ((int)n > budget) {
        buf[budget - 1] = '.';
        buf[budget - 2] = '.';
        buf[budget] = '\0';
    }
    vbe_draw_text(x, y, buf, color, 1);
}

void wm_toast_render(uint32_t *fb, int fb_w, int fb_h)
{
    (void)fb;
    /* The centre shows the same items in full: cards over it would be the panel talking to itself. */
    if (notif_center_is_open()) return;
    const WubuThemeColors *c = tc();
    int count = wubu_notify_active_count();

    int shown = 0;
    for (int i = count - 1; i >= 0 && shown < TOAST_MAX_VISIBLE; i--, shown++) {
        const Notification *n = wubu_notify_get(i);
        if (!n) continue;

        int x, y, w, h;
        wm_toast_rect(shown, fb_w, fb_h, &x, &y, &w, &h);

        /* The card is a window, not an accent chip: the reference desktop paints a neutral surface
         * and lets the urgency stripe carry the colour. */
        vbe_fill_rect_rounded(x, y, w, h, WUBU_METRIC_RADIUS_CONTROL, c->win_face);
        vbe_rect_rounded(x, y, w, h, WUBU_METRIC_RADIUS_CONTROL, c->border_light);

        /* Urgency stripe, the same reading the centre uses. */
        uint32_t urg = (n->urgency == NOTIFY_URGENCY_CRITICAL) ? WUBU_STATUS_CRIT
                     : (n->urgency == NOTIFY_URGENCY_NORMAL)   ? WUBU_STATUS_OK
                                                               : WUBU_STATUS_WARN;
        vbe_fill_rect(x + WUBU_NOTIF_URG_INSET, y + WUBU_NOTIF_URG_INSET,
                      WUBU_NOTIF_URG_W, h - 2 * WUBU_NOTIF_URG_INSET, urg);

        draw_clipped(x + WUBU_NOTIF_TEXT_DX, y + WUBU_NOTIF_URG_INSET,
                     n->app_name, c->icon_text, w - WUBU_NOTIF_TEXT_DX - WUBU_SPACE_SM);
        draw_clipped(x + WUBU_NOTIF_TEXT_DX, y + WUBU_NOTIF_SUMMARY_DY,
                     n->summary, c->win_title_text, w - WUBU_NOTIF_TEXT_DX - WUBU_SPACE_SM);
        /* The body wraps to two lines instead of being cut after the first: "An application stopped
         * unexpectedly. Reports are" is a message that stops mid-sentence. */
        if (n->body[0]) {
            char line[128];
            size_t len = strlen(n->body);
            int per_line = (w - WUBU_NOTIF_TEXT_DX - WUBU_SPACE_SM) / 6;
            if (per_line < 8) per_line = 8;
            size_t cut = (size_t)per_line;
            if (cut > len) cut = len;
            /* Prefer a word boundary. */
            if (cut < len) {
                size_t back = cut;
                while (back > 0 && n->body[back] != ' ') back--;
                if (back > (size_t)per_line / 3) cut = back;
            }
            size_t n1 = cut;
            if (n1 >= sizeof line) n1 = sizeof line - 1;
            memcpy(line, n->body, n1);
            line[n1] = '\0';
            vbe_draw_text(x + WUBU_NOTIF_TEXT_DX, y + WUBU_TOAST_BODY_DY, line, c->icon_text_shadow, 1);

            if (cut < len) {
                const char *rest = n->body + cut;
                while (*rest == ' ') rest++;
                draw_clipped(x + WUBU_NOTIF_TEXT_DX, y + WUBU_TOAST_BODY_DY + WUBU_TOAST_BODY_LINE,
                             rest, c->icon_text_shadow, w - WUBU_NOTIF_TEXT_DX - WUBU_SPACE_SM);
            }
        }
    }
}

/* Which visible card is under the point, or -1. The renderer and this share wm_toast_rect(). */
int wm_toast_at(int mx, int my, int fb_w, int fb_h)
{
    int count = wubu_notify_active_count();
    int shown = 0;
    for (int i = count - 1; i >= 0 && shown < TOAST_MAX_VISIBLE; i--, shown++) {
        int x, y, w, h;
        wm_toast_rect(shown, fb_w, fb_h, &x, &y, &w, &h);
        if (mx >= x && mx < x + w && my >= y && my < y + h) return i;
    }
    /* WUBU_TOAST_PROBE: a press that "did nothing" is answerable from the log -- x/y, the framebuffer
     * the hit-test used, how many cards the model holds, and the first card's rect. */
    if (getenv("WUBU_TOAST_PROBE")) {
        int x, y, w, h;
        wm_toast_rect(0, fb_w, fb_h, &x, &y, &w, &h);
        fprintf(stderr, "[toast] hit at %d,%d fb=%dx%d count=%d card0=%d,%d %dx%d -> -1\n",
                mx, my, fb_w, fb_h, count, x, y, w, h);
    }
    return -1;
}
