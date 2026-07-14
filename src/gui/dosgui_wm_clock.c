/*
 * dosgui_wm_clock.c -- WuBuOS DosGui WM: taskbar clock
 *
 * Self-contained concern split out of dosgui_wm_ctxmenu.c: the taskbar
 * clock update + string formatting. Depends only on the shared WM state
 * (dosgui_wm_internal.h). No context-menu engine, no actions, no dialogs.
 */

#include "dosgui_wm_internal.h"

#include <time.h>
#include <stdio.h>

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
