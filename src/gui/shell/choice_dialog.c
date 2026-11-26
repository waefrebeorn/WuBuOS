/*
 * choice_dialog.c -- see choice_dialog.h. One card, one geometry, one key handler.
 *
 * C11.
 */
#include "choice_dialog.h"

#include "../../kernel/vbe.h"
#include "../wubu_theme.h"
#include "wubu_gui_metrics.h"

#include <stdio.h>
#include <string.h>

/* The card's own numbers. They are layout constants of ONE surface, not per-dialog decisions. */
#define CD_W       460
#define CD_ROW_H   26
#define CD_HEAD    74
#define CD_FOOT    52
#define CD_BTN_W   88
#define CD_BTN_H   22
#define CD_MARGIN  16

void cd_open(ChoiceDialog *d, const char *title, const char *subtitle, bool multi,
             int surface_w, int surface_h)
{
    if (!d) return;
    memset(d, 0, sizeof(*d));
    d->open = true;
    d->multi = multi;
    d->surface_w = surface_w;
    d->surface_h = surface_h;
    snprintf(d->title, sizeof d->title, "%s", title ? title : "");
    snprintf(d->subtitle, sizeof d->subtitle, "%s", subtitle ? subtitle : "");
    snprintf(d->ok_label, sizeof d->ok_label, "OK");
    snprintf(d->cancel_label, sizeof d->cancel_label, "Cancel");
}

int cd_add(ChoiceDialog *d, const char *label, const char *detail, bool checked)
{
    if (!d || !label) return -1;
    if (d->count >= CD_MAX_ROWS) return -1;
    ChoiceRow *r = &d->rows[d->count];
    memset(r, 0, sizeof(*r));
    snprintf(r->label, sizeof r->label, "%s", label);
    snprintf(r->detail, sizeof r->detail, "%s", detail ? detail : "");
    r->checked = checked;
    return d->count++;
}

void cd_close(ChoiceDialog *d)
{
    if (!d) return;
    d->open = false;
    d->count = 0;
    d->selected = 0;
    d->footer[0] = '\0';
    d->footer_checked = false;
}

/* -- Geometry: every rect is a pure function of the surface the dialog was opened on --------------- */

bool cd_rect(const ChoiceDialog *d, int *x, int *y, int *w, int *h)
{
    if (!d || !d->open) return false;
    int n = d->count > 0 ? d->count : 1;
    int hh = CD_HEAD + n * CD_ROW_H + CD_FOOT;
    if (d->surface_w < CD_W + 40 || d->surface_h < hh + 40) return false;
    if (x) *x = (d->surface_w - CD_W) / 2;
    if (y) *y = (d->surface_h - hh) / 2;
    if (w) *w = CD_W;
    if (h) *h = hh;
    return true;
}

bool cd_row_rect(const ChoiceDialog *d, int i, int *x, int *y, int *w, int *h)
{
    int cx, cy, cw, ch;
    if (i < 0 || !d || i >= d->count) return false;
    if (!cd_rect(d, &cx, &cy, &cw, &ch)) return false;
    (void)cw; (void)ch;
    if (x) *x = cx + CD_MARGIN;
    if (y) *y = cy + CD_HEAD + i * CD_ROW_H;
    if (w) *w = CD_W - 2 * CD_MARGIN;
    if (h) *h = CD_ROW_H;
    return true;
}

static bool foot_y(const ChoiceDialog *d, int *cy, int *fy)
{
    int cx, ch;
    if (!cd_rect(d, &cx, cy, NULL, &ch)) return false;
    (void)cx;
    if (fy) *fy = *cy + ch - CD_FOOT + 6;
    return true;
}

bool cd_ok_rect(const ChoiceDialog *d, int *x, int *y, int *w, int *h)
{
    int cx, cy, fy;
    if (!cd_rect(d, &cx, &cy, NULL, NULL)) return false;
    if (!foot_y(d, &cy, &fy)) return false;
    if (x) *x = cx + CD_W - CD_MARGIN - CD_BTN_W - 8 - CD_BTN_W;
    if (y) *y = fy;
    if (w) *w = CD_BTN_W;
    if (h) *h = CD_BTN_H;
    return true;
}

bool cd_cancel_rect(const ChoiceDialog *d, int *x, int *y, int *w, int *h)
{
    int cx, cy, fy;
    if (!cd_rect(d, &cx, &cy, NULL, NULL)) return false;
    if (!foot_y(d, &cy, &fy)) return false;
    if (x) *x = cx + CD_W - CD_MARGIN - CD_BTN_W;
    if (y) *y = fy;
    if (w) *w = CD_BTN_W;
    if (h) *h = CD_BTN_H;
    return true;
}

bool cd_footer_rect(const ChoiceDialog *d, int *x, int *y, int *w, int *h)
{
    int cx, cy, fy;
    if (!d || !d->footer[0]) return false;
    if (!cd_rect(d, &cx, &cy, NULL, NULL)) return false;
    if (!foot_y(d, &cy, &fy)) return false;
    if (x) *x = cx + CD_MARGIN;
    if (y) *y = fy;
    if (w) *w = 240;
    if (h) *h = CD_BTN_H;
    return true;
}

/* -- Input ----------------------------------------------------------------------------------------- */

