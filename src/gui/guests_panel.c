/*
 * guests_panel.c -- L8 desktop Guests window.
 *
 * Lists every imported rootfs (the managed store scanned live by
 * wubu_guest_import_count/entry) with its state, and a Launch button per
 * row that runs the guest through wubu_guest_launch -- the SAME validated
 * wubu_ct_* exec path the terminal 'guest launch' and a 9P write to
 * /n/guests/<name>/launch use. One geometry owner (guests_btn_rect) for
 * the drawn button and its hit-test.
 *
 * The import API is weak here so GUI link lines without the runtime
 * import module still link (the window then honestly says so); the
 * product carries the real one through RT_OBJS.
 */
#include "../egojar.h"
#include "guests_panel.h"
#include "wm.h"
#include "wubu_theme.h"
#include "shell/window_chrome.h"
#include "../kernel/vbe.h"
#include "../runtime/wubu_guest_import.h"

#include <stdio.h>
#include <string.h>

__attribute__((weak)) int wubu_guest_import_count(void);
__attribute__((weak)) int wubu_guest_import_entry(int idx, char *name,
    size_t namesz, char *root, size_t rootsz, char *state, size_t statesz);
__attribute__((weak)) int wubu_guest_launch(const char *name, const char *cmdline);

#define GP_MAX_ROWS 16
#define GP_ROW_H    24
#define GP_BTN_W    76
#define GP_BTN_H    18

/* Last drawn content rect: the mouse hit-test reads what the renderer
 * actually laid out (draw runs every frame before input dispatch). */
static ChromeContentRect g_content;
static int g_content_valid = 0;

/* ONE owner of the per-row Launch button rect (absolute coordinates). */
static void guests_btn_rect(const ChromeContentRect *c, int row,
                            int *x, int *y, int *w, int *h) {
    *x = c->x + c->w - GP_BTN_W - 8;
    *y = c->y + 4 + row * GP_ROW_H + 3;
    *w = GP_BTN_W;
    *h = GP_BTN_H;
}

static void guests_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    const WubuThemeColors *tc = wubu_theme_colors();
    ChromeContentRect c = chrome_draw_window(win, fb, fb_w, fb_h);
    g_content = c;
    g_content_valid = 1;

    vbe_fill_rect(c.x, c.y, c.w, c.h, tc->startmenu_bg);

    if (!wubu_guest_import_count) {
        vbe_draw_text(c.x + 8, c.y + 8,
                      "guest import subsystem not linked in this build",
                      tc->border_dark, 1);
        return;
    }

    int n = wubu_guest_import_count();
    int maxrows = (c.h - 28) / GP_ROW_H;
    if (maxrows > GP_MAX_ROWS) maxrows = GP_MAX_ROWS;
    if (maxrows < 0) maxrows = 0;

    if (n == 0) {
        vbe_draw_text(c.x + 8, c.y + 8, "No guests imported yet.",
                      tc->startmenu_text, 1);
        vbe_draw_text(c.x + 8, c.y + 24,
                      "Terminal: guest import <path> <name>",
                      tc->border_dark, 1);
        vbe_draw_text(c.x + 8, c.y + 40,
                      "or write \"<path> <name>\" to /n/guests/import",
                      tc->border_dark, 1);
        return;
    }

    for (int i = 0; i < n && i < maxrows; i++) {
        char name[64], root[512], state[16];
        if (wubu_guest_import_entry(i, name, sizeof(name), root, sizeof(root),
                                    state, sizeof(state)) != 0) continue;
        int ry = c.y + 4 + i * GP_ROW_H;
        uint32_t bg = (i % 2 == 0) ? tc->startmenu_bg : tc->win_face;
        vbe_fill_rect(c.x + 2, ry, c.w - 4, GP_ROW_H - 1, bg);

        /* state dot: green running, grey imported */
        uint32_t dot = (strcmp(state, "running") == 0) ? 0x0000AA00 : 0x00888888;
        vbe_fill_rect(c.x + 6, ry + 8, 8, 8, dot);

        vbe_draw_text(c.x + 20, ry + 5, name, tc->startmenu_text, 1);
        vbe_draw_text(c.x + 170, ry + 5, state, tc->border_dark, 1);

        int bx, by, bw, bh;
        guests_btn_rect(&c, i, &bx, &by, &bw, &bh);
        chrome_draw_button(bx, by, bw, bh, "Launch", false);
    }

    /* footer hint */
    vbe_draw_text(c.x + 8, c.y + c.h - 16,
                  "Launch runs /bin/sh in the guest root",
                  tc->border_dark, 1);
}

/* What the last click DID, so a failing gate can say "launch returned -3" instead of
 * "no container appeared". A verdict without a cause is a verdict nobody can act on. */
static int g_last_launch_rc = GUESTS_NO_CLICK;
int guests_last_launch_rc(void) { return g_last_launch_rc; }

static void guests_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    (void)win;
    /* This handler used to return SILENTLY on four different conditions, so a
     * click that never launched anything left no evidence at all: the gate could
     * only report "click started a REAL container: false" and had to guess.
     * WUBU_GP_DEBUG=1 says which condition ended the click. */
    int dbg = getenv("WUBU_GP_DEBUG") != NULL;
    g_last_launch_rc = GUESTS_NO_CLICK;
    if (kind != 1 || btn != 1) return;      /* press only */
    if (!g_content_valid || !wubu_guest_import_count || !wubu_guest_launch) {
        if (dbg) fprintf(stderr, "[gp] no click: content_valid=%d count_fn=%d launch_fn=%d\n",
                         g_content_valid, wubu_guest_import_count != NULL,
                         wubu_guest_launch != NULL);
        return;
    }
    /* window-relative -> absolute (the rects are absolute) */
    int ax = win->x + x, ay = win->y + y;
    int n = wubu_guest_import_count();
    int maxrows = (g_content.h - 28) / GP_ROW_H;
    if (maxrows > GP_MAX_ROWS) maxrows = GP_MAX_ROWS;
    for (int i = 0; i < n && i < maxrows; i++) {
        int bx, by, bw, bh;
        guests_btn_rect(&g_content, i, &bx, &by, &bw, &bh);
        if (dbg && i == 0) fprintf(stderr, "[gp] click (%d,%d) row0 rect %d,%d %dx%d\n",
                                  ax, ay, bx, by, bw, bh);
        if (ax < bx || ax >= bx + bw || ay < by || ay >= by + bh) continue;
        char name[64];
        if (wubu_guest_import_entry(i, name, sizeof(name), NULL, 0,
                                    NULL, 0) != 0) return;
        g_last_launch_rc = wubu_guest_launch(name, NULL);
        if (g_last_launch_rc == 0) {
            char body[128];
            snprintf(body, sizeof(body), "%s: /bin/sh started in guest root", name);
            notif_center_add("Guests", "Guest launched", body, 1);
        } else {
            char body[128];
            if (dbg) fprintf(stderr, "[gp] wubu_guest_launch(\"%s\") != 0\n", name);
            snprintf(body, sizeof(body), "%s: launch refused (running?)", name);
            notif_center_add("Guests", "Launch refused", body, 2);
        }
        return;
    }
    g_last_launch_rc = GUESTS_NO_HIT;
}

DosGuiWindow *launch_guests(void) {
    DosGuiWindow *win = wm_create(140, 100, 520, 340, "Guests");
    if (win) {
        win->on_draw = guests_draw;
        win->on_mouse = guests_mouse;
    }
    return win;
}
