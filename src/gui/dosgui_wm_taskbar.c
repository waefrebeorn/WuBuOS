/* dosgui_wm_taskbar.c -- Taskbar render + geometry.
 *
 * Self-contained: renders the taskbar (Start button, window buttons, clock,
 * systray) using the shared g_dwm state (extern in dosgui_wm_internal.h) and
 * the public vbe_* and theme APIs. Clock strings come from dosgui_taskbar_* (declared
 * in dosgui_wm.h). Minimal includes.
 */

#include "dosgui_wm_internal.h"
#include <time.h>

/* -- Taskbar ----------------------------------------------------- */

int dosgui_taskbar_height(void) { return taskbar_height_dynamic(); }

void dosgui_taskbar_render(uint32_t *fb, int fb_w, int fb_h) {
    int th = taskbar_height_dynamic();
    int ty = fb_h - th;

    vbe_fill_rect(0, ty, fb_w, th, tc()->taskbar_bg);
    vbe_hline(0, fb_w - 1, ty, tc()->taskbar_border);
    int by = ty + (th - 24) / 2;
    int start_w = theme()->Luna_start_button ? 54 : 60;
    
    if (theme()->Luna_start_button) {
        vbe_fill_rect_rounded(4, by, start_w + 20, 24, 4, tc()->start_btn_face);
        vbe_3d_raised_rounded_colors(4, by, start_w + 20, 24, 4,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);
        vbe_draw_text(8, by + 8, "Start", tc()->start_btn_text, 1);
    } else {
        vbe_fill_rect(4, by, 60, 22, tc()->start_btn_face);
        vbe_3d_raised_colors(4, by, 60, 22,
                              tc()->border_light, tc()->border_face,
                              tc()->border_dark, tc()->border_darkest);
        vbe_draw_text(8, by + 6, "+ NEW", tc()->start_btn_text, 1);
    }

    int bx = theme()->Luna_start_button ? 82 : 72;
    
    /* Reserve space for clock + tray icons on the right */
    dosgui_taskbar_update_clock(time(NULL));
    char *clk = dosgui_taskbar_get_clock_str();
    int clk_w = vbe_text_width(clk, 1);
    int clock_reserve = clk_w + 20; /* clock + padding */
    int tray_reserve = g_dwm.systray_count * (DOSGUI_SYSTRAY_SIZE + 4) + 10;
    int right_reserve = clock_reserve + tray_reserve;
    
    for (int j = 0; j < g_dwm.nz; j++) {
        DosGuiWindow *w = &g_dwm.windows[g_dwm.zorder[j]];
        if (!w->alive || (w->flags & DOSGUI_WIN_MINIMIZED)) continue;
        if (w->desktop != g_dwm.current_desktop) continue;
        int bw = (int)strlen(w->title) * 6 + 16;
        if (bw > 160) bw = 160;
        bool focused = (g_dwm.zorder[j] == g_dwm.focused_id);
        
        /* Check if button would overlap reserved area */
        if (bx + bw > g_dwm.screen_w - right_reserve) break;
        
        if (theme()->rounded_buttons) {
            if (focused) {
                vbe_fill_rect_rounded(bx, by, bw, 22, 3, tc()->select_bg);
                vbe_3d_sunken_rounded_colors(bx, by, bw, 22, 3,
                                              tc()->border_light, tc()->border_face,
                                              tc()->border_dark, tc()->border_darkest);
                /* Draw truncated title with ellipsis */
                {
                    int text_w = vbe_text_width(w->title, 1);
                    int max_text_w = bw - 16;
                    if (text_w > max_text_w) {
                        char truncated[64];
                        int len = strlen(w->title);
                        while (len > 0 && vbe_text_width(truncated, 1) > max_text_w - 6) { /* -6 for "..." */
                            len--;
                            strncpy(truncated, w->title, len);
                            truncated[len] = '\0';
                        }
                        if (len > 0) {
                            strncpy(truncated + len, "...", 3);
                            truncated[len + 3] = '\0';
                        } else {
                            strcpy(truncated, "...");
                        }
                        vbe_draw_text(bx + 8, by + 7, truncated, tc()->select_text, 1);
                    } else {
                        vbe_draw_text(bx + 8, by + 7, w->title, tc()->select_text, 1);
                    }
                }
            } else {
                vbe_fill_rect_rounded(bx, by, bw, 22, 3, tc()->btn_face);
                vbe_3d_raised_rounded_colors(bx, by, bw, 22, 3,
                                              tc()->border_light, tc()->border_face,
                                              tc()->border_dark, tc()->border_darkest);
                /* Draw truncated title with ellipsis */
                {
                    int text_w = vbe_text_width(w->title, 1);
                    int max_text_w = bw - 16;
                    if (text_w > max_text_w) {
                        char truncated[64];
                        int len = strlen(w->title);
                        while (len > 0 && vbe_text_width(truncated, 1) > max_text_w - 6) {
                            len--;
                            strncpy(truncated, w->title, len);
                            truncated[len] = '\0';
                        }
                        if (len > 0) {
                            strncpy(truncated + len, "...", 3);
                            truncated[len + 3] = '\0';
                        } else {
                            strcpy(truncated, "...");
                        }
                        vbe_draw_text(bx + 8, by + 7, truncated, tc()->btn_text, 1);
                    } else {
                        vbe_draw_text(bx + 8, by + 7, w->title, tc()->btn_text, 1);
                    }
                }
            }
        } else {
            if (focused) {
                vbe_fill_rect(bx, by, bw, 22, 0x000080);
                vbe_3d_sunken_colors(bx, by, bw, 22,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);
                /* Draw truncated title with ellipsis */
                {
                    int text_w = vbe_text_width(w->title, 1);
                    int max_text_w = bw - 16;
                    if (text_w > max_text_w) {
                        char truncated[64];
                        int len = strlen(w->title);
                        while (len > 0 && vbe_text_width(truncated, 1) > max_text_w - 6) {
                            len--;
                            strncpy(truncated, w->title, len);
                            truncated[len] = '\0';
                        }
                        if (len > 0) {
                            strncpy(truncated + len, "...", 3);
                            truncated[len + 3] = '\0';
                        } else {
                            strcpy(truncated, "...");
                        }
                        vbe_draw_text(bx + 8, by + 6, truncated, 0xFFFFFF, 1);
                    } else {
                        vbe_draw_text(bx + 8, by + 6, w->title, 0xFFFFFF, 1);
                    }
                }
            } else {
                vbe_fill_rect(bx, by, bw, 22, tc()->btn_face);
                vbe_3d_raised_colors(bx, by, bw, 22,
                                      tc()->border_light, tc()->border_face,
                                      tc()->border_dark, tc()->border_darkest);
                /* Draw truncated title with ellipsis */
                {
                    int text_w = vbe_text_width(w->title, 1);
                    int max_text_w = bw - 16;
                    if (text_w > max_text_w) {
                        char truncated[64];
                        int len = strlen(w->title);
                        while (len > 0 && vbe_text_width(truncated, 1) > max_text_w - 6) {
                            len--;
                            strncpy(truncated, w->title, len);
                            truncated[len] = '\0';
                        }
                        if (len > 0) {
                            strncpy(truncated + len, "...", 3);
                            truncated[len + 3] = '\0';
                        } else {
                            strcpy(truncated, "...");
                        }
                        vbe_draw_text(bx + 8, by + 6, truncated, tc()->btn_text, 1);
                    } else {
                        vbe_draw_text(bx + 8, by + 6, w->title, tc()->btn_text, 1);
                    }
                }
            }
        }
        bx += bw + 2;
        if (bx > g_dwm.screen_w - right_reserve) break;
    }

    /* System tray icons (drawn from right to left, before clock) */
    int tray_x = fb_w - 10;

    /* Clock - use clk/clk_w from earlier in function */
    dosgui_taskbar_update_clock(time(NULL));

    /* Ensure clock doesn't overlap window buttons - use bx as the left boundary */
    int clock_x = fb_w - clk_w - 10;
    if (clock_x + clk_w > bx) {
        clock_x = bx - clk_w - 10;
    }
    if (clock_x < 0) clock_x = 10;

    vbe_draw_text(clock_x, ty + (th - 8) / 2, clk,
                  theme()->Luna_start_button ? 0xFFFFFF : tc()->icon_text, 1);

    tray_x = clock_x - 10;

    /* Draw system tray icons */
    for (int i = g_dwm.systray_count - 1; i >= 0; i--) {
        if (g_dwm.systray_icons[i].visible) {
            int x = tray_x - DOSGUI_SYSTRAY_SIZE - 4;
            int y = ty + (th - DOSGUI_SYSTRAY_SIZE) / 2;

            vbe_fill_rect(x, y, DOSGUI_SYSTRAY_SIZE, DOSGUI_SYSTRAY_SIZE, tc()->btn_face);
            vbe_3d_raised_colors(x, y, DOSGUI_SYSTRAY_SIZE, DOSGUI_SYSTRAY_SIZE,
                                 tc()->border_light, tc()->border_face,
                                 tc()->border_dark, tc()->border_darkest);

            vbe_fill_rect(x + 4, y + 4, 16, 16, g_dwm.systray_icons[i].icon_color);

            /* Draw notification badge if count > 0 */
            if (g_dwm.systray_icons[i].notification_count > 0) {
                char badge[8];
                snprintf(badge, sizeof(badge), "%d", 
                    g_dwm.systray_icons[i].notification_count > 9 ? 9 : g_dwm.systray_icons[i].notification_count);
                int bx = x + DOSGUI_SYSTRAY_SIZE - 8;
                int by = y;
                vbe_fill_rect_rounded(bx, by, 12, 12, 6, 0xFF0000);
                vbe_draw_text(bx + 2, by + 1, badge, 0xFFFFFF, 1);
            }
            tray_x -= DOSGUI_SYSTRAY_SIZE + 4;
        }
    }

    int desk_x = tray_x - 150;
    for (int d = 0; d < g_dwm.desktop_count; d++) {
        int dx = desk_x + d * 16;
        if (d == g_dwm.current_desktop) {
            vbe_fill_rect_rounded(dx, ty + (th - 16) / 2, 14, 16, 2, tc()->select_bg);
            vbe_3d_sunken_rounded_colors(dx, ty + (th - 16) / 2, 14, 16, 2,
                                          tc()->border_light, tc()->border_face,
                                          tc()->border_dark, tc()->border_darkest);
            char label = (d == 9) ? 'M' : ('1' + d);
            vbe_draw_text(dx + 3, ty + (th - 8) / 2, &label, tc()->select_text, 1);
        } else {
            vbe_fill_rect_rounded(dx, ty + (th - 16) / 2, 14, 16, 2, tc()->btn_face);
            vbe_3d_raised_rounded_colors(dx, ty + (th - 16) / 2, 14, 16, 2,
                                          tc()->border_light, tc()->border_face,
                                          tc()->border_dark, tc()->border_darkest);
            char label = (d == 9) ? 'M' : ('1' + d);
            vbe_draw_text(dx + 3, ty + (th - 8) / 2, &label, tc()->btn_text, 1);
        }
    }

    /* -- Status bar tip (cycling keyboard hint) ------------------ */
    {
        static time_t last_tip_swap = 0;
        static int tip_index = 0;
        time_t now = time(NULL);

        if (now != last_tip_swap) {
            /* Cycle tip every ~10 seconds (checked every render frame) */
            if (now - last_tip_swap >= 10) {
                tip_index = (tip_index + 1) % 6;
                last_tip_swap = now;
            }
            /* Initialize clock on first render */
            if (last_tip_swap == 0) {
                last_tip_swap = now;
                tip_index = 0;
            }
        }

        const char *tips[] = {
            "Ctrl+T = cycle theme",
            "Alt+F4 = close window",
            "Win key = Start menu",
            "Ctrl+Alt+Left/Right = desktop",
            "Shift+F10 = context menu",
            "Ctrl+T = cycle theme",
        };
        const char *tip = tips[tip_index % 6];
        int tip_x = desk_x - vbe_text_width(tip, 1) - 20;
        if (tip_x > start_w + 90) {
            vbe_draw_text(tip_x, ty + (th - 8) / 2, tip, tc()->icon_text, 1);
        }
    }
}
