/* dosgui_term_render.c -- WuBuOS terminal rendering layer.
 *
 * Self-contained module extracted from dosgui_term.c. Draws the tab bar,
 * content area, and PTY/holyc/container session views. Uses the shared g_term
 * state + theme helpers via dosgui_term_internal.h. Minimal includes.
 */

#include "dosgui_term_internal.h"
#include <string.h>

void term_tab_bar_layout(TermState *term, int *tab_x, int *tab_w) {
    int available_w = term->w - 4;
    int max_tab_w = 150;
    int min_tab_w = 60;
    
    if (term->tab_count <= 0) {
        *tab_x = 4;
        *tab_w = min_tab_w;
        return;
    }

    *tab_w = available_w / term->tab_count;
    if (*tab_w > max_tab_w) *tab_w = max_tab_w;
    if (*tab_w < min_tab_w) *tab_w = min_tab_w;
    
    int total_w = *tab_w * term->tab_count;
    *tab_x = (term->w - total_w) / 2;
}


void dosgui_term_handle_resize(int w, int h) {
    if (!dosgui_term_is_open()) return;
    g_term.w = w;
    g_term.h = h;

    if (g_term.active_tab >= 0) {
        TermTab *tab = &g_term.tabs[g_term.active_tab];
        if (tab->type == TERM_SESSION_SHELL) {
            int cols = (w - 2 * term_side_padding()) / term_char_w();
            int rows = (h - g_term.tab_bar_h - 2 * term_top_padding()) / term_char_h();
            if (cols < 10) cols = 10;
            if (rows < 5) rows = 5;
            term_update_pty_size(&tab->session.pty, cols, rows);
        }
    }
}

/* -- Rendering ---------------------------------------------------- */

void dosgui_term_render(uint32_t *fb, int fb_w, int fb_h) {
    if (!dosgui_term_is_open()) return;

    dosgui_term_render_tab_bar(fb, fb_w, fb_h);
    dosgui_term_render_content(fb, fb_w, fb_h);
}

void dosgui_term_render_tab_bar(uint32_t *fb, int fb_w, int fb_h) {
    (void)fb_w; (void)fb_h;
    TermState *term = &g_term;
    if (!term->show_tab_bar || term->tab_count == 0) return;

    int tab_x, tab_w;
    term_tab_bar_layout(term, &tab_x, &tab_w);
    int y = term->y;
    int h = term->tab_bar_h;

    /* Tab bar background */
    vbe_fill_rect(term->x, y, term->w, h, tc()->win_face);
    vbe_hline(term->x, term->x + term->w, y + h - 1, tc()->border_dark);

    for (int i = 0; i < term->tab_count; i++) {
        TermTab *tab = &term->tabs[i];
        int tx = term->x + tab_x + i * tab_w;
        bool active = (i == term->active_tab);
        bool hover = (i == term->hovered_tab);

        uint32_t bg = active ? tc()->select_bg : (hover ? tc()->btn_hover : tc()->win_face);
        uint32_t fg = active ? tc()->select_text : tc()->win_title_text;

        /* Tab background */
        if (th()->rounded_buttons) {
            vbe_fill_rect_rounded(tx, y + 2, tab_w - 2, h - 4, 4, bg);
        } else {
            vbe_fill_rect(tx, y + 2, tab_w - 2, h - 4, bg);
        }

        /* Tab text */
        int text_w = vbe_text_width(tab->label, 1);
        vbe_draw_text(tx + (tab_w - text_w) / 2, y + (h - 8) / 2, tab->label, fg, 1);

        /* Close button on active tab */
        if (active && tab_w > 40) {
            int cx = tx + tab_w - 20;
            int cy = y + (h - 12) / 2;
            vbe_fill_rect(cx, cy, 14, 12, active ? tc()->border_darkest : tc()->btn_face);
            vbe_rect(cx, cy, 14, 12, tc()->border_dark);
            vbe_draw_text(cx + 5, cy + 2, "x", active ? 0xFFFFFF : 0x808080, 1);
        }
    }

    /* New tab button (+) */
    int nx = term->x + tab_x + term->tab_count * tab_w;
    if (nx + 30 < term->x + term->w) {
        vbe_fill_rect(nx, y + 4, 24, h - 8, tc()->btn_face);
        vbe_3d_raised_colors(nx, y + 4, 24, h - 8,
                            tc()->border_light, tc()->border_face, tc()->border_dark, tc()->border_darkest);
        vbe_draw_text(nx + 8, y + (h - 8) / 2, "+", tc()->btn_text, 1);
    }
}

