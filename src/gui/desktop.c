/*
 * desktop.c — My Seed Desktop Manager
 */
#include "wm.h"
#include "../kernel/vbe.h"

void desktop_draw(int screen_w, int screen_h, int taskbar_h) {
    /* Background */
    vbe_fill_rect(0, 0, screen_w, screen_h - taskbar_h, C_WIN_DESKTOP);
    
    /* Desktop icons (fixed set for now) */
    /* My Computer icon at (20, 20) */
    vbe_fill_rect(20, 20, 32, 32, 0x00C0C0C0);
    vbe_rect(20, 20, 32, 32, C_WIN_BORDER_DK);
    
    /* Temple REPL icon at (20, 80) */
    vbe_fill_rect(20, 80, 32, 32, 0x00008080);
    vbe_rect(20, 80, 32, 32, C_WIN_BORDER_DK);
}