int cd_hit(ChoiceDialog *d, int x, int y)
{
    int cx, cy, cw, ch;
    if (!d || !cd_rect(d, &cx, &cy, &cw, &ch)) return CD_HIT_NONE;
    if (x < cx || x >= cx + cw || y < cy || y >= cy + ch) return CD_HIT_NONE;

    for (int i = 0; i < d->count; i++) {
        int rx, ry, rw, rh;
        if (!cd_row_rect(d, i, &rx, &ry, &rw, &rh)) continue;
        if (x >= rx && x < rx + rw && y >= ry && y < ry + rh) return i;
    }
    int bx, by, bw, bh;
    if (cd_footer_rect(d, &bx, &by, &bw, &bh) &&
        x >= bx && x < bx + bw && y >= by && y < by + bh) return CD_HIT_FOOTER;
    if (cd_ok_rect(d, &bx, &by, &bw, &bh) &&
        x >= bx && x < bx + bw && y >= by && y < by + bh) return CD_HIT_OK;
    if (cd_cancel_rect(d, &bx, &by, &bw, &bh) &&
        x >= bx && x < bx + bw && y >= by && y < by + bh) return CD_HIT_CANCEL;
    return CD_HIT_NONE;
}

static void move_to(ChoiceDialog *d, int dir)
{
    if (!d || d->count <= 0) return;
    int i = d->selected;
    for (int guard = 0; guard < d->count; guard++) {
        i = (i + dir + d->count) % d->count;
        if (!d->rows[i].disabled) { d->selected = i; return; }
    }
}

bool cd_key(ChoiceDialog *d, uint32_t key)
{
    if (!d || !d->open) return false;

    switch (key) {
    case 0xE048: case 0xE04B:   /* Up / Left */
        move_to(d, -1);
        return true;
    case 0xE050: case 0xE04D:   /* Down / Right */
        move_to(d, +1);
        return true;
    case ' ':
        if (d->footer[0] && d->selected < 0) { d->footer_checked = !d->footer_checked; return true; }
        if (d->count > 0 && !d->rows[d->selected].disabled) {
            if (d->multi) d->rows[d->selected].checked = !d->rows[d->selected].checked;
            else d->rows[d->selected].checked = true;
        }
        return true;
    case 'a': case 'A':          /* multi-select: all / none is a Windows norm worth keeping */
        if (!d->multi) return false;
        {
            bool all = true;
            for (int i = 0; i < d->count; i++)
                if (!d->rows[i].disabled && !d->rows[i].checked) all = false;
            for (int i = 0; i < d->count; i++)
                if (!d->rows[i].disabled) d->rows[i].checked = !all;
        }
        return true;
    default:
        return false;            /* Enter/Esc belong to the owner: only IT knows what accepting means */
    }
}

/* -- Render ---------------------------------------------------------------------------------------- */

void cd_render(const ChoiceDialog *d, uint32_t *fb, int fb_w, int fb_h)
{
    int x, y, w, h;
    if (!d || !cd_rect(d, &x, &y, &w, &h)) return;
    const WubuThemeColors *c = wubu_theme_colors();

    /* The scrim: what is behind this card is not clickable, so it must not look live. */
    for (int yy = 0; yy < fb_h; yy += 2)
        vbe_fill_rect(0, yy, fb_w, 1, c->win_shadow);

    vbe_fill_rect_rounded(x, y, w, h, WUBU_METRIC_RADIUS_WINDOW, c->win_face);
    vbe_rect_rounded(x, y, w, h, WUBU_METRIC_RADIUS_WINDOW, c->border_dark);

    vbe_draw_text(x + CD_MARGIN, y + 14, d->title, c->win_title_text, 1);
    if (d->subtitle[0]) vbe_draw_text(x + CD_MARGIN, y + 36, d->subtitle, c->body_text_ina, 1);

    for (int i = 0; i < d->count; i++) {
        int rx, ry, rw, rh;
        if (!cd_row_rect(d, i, &rx, &ry, &rw, &rh)) continue;
        const ChoiceRow *r = &d->rows[i];
        bool highlight = (i == d->selected);
        bool on = d->multi ? r->checked : highlight;

        if (on) vbe_fill_rect_rounded(rx, ry, rw, rh, WUBU_METRIC_RADIUS_CONTROL,
                                      r->disabled ? c->border_dark : c->select_bg);

        int text_x = rx + 10;
        if (d->multi) {
            vbe_rect_rounded(rx + 8, ry + 7, 12, 12, 2, c->border_dark);
            if (r->checked) vbe_fill_rect_rounded(rx + 11, ry + 10, 6, 6, 1, c->select_bg);
            text_x = rx + 28;
        }

        uint32_t fg = r->disabled ? c->body_text_ina
                     : (on && !d->multi ? c->select_text : c->body_text);
        vbe_draw_text(text_x, ry + 6, r->label, fg, 1);

        if (r->detail[0]) {
            int tw = vbe_text_width(r->detail, 1);
            vbe_draw_text(rx + rw - tw - 10, ry + 6, r->detail, c->body_text_ina, 1);
        }
    }

    int fy;
    int cy;
    if (foot_y(d, &cy, &fy)) {
        int bx, by, bw, bh;
        if (cd_footer_rect(d, &bx, &by, &bw, &bh)) {
            vbe_rect_rounded(bx, by + 3, 12, 12, 2, c->border_dark);
            if (d->footer_checked) vbe_fill_rect_rounded(bx + 3, by + 6, 6, 6, 1, c->select_bg);
            vbe_draw_text(bx + 20, by + 2, d->footer, c->body_text, 1);
        }
        if (cd_ok_rect(d, &bx, &by, &bw, &bh)) {
            vbe_fill_rect_rounded(bx, by, bw, bh, WUBU_METRIC_RADIUS_CONTROL, c->select_bg);
            vbe_draw_text(bx + 8, by + 6, d->ok_label, c->select_text, 1);
        }
        if (cd_cancel_rect(d, &bx, &by, &bw, &bh)) {
            vbe_fill_rect_rounded(bx, by, bw, bh, WUBU_METRIC_RADIUS_CONTROL, c->btn_face);
            vbe_draw_text(bx + 8, by + 6, d->cancel_label, c->btn_text, 1);
        }
    }
}