void dosgui_term_render_content(uint32_t *fb, int fb_w, int fb_h) {
    if (g_term.active_tab < 0 || g_term.active_tab >= g_term.tab_count) return;

    TermTab *tab = &g_term.tabs[g_term.active_tab];
    int x = g_term.x + term_side_padding();
    int y = g_term.y + g_term.tab_bar_h + term_top_padding();
    int w = g_term.w - 2 * term_side_padding();
    int h = g_term.h - g_term.tab_bar_h - 2 * term_top_padding();

    /* Content background */
    vbe_fill_rect(x, y, w, h, g_term.color_bg);

    switch (tab->type) {
        case TERM_SESSION_SHELL:
            term_render_pty_session(&tab->session.pty, fb, x, y, w, h);
            break;
        case TERM_SESSION_HOLYC:
            term_render_pty_session(&tab->session.holyc.pty, fb, x, y, w, h);
            break;
        case TERM_SESSION_CONTAINER:
            term_render_container_session(&tab->session.container, fb, x, y, w, h);
            break;
    }
}

void term_render_pty_session(TermPtySession *pty, uint32_t *fb, int x, int y, int w, int h) {
    int cols = pty->cols;
    int rows = pty->rows;
    int char_w = term_char_w();
    int char_h = term_char_h();

    /* Process any pending PTY output */
    term_process_pty_output(pty);

    /* Render visible rows from screen buffer */
    for (int r = 0; r < rows && r < TERM_MAX_ROWS; r++) {
        int ry = y + r * char_h;
        for (int c = 0; c < cols && c < TERM_MAX_COLS; c++) {
            int cx = x + c * char_w;
            char ch = pty->screen[r][c];
            uint8_t attr = pty->attrs[r][c];
            
            if (ch != ' ') {
                uint32_t fg = g_term.color_fg;
                uint32_t bg = g_term.color_bg;
                
                /* Parse attributes (simplified) */
                if (attr & 0x01) fg = 0xFFFFFF;  /* Bold */
                if (attr & 0x02) fg = 0xFF0000;  /* Red */
                if (attr & 0x04) fg = 0x00FF00;  /* Green */
                
                vbe_draw_char(cx, ry, ch, fg, 1);
            }

            /* Selection highlight */
            if (pty->selecting || pty->sel_start_y != pty->sel_end_y || pty->sel_start_x != pty->sel_end_x) {
                int sel_top = pty->sel_start_y < pty->sel_end_y ? pty->sel_start_y : pty->sel_end_y;
                int sel_bot = pty->sel_start_y > pty->sel_end_y ? pty->sel_start_y : pty->sel_end_y;
                int sel_left = pty->sel_start_x < pty->sel_end_x ? pty->sel_start_x : pty->sel_end_x;
                int sel_right = pty->sel_start_x > pty->sel_end_x ? pty->sel_start_x : pty->sel_end_x;
                
                if (r >= sel_top && r <= sel_bot) {
                    int cl = (r == sel_top) ? sel_left : 0;
                    int cr = (r == sel_bot) ? sel_right : cols - 1;
                    if (c >= cl && c <= cr) {
                        vbe_fill_rect(cx, ry, char_w, char_h, g_term.color_selection);
                        if (ch != ' ') vbe_draw_char(cx, ry, ch, 0xFFFFFF, 1);
                    }
                }
            }
        }
    }

    /* Render cursor */
    if (pty->cursor_visible && (pty->cursor_blink / 10) % 2 == 0) {
        int cx = x + pty->cursor_x * char_w;
        int cy = y + pty->cursor_y * char_h;
        vbe_fill_rect(cx, cy, char_w, char_h, g_term.color_cursor);
    }
    pty->cursor_blink++;
}

