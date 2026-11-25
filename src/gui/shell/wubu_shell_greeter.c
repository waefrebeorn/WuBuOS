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
#include "wubu_gui_metrics.h"
#include "wubu_shell_greeter.h"
#include "wm.h"
#include "wubu_theme.h"
#include "../wubu_settings.h"
#include "../../kernel/vbe.h"
#include "../../runtime/wubu_onboarding.h"
#include "../../runtime/wubu_account.h"
#include "../../apps/apps.h"
#include <dirent.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

/* The offers launch apps that live in the app registry.  Weak, so a test (or a
 * minimal build) that does not link the registry still builds -- the offer is
 * simply inert there. */
__attribute__((weak)) DosGuiWindow *launch_getting_started(void);
__attribute__((weak)) DosGuiWindow *launch_vws_designer(void);
__attribute__((weak)) DosGuiWindow *launch_terminal(void);
/* -- Layout (window-relative) --------------------------------------- */
#define GR_PAD        WUBU_METRIC_MARGIN_LG /* was 20: off the canonical ramp */
#define GR_TITLE_Y    22
#define GR_SUB_Y      50
#define GR_ROWS_Y     92
#define GR_ROW_H      WUBU_SHELL_GREETER_ROW_H
#define GR_BTN_W      140
#define GR_BTN_H      WUBU_METRIC_CONTROL_H /* was 34: off the 4px grid */
#define GR_BTN_X      (WUBU_SHELL_GREETER_WIDTH - GR_PAD - GR_BTN_W)
#define GR_BTN_Y      (WUBU_SHELL_GREETER_HEIGHT - GR_PAD - GR_BTN_H)
#define GR_OFFER_Y    (GR_BTN_Y - 12 - 28)
#define GR_OFFER_H    28

static DosGuiWindow *g_win;
static int   g_step;                /* which wizard page is showing */
static int   g_theme = THEME_WIN98_CLASSIC;
static int   g_hover = -1;
static int   g_signed;              /* onboarding_complete() was called */
static char  g_sig[512];
static char  g_user[33],  g_pw[65],  g_pw2[65], g_field;
static char  g_host[65] = "", g_kbd[16] = "us";  static int g_kbd_idx;
/* the offered keyboard layouts, cycled on the account page's 4th field */
static const char *const kLayouts[] = {
    "us", "uk", "de", "fr", "es", "it", "se", "br", NULL };
#define KBD_LAYOUTS ((int)(sizeof(kLayouts)/sizeof(kLayouts[0]) - 1))
/* account field geometry (window-relative) */
#define GR_FLD_LBL   20
#define GR_FLD_VAL   (GR_PAD + 110)
#define GR_FLD_W     (WUBU_SHELL_GREETER_WIDTH - GR_PAD - 110 - GR_PAD)

/* -- Wizard pages --------------------------------------------------- */

#define GR_STEPS WUBU_SHELL_GREETER_STEPS

