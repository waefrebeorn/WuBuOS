/*
 * control_panel_people.c -- the People page (PeopleExperienceHost parity).
 *
 * Shows the contacts that are actually in the store -- name, and the first thing that identifies them
 * (email, else phone). No contacts means "no contacts", not an empty card with a heading, and the store
 * path is on the page so a user can find the file they are editing.
 *
 * Read-only by design: this surface lists, it does not pretend to be a full editor. F5 reloads, so an
 * external edit shows up without restarting the panel.
 */
#include "control_panel.h"
#include "control_panel_text.h"
#include "wubu_theme.h"
#include "../runtime/wubu_people.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static WubuPeople g_db;
static char g_last_drawn[512];

#define PEOPLE_ROWS 6      /* the count line + five names */

static void people_reload(void)
{
    wubu_people_load(&g_db);
}

static void people_applet_init(CpApplet *applet)
{
    if (!applet->user_data)
        applet->user_data = (void *)calloc(1, sizeof(WubuPeople));
    people_reload();
}

static void people_applet_key(CpApplet *applet, unsigned key, unsigned mods)
{
    (void)applet; (void)mods;
    if (key == 0xE034) people_reload();          /* F5 */
}

static void people_render(CpApplet *applet, uint32_t *fb,
                          int fb_w, int fb_h, int x, int y, int w, int h)
{
    const WubuThemeColors *c = wubu_theme_colors();
    if (!c) return;

    cp_fill(fb, fb_w, fb_h, x, y, w, h, c->win_face);
    cp_draw_text(fb, fb_w, fb_h, x + 14, y + 12, applet->name, c->select_bg, 2);

    char count[64], store[256], who[PEOPLE_ROWS][2][128];
    wubu_people_line(&g_db, count, sizeof count);
    snprintf(store, sizeof store, "%s", wubu_people_default_path());

    int rows = 0;
    const char *label[PEOPLE_ROWS];
    const char *value[PEOPLE_ROWS];
    label[rows] = "Contacts"; value[rows] = count; rows++;
    for (int i = 0; i < g_db.count && rows < PEOPLE_ROWS; i++) {
        const WubuPerson *p = &g_db.p[i];
        snprintf(who[i][0], sizeof who[i][0], "%s", p->name);
        snprintf(who[i][1], sizeof who[i][1], "%s",
                 p->email[0] ? p->email : (p->phone[0] ? p->phone : "-"));
        label[rows] = who[i][0];
        value[rows] = who[i][1];
        rows++;
    }
    label[rows] = "Store"; value[rows] = store; rows++;

    size_t off = 0;
    g_last_drawn[0] = 0;
    for (int i = 0; i < rows; i++) {
        int n = snprintf(g_last_drawn + off, sizeof g_last_drawn - off, "%s%s: %s",
                         i ? "; " : "", label[i], value[i]);
        if (n < 0) break;
        off += (size_t)n;
        int ry = y + 44 + i * 26;
        if (ry + 10 > y + h) break;
        cp_draw_text(fb, fb_w, fb_h, x + 14, ry, label[i], c->body_text_ina, 1);
        cp_draw_text(fb, fb_w, fb_h, x + 150, ry, value[i], c->body_text, 1);
    }
}

CpApplet cp_create_people_applet(void)
{
    CpApplet a;
    memset(&a, 0, sizeof(a));
    a.id = CP_APPLET_PEOPLE;
    strncpy(a.name, "People", sizeof(a.name) - 1);
    strncpy(a.desc, "The contacts on this machine (PeopleExperienceHost)",
            sizeof(a.desc) - 1);
    a.icon_color = 0xFFD05C8A;
    a.init = people_applet_init;
    a.render = people_render;
    a.mouse = NULL;
    a.key = people_applet_key;
    a.cleanup = NULL;
    a.user_data = NULL;
    a.initialized = false;
    return a;
}

/* Test hooks. */
const char *cp_people_last_drawn(void) { return g_last_drawn; }
void cp_people_test_reload(void) { people_reload(); }
