/*
 * print_dialog.c -- Win11's Print dialog, over the real spooler.
 *
 * The gap matrix had printing as the largest hole with NO owner at all. The backend
 * (runtime/wubu_print.c) enumerates the machine's printers and submits jobs; this is the surface a
 * user actually sees: pick a printer, set copies, Print.
 *
 * RULES IT FOLLOWS
 *   * Every number comes from include/wubu_gui_metrics.h or the theme -- no hand-typed rows.
 *   * It never claims success the backend did not report: the result line says SUBMITTED with the
 *     spooler's job id, or the spooler's own refusal.
 *   * A machine with no spooler says so in the dialog instead of offering a Print button that
 *     cannot work.
 *   * Render and hit-test are built from ONE geometry function so a click cannot land on a button
 *     that is not drawn there.
 *
 * C11, minimal includes.
 */
#include "print_dialog.h"

#include "wubu_gui_metrics.h"
#include "../wubu_tokens.h"
#include "../kernel/vbe.h"
#include "../wubu_theme.h"
#include "../../runtime/wubu_print.h"

#include <stdio.h>
#include <string.h>

#define PD_W        WUBU_METRIC_DIALOG_W
#define PD_H        WUBU_METRIC_DIALOG_H
#define PD_PAD      WUBU_SPACE_LG
#define PD_ROW_H    WUBU_METRIC_CONTROL_H
#define PD_BTN_W    112
#define PD_STEP_W   32
#define PD_MAX_PRINTERS 8

static struct {
    int  open;
    int  selected;
    int  copies;
    int  count;
    char path[256];
    char message[192];
    WubuPrintResult result;
    int  have_result;
    WubuPrinter printers[PD_MAX_PRINTERS];
} g_pd;

/* -- geometry: one owner for every rect ---------------------------------- */
typedef struct { int x, y, w, h; } PdRect;

static void pd_box(int fb_w, int fb_h, PdRect *r) {
    r->w = PD_W < fb_w - 2 * WUBU_SPACE_MD ? PD_W : fb_w - 2 * WUBU_SPACE_MD;
    r->h = PD_H < fb_h - 2 * WUBU_SPACE_MD ? PD_H : fb_h - 2 * WUBU_SPACE_MD;
    r->x = (fb_w - r->w) / 2;
    r->y = (fb_h - r->h) / 2;
}

static void pd_layout(int fb_w, int fb_h, PdRect *box, PdRect *list, PdRect *copies,
                      PdRect *print, PdRect *cancel) {
    pd_box(fb_w, fb_h, box);
    int inner_w = box->w - 2 * PD_PAD;
    int y = box->y + PD_PAD;

    /* title + path strip */
    y += WUBU_FONT_INK_H + WUBU_SPACE_MD;
    y += WUBU_FONT_INK_H + WUBU_SPACE_LG;

    list->x = box->x + PD_PAD;
    list->y = y;
    list->w = inner_w;
    list->h = PD_ROW_H * PD_MAX_PRINTERS + WUBU_SPACE_XS * 2;

    y = list->y + list->h + WUBU_SPACE_LG;
    copies->x = box->x + PD_PAD;
    copies->y = y;
    copies->w = inner_w;
    copies->h = PD_ROW_H;

    int by = box->y + box->h - PD_PAD - PD_ROW_H;
    cancel->x = box->x + box->w - PD_PAD - PD_BTN_W;
    cancel->y = by; cancel->w = PD_BTN_W; cancel->h = PD_ROW_H;
    print->x  = cancel->x - WUBU_SPACE_SM - PD_BTN_W;
    print->y  = by; print->w = PD_BTN_W; print->h = PD_ROW_H;
    (void)copies;
}

static int pd_hits(PdRect *r, int mx, int my) {
    return mx >= r->x && mx < r->x + r->w && my >= r->y && my < r->y + r->h;
}

