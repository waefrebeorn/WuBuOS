/*
 * control_panel_limits.c -- "Family limits" (ParentalControls parity).
 *
 * Windows puts this behind a cloud account; a machine can do it locally. The panel is an EDITOR, not a
 * read-only page: a limit you can only change by editing /etc/wubu/parental.conf is a limit most people
 * will never set. Keys:
 *     Space      enable / disable
 *     Tab        move between the start hour and the end hour
 *     Up / Down  move the selected hour by 30 minutes (wrapping through the day)
 *     Enter      save
 *     F5         re-read the file
 * Saving normalizes first (wubu_parental_normalize) so the panel cannot write an impossible policy.
 *
 * C11.
 */
#include "control_panel.h"
#include "wubu_tokens.h"
#include "control_panel_text.h"
#include "../runtime/wubu_parental.h"
#include "wubu_theme.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
    WubuParentalPolicy policy;
    int  field;                 /* 0 = start, 1 = end */
    bool dirty;
    char note[128];             /* what the last action did */
} LimitsData;

static void limits_load(LimitsData *d)
{
    if (!d) return;
    if (wubu_parental_load(&d->policy) != 0) {
        /* No file: a disabled policy with sensible defaults, and the panel says it is not configured
         * rather than pretending the machine has limits it does not have. */
        memset(&d->policy, 0, sizeof d->policy);
        d->policy.start_min = 8 * 60;
        d->policy.end_min = 20 * 60;
        d->policy.days = WUBU_PARENTAL_ALL_DAYS;
        snprintf(d->note, sizeof d->note, "No policy file yet -- press Space, then Enter, to create one");
    } else {
        d->note[0] = '\0';
    }
    d->field = 0;
    d->dirty = false;
}

static void limits_init(CpApplet *applet)
{
    if (!applet->user_data) applet->user_data = calloc(1, sizeof(LimitsData));
    limits_load((LimitsData *)applet->user_data);
}

static void row(uint32_t *fb, int fb_w, int fb_h, int x, int y, int w,
                const char *label, const char *value, uint32_t value_color)
{
    const WubuThemeColors *c = wubu_theme_colors();
    cp_row(fb, fb_w, fb_h, x, y, w, label, value ? value : "", c->body_text, value_color);
}

static void limits_render(CpApplet *applet, uint32_t *fb,
                          int fb_w, int fb_h, int x, int y, int w, int h)
{
    const WubuThemeColors *c = wubu_theme_colors();
    LimitsData *d = (LimitsData *)applet->user_data;
    if (!d) return;

    cp_fill(fb, fb_w, fb_h, x, y, w, h, c->win_face);
    cp_draw_text(fb, fb_w, fb_h, x + 10, y + 6, "Family limits", c->select_bg, 2);
    cp_draw_text(fb, fb_w, fb_h, x + 10, y + 24,
                 "The hours and days this machine may be used. Outside them the session locks itself.",
                 c->body_text_ina, 1);

    char win[32], days[64];
    wubu_parental_window_str(&d->policy, win, sizeof win);
    wubu_parental_days_str(&d->policy, days, sizeof days);

    int ry = y + 52;
    const int step = 22;
    row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Limits:    ",
        d->policy.enabled ? "on" : "off", d->policy.enabled ? c->select_bg : c->body_text);
    ry += step;
    row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Hours:     ", win,
        d->field == 0 ? c->select_bg : c->body_text);
    ry += step;
    row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Days:      ", days, c->body_text);
    ry += step;

    /* The live answer: what the policy says about right now, with the same function the session uses. */
    time_t now = time(NULL);
    struct tm local;
    localtime_r(&now, &local);
    char why[160];
    bool allowed = wubu_parental_allowed(&d->policy, &local, why, sizeof why);
    row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Right now: ",
        allowed ? "allowed" : why, allowed ? c->select_bg : 0xFFD06040u);
    ry += step;
    row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Policy:    ",
        d->policy.source[0] ? d->policy.source : "not configured", c->body_text_ina);
    ry += step;
    row(fb, fb_w, fb_h, x + 10, ry, w - 20, "Editor:    ",
        d->field == 0 ? "start hour selected" : "end hour selected", c->body_text);

    if (d->dirty)
        cp_draw_text(fb, fb_w, fb_h, x + 10, y + h - 2 * cp_text_line_h() - WUBU_SPACE_XS, "Unsaved -- Enter writes the policy",
                     c->select_bg, 1);
    else if (d->note[0])
        cp_draw_text(fb, fb_w, fb_h, x + 10, y + h - 2 * cp_text_line_h() - WUBU_SPACE_XS, d->note, c->body_text_ina, 1);

    cp_draw_text(fb, fb_w, fb_h, x + 10, y + h - cp_text_line_h() - WUBU_SPACE_XS,
                 "Space on/off - Tab switches hour - Up/Down 30 min - Enter saves", c->body_text_ina, 1);
}

