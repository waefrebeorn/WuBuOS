/*
 * control_panel_ai.c -- the "AI" page (Windows 11's Client.AIX surface).
 *
 * The one rule that shapes this file: THE FRAME LOOP NEVER PROBES. A reachability probe costs up to its
 * deadline, and a page that measures on every repaint is the jank this project keeps chasing. The probe
 * runs once at page init and again only when the user asks (F5), and the page always shows WHEN the last
 * measurement happened, so a stale green cannot be mistaken for a live one.
 */
#include "control_panel.h"
#include "control_panel_text.h"
#include "wubu_theme.h"
#include "../runtime/wubu_ai_link.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    WubuAiLink link;
    bool        probed;          /* a measurement was taken at least once */
    bool        probing;         /* recorded only, never true across a frame */
} AiAppletData;

static AiAppletData g_ai;
static char g_last_drawn[512];   /* the rows this page last drew */

#define AI_PROBE_MS 300

static void ai_measure(void)
{
    wubu_ai_link_probe(&g_ai.link, AI_PROBE_MS);
    g_ai.probed = true;
}

static void ai_applet_init(CpApplet *applet)
{
    if (!applet->user_data)
        applet->user_data = (void *)calloc(1, sizeof(AiAppletData));
    ai_measure();
}

static void ai_applet_key(CpApplet *applet, int key)
{
    (void)applet;
    if (key == 0xE034)              /* F5: re-measure on demand */
        ai_measure();
}

static void ai_render(CpApplet *applet, uint32_t *fb,
                      int fb_w, int fb_h, int x, int y, int w, int h)
{
    const WubuThemeColors *c = wubu_theme_colors();
    if (!c) return;

    cp_fill(fb, fb_w, fb_h, x, y, w, h, c->win_face);
    cp_draw_text(fb, fb_w, fb_h, x + 14, y + 12, applet->name, c->select_bg, 2);

    const WubuAiLink *l = &g_ai.link;
    const char *rows[4][2];
    char v0[WUBU_AI_ENDPOINT_MAX + 8], v1[64], v2[64], v3[96];

    if (l->configured) snprintf(v0, sizeof v0, "%s", l->endpoint);
    else               snprintf(v0, sizeof v0, "none configured");
    snprintf(v1, sizeof v1, "%s", l->configured ? l->source : "not configured");

    if (!g_ai.probed)                        snprintf(v2, sizeof v2, "not measured");
    else if (!l->configured)                 snprintf(v2, sizeof v2, "nothing to reach");
    else if (l->reachable)                   snprintf(v2, sizeof v2, "yes, %d ms", l->latency_ms);
    else                                     snprintf(v2, sizeof v2, "no");

    /* What this page does NOT know is part of the answer: a TCP connect proves something is listening, not
     * that it is a model, and not that it will answer. */
    snprintf(v3, sizeof v3, "F5 re-measures; a listen is not a model");

    rows[0][0] = "Endpoint";  rows[0][1] = v0;
    rows[1][0] = "From";      rows[1][1] = v1;
    rows[2][0] = "Reachable"; rows[2][1] = v2;
    rows[3][0] = "Note";      rows[3][1] = v3;

    size_t off = 0;
    g_last_drawn[0] = 0;
    for (int i = 0; i < 4; i++) {
        int n = snprintf(g_last_drawn + off, sizeof g_last_drawn - off, "%s%s: %s",
                         i ? "; " : "", rows[i][0], rows[i][1]);
        if (n < 0) break;
        off += (size_t)n;
        int ry = y + 44 + i * 26;
        if (ry + 10 > y + h) break;
        cp_draw_text(fb, fb_w, fb_h, x + 14, ry, rows[i][0], c->body_text_ina, 1);
        cp_draw_text(fb, fb_w, fb_h, x + 150, ry, rows[i][1], c->body_text, 1);
    }
}

CpApplet cp_create_ai_applet(void)
{
    CpApplet a;
    memset(&a, 0, sizeof(a));
    a.id = CP_APPLET_AI;
    strncpy(a.name, "AI", sizeof(a.name) - 1);
    strncpy(a.desc, "The model endpoint this machine can reach (AI actions)",
            sizeof(a.desc) - 1);
    a.icon_color = 0xFF8A5CD0;
    a.init = ai_applet_init;
    a.render = ai_render;
    a.mouse = NULL;
    a.key = ai_applet_key;
    a.cleanup = NULL;
    a.user_data = NULL;
    a.initialized = false;
    return a;
}

/* Test hooks: what the page drew, and a way to force a measurement without a frame loop. */
const char *cp_ai_last_drawn(void) { return g_last_drawn; }
void cp_ai_test_measure(void) { ai_measure(); }
