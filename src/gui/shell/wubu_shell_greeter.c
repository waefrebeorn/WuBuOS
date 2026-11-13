/*
 * wubu_shell_greeter.c -- GAP F: the visible first-boot greeter.
 *
 * A real modal dialog on the * stack: theme engine colours, VBE
 * primitives, WM input dispatch.  The user picks a desktop theme and presses
 * Start; the greeter persists the onboarding sentinel through
 * wubu_onboarding_complete(), so the next boot goes straight to the desktop.
 *
 * C11, minimal includes, public WM/VBE/theme APIs only.
 */
#include "wubu_shell_greeter.h"
#include "wm.h"
#include "wubu_theme.h"
#include "../../kernel/vbe.h"
#include "../../runtime/wubu_onboarding.h"
#include "../../apps/apps.h"
#include <dirent.h>

/* The offers launch apps that live in the app registry.  Weak, so a test (or a
 * minimal build) that does not link the registry still builds -- the offer is
 * simply inert there. */
__attribute__((weak)) DosGuiWindow *launch_getting_started(void);
__attribute__((weak)) DosGuiWindow *launch_vws_designer(void);
__attribute__((weak)) DosGuiWindow *launch_terminal(void);
#include <string.h>
#include <stdio.h>

/* -- Layout (window-relative) --------------------------------------- */
#define GR_PAD        20
#define GR_TITLE_Y    22
#define GR_SUB_Y      50
#define GR_ROWS_Y     92
#define GR_ROW_H      WUBU_SHELL_GREETER_ROW_H
#define GR_BTN_W      140
#define GR_BTN_H      34
#define GR_BTN_X      (WUBU_SHELL_GREETER_WIDTH - GR_PAD - GR_BTN_W)
#define GR_BTN_Y      (WUBU_SHELL_GREETER_HEIGHT - GR_PAD - GR_BTN_H)
#define GR_OFFER_Y    (GR_BTN_Y - 12 - 28)
#define GR_OFFER_H    28

static DosGuiWindow *g_win;
static int   g_step;                /* which wizard page is showing */
static int   g_theme = THEME_WIN98_CLASSIC;
static int   g_hover = -1;
static int   g_signed;              /* onboarding_complete() was called */
static char  g_sig[128];

/* -- Wizard pages --------------------------------------------------- */

#define GR_STEPS WUBU_SHELL_GREETER_STEPS

static const char *const kStepTitle[GR_STEPS] = {
    "Welcome to WuBuOS",
    "Connect to the network",
    "Ready to start",
};
static const char *const kStepSub[GR_STEPS] = {
    "Choose your desktop theme.",
    "These are the network interfaces WuBuOS can see.",
    "Everything is set. Start when you are ready.",
};
static const char *const kStepHint[GR_STEPS] = {
    "Up/Down to choose a theme, Enter for the next step",
    "Enter to continue, Backspace to go back",
    "Enter to start, Backspace to go back",
};

static void finish(void);       /* defined below; advance() reaches it */

int  wubu_shell_greeter_steps(void) { return GR_STEPS; }
int  wubu_shell_greeter_step(void)  { return g_step; }

void wubu_shell_greeter_back_rect(int *x, int *y, int *w, int *h) {
    int bx, by, bw, bh;
    wubu_shell_greeter_button_rect(&bx, &by, &bw, &bh);
    if (x) *x = bx - 12 - bw;
    if (y) *y = by;
    if (w) *w = bw;
    if (h) *h = bh;
}

void wubu_shell_greeter_advance(void) {
    if (!wubu_shell_greeter_is_open()) return;
    if (g_step < GR_STEPS - 1) { g_step++; return; }
    finish();                       /* last step: same as Start */
}

void wubu_shell_greeter_back(void) {
    if (!wubu_shell_greeter_is_open()) return;
    if (g_step > 0) g_step--;
}

/* Real interface names from the kernel, so the network page shows this machine
 * rather than a generic promise. */
