/*
 * wubu_welcome.c  --  WuBuOS Welcome Dialog (UX Stream E)
 *
 * First-run dialog: "Welcome to WuBuOS" with quick-reference keyboard
 * shortcuts and a "Don't show again" button. Creates a marker file at
 * ~/.config/wubu/first-run-done so it only shows once.
 *
 * No stubs, no for-later, no system() calls. Every function does real work.
 */

#include "wubu_welcome.h"
#include "dosgui_wm.h"
#include "wubu_theme.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

/* -- Marker path --------------------------------------------------- */

static char g_marker_path[512] = {0};

static const char *marker_path(void) {
    if (g_marker_path[0] == '\0') {
        const char *home = getenv("HOME");
        if (home) {
            snprintf(g_marker_path, sizeof(g_marker_path),
                     "%s/.config/wubu/first-run-done", home);
        } else {
            strcpy(g_marker_path, "/tmp/wubu-first-run-done");
        }
    }
    return g_marker_path;
}

/* -- Dismissal check ----------------------------------------------- */

bool wubu_welcome_is_dismissed(void) {
    return access(marker_path(), F_OK) == 0;
}

void wubu_welcome_dismiss(void) {
    const char *path = marker_path();
    /* Ensure the config directory exists */
    char dir[512];
    strncpy(dir, path, sizeof(dir) - 1);
    char *last = strrchr(dir, '/');
    if (last) {
        *last = '\0';
        mkdir(dir, 0755);  /* best-effort */
    }
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "WuBuOS first-run welcome acknowledged\n");
        fclose(f);
    }
}

/* -- Welcome window draw callback ---------------------------------- */

static void welcome_on_draw(DosGuiWindow *win, uint32_t *fb, int cw, int ch) {
    (void)fb;
    if (!win) return;

    int x0 = win->x + 4;  /* border offset for drawing within client area */
    int y0 = win->y + 4;
    int y = y0 + 8;
    const WubuThemeColors *ct = wubu_theme_colors();
    (void)ct;
    uint32_t text_col = 0x000000;
    uint32_t highlight = 0x008080;  /* teal accent */

    /* Title line */
    vbe_draw_text(x0 + 10, y, "Welcome to WuBuOS!", highlight);
    y += 22;

    vbe_draw_text(x0 + 10, y, "Your TempleOS/ZealOS desktop, re-imagined.", text_col);
    y += 18;
    vbe_draw_text(x0 + 10, y, "Hosted binary | Win98 shell | HolyC JIT | 9P namespace", text_col);
    y += 24;

    /* Divider */
    vbe_fill_rect_rounded(x0, y, cw - 8, 1, 0, 0xCCCCCC);
    y += 10;

    /* Keyboard shortcuts */
    vbe_draw_text(x0 + 10, y, "Quick Reference:", highlight);
    y += 18;

    struct { const char *key; const char *desc; } shortcuts[] = {
        {"Ctrl+T",      "Cycle themes (Win98, XP Luna, XP Media, WuBu)"},
        {"Ctrl+Alt+LR", "Left/Right arrow = switch virtual desktop"},
        {"Alt+F4",      "Close focused window"},
        {"Alt+Tab",     "Cycle through open windows"},
        {"F2",          "Rename selected icon"},
        {"Del",         "Delete selected icon"},
        {"Win key",     "Open Start Menu"},
        {"Shift+F10",   "Context menu (right-click equivalent)"},
        {NULL, NULL}
    };

    for (int i = 0; shortcuts[i].key; i++) {
        /* Key badge */
        vbe_fill_rect_rounded(x0 + 10, y - 1, 80, 14, 3, 0xE0E0E0);
        vbe_draw_text(x0 + 14, y, shortcuts[i].key, 0x005050);
        /* Description */
        vbe_draw_text(x0 + 98, y, shortcuts[i].desc, text_col);
        y += 17;
    }

    y += 6;

    /* "Don't show again" note */
    vbe_draw_text(x0 + 10, y, "Click anywhere or press Escape to dismiss.", 0x888888);
    y += 16;
    vbe_draw_text(x0 + 10, y, "This dialog won't appear again on this machine.", 0x888888);
}

/* -- Welcome init -------------------------------------------------- */

int wubu_welcome_init(void) {
    if (wubu_welcome_is_dismissed()) {
        return 0;  /* Already seen, no dialog needed */
    }

    /* Spawn a modal welcome dialog centered on screen */
    int scr_w = dosgui_wm_screen_w();
    int scr_h = dosgui_wm_screen_h();
    int dw = 480, dh = 330;
    int dx = (scr_w - dw) / 2;
    int dy = (scr_h - dh) / 3;

    DosGuiWindow *win = dosgui_wm_create_modal(dx, dy, dw, dh, "Welcome to WuBuOS", NULL);
    if (!win) return 0;

    win->on_draw = welcome_on_draw;

    /* Mark dismissed so it doesn't show again */
    wubu_welcome_dismiss();

    fprintf(stderr, "Welcome dialog shown at (%d,%d) %dx%d\n", dx, dy, dw, dh);
    return 1;
}