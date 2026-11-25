/*
 * projection_flyout.c -- Win+P, the display-projection selector.
 *
 * The four rows are Windows': PC screen only, Duplicate, Extend, Second screen only. This
 * machine has ONE output, so only the first can be applied; the other three are drawn greyed
 * and cannot be selected. That is deliberately not a hidden row and not a dead-but-clickable
 * one: Windows greys unavailable modes, and a control that cannot act must not pretend it can.
 *
 * The model is public (projection_row_*) so a test can assert what the flyout offers without
 * reading pixels, and the card is drawn through vbe_* so a pixel gate can see it too.
 */

#include "projection_flyout.h"

#include "wubu_gui_metrics.h"
#include "../wm_internal.h"
#include "../kernel/vbe.h"
#include "../kernel/input.h"
#include "../wubu_theme.h"

#define PW  360
#define PHH 216
#define ROW_H 40
#define PAD   WUBU_SPACE_SM

static const struct {
    const char *label;
    int         mode;
} g_rows[PROJECTION_ROW_COUNT] = {
    { "PC screen only",   PROJECTION_MODE_PC_ONLY   },
    { "Duplicate",        PROJECTION_MODE_DUPLICATE },
    { "Extend",           PROJECTION_MODE_EXTEND    },
    { "Second screen only", PROJECTION_MODE_SECOND  },
};

static bool g_open;
static int  g_selected;          /* always an ENABLED row */
static int  g_mode = PROJECTION_MODE_PC_ONLY;

/* One output is the whole truth on this machine; a second would make rows 1..3 selectable. */
static bool row_has_hardware(int i) { (void)i; return i == PROJECTION_MODE_PC_ONLY; }

int projection_row_count(void)        { return PROJECTION_ROW_COUNT; }
const char *projection_row_label(int i) { return (i >= 0 && i < PROJECTION_ROW_COUNT) ? g_rows[i].label : NULL; }
bool projection_row_enabled(int i)    { return i >= 0 && i < PROJECTION_ROW_COUNT && row_has_hardware(i); }
bool projection_row_checked(int i)    { return i >= 0 && i < PROJECTION_ROW_COUNT && g_rows[i].mode == g_mode; }
int  projection_selected_row(void)    { return g_selected; }
int  projection_mode(void)            { return g_mode; }
bool projection_flyout_is_open(void)  { return g_open; }

int projection_rows_enabled_count(void) {
    int n = 0;
    for (int i = 0; i < PROJECTION_ROW_COUNT; i++) if (projection_row_enabled(i)) n++;
    return n;
}

void projection_flyout_close(void) { g_open = false; }

void projection_flyout_toggle(void) {
    g_open = !g_open;
    if (g_open) {
        /* Start on the applied mode when it is available, otherwise the first that is. */
        g_selected = row_has_hardware(g_mode) ? g_mode : 0;
        while (!projection_row_enabled(g_selected) && g_selected < PROJECTION_ROW_COUNT - 1)
            g_selected++;
    }
}

/* Move the selection within the ENABLED rows only: an arrow landing on a greyed row would
 * make it look pickable. */
static void move_selection(int delta) {
    int i = g_selected;
    for (int step = 0; step < PROJECTION_ROW_COUNT; step++) {
        i += delta;
        if (i < 0) i = PROJECTION_ROW_COUNT - 1;
        if (i >= PROJECTION_ROW_COUNT) i = 0;
        if (projection_row_enabled(i)) { g_selected = i; return; }
    }
}

static void apply_selected(void) {
    if (!projection_row_enabled(g_selected)) return;   /* a disabled row cannot act */
    g_mode = g_rows[g_selected].mode;
    g_open = false;
}

bool projection_flyout_key(uint32_t key, uint32_t mods) {
    if (!g_open) return false;
    if (key == 0x01) { g_open = false; return true; }               /* Esc */
    if (key == 0xE048 || key == 0x48) { move_selection(-1); return true; }
    if (key == 0xE050 || key == 0x50) { move_selection(+1); return true; }
    if (key == 0x1C || key == 0xE01C) { apply_selected(); return true; }
    (void)mods;
    return true;   /* while open it owns the keyboard */
}

bool projection_card_rect(int fb_w, int fb_h, int *x, int *y, int *w, int *h) {
    int cx = fb_w / 2 - PW / 2;
    int cy = fb_h / 2 - PHH / 2;
    if (cx < 0) cx = 0;
    if (cy < 0) cy = 0;
    if (x) *x = cx;
    if (y) *y = cy;
    if (w) *w = PW;
    if (h) *h = PHH;
    return true;
}

bool projection_row_rect(int i, int fb_w, int fb_h, int *x, int *y, int *w, int *h) {
    if (i < 0 || i >= PROJECTION_ROW_COUNT) return false;
    int cx, cy, cw, ch;
    projection_card_rect(fb_w, fb_h, &cx, &cy, &cw, &ch);
    if (x) *x = cx + PAD;
    if (y) *y = cy + PAD + 28 + i * ROW_H;
    if (w) *w = cw - 2 * PAD;
    if (h) *h = ROW_H - 4;
    return true;
}

bool projection_flyout_click(int x, int y) {
    if (!g_open) return true;                 /* open: the card owns the pointer */
    int hit = -1;
    for (int i = 0; i < PROJECTION_ROW_COUNT; i++) {
        int rx, ry, rw, rh;
        if (!projection_row_rect(i, g_dwm.screen_w, g_dwm.screen_h, &rx, &ry, &rw, &rh)) continue;
        if (x >= rx && x < rx + rw && y >= ry && y < ry + rh) { hit = i; break; }
    }
    int cx, cy, cw, ch;
    projection_card_rect(g_dwm.screen_w, g_dwm.screen_h, &cx, &cy, &cw, &ch);
    if (hit >= 0) {
        if (projection_row_enabled(hit)) {
            g_selected = hit;
            apply_selected();
        }
        /* A greyed row consumes the click and changes nothing -- Windows does the same. */
        return true;
    }
    if (x >= cx && x < cx + cw && y >= cy && y < cy + ch) return true;   /* inside the card: consumed */
    g_open = false;                            /* click outside dismisses */
    return true;
}

void projection_flyout_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    if (!g_open) return;
    int cx, cy, cw, ch;
    projection_card_rect(fb_w, fb_h, &cx, &cy, &cw, &ch);

    vbe_fill_rect_rounded(cx, cy, cw, ch, WUBU_METRIC_RADIUS_WINDOW, tc()->win_face);
    vbe_rect(cx, cy, cw, ch, tc()->border_face);
    vbe_draw_text(cx + PAD, cy + PAD, "Project", tc()->win_title_text, 1);

    for (int i = 0; i < PROJECTION_ROW_COUNT; i++) {
        int rx, ry, rw, rh;
        if (!projection_row_rect(i, fb_w, fb_h, &rx, &ry, &rw, &rh)) continue;
        bool en = projection_row_enabled(i);
        if (i == g_selected && en)
            vbe_fill_rect_rounded(rx, ry, rw, rh, WUBU_METRIC_RADIUS_CONTROL, tc()->select_bg);
        uint32_t txt = !en ? tc()->border_dark
                           : (i == g_selected ? tc()->select_text : tc()->win_title_text);
        vbe_draw_text(rx + 12, ry + 10, g_rows[i].label, txt, 1);
        if (projection_row_checked(i))
            vbe_draw_text(rx + rw - 20, ry + 10, "*", txt, 1);
        if (!en)
            vbe_draw_text(rx + rw - 96, ry + 10, "no output", tc()->border_dark, 1);
    }
}
