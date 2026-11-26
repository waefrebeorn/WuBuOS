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

/* The inline editor. A contacts page you can only read is not a contacts app: 'n' starts a person, Tab
 * moves between name and email, Enter saves through the store, Escape cancels. */
typedef struct {
    int  active;
    int  field;                 /* 0 = name, 1 = email */
    char name[WUBU_PEOPLE_NAME_MAX];
    char email[WUBU_PEOPLE_MAIL_MAX];
    char status[96];            /* what the last save attempt did -- refusals included */
} PeopleEdit;

static PeopleEdit g_edit;

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

    if (key == 0xE034) { people_reload(); return; }      /* F5 */

    if (!g_edit.active) {
        if (key == 'n' || key == 'N') {
            memset(&g_edit, 0, sizeof g_edit);
            g_edit.active = 1;
            g_edit.field = 0;
            snprintf(g_edit.status, sizeof g_edit.status, "typing a new contact; Enter saves, Esc cancels");
        }
        return;
    }

    char *buf = g_edit.field == 0 ? g_edit.name : g_edit.email;
    size_t cap = g_edit.field == 0 ? sizeof g_edit.name : sizeof g_edit.email;

    if (key == 0x1B) {                                   /* Escape: cancel, and change nothing */
        snprintf(g_edit.status, sizeof g_edit.status, "cancelled");
        g_edit.active = 0;
        return;
    }
    if (key == 0x09) {                                   /* Tab: switch field */
        g_edit.field = !g_edit.field;
        return;
    }
    if (key == 0x08 || key == 0x7F) {                    /* Backspace */
        size_t n = strlen(buf);
        if (n) buf[n - 1] = 0;
        return;
    }
    if (key == 0x0D || key == 0x0A) {                    /* Enter: save */
        int rc = wubu_people_add(&g_db, g_edit.name, g_edit.email, "");
        if (rc == 0) {
            if (wubu_people_save(&g_db) == 0)
                snprintf(g_edit.status, sizeof g_edit.status, "saved %s", g_edit.name);
            else
                snprintf(g_edit.status, sizeof g_edit.status, "could not write the store");
        } else if (rc == 2) {
            snprintf(g_edit.status, sizeof g_edit.status,
                     "%s needs a name that is not already here", g_edit.name[0] ? g_edit.name : "that");
        } else {
            snprintf(g_edit.status, sizeof g_edit.status, "the store is full (%d)", WUBU_PEOPLE_MAX);
        }
        g_edit.active = 0;
        return;
    }
    if (key >= 0x20 && key <= 0x7E && strlen(buf) + 1 < cap) {   /* printable */
        size_t n = strlen(buf);
        buf[n] = (char)key;
        buf[n + 1] = 0;
    }
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

    if (g_edit.active) {
        char nm[WUBU_PEOPLE_NAME_MAX + 2], em[WUBU_PEOPLE_MAIL_MAX + 2];
        snprintf(nm, sizeof nm, "%s%s", g_edit.name, g_edit.field == 0 ? "_" : "");
        snprintf(em, sizeof em, "%s%s", g_edit.email, g_edit.field == 1 ? "_" : "");
        label[rows] = "Name";  value[rows] = nm; rows++;
        label[rows] = "Email"; value[rows] = em; rows++;
        label[rows] = "Status"; value[rows] = g_edit.status; rows++;
        label[rows] = "Store"; value[rows] = store; rows++;

        size_t eoff = 0;
        g_last_drawn[0] = 0;
        for (int i = 0; i < rows; i++) {
            int n = snprintf(g_last_drawn + eoff, sizeof g_last_drawn - eoff, "%s%s: %s",
                             i ? "; " : "", label[i], value[i]);
            if (n < 0) break;
            eoff += (size_t)n;
            int ry = y + 44 + i * 26;
            if (ry + 10 > y + h) break;
            cp_draw_text(fb, fb_w, fb_h, x + 14, ry, label[i], c->body_text_ina, 1);
            cp_draw_text(fb, fb_w, fb_h, x + 150, ry, value[i], c->body_text, 1);
        }
        return;
    }

    label[rows] = "Contacts"; value[rows] = count; rows++;
    for (int i = 0; i < g_db.count && rows < PEOPLE_ROWS - 2; i++) {   /* leave room for Store */
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