static const char *const kStepTitle[GR_STEPS] = {
    "Welcome to WuBuOS",
    "Create your account",
    "Connect to the network",
    "Ready to start",
};
static const char *const kStepSub[GR_STEPS] = {
    "Choose your desktop theme.",
    "A username and password protect your machine.",
    "These are the network interfaces WuBuOS can see.",
    "Everything is set. Start when you are ready.",
};
static const char *const kStepHint[GR_STEPS] = {
    "Up/Down to choose a theme, Enter for the next step",
    "Type in the active field, Enter to move to the next, Up/Down to switch",
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
    /* Fail-closed account validation: a blank username, a blank password, or a
     * mismatched confirmation keeps the user on this page. Refusing is cheaper than
     * persisting an account that either is empty or was never confirmed. */
    if (g_step == 1) {
        if (!g_user[0] || !g_pw[0] || strcmp(g_pw, g_pw2) != 0) return;
    }
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
    /* Microsoft: 8epx between buttons. GAP_SM was 12, which read as "slightly too wide". */
    int gap = WUBU_SPACE_XS;
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
            vbe_fill_rect_rounded(dcx, dcy, 8, 8, 4,
                                  i == g_step ? tc->select_bg : tc->border_light);
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

        /* An unselected row must not look like a button: Win11 list rows are transparent
         * until hover, and the selection is the accent pill -- not a button face with an
         * outline in border_dark (0x1A on a 0x24 card is invisible, so the "selected" row
         * was marked only by an outline nobody could see). */
        uint32_t face = sel ? tc->select_bg : (hov ? tc->btn_hover : tc->win_face);
        vbe_fill_rect_rounded(sx, sy, rw, rh - 2, WUBU_METRIC_RADIUS_CONTROL, face);

        const char *nm = wubu_theme_name((WubuThemeId)i);
        uint32_t tc_col = sel ? tc->select_text : tc->btn_text;
        int tw = vbe_text_width(nm, 1);
        vbe_draw_text(sx + 10, sy + (rh - 2 - 8) / 2, nm, tc_col, 1);

        /* Selected marker: a filled square at the row's right edge. */
        if (sel) {
            vbe_fill_rect(sx + rw - 18, sy + (rh - 2 - 10) / 2, 10, 10,
                          tc->select_text);
        }
        (void)tw;
    }
    } else if (g_step == 1) {
        /* Account page: username + masked password + confirm.  Three fields on the
         * token row grid; the active one gets a highlight so the user can tell where
         * typing lands even though the password shows only asterisks. */
        static const char *const labels[4] = { "Username", "Password", "Confirm", "Keyboard" };
        for (int i = 0; i < 4; i++) {
            int ry = GR_ROWS_Y + i * GR_ROW_H;
            vbe_draw_text(x0 + GR_PAD, y0 + ry, labels[i], tc->btn_text, 1);
            char shown[65];
            if (i == 3) {
                snprintf(shown, sizeof(shown), "%s (Up/Down to change)", g_kbd);
            } else {
                char *val = (i == 0) ? g_user : (i == 1) ? g_pw : g_pw2;
                if (i == 0) {
                    snprintf(shown, sizeof(shown), "%s", val[0] ? val : "(empty)");
                } else {
                    size_t nn = strlen(val);
                    for (size_t k = 0; k < nn && k < sizeof(shown) - 1; k++) shown[k] = '*';
                    shown[nn] = 0;
                    if (!nn) snprintf(shown, sizeof(shown), "(empty)");
                }
            }
            bool active = (g_field == i);
            uint32_t face = active ? tc->select_bg : tc->btn_face;
            vbe_fill_rect_rounded(x0 + GR_FLD_VAL, y0 + ry, GR_FLD_W, GR_ROW_H - 4, 2, face);
            if (active) vbe_rect_rounded(x0 + GR_FLD_VAL, y0 + ry, GR_FLD_W, GR_ROW_H - 4, 2,
                                         tc->border_dark);
            vbe_draw_text(x0 + GR_FLD_VAL + WUBU_METRIC_GAP_SM, y0 + ry + (GR_ROW_H - 4 - WUBU_METRIC_GAP_SM) / 2, shown,
                          active ? tc->select_text : tc->btn_text, 1);
        }
    } else if (g_step == 2) {
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
    /* Win11 default button: flat accent fill, control radius, accent-contrast label. The 3D
     * bevel that used to be here is a Win95/Solaris-era edge -- Microsoft's own guidance is a
     * flat fill with at most a 1px border, and a raise drawn with four colour bands reads as
     * "old" no matter how correct the geometry is. */
    const char *bl = (g_step == GR_STEPS - 1) ? "Start WuBuOS" : "Next";
    vbe_fill_rect_rounded(x0 + bx, y0 + by, bw, bh, WUBU_METRIC_RADIUS_CONTROL,
                          tc->select_bg);
    int blw = vbe_text_width(bl, 1);
    vbe_draw_text(x0 + bx + (bw - blw) / 2, y0 + by + (bh - WUBU_FONT_CAP_H) / 2,
                  bl, tc->select_text, 1);

    /* First-run offers: the "where do I start" answers.  These answer "what do
     * I do next", a question about the FIRST page -- showing them on every step
     * buries that step's own content. */
    static const char *const offers[3] = {
        "Getting Started", "GUI Designer", "Terminal"
    };
    for (int i = 0; (g_step == 0) && i < 3; i++) {
        int ox, oy, ow, oh;
        wubu_shell_greeter_offer_rect(i, &ox, &oy, &ow, &oh);
        vbe_fill_rect_rounded(x0 + ox, y0 + oy, ow, oh, WUBU_METRIC_RADIUS_CONTROL,
                              tc->btn_face);
        vbe_rect_rounded(x0 + ox, y0 + oy, ow, oh, WUBU_METRIC_RADIUS_CONTROL,
                         tc->border_light);
        int tw = vbe_text_width(offers[i], 1);
        vbe_draw_text(x0 + ox + (ow - tw) / 2, y0 + oy + (oh - WUBU_FONT_CAP_H) / 2,
                      offers[i], tc->btn_text, 1);
    }

    /* Footer hint, specific to the current page. */
    /* icon_text_shadow is a DROP-SHADOW colour: using it for text put the hint at the lowest
     * contrast in the palette against the card, which is why it read as a smudge. The secondary
     * body colour is the readable choice, and it sits a full margin off the edge. */
    vbe_draw_text(x0 + GR_PAD, y0 + WUBU_SHELL_GREETER_HEIGHT - GR_PAD + 6,
                  kStepHint[g_step], tc->win_title_text_ina, 1);
    (void)fb_w; (void)fb_h;
}

/* -- Input ---------------------------------------------------------- */

