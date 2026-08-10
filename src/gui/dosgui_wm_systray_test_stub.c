/*
 * dosgui_wm_systray_test_stub.c -- the systray STUB for the
 * world-state tray test.
 *
 * The real dosgui_wm_systray.c pulls the whole GUI render chain
 * (vbe, taskbar, notifications). The world-state tray only needs
 * add/update_color/color — this stub provides a tiny registry so the
 * tray logic is tested in isolation. (Precedent:
 * dosgui_dos_window_test_stub.c.)
 * C11.
 */
#include "dosgui_wm.h"

#include <stdint.h>
#include <string.h>

#define STUB_MAX 8
static struct {
    char     name[32];
    uint32_t color;
} g_icons[STUB_MAX];
static int g_n;

int dosgui_systray_add(const char *name, uint32_t color,
                       void (*on_click)(void),
                       void (*on_right_click)(void))
{
    (void)on_click; (void)on_right_click;
    if (!name || g_n >= STUB_MAX) return -1;
    snprintf(g_icons[g_n].name, sizeof(g_icons[g_n].name), "%s", name);
    g_icons[g_n].color = color;
    return g_n++;
}

int dosgui_systray_update_color(const char *name, uint32_t color)
{
    for (int i = 0; i < g_n; i++)
        if (strcmp(g_icons[i].name, name) == 0) {
            g_icons[i].color = color;
            return 0;
        }
    return -1;
}

uint32_t dosgui_systray_color(const char *name)
{
    for (int i = 0; i < g_n; i++)
        if (strcmp(g_icons[i].name, name) == 0)
            return g_icons[i].color;
    return 0;
}