static void limits_key(CpApplet *applet, uint32_t key, uint32_t mods)
{
    (void)mods;
    LimitsData *d = (LimitsData *)applet->user_data;
    if (!d) return;

    switch (key) {
    case ' ':
        d->policy.enabled = !d->policy.enabled;
        d->dirty = true;
        snprintf(d->note, sizeof d->note, "%s", d->policy.enabled ? "Limits on (unsaved)" : "Limits off (unsaved)");
        break;
    case 9:                       /* Tab */
    case 0xE04D:                  /* Right */
        d->field = 1;
        break;
    case 0xE04B:                  /* Left */
        d->field = 0;
        break;
    case 0xE048:                  /* Up: 30 minutes earlier */
    case 0xE050: {                /* Down: 30 minutes later */
        int delta = (key == 0xE048) ? -30 : 30;
        int *v = (d->field == 0) ? &d->policy.start_min : &d->policy.end_min;
        *v += delta;
        if (*v < 0) *v += 24 * 60;
        if (*v > 24 * 60) *v -= 24 * 60;
        d->dirty = true;
        break;
    }
    case 0x1C: case 0xE01C: case '\r': case '\n': {
        (void)wubu_parental_normalize(&d->policy);
        if (wubu_parental_save(&d->policy) == 0) {
            d->dirty = false;
            snprintf(d->note, sizeof d->note, "Saved to %s", wubu_parental_conf_path());
            /* The guard must see the new policy on its next tick, not in a minute. Weak: a panel test
             * graph has the panel without the frame loop, and that is a legitimate graph. */
            extern void wubu_parental_guard_reset(void) __attribute__((weak));
            if (wubu_parental_guard_reset) wubu_parental_guard_reset();
        } else {
            snprintf(d->note, sizeof d->note, "Could not write %s", wubu_parental_conf_path());
        }
        break;
    }
    case 0x3F:                    /* F5 */
        limits_load(d);
        break;
    default:
        break;
    }
}

static void limits_mouse(CpApplet *applet, int x, int y, int btn, int kind)
{
    (void)applet; (void)x; (void)y; (void)btn; (void)kind;
}

static void limits_cleanup(CpApplet *applet)
{
    if (applet->user_data) {
        free(applet->user_data);
        applet->user_data = NULL;
    }
}

CpApplet cp_create_limits_applet(void)
{
    CpApplet a;
    memset(&a, 0, sizeof(a));
    a.id = CP_APPLET_LIMITS;
    snprintf(a.name, sizeof(a.name), "Family limits");
    snprintf(a.desc, sizeof(a.desc), "The hours and days this machine may be used");
    a.icon_color = 0xFFD06040;
    a.init = limits_init;
    a.render = limits_render;
    a.mouse = limits_mouse;
    a.key = limits_key;
    a.cleanup = limits_cleanup;
    return a;
}

/* Test hook: the policy the panel is editing, and whether it has unsaved changes. */
const void *cp_limits_test_policy(bool *dirty)
{
    CpApplet *a = controlpanel_get_applet(CP_APPLET_LIMITS);
    if (!a || !a->user_data) return NULL;
    LimitsData *d = (LimitsData *)a->user_data;
    if (dirty) *dirty = d->dirty;
    return &d->policy;
}