/* -- state --------------------------------------------------------------- */
void print_dialog_open(const char *path) {
    memset(&g_pd, 0, sizeof g_pd);
    snprintf(g_pd.path, sizeof g_pd.path, "%s", path ? path : "");
    g_pd.copies = 1;
    g_pd.count = wubu_print_enum(g_pd.printers, PD_MAX_PRINTERS);
    if (g_pd.count < 0) {                 /* no CUPS: say it here, not after a click */
        g_pd.count = 0;
        snprintf(g_pd.message, sizeof g_pd.message,
                 "No print spooler on this machine (CUPS not found)");
    } else {
        for (int i = 0; i < g_pd.count; i++)
            if (g_pd.printers[i].is_default) g_pd.selected = i;
        if (g_pd.count == 0)
            snprintf(g_pd.message, sizeof g_pd.message, "No printers configured");
    }
    g_pd.open = 1;
}

void print_dialog_close(void) { g_pd.open = 0; }
int  print_dialog_is_open(void) { return g_pd.open; }
int  print_dialog_printer_count(void) { return g_pd.count; }
int  print_dialog_copies(void) { return g_pd.copies; }
const char *print_dialog_message(void) { return g_pd.message; }

/* Do it. The result comes from the spooler, and the dialog shows it verbatim. */
static void pd_submit(void) {
    if (g_pd.count <= 0 || g_pd.selected < 0 || g_pd.selected >= g_pd.count) {
        snprintf(g_pd.message, sizeof g_pd.message, "No printer selected");
        g_pd.have_result = 1;
        return;
    }
    WubuPrintOut out;
    g_pd.result = wubu_print_submit(g_pd.printers[g_pd.selected].name, g_pd.path,
                                    g_pd.copies, &out);
    g_pd.have_result = 1;
    if (g_pd.result == WUBU_PRINT_SUBMITTED)
        snprintf(g_pd.message, sizeof g_pd.message, "Sent: job %s", out.job_id);
    else
        snprintf(g_pd.message, sizeof g_pd.message, "%s %s",
                 wubu_print_result_name(g_pd.result), out.message);
}

void print_dialog_handle_key(uint32_t key, uint32_t mods) {
    (void)mods;
    if (!g_pd.open) return;
    if (key == 27 || key == 0x01) { print_dialog_close(); return; }
    if (key == '\r' || key == '\n' || key == 0x1C) { pd_submit(); return; }
    if (key == 0x48 || key == 0xE048) {          /* up */
        if (g_pd.count) g_pd.selected = (g_pd.selected + g_pd.count - 1) % g_pd.count;
        return;
    }
    if (key == 0x50 || key == 0xE050) {          /* down */
        if (g_pd.count) g_pd.selected = (g_pd.selected + 1) % g_pd.count;
        return;
    }
}

/* Returns true when the dialog consumed the press. */
int print_dialog_handle_click(int mx, int my, int fb_w, int fb_h) {
    if (!g_pd.open) return 0;
    PdRect box, list, copies, print, cancel;
    pd_layout(fb_w, fb_h, &box, &list, &copies, &print, &cancel);

    if (pd_hits(&print, mx, my))  { pd_submit(); return 1; }
    if (pd_hits(&cancel, mx, my)) { print_dialog_close(); return 1; }

    if (pd_hits(&list, mx, my)) {
        int row = (my - list.y - WUBU_SPACE_XS) / PD_ROW_H;
        if (row >= 0 && row < g_pd.count) g_pd.selected = row;
        return 1;
    }
    if (pd_hits(&copies, mx, my)) {
        /* left third steps down, right third steps up, middle does nothing surprising */
        if (mx > copies.x + copies.w * 2 / 3)      g_pd.copies++;
        else if (mx > copies.x + copies.w / 3)     { if (g_pd.copies > 1) g_pd.copies--; }
        return 1;
    }
    /* inside the box: swallowed; outside: dismiss */
    if (!pd_hits(&box, mx, my)) print_dialog_close();
    return 1;
}

void print_dialog_rect(int fb_w, int fb_h, int *x, int *y, int *w, int *h) {
    PdRect box;
    pd_box(fb_w, fb_h, &box);
    if (x) *x = box.x;
    if (y) *y = box.y;
    if (w) *w = box.w;
    if (h) *h = box.h;
}