static void finish(void) {
    if (g_signed) return;

    /* APPLY what the user chose.  Until now this function recorded the choice in
     * the signature and closed the window, but never called wubu_theme_set() and
     * never touched settings -- so picking "Zune" left the desktop on whatever
     * theme settings.conf already held (typically Dark, id 5), while the sentinel
     * claimed a theme that was not in effect.  Two sources of truth disagreeing
     * about the single most visible setting in OOBE.
     *
     * The settings object owns the theme; write through it, then sign the file
     * only AFTER the change is real so the signature cannot lie. */
    wubu_theme_set((WubuThemeId)g_theme);
    wubu_settings_mut()->theme.theme_id = g_theme;
    wubu_settings_save();

    /* The signature is now an ACCOUNT RECORD, not just a theme choice: omarchy's
     * deferred provisioning stores the same four facts ($6$ password hash included).
     * If the account cannot be built (shouldn't happen, since advance() validated),
     * fall back to theme-only rather than block the first boot with a fake record. */
    wubu_account_t acc;
    if (g_user[0] && wubu_account_build(&acc, g_user, g_host, g_kbd, g_pw, NULL) == 0) {
        char arec[WUBU_ACCOUNT_RECORD_MAX];
        wubu_account_record_str(&acc, arec, sizeof(arec));
        snprintf(g_sig, sizeof(g_sig), "theme=%s %s",
                 wubu_theme_name((WubuThemeId)g_theme), arec);
    } else {
        snprintf(g_sig, sizeof(g_sig), "theme=%s",
                 wubu_theme_name((WubuThemeId)g_theme));
    }
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

    /* Account page input runs BEFORE the generic back/escape handling, because on
     * this page Backspace edits the field and Enter moves between fields -- treating
     * them as "back one step" would make the form unusable. */
    if (g_step == 1) {
        /* ESC falls through to the generic handling below (back, then dismiss on
         * the theme page). Everything else is consumed by the form. */
        if (key != 27 && key != 0x01) {
        if (key == 8 || key == 127) {                       /* backspace */
            char *f = (g_field == 0) ? g_user : (g_field == 1) ? g_pw : g_pw2;
            size_t n = strlen(f);
            if (n > 0) f[n - 1] = 0;
            return;
        }
        if (key == 0x1C || key == '\r' || key == '\n') {  /* enter: next field */
            if (g_field == 0)        g_field = 1;
            else if (g_field == 1)   g_field = 2;
            else if (g_field == 2)   g_field = 3;          /* -> keyboard */
            else                     wubu_shell_greeter_advance();  /* keyboard: go */
            return;
        }
        if (key == 0xE048) {                               /* up  */
            if (g_field == 3) { g_kbd_idx = (g_kbd_idx - 1 + KBD_LAYOUTS) % KBD_LAYOUTS;
                strncpy(g_kbd, kLayouts[g_kbd_idx], sizeof(g_kbd)-1); return; }
            if (g_field > 0) { g_field--; return; }
        }
        if (key == 0xE050 || key == 0xE04D) {              /* down/right */
            if (g_field == 3) { g_kbd_idx = (g_kbd_idx + 1) % KBD_LAYOUTS;
                strncpy(g_kbd, kLayouts[g_kbd_idx], sizeof(g_kbd)-1); return; }
            if (g_field < 3) { g_field++; return; }
        }
        if (key >= 32 && key < 127) {                        /* printable */
            if (g_field == 3) {                              /* keyboard field: cycle */
                g_kbd_idx = (g_kbd_idx + 1) % KBD_LAYOUTS;
                strncpy(g_kbd, kLayouts[g_kbd_idx], sizeof(g_kbd)-1);
                return;
            }
            char *f = (g_field == 0) ? g_user : (g_field == 1) ? g_pw : g_pw2;
            size_t n = strlen(f);
            size_t cap = (g_field == 0) ? sizeof(g_user) - 1 : sizeof(g_pw) - 1;
            if (n + 1 < cap && !(g_field != 0 && n + 1 >= sizeof(g_pw))) {
                f[n] = (char)key; f[n + 1] = 0;
            }
            return;
        }
        return;   /* all other keys inert on the account page */
        }
        /* ESC: fall through to the generic back/escape handler below. */
    }

    /* Other steps: Escape and Backspace walk the wizard backward; only on the first
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

    if (g_step == 1) {
        for (int i = 0; i < 4; i++) {
            int ry = GR_ROWS_Y + i * GR_ROW_H;
            if (x >= GR_FLD_VAL && x < GR_FLD_VAL + GR_FLD_W &&
                y >= ry && y < ry + GR_ROW_H) { g_field = i; return; }
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
    /* The first-run cursor starts on the PRODUCT DEFAULT theme, not a
     * hardcoded classic: the settings object owns the default (THEME_DARK
     * today). A greeter that preselects a different theme than the product
     * ships is two sources of truth disagreeing about the user's desktop. */
    g_theme = wubu_settings_get()->theme.theme_id;
    if (g_theme < 0 || g_theme >= THEME_COUNT) g_theme = THEME_WIN98_CLASSIC;
    g_user[0] = g_pw[0] = g_pw2[0] = 0; g_field = 0; g_kbd_idx = 0;
    strncpy(g_kbd, kLayouts[0], sizeof(g_kbd) - 1); g_kbd[sizeof(g_kbd)-1] = 0;
    g_host[0] = 0;
    if (gethostname(g_host, sizeof(g_host) - 1) != 0) g_host[0] = 0;

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
