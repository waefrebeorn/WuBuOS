/*
 * control_panel_signin.c -- "Sign-in options": what this machine can do about biometrics, measured.
 *
 * The page reports three facts (reader, enrolment service, enrolled prints) through
 * wubu_biometric_probe, and it never pretends to enrol: enrolment is the service's job, and a button
 * that cannot run it would be the greyed-button lie this tree keeps deleting. What it DOES do is tell
 * the user the exact command, which is the difference between "unavailable" and "here is what to type".
 *
 * F5 re-probes (plugging a reader in should not need a restart).
 *
 * C11.
 */
#include "control_panel.h"
#include "wubu_tokens.h"
#include "control_panel_text.h"
#include "../runtime/wubu_biometric.h"
#include "wubu_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    WubuBiometric bio;
} SignInData;

static void signin_probe(SignInData *d)
{
    if (!d) return;
    (void)wubu_biometric_probe(&d->bio);
}

static void signin_init(CpApplet *applet)
{
    if (!applet->user_data) applet->user_data = calloc(1, sizeof(SignInData));
    signin_probe((SignInData *)applet->user_data);
}

static void row(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w,
                const char *label, const char *value, uint32_t value_color)
{
    const WubuThemeColors *c = wubu_theme_colors();
    cp_row(fb, fb_w, fb_h, x, y, w, label, value ? value : "", c->body_text, value_color);
}

static void signin_render(CpApplet *applet, uint32_t *fb,
                          int fb_w, int fb_h, int x, int y, int w, int h)
{
    const WubuThemeColors *c = wubu_theme_colors();
    SignInData *d = (SignInData *)applet->user_data;
    if (!d) return;

    cp_fill(fb, fb_w, fb_h, x, y, w, h, c->win_face);
    cp_page_title(fb, fb_w, fb_h, x + 10, y + 6, "Sign-in options", c->select_bg);
    cp_draw_text(fb, fb_w, fb_h, x + 10, y + 24,
                 "Fingerprint sign-in, as this machine reports it.", c->body_text_ina, 1);

    int ry = y + 52;
    const int step = 22;
    const WubuBiometric *b = &d->bio;

    row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Reader:    ",
        b->reader_present ? b->reader_path : "none detected",
        b->reader_present ? c->select_bg : 0xFFD06040u);
    ry += step;
    row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Service:   ",
        b->agent_enroll ? b->enroll_path : "no enrolment service (fprintd) installed",
        b->agent_enroll ? c->body_text : c->body_text_ina);
    ry += step;
    char enrolled[64];
    snprintf(enrolled, sizeof enrolled, "%d", b->enrolled);
    row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Enrolled:  ",
        b->enrolled > 0 ? enrolled : "no fingerprints", b->enrolled > 0 ? c->select_bg : c->body_text);
    ry += step;

    char status[200];
    wubu_biometric_status(b, status, sizeof status);
    cp_draw_text(fb, fb_w, fb_h, x + 10, ry + 4, status,
                 b->reader_present && b->agent_enroll ? c->body_text : c->body_text_ina, 1);
    ry += step + 8;

    if (b->reader_present && b->agent_enroll) {
        cp_draw_text(fb, fb_w, fb_h, x + 10, ry,
                     "To enrol a finger, run this in a terminal:", c->body_text, 1);
        ry += 16;
        char cmd[300];
        snprintf(cmd, sizeof cmd, "%s %s", b->enroll_path, getenv("USER") ? getenv("USER") : "");
        cp_draw_text(fb, fb_w, fb_h, x + 20, ry, cmd, c->select_bg, 1);
    }

    cp_draw_text(fb, fb_w, fb_h, x + 10, y + h - cp_text_line_h() - WUBU_SPACE_SM,
                 "Nothing here is simulated: each line is a device or a file that was found. F5 re-checks.",
                 c->body_text_ina, 1);
}

static void signin_key(CpApplet *applet, uint32_t key, uint32_t mods)
{
    (void)mods;
    if (key == 0x3F && applet->user_data)      /* F5: re-probe, a plugged-in reader must appear */
        signin_probe((SignInData *)applet->user_data);
}

static void signin_mouse(CpApplet *applet, int x, int y, int btn, int kind)
{
    (void)applet; (void)x; (void)y; (void)btn; (void)kind;
}

static void signin_cleanup(CpApplet *applet)
{
    if (applet->user_data) {
        free(applet->user_data);
        applet->user_data = NULL;
    }
}

CpApplet cp_create_signin_applet(void)
{
    CpApplet a;
    memset(&a, 0, sizeof(a));
    a.id = CP_APPLET_SIGNIN;
    snprintf(a.name, sizeof(a.name), "Sign-in options");
    snprintf(a.desc, sizeof(a.desc), "Fingerprint sign-in: reader, service and enrolled prints");
    a.icon_color = 0xFF7A5AF8;
    a.init = signin_init;
    a.render = signin_render;
    a.mouse = signin_mouse;
    a.key = signin_key;
    a.cleanup = signin_cleanup;
    return a;
}

/* Test hook: the probe the page is showing. */
const void *cp_signin_test_probe(void)
{
    CpApplet *a = controlpanel_get_applet(CP_APPLET_SIGNIN);
    if (!a || !a->user_data) return NULL;
    return &((SignInData *)a->user_data)->bio;
}