static int net_interface_names(char out[][48], int cap)
{
    DIR *d = opendir("/sys/class/net");
    if (!d) return -1;
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL && n < cap) {
        if (e->d_name[0] == '.') continue;
        snprintf(out[n], 48, "%s", e->d_name);
        n++;
    }
    closedir(d);
    return n;
}

/* -- Geometry (harness-facing) -------------------------------------- */

void wubu_shell_greeter_theme_row(int i, int *x, int *y, int *w, int *h) {
    if (i < 0 || i >= THEME_COUNT) { if (x) *x = 0; if (y) *y = 0;
                                     if (w) *w = 0; if (h) *h = 0; return; }
    if (x) *x = GR_PAD;
    if (y) *y = GR_ROWS_Y + i * GR_ROW_H;
    if (w) *w = WUBU_SHELL_GREETER_WIDTH - GR_PAD * 2;
    if (h) *h = GR_ROW_H;
}

void wubu_shell_greeter_button_rect(int *x, int *y, int *w, int *h) {
    if (x) *x = GR_BTN_X;
    if (y) *y = GR_BTN_Y;
    if (w) *w = GR_BTN_W;
    if (h) *h = GR_BTN_H;
}

void wubu_shell_greeter_offer_rect(int i, int *x, int *y, int *w, int *h) {
    if (i < 0 || i >= 3) { if (x) *x = 0; if (y) *y = 0;
                           if (w) *w = 0; if (h) *h = 0; return; }
    int gap = 8;
    int w3 = (WUBU_SHELL_GREETER_WIDTH - GR_PAD * 2 - gap * 2) / 3;
    if (x) *x = GR_PAD + i * (w3 + gap);
    if (y) *y = GR_OFFER_Y;
    if (w) *w = w3;
    if (h) *h = GR_OFFER_H;
}

int wubu_shell_greeter_selected_theme(void) { return g_theme; }
const char *wubu_shell_greeter_signature(void) { return g_sig; }
bool wubu_shell_greeter_is_open(void) { return g_win && g_win->alive; }

/* -- First-run offers ------------------------------------------------ */
/*
 * The desktop is otherwise silent on a first boot: a theme picker and a Start
 * button.  These three answers are what a new user actually needs -- the path
 * into writing something, the path into designing a window, and the shell.
 */

void wubu_shell_greeter_offer(int i) {
    switch (i) {
    case 0: if (launch_getting_started) launch_getting_started(); break;
    case 1: if (launch_vws_designer)    launch_vws_designer();    break;
    case 2: if (launch_terminal)        launch_terminal();        break;
    default: break;
    }
    /* The offer opened a window of its own; the greeter is a first-run
     * setup dialog and must get out of the way or it sits on top, hiding
     * what it just launched. */
    if (g_win) { wm_destroy(g_win); g_win = NULL; }
}

/* -- Rendering ------------------------------------------------------ */

