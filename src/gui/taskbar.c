/*
 * taskbar.c  --  My Seed Taskbar (Win98-style, 28px bottom bar) (LEGACY)
 */
#include "wm.h"
#include "../kernel/vbe_legacy.h"
#include <string.h>

static int g_taskbar_height = 28;

void taskbar_init(void) {
    g_taskbar_height = 28;
}

void taskbar_draw(int screen_w, int screen_h) {
    int y = screen_h - g_taskbar_height;
    vbe_fill_rect(0, y, screen_w, g_taskbar_height, C_WIN_FACE);
    vbe_3d_raised(0, y, screen_w, g_taskbar_height);
    
    /* Start button */
    vbe_fill_rect(4, y+3, 60, 22, C_WIN_FACE);
    vbe_3d_raised(4, y+3, 60, 22);
    
    /* WmWindow buttons in taskbar */
    int bx = 70;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        WmWindow *w = wm_find_by_id(i+1);
        if (!w || w->flags == WIN_UNUSED) continue;
        int bw = strlen(w->title) * 8 + 16;
        if (bw > 160) bw = 160;
        vbe_fill_rect(bx, y+3, bw, 22, C_WIN_FACE);
        if (w->flags & WIN_FOCUSED)
            vbe_3d_sunken(bx, y+3, bw, 22);
        else
            vbe_3d_raised(bx, y+3, bw, 22);
        bx += bw + 2;
    }
}

int taskbar_height(void) { return g_taskbar_height; }
