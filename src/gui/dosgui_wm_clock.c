/*
 * dosgui_wm_clock.c -- WuBuOS DosGui WM: taskbar clock + clock menu
 *
 * Self-contained concern split out of dosgui_wm_ctxmenu.c:
 *   1. taskbar clock update + string formatting
 *   2. the clock's OWN popup menu (Win98 parity): clicking the clock well
 *      opens a raised panel showing the full date, HH:MM:SS time, and a
 *      mini month calendar with today highlighted.
 * Depends only on the shared WM state (dosgui_wm_internal.h).
 */

#include "dosgui_wm_internal.h"

#include <time.h>
#include <stdio.h>
#include <string.h>

void dosgui_taskbar_update_clock(time_t now) {
    g_dwm.last_clock_update = now;
}

char *dosgui_taskbar_get_clock_str(void) {
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    static char clk[16];
    snprintf(clk, sizeof(clk), "%02d:%02d", tm->tm_hour, tm->tm_min);
    return clk;
}

/* -- Clock popup menu (Win98 parity) -------------------------------- */

/* Popup panel geometry: bottom-right, above the taskbar. */
#define CLK_MENU_W  224
#define CLK_MENU_H  176

void dosgui_clock_menu_rect(int fb_w, int fb_h, int *x, int *y,
                            int *w, int *h) {
    int task_h = taskbar_height_dynamic();
    if (x) *x = fb_w - CLK_MENU_W - 6;
    if (y) *y = fb_h - task_h - CLK_MENU_H - 6;
    if (w) *w = CLK_MENU_W;
    if (h) *h = CLK_MENU_H;
}

void dosgui_clock_menu_toggle(void) { g_dwm.clock_menu_open = !g_dwm.clock_menu_open; }
void dosgui_clock_menu_close(void)  { g_dwm.clock_menu_open = false; }
int  dosgui_clock_menu_is_open(void){ return g_dwm.clock_menu_open; }

void dosgui_clock_well(int *x, int *y, int *w, int *h) {
    if (x) *x = g_dwm.clock_well_x;
    if (y) *y = g_dwm.clock_well_y;
    if (w) *w = g_dwm.clock_well_w;
    if (h) *h = g_dwm.clock_well_h;
}

static const char *k_mon[]  = { "January","February","March","April","May","June",
                                "July","August","September","October","November","December" };
static const char *k_dow[]  = { "Su","Mo","Tu","We","Th","Fr","Sa" };
static const int   k_dom[]  = { 31,28,31,30,31,30,31,31,30,31,30,31 };

void dosgui_clock_menu_render(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb;
    if (!g_dwm.clock_menu_open) return;

    int mx, my, mw, mh;
    dosgui_clock_menu_rect(fb_w, fb_h, &mx, &my, &mw, &mh);

    /* Raised Win98 panel. */
    vbe_fill_rect_rounded(mx, my, mw, mh, 3, tc()->win_face);
    vbe_3d_raised_colors(mx, my, mw, mh,
                         tc()->border_light, tc()->border_face,
                         tc()->border_dark, tc()->border_darkest);

    /* Title bar. */
    vbe_fill_rect(mx + 2, my + 2, mw - 4, 16, tc()->win_title_active);
    vbe_draw_text(mx + 6, my + 5, "Date / Time", tc()->win_title_text, 1);

    time_t now = time(NULL);
    struct tm *tm = localtime(&now);

    /* Date line: "Thursday, August 6, 2026" */
    char date[64];
    static const char *days[] = { "Sunday","Monday","Tuesday","Wednesday",
                                  "Thursday","Friday","Saturday" };
    snprintf(date, sizeof(date), "%s, %s %d, %d",
             days[tm->tm_wday], k_mon[tm->tm_mon], tm->tm_mday,
             tm->tm_year + 1900);
    vbe_draw_text(mx + 8, my + 22, date, tc()->icon_text, 1);

    /* Time line with seconds. */
    char tstr[16];
    snprintf(tstr, sizeof(tstr), "%02d:%02d:%02d",
             tm->tm_hour, tm->tm_min, tm->tm_sec);
    vbe_draw_text(mx + 8, my + 34, tstr, tc()->icon_text, 1);

    /* -- Mini calendar -- */
    char head[32];
    snprintf(head, sizeof(head), "%s %d", k_mon[tm->tm_mon], tm->tm_year + 1900);
    vbe_draw_text(mx + 8, my + 48, head, tc()->win_title_active, 1);

    /* First day-of-week of this month (normalize via mktime). */
    struct tm t0 = *tm;
    t0.tm_mday = 1; t0.tm_hour = 0; t0.tm_min = 0; t0.tm_sec = 0;
    mktime(&t0);
    int first_wday = t0.tm_wday;              /* 0 = Sunday */
    int days_in_month = k_dom[tm->tm_mon];
    if (tm->tm_mon == 1 && ((tm->tm_year + 1900) % 4 == 0 &&
        ((tm->tm_year + 1900) % 100 != 0 || (tm->tm_year + 1900) % 400 == 0)))
        days_in_month = 29;                   /* leap year */

    int gx = mx + 10, gy = my + 58;
    int cell_w = 28, cell_h = 15;
    /* Weekday header. */
    for (int d = 0; d < 7; d++)
        vbe_draw_text(gx + d * cell_w + 2, gy, k_dow[d], tc()->win_title_active, 1);
    gy += 13;

    for (int day = 1, wd = first_wday; day <= days_in_month; day++, wd++) {
        if (wd == 7) { wd = 0; gy += cell_h; }
        int dx = gx + wd * cell_w;
        char buf[4];
        snprintf(buf, sizeof(buf), "%d", day);
        if (day == tm->tm_mday) {
            /* Today: navy plate + white text (Win98). */
            vbe_fill_rect(dx - 1, gy - 1, cell_w - 2, 12, tc()->select_bg);
            vbe_draw_text(dx + 2, gy, buf, tc()->win_title_text, 1);
        } else {
            vbe_draw_text(dx + 2, gy, buf, tc()->icon_text, 1);
        }
    }
}