void wubu_shell_greeter_draw(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    if (!g_win || !g_win->alive) return;

    const WubuThemeColors *tc = wubu_theme_colors();
    int x0 = g_win->x, y0 = g_win->y;

    /* Panel */
    vbe_fill_rect(x0, y0, g_win->w, g_win->h, tc->win_face);

    /* Title + subtitle follow the current page. */
    vbe_draw_text(x0 + GR_PAD, y0 + GR_TITLE_Y, kStepTitle[g_step],
                  tc->win_title_active, 2);
    vbe_draw_text(x0 + GR_PAD, y0 + GR_SUB_Y, kStepSub[g_step],
                  tc->icon_text, 1);

    /* Progress: "Step N of M" plus one pip per page.  A wizard the user cannot
     * locate themselves in is not a wizard. */
    {
        char prog[48];
        snprintf(prog, sizeof(prog), "Step %d of %d", g_step + 1, GR_STEPS);
        int pw = vbe_text_width(prog, 1);
        vbe_draw_text(x0 + g_win->w - GR_PAD - pw, y0 + GR_TITLE_Y + 6,
                      prog, tc->icon_text_shadow, 1);
        for (int i = 0; i < GR_STEPS; i++) {
            int dcx = x0 + g_win->w - GR_PAD - (GR_STEPS - i) * 14;
            int dcy = y0 + GR_TITLE_Y + 22;
            vbe_fill_rect(dcx, dcy, 8, 8,
                          i == g_step ? tc->win_title_active : tc->btn_face);
            vbe_rect(dcx, dcy, 8, 8, tc->border_dark);
        }
    }

    if (g_step == 0) {
    /* Theme rows */
    for (int i = 0; i < THEME_COUNT; i++) {
        int rx, ry, rw, rh;
        wubu_shell_greeter_theme_row(i, &rx, &ry, &rw, &rh);
        int sx = x0 + rx, sy = y0 + ry;
        bool sel = (i == g_theme);
        bool hov = (i == g_hover);

        uint32_t face = sel ? tc->select_bg : (hov ? tc->btn_hover : tc->btn_face);
        vbe_fill_rect_rounded(sx, sy, rw, rh - 2, 2, face);
        if (sel) {
            vbe_rect_rounded(sx, sy, rw, rh - 2, 2, tc->border_dark);
        }

        const char *nm = wubu_theme_name((WubuThemeId)i);
        uint32_t tc_col = sel ? tc->select_text : tc->btn_text;
        int tw = vbe_text_width(nm, 1);
        vbe_draw_text(sx + 10, sy + (rh - 2 - 8) / 2, nm, tc_col, 1);

        /* Selected marker: a filled square at the row's right edge. */
        if (sel) {
            vbe_fill_rect(sx + rw - 18, sy + (rh - 2 - 10) / 2, 10, 10,
                          tc->win_title_active);
        }
        (void)tw;
    }
    } else if (g_step == 1) {
        /* Network page: this machine's real interfaces, or an honest refusal. */
        static char names[8][48];
        int n = net_interface_names(names, 8);
        if (n < 0) {
            vbe_draw_text(x0 + GR_PAD, y0 + GR_ROWS_Y + 6,
                          "Could not read /sys/class/net on this machine.",
                          tc->btn_text, 1);
        } else if (n == 0) {
            vbe_draw_text(x0 + GR_PAD, y0 + GR_ROWS_Y + 6,
                          "No network interfaces found.", tc->btn_text, 1);
        } else {
            for (int i = 0; i < n; i++) {
                char line[72];
                snprintf(line, sizeof(line), "%s", names[i]);
                vbe_draw_text(x0 + GR_PAD, y0 + GR_ROWS_Y + i * 18, line,
                              tc->btn_text, 1);
            }
        }
        vbe_draw_text(x0 + GR_PAD, y0 + GR_ROWS_Y + THEME_COUNT * GR_ROW_H,
                      "Connection setup happens after first boot.",
                      tc->icon_text_shadow, 1);
    } else {
        /* Summary page: what was chosen, and what this machine is. */
        const char *tn = wubu_theme_name((WubuThemeId)g_theme);
        char line[96];
        snprintf(line, sizeof(line), "Theme      %s", tn);
        vbe_draw_text(x0 + GR_PAD, y0 + GR_ROWS_Y + 6, line, tc->btn_text, 1);
        snprintf(line, sizeof(line), "System     WuBuOS");
        vbe_draw_text(x0 + GR_PAD, y0 + GR_ROWS_Y + 28, line, tc->btn_text, 1);
        snprintf(line, sizeof(line), "Steps      %d of %d", g_step + 1, GR_STEPS);
        vbe_draw_text(x0 + GR_PAD, y0 + GR_ROWS_Y + 50, line, tc->btn_text, 1);
        vbe_draw_text(x0 + GR_PAD, y0 + GR_ROWS_Y + 78,
                      "Your choices are saved when you start.",
                      tc->icon_text_shadow, 1);
    }

    /* Primary button (Next, or Start on the last page) */
    int bx, by, bw, bh;
    wubu_shell_greeter_button_rect(&bx, &by, &bw, &bh);
    uint32_t bface = tc->btn_face;
    vbe_fill_rect_rounded(x0 + bx, y0 + by, bw, bh, 3, bface);
    vbe_3d_raised_rounded_colors(x0 + bx, y0 + by, bw, bh, 3,
                                 tc->border_light, tc->btn_face,
                                 tc->border_dark, tc->border_darkest);
    const char *bl = (g_step == GR_STEPS - 1) ? "Start WuBuOS" : "Next";
    int blw = vbe_text_width(bl, 1);
    vbe_draw_text(x0 + bx + (bw - blw) / 2, y0 + by + (bh - 8) / 2,
                  bl, tc->btn_text, 1);

    /* First-run offers: the "where do I start" answers.  These answer "what do
     * I do next", a question about the FIRST page -- showing them on every step
     * buries that step's own content. */
    static const char *const offers[3] = {
        "Getting Started", "GUI Designer", "Terminal"
    };
    for (int i = 0; (g_step == 0) && i < 3; i++) {
        int ox, oy, ow, oh;
        wubu_shell_greeter_offer_rect(i, &ox, &oy, &ow, &oh);
        vbe_fill_rect_rounded(x0 + ox, y0 + oy, ow, oh, 3, tc->btn_face);
        vbe_3d_raised_rounded_colors(x0 + ox, y0 + oy, ow, oh, 3,
                                     tc->border_light, tc->btn_face,
                                     tc->border_dark, tc->border_darkest);
        int tw = vbe_text_width(offers[i], 1);
        vbe_draw_text(x0 + ox + (ow - tw) / 2, y0 + oy + (oh - 8) / 2,
                      offers[i], tc->btn_text, 1);
    }

    /* Footer hint, specific to the current page. */
    vbe_draw_text(x0 + GR_PAD, y0 + WUBU_SHELL_GREETER_HEIGHT - 14,
                  kStepHint[g_step], tc->icon_text_shadow, 1);
    (void)fb_w; (void)fb_h;
}