/* -- render -------------------------------------------------------------- */
void print_dialog_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    if (!g_pd.open) return;

    const WubuThemeColors *c = tc();
    PdRect box, list, copies, print, cancel;
    pd_layout(fb_w, fb_h, &box, &list, &copies, &print, &cancel);

    /* dim the desktop behind a modal */
    vbe_fill_rect(0, 0, fb_w, fb_h, 0x000000);

    vbe_fill_rect_rounded(box.x, box.y, box.w, box.h, WUBU_METRIC_RADIUS_WINDOW, c->win_face);
    vbe_rect_rounded(box.x, box.y, box.w, box.h, WUBU_METRIC_RADIUS_WINDOW, c->border_light);

    int tx = box.x + PD_PAD;
    int ty = box.y + PD_PAD;
    vbe_draw_text(tx, ty, "Print", c->win_title_text, 1);
    ty += WUBU_FONT_INK_H + WUBU_SPACE_MD;
    vbe_draw_text(tx, ty, g_pd.path[0] ? g_pd.path : "(no document)",
                  c->body_text, 1);

    /* printer list */
    vbe_fill_rect_rounded(list.x, list.y, list.w, list.h, WUBU_METRIC_RADIUS_CONTROL, c->btn_face);
    vbe_rect_rounded(list.x, list.y, list.w, list.h, WUBU_METRIC_RADIUS_CONTROL, c->border_light);
    for (int i = 0; i < g_pd.count && i < PD_MAX_PRINTERS; i++) {
        int ry = list.y + WUBU_SPACE_XS + i * PD_ROW_H;
        if (i == g_pd.selected)
            vbe_fill_rect(list.x + WUBU_SPACE_XS, ry, list.w - 2 * WUBU_SPACE_XS, PD_ROW_H,
                          c->select_bg);
        char line[160];
        snprintf(line, sizeof line, "%s%s  \u00b7  %s", g_pd.printers[i].name,
                 g_pd.printers[i].is_default ? "  (default)" : "",
                 g_pd.printers[i].state);
        vbe_draw_text(list.x + WUBU_SPACE_MD,
                      WUBU_TEXT_LABEL_Y(ry, PD_ROW_H), line,
                      i == g_pd.selected ? c->select_text : c->body_text, 1);
    }
    if (g_pd.count == 0)
        vbe_draw_text(list.x + WUBU_SPACE_MD, WUBU_TEXT_LABEL_Y(list.y, PD_ROW_H),
                      "(no printers)", c->body_text, 1);

    /* copies stepper */
    char cl[64];
    snprintf(cl, sizeof cl, "Copies:  %d        (\u25c0 fewer    more \u25b6)", g_pd.copies);
    vbe_draw_text(copies.x, WUBU_TEXT_LABEL_Y(copies.y, copies.h), cl, c->body_text, 1);

    /* result / status line, above the buttons */
    if (g_pd.message[0])
        vbe_draw_text(copies.x, copies.y + copies.h + WUBU_SPACE_XS, g_pd.message,
                      g_pd.have_result ? c->body_text : c->border_dark, 1);

    /* buttons: primary is the accent, secondary is the surface */
    vbe_fill_rect_rounded(print.x, print.y, print.w, print.h, WUBU_METRIC_RADIUS_CONTROL,
                          g_pd.count ? c->select_bg : c->btn_face);
    vbe_draw_text(print.x + (print.w - vbe_text_width("Print", 1)) / 2,
                  WUBU_TEXT_LABEL_Y(print.y, print.h), "Print",
                  g_pd.count ? c->select_text : c->border_dark, 1);

    vbe_fill_rect_rounded(cancel.x, cancel.y, cancel.w, cancel.h, WUBU_METRIC_RADIUS_CONTROL,
                          c->btn_face);
    vbe_rect_rounded(cancel.x, cancel.y, cancel.w, cancel.h, WUBU_METRIC_RADIUS_CONTROL,
                     c->border_light);
    vbe_draw_text(cancel.x + (cancel.w - vbe_text_width("Cancel", 1)) / 2,
                  WUBU_TEXT_LABEL_Y(cancel.y, cancel.h), "Cancel", c->btn_text, 1);

    (void)PD_STEP_W;
}
