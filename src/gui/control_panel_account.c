/*
 * control_panel_account.c -- "Your account" (AccountsControl parity).
 *
 * Windows shows name, picture, type and what you can change. Everything here comes from
 * wubu_userinfo (passwd, the home directory, the password tool's presence), so the page cannot drift
 * from the machine: it is the same reading the sign-in path would do.
 *
 * C11.
 */
#include "control_panel.h"
#include "wubu_tokens.h"
#include "control_panel_text.h"
#include "../runtime/wubu_userinfo.h"
#include "wubu_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct {
    WubuUserInfo info;
    bool         loaded;
} AcctAppletData;

static void acct_load(AcctAppletData *d)
{
    if (!d) return;
    wubu_userinfo_read((int)getuid(), &d->info);
    d->loaded = true;
}

static void acct_init(CpApplet *applet)
{
    if (!applet->user_data)
        applet->user_data = calloc(1, sizeof(AcctAppletData));
    acct_load((AcctAppletData *)applet->user_data);
}

static void row(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w,
                const char *label, const char *value)
{
    const WubuThemeColors *c = wubu_theme_colors();
    cp_row(fb, fb_w, fb_h, x, y, w, label, value ? value : "", c->body_text, c->body_text);
}

static void acct_render(CpApplet *applet, uint32_t *fb,
                        int fb_w, int fb_h, int x, int y, int w, int h)
{
    const WubuThemeColors *c = wubu_theme_colors();
    AcctAppletData *d = (AcctAppletData *)applet->user_data;
    if (!d) return;
    if (!d->loaded) acct_load(d);
    const WubuUserInfo *u = &d->info;

    cp_fill(fb, fb_w, fb_h, x, y, w, h, c->win_face);
    cp_draw_text(fb, fb_w, fb_h, x + 10, y + 6, "Your account", c->select_bg, 2);
    cp_draw_text(fb, fb_w, fb_h, x + 10, y + 24,
                 "Who this session belongs to, as the machine defines it.", c->body_text_ina, 1);

    int ry = y + 52;
    const int step = 22;

    if (!u->found) {
        row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Account:   ", "not found in this machine's passwd");
        ry += step;
        row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Signed in: ", "uid has no passwd record");
    } else {
        row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Name:      ", wubu_userinfo_display(u));
        ry += step;
        row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Type:      ", wubu_userinfo_type(u));
        ry += step;
        row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Sign-in:   ", u->login);
        ry += step;
        row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Home:      ", u->home);
        ry += step;
        row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Shell:     ", u->shell);
        ry += step;
        row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Picture:   ",
            u->has_face ? "custom (~/.face)" : "default (no ~/.face)");
        ry += step;
        row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Password:  ",
            u->can_change_password ? u->passwd_tool : "no password tool on this machine");
    }

    cp_draw_text(fb, fb_w, fb_h, x + 10, y + h - cp_text_line_h() - WUBU_SPACE_SM,
                 "Read from /etc/passwd and the home directory; nothing here is simulated.",
                 c->body_text_ina, 1);
}

static void acct_mouse(CpApplet *applet, int x, int y, int btn, int kind)
{
    (void)x; (void)y; (void)btn; (void)kind;
    if (applet->user_data) acct_load((AcctAppletData *)applet->user_data);
}

static void acct_key(CpApplet *applet, uint32_t key, uint32_t mods)
{
    (void)mods;
    if (key == 0x3F && applet->user_data)      /* F5 re-reads the machine */
        acct_load((AcctAppletData *)applet->user_data);
}

static void acct_cleanup(CpApplet *applet)
{
    if (applet->user_data) {
        free(applet->user_data);
        applet->user_data = NULL;
    }
}

CpApplet cp_create_account_applet(void)
{
    CpApplet a;
    memset(&a, 0, sizeof(a));
    a.id = CP_APPLET_ACCOUNT;
    snprintf(a.name, sizeof(a.name), "Your account");
    snprintf(a.desc, sizeof(a.desc), "The signed-in user, their home, and what can be changed");
    a.icon_color = 0xFF00A050;
    a.init = acct_init;
    a.render = acct_render;
    a.mouse = acct_mouse;
    a.key = acct_key;
    a.cleanup = acct_cleanup;
    return a;
}

/* Test hook: the login the page would draw. */
int cp_account_test_login(char *out, size_t cap)
{
    if (!out || cap == 0) return -1;
    out[0] = '\0';
    CpApplet *a = controlpanel_get_applet(CP_APPLET_ACCOUNT);
    if (!a || !a->user_data) return -1;
    AcctAppletData *d = (AcctAppletData *)a->user_data;
    snprintf(out, cap, "%s", d->info.login);
    return d->info.found ? 0 : -1;
}