/* -- Input ---------------------------------------------------------- */

static void finish(void) {
    if (g_signed) return;
    snprintf(g_sig, sizeof(g_sig), "theme=%s", wubu_theme_name((WubuThemeId)g_theme));
    wubu_onboarding_complete(g_sig);
    g_signed = 1;
    if (g_win) {
        wm_set_focus(NULL);
        wm_destroy(g_win);
        g_win = NULL;
    }
}

void wubu_shell_greeter_key(uint32_t key) {
    if (!wubu_shell_greeter_is_open()) return;

    /* Escape and Backspace walk the wizard backward first; only on the first
     * page does Escape still dismiss, because there is nowhere left to go. */
    if (key == 8 || key == 127) { wubu_shell_greeter_back(); return; }
    if (key == 0x01 || key == 27) {
        if (g_step > 0) { wubu_shell_greeter_back(); return; }
        if (g_win) { wm_destroy(g_win); g_win = NULL; }
        return;
    }

    switch (key) {
    case 0xE048: /* up */
        if (g_step != 0) break;
        if (g_theme > 0) g_theme--;
        break;
    case 0xE050: /* down */
        if (g_step != 0) break;
        if (g_theme < THEME_COUNT - 1) g_theme++;
        break;
    case 0xE04B: /* left */
        if (g_step != 0) break;
        if (g_theme > 0) g_theme--;
        break;
    case 0xE04D: /* right */
        if (g_step != 0) break;
        if (g_theme < THEME_COUNT - 1) g_theme++;
        break;
    case 0x1C:  /* enter */
    case '\r':
    case '\n':
        wubu_shell_greeter_advance();
        break;
    default:
        break;
    }
}

void wubu_shell_greeter_click(int x, int y, int kind) {
    if (!wubu_shell_greeter_is_open()) return;
    if (kind != 1) return;   /* act on press only */

    /* Back sits where it was drawn; check it before anything else so a press
     * near the button row cannot be mistaken for the primary action. */
    if (g_step > 0) {
        int kx, ky, kw, kh;
        wubu_shell_greeter_back_rect(&kx, &ky, &kw, &kh);
        if (x >= kx && x < kx + kw && y >= ky && y < ky + kh) {
            wubu_shell_greeter_back();
            return;
        }
    }

    if (g_step == 0) {
        for (int i = 0; i < THEME_COUNT; i++) {
            int rx, ry, rw, rh;
            wubu_shell_greeter_theme_row(i, &rx, &ry, &rw, &rh);
            if (x >= rx && x < rx + rw && y >= ry && y < ry + rh) {
                g_theme = i;
                return;
            }
        }
        for (int i = 0; i < 3; i++) {
            int ox, oy, ow, oh;
            wubu_shell_greeter_offer_rect(i, &ox, &oy, &ow, &oh);
            if (x >= ox && x < ox + ow && y >= oy && y < oy + oh) {
                wubu_shell_greeter_offer(i);
                return;
            }
        }
    }

    int bx, by, bw, bh;
    wubu_shell_greeter_button_rect(&bx, &by, &bw, &bh);
    if (x >= bx && x < bx + bw && y >= by && y < by + bh)
        wubu_shell_greeter_advance();
}

/* -- Callbacks wired into the WM ------------------------------------ */

static void greeter_on_draw(DosGuiWindow *w, uint32_t *fb, int cw, int ch) {
    (void)w; (void)cw; (void)ch;
    wubu_shell_greeter_draw(fb, 0, 0);
}

static void greeter_on_key(DosGuiWindow *w, uint32_t key, uint32_t mods) {
    (void)w; (void)mods;
    wubu_shell_greeter_key(key);
}

static void greeter_on_mouse(DosGuiWindow *w, int x, int y, int btn, int kind) {
    (void)w; (void)btn;
    if (kind == 0) {   /* move: track hover */
        g_hover = -1;
        if (g_step != 0) return;
        for (int i = 0; i < THEME_COUNT; i++) {
            int rx, ry, rw, rh;
            wubu_shell_greeter_theme_row(i, &rx, &ry, &rw, &rh);
            if (x >= rx && x < rx + rw && y >= ry && y < ry + rh) { g_hover = i; break; }
        }
        return;
    }
    wubu_shell_greeter_click(x, y, kind);
}

/* -- Lifecycle ------------------------------------------------------ */

void wubu_shell_greeter_open(void) {
    if (wubu_shell_greeter_is_open()) return;

    int sw = wm_screen_w();
    int sh = wm_screen_h();
    if (sw <= 0 || sh <= 0) return;

    /* Fresh run: a previous completion must not suppress this one. */
    g_signed = 0;
    g_sig[0] = '\0';
    g_hover = -1;
    g_step  = 0;                 /* always start at the first page */

    int dx = (sw - WUBU_SHELL_GREETER_WIDTH) / 2;
    int dy = (sh - WUBU_SHELL_GREETER_HEIGHT) / 3;
    if (dx < 0) dx = 0;
    if (dy < 0) dy = 0;

    DosGuiWindow *w = wm_create_modal(dx, dy, WUBU_SHELL_GREETER_WIDTH,
                                             WUBU_SHELL_GREETER_HEIGHT,
                                             "Welcome to WuBuOS", NULL);
    if (!w) return;
    w->on_draw  = greeter_on_draw;
    w->on_key   = greeter_on_key;
    w->on_mouse = greeter_on_mouse;
    g_win = w;
    wm_set_focus(w);
}

int wubu_shell_greeter_run(void) {
    if (wm_screen_w() <= 0) return -1;   /* no WM initialised */
    wubu_shell_greeter_open();
    return wubu_shell_greeter_is_open() ? 0 : -1;
}
