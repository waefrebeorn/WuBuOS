/*
 * dosgui_term.c  --  WuBuOS Terminal (PTY + HolyC REPL + Tabbed)
 *
 * Phase 6: Full-featured terminal with:
 *   - PTY backend for shell sessions (bash, zsh, etc.)
 *   - Tabbed sessions (multiple shells in one window)
 *   - HolyC REPL pane integration
 *   - GPU-accelerated render via VBE double-buffer
 *   - Scrollback buffer with search
 *   - Copy/paste, selection, URL detection
 *   - Keyboard shortcuts
 */
#include "dosgui_term.h"
#include "dosgui_term_internal.h"   /* shared ANSI parser */
#include "dosgui_term_pty.h"        /* PTY/container session management */
#include "dosgui_wm.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include "../runtime/wubu_container.h"
#include "../runtime/wubu_exec.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <time.h>
#include <pty.h>

/* -- Safe String Macros (WUBU_SAFE_STRING) -------------------------- */

#define WUBU_STRCPY(dst, src, dst_size) \
    do { \
        if (dst_size > 0) { \
            strncpy((dst), (src), (dst_size) - 1); \
            (dst)[(dst_size) - 1] = '\0'; \
        } \
    } while (0)

#define WUBU_SNPRINTF(dst, dst_size, fmt, ...) \
    do { \
        if (dst_size > 0) { \
            snprintf((dst), (dst_size), (fmt), __VA_ARGS__); \
            (dst)[(dst_size) - 1] = '\0'; \
        } \
    } while (0)

#define WUBU_STRLCAT(dst, src, dst_size) \
    do { \
        size_t _dst_len = strlen(dst); \
        size_t _src_len = strlen(src); \
        if (_dst_len + _src_len + 1 <= dst_size) { \
            memcpy((dst) + _dst_len, (src), _src_len + 1); \
        } else if (_dst_len < dst_size) { \
            size_t _avail = (dst_size) - _dst_len - 1; \
            memcpy((dst) + _dst_len, (src), _avail); \
            (dst)[(dst_size) - 1] = '\0'; \
        } \
    } while (0)

/* -- Global State ------------------------------------------------- */

TermState g_term = {0};

/* -- Forward Declarations ----------------------------------------- */

static void term_render_tab_bar(TermState *term, uint32_t *fb, int fb_w, int fb_h);
static void term_render_content(TermState *term, uint32_t *fb, int fb_w, int fb_h);
void term_render_pty_session(TermPtySession *pty, uint32_t *fb, int x, int y, int w, int h);
static void term_render_holyc_session(TermHolycSession *holyc, uint32_t *fb, int x, int y, int w, int h);
void term_render_container_session(TermContainerSession *container, uint32_t *fb, int x, int y, int w, int h);
static void term_tab_bar_layout(TermState *term, int *tab_x, int *tab_w);
static void term_handle_mouse_tab_bar(TermState *term, int x, int y, int btn, int kind);
static void term_handle_mouse_content(TermState *term, int x, int y, int btn, int kind);
static void term_copy_selection(TermState *term);
static void term_paste_to_pty(TermState *term);

static const WubuThemeColors *tc(void) { return wubu_theme_colors(); }
static const WubuTheme *th(void) { return wubu_theme_get(); }
static int term_tab_bar_h(void) { return th()->rounded_buttons ? 28 : 24; }
static int term_char_w(void) { return 6; }
static int term_char_h(void) { return 10; }
static int term_side_padding(void) { return 4; }
static int term_top_padding(void) { return 2; }

/* -- Lifecycle ---------------------------------------------------- */

int dosgui_term_init(void) {
    memset(&g_term, 0, sizeof(g_term));
    g_term.tab_bar_h = term_tab_bar_h();
    g_term.show_tab_bar = true;
    
    dosgui_term_update_colors();
    return 0;
}

void dosgui_term_shutdown(void) {
    for (int i = 0; i < g_term.tab_count; i++) {
        TermTab *tab = &g_term.tabs[i];
        if (tab->type == TERM_SESSION_SHELL) {
            term_pty_cleanup(&tab->session.pty);
        } else if (tab->type == TERM_SESSION_CONTAINER) {
            term_container_cleanup(&tab->session.container);
        }
    }
    memset(&g_term, 0, sizeof(g_term));
}

/* -- Window Management -------------------------------------------- */

void dosgui_term_show(int x, int y, int w, int h) {
    if (dosgui_term_is_open()) return;

    g_term.x = x;
    g_term.y = y;
    g_term.w = w;
    g_term.h = h;

    DosGuiWindow *win = dosgui_wm_create(x, y, w, h, "Terminal");
    if (win) {
        g_term.win_id = win->id;
        win->user_data = &g_term;
        
        /* Create initial shell tab */
        dosgui_term_new_tab(TERM_SESSION_SHELL, "Shell", NULL);
    }
}

void dosgui_term_hide(void) {
    if (!dosgui_term_is_open()) return;
    
    DosGuiWindow *win = dosgui_wm_find_by_id(g_term.win_id);
    if (win) {
        dosgui_wm_destroy(win);
    }
    g_term.win_id = 0;
}

bool dosgui_term_is_open(void) {
    return g_term.win_id > 0 && dosgui_wm_find_by_id(g_term.win_id) != NULL;
}

void dosgui_term_toggle(void) {
    if (dosgui_term_is_open()) {
        dosgui_term_hide();
    } else {
        int w = vbe_state()->width * 3 / 4;
        int h = vbe_state()->height * 3 / 4;
        int x = (vbe_state()->width - w) / 2;
        int y = (vbe_state()->height - h) / 2;
        dosgui_term_show(x, y, w, h);
    }
}

/* -- Tab Management ----------------------------------------------- */

static const char *term_default_shell(void) {
    const char *shell = getenv("SHELL");
    return shell ? shell : "/bin/bash";
}

int dosgui_term_new_tab(TermSessionType type, const char *label, const char *shell) {
    if (g_term.tab_count >= TERM_MAX_TABS) return -1;

    int idx = g_term.tab_count;
    TermTab *tab = &g_term.tabs[idx];
    memset(tab, 0, sizeof(TermTab));
    
    tab->type = type;
    tab->active = false;
    tab->dirty = true;

    if (label) {
        strncpy(tab->label, label, TERM_TAB_LABEL_LEN - 1);
    } else {
        switch (type) {
            case TERM_SESSION_SHELL: snprintf(tab->label, sizeof(tab->label), "Shell %d", idx + 1); break;
            case TERM_SESSION_HOLYC: snprintf(tab->label, sizeof(tab->label), "HolyC %d", idx + 1); break;
            case TERM_SESSION_CONTAINER: snprintf(tab->label, sizeof(tab->label), "Container %d", idx + 1); break;
        }
    }

    /* Initialize session */
    switch (type) {
        case TERM_SESSION_SHELL:
            if (term_pty_spawn(shell ? shell : term_default_shell(), getenv("HOME"), &tab->session.pty, NULL) < 0) {
                return -1;
            }
            break;
        case TERM_SESSION_HOLYC:
            /* E4: embed the wubu_holyd HolyC REPL as a real PTY-backed
             * process so the Desktop terminal hosts a live interactive REPL. */
            {
                static const char *holy_argv[] = { "--repl", NULL };
                if (term_pty_spawn("wubu_holyd", getenv("HOME"), &tab->session.holyc.pty, holy_argv) < 0) {
                    return -1;
                }
            }
            break;
        case TERM_SESSION_CONTAINER:
            if (shell) {
                strncpy(tab->session.container.shell, shell, sizeof(tab->session.container.shell) - 1);
            } else {
                strncpy(tab->session.container.shell, "/bin/bash", sizeof(tab->session.container.shell) - 1);
            }
            if (term_container_spawn(&tab->session.container, tab->session.container.container_name) < 0) {
                return -1;
            }
            break;
    }

    g_term.tab_count++;
    
    /* Make it active */
    if (g_term.tab_count == 1) {
        g_term.active_tab = 0;
        tab->active = true;
    }

    return idx;
}

void dosgui_term_close_tab(int idx) {
    if (idx < 0 || idx >= g_term.tab_count) return;

    TermTab *tab = &g_term.tabs[idx];
    
    if (tab->type == TERM_SESSION_SHELL) {
        term_pty_cleanup(&tab->session.pty);
    } else if (tab->type == TERM_SESSION_CONTAINER) {
        term_container_cleanup(&tab->session.container);
    }

    /* Shift remaining tabs left */
    for (int i = idx; i < g_term.tab_count - 1; i++) {
        g_term.tabs[i] = g_term.tabs[i + 1];
    }
    g_term.tab_count--;

    /* Adjust active tab */
    if (g_term.active_tab >= g_term.tab_count) {
        g_term.active_tab = g_term.tab_count - 1;
    }
    if (g_term.active_tab >= 0 && g_term.tab_count > 0) {
        g_term.tabs[g_term.active_tab].active = true;
    }

    /* If no tabs left, close window */
    if (g_term.tab_count == 0) {
        dosgui_term_hide();
    }
}

void dosgui_term_switch_tab(int idx) {
    if (idx < 0 || idx >= g_term.tab_count) return;
    if (idx == g_term.active_tab) return;

    g_term.tabs[g_term.active_tab].active = false;
    g_term.active_tab = idx;
    g_term.tabs[g_term.active_tab].active = true;
    g_term.tabs[g_term.active_tab].dirty = true;
}

void dosgui_term_move_tab(int from, int to) {
    if (from < 0 || from >= g_term.tab_count) return;
    if (to < 0 || to >= g_term.tab_count) return;
    if (from == to) return;

    TermTab tab = g_term.tabs[from];
    
    if (from < to) {
        for (int i = from; i < to; i++) {
            g_term.tabs[i] = g_term.tabs[i + 1];
        }
    } else {
        for (int i = from; i > to; i--) {
            g_term.tabs[i] = g_term.tabs[i - 1];
        }
    }
    g_term.tabs[to] = tab;

    /* Update active_tab if it moved */
    if (g_term.active_tab == from) {
        g_term.active_tab = to;
    } else if (from < g_term.active_tab && g_term.active_tab <= to) {
        g_term.active_tab--;
    } else if (to <= g_term.active_tab && g_term.active_tab < from) {
        g_term.active_tab++;
    }
}

int dosgui_term_get_active_tab(void) {
    return g_term.active_tab;
}

TermTab *dosgui_term_get_tab(int idx) {
    if (idx < 0 || idx >= g_term.tab_count) return NULL;
    return &g_term.tabs[idx];
}

/* -- Session Spawning --------------------------------------------- */

int dosgui_term_spawn_shell(const char *shell, const char *cwd) {
    return dosgui_term_new_tab(TERM_SESSION_SHELL, NULL, shell);
}

int dosgui_term_spawn_holyc(void) {
    return dosgui_term_new_tab(TERM_SESSION_HOLYC, "HolyC REPL", NULL);
}

int dosgui_term_spawn_container(const char *container_name) {
    int idx = dosgui_term_new_tab(TERM_SESSION_CONTAINER, container_name ? container_name : "Container", NULL);
    if (idx >= 0) {
        TermTab *tab = &g_term.tabs[idx];
        if (container_name) {
            strncpy(tab->session.container.container_name, container_name, sizeof(tab->session.container.container_name) - 1);
        }
        /* Initialize container PTY */
        term_container_spawn(&tab->session.container, container_name);
    }
    return idx;
}

/* -- Container PTY Implementation --------------------------------- */

/* -- PTY Implementation ------------------------------------------- */

/* -- Input Handling ----------------------------------------------- */

void dosgui_term_handle_key(uint32_t key, uint32_t mods) {
    if (!dosgui_term_is_open()) return;

    bool ctrl = mods & 0x04;
    bool shift = mods & 0x01;
    bool alt = mods & 0x08;

    /* Global terminal shortcuts */
    if (ctrl && shift) {
        switch (key) {
            case 't': case 'T':  /* New tab */
                dosgui_term_new_tab(TERM_SESSION_SHELL, NULL, NULL);
                return;
            case 'w': case 'W':  /* Close tab */
                dosgui_term_close_tab(g_term.active_tab);
                return;
            case 'c': case 'C':  /* Copy */
                dosgui_term_copy_selection();
                return;
            case 'v': case 'V':  /* Paste */
                dosgui_term_paste();
                return;
            case 'f': case 'F':  /* Find */
                g_term.searching = true;
                g_term.search_buf[0] = '\0';
                g_term.search_pos = 0;
                return;
        }
    }

    if (ctrl && !shift) {
        switch (key) {
            case 0xE04E:  /* Ctrl+PgDn - next tab */
                if (g_term.active_tab < g_term.tab_count - 1) {
                    dosgui_term_switch_tab(g_term.active_tab + 1);
                }
                return;
            case 0xE04F:  /* Ctrl+PgUp - prev tab */
                if (g_term.active_tab > 0) {
                    dosgui_term_switch_tab(g_term.active_tab - 1);
                }
                return;
            case 'l': case 'L':  /* Clear screen */
                if (g_term.active_tab >= 0) {
                    TermTab *tab = &g_term.tabs[g_term.active_tab];
                    if (tab->type == TERM_SESSION_SHELL) {
                        term_reset_pty_screen(&tab->session.pty);
                        write(tab->session.pty.ptm_fd, "clear\n", 6);
                    }
                }
                return;
        }
    }

    /* Search mode */
    if (g_term.searching) {
        if (key == 27) {  /* Escape - exit search */
            g_term.searching = false;
            return;
        } else if (key == '\r' || key == '\n') {
            g_term.searching = false;
            return;
        } else if (key == 8 && g_term.search_pos > 0) {  /* Backspace */
            g_term.search_buf[--g_term.search_pos] = '\0';
            return;
        } else if (key >= 32 && key < 127 && g_term.search_pos < TERM_SEARCH_BUF - 1) {
            g_term.search_buf[g_term.search_pos++] = (char)key;
            g_term.search_buf[g_term.search_pos] = '\0';
            return;
        }
    }

    /* Delegate to active session */
    if (g_term.active_tab >= 0) {
        TermTab *tab = &g_term.tabs[g_term.active_tab];
        if (tab->type == TERM_SESSION_SHELL) {
            term_handle_key_pty(&g_term, key, mods);
        } else if (tab->type == TERM_SESSION_HOLYC) {
            term_handle_key_holyc(&g_term, key, mods);
        } else if (tab->type == TERM_SESSION_CONTAINER) {
            term_handle_key_container(&g_term, key, mods);
        }
    }
}

void dosgui_term_handle_mouse(int x, int y, int btn, int kind) {
    if (!dosgui_term_is_open()) return;

    TermState *term = &g_term;
    int tab_bar_y = term->y;
    int content_y = term->y + term->tab_bar_h;

    if (y >= tab_bar_y && y < content_y) {
        term_handle_mouse_tab_bar(term, x, y, btn, kind);
    } else if (y >= content_y && y < term->y + term->h) {
        term_handle_mouse_content(term, x, y, btn, kind);
    }
}

static void term_handle_mouse_tab_bar(TermState *term, int x, int y, int btn, int kind) {
    int tab_x, tab_w;
    term_tab_bar_layout(term, &tab_x, &tab_w);

    if (kind == 1 && btn == 1) {  /* Left click down */
        int rel_x = x - term->x - tab_x;
        if (rel_x >= 0) {
            int tab_idx = rel_x / tab_w;
            if (tab_idx >= 0 && tab_idx < term->tab_count) {
                dosgui_term_switch_tab(tab_idx);
                if (btn == 1 && kind == 1) {  /* Start drag on second click? */
                    term->tab_drag = true;
                    term->drag_tab_idx = tab_idx;
                    term->drag_offset_x = rel_x % tab_w;
                }
            }
            /* Check close button on active tab */
            int active_x = tab_x + g_term.active_tab * tab_w;
            int close_x = active_x + tab_w - 18;
            if (x >= close_x && x < close_x + 14) {
                dosgui_term_close_tab(g_term.active_tab);
            }
        }
    } else if (kind == 2 && btn == 1) {  /* Left click up */
        if (term->tab_drag) {
            int rel_x = x - term->x - tab_x;
            int new_idx = rel_x / tab_w;
            if (new_idx >= 0 && new_idx < term->tab_count) {
                dosgui_term_move_tab(term->drag_tab_idx, new_idx);
            }
            term->tab_drag = false;
            term->drag_tab_idx = -1;
        }
    }
}

static void term_tab_bar_layout(TermState *term, int *tab_x, int *tab_w) {
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

static void term_handle_mouse_content(TermState *term, int x, int y, int btn, int kind) {
    if (term->active_tab < 0) return;
    TermTab *tab = &term->tabs[term->active_tab];
    
    if (tab->type == TERM_SESSION_SHELL) {
        TermPtySession *pty = &tab->session.pty;
        
        if (kind == 1 && btn == 1) {
            /* Start selection */
            pty->selecting = true;
            pty->sel_start_x = (x - term->x - term_side_padding()) / term_char_w();
            pty->sel_start_y = (y - term->y - term->tab_bar_h - term_top_padding()) / term_char_h() + pty->scrollback_view;
            pty->sel_end_x = pty->sel_start_x;
            pty->sel_end_y = pty->sel_start_y;
        } else if (kind == 0 && pty->selecting) {
            /* Update selection */
            pty->sel_end_x = (x - term->x - term_side_padding()) / term_char_w();
            pty->sel_end_y = (y - term->y - term->tab_bar_h - term_top_padding()) / term_char_h() + pty->scrollback_view;
        } else if (kind == 2 && btn == 1) {
            /* End selection */
            pty->selecting = false;
        } else if (kind == 1 && btn == 2) {
            /* Middle click - paste */
            dosgui_term_paste();
        }
    } else if (tab->type == TERM_SESSION_CONTAINER) {
        TermContainerSession *container = &tab->session.container;
        
        if (kind == 1 && btn == 1) {
            /* Start selection */
            container->selecting = true;
            container->sel_start_x = (x - term->x - term_side_padding()) / term_char_w();
            container->sel_start_y = (y - term->y - term->tab_bar_h - term_top_padding()) / term_char_h();
            container->sel_end_x = container->sel_start_x;
            container->sel_end_y = container->sel_start_y;
        } else if (kind == 0 && container->selecting) {
            /* Update selection */
            container->sel_end_x = (x - term->x - term_side_padding()) / term_char_w();
            container->sel_end_y = (y - term->y - term->tab_bar_h - term_top_padding()) / term_char_h();
        } else if (kind == 2 && btn == 1) {
            /* End selection */
            container->selecting = false;
        } else if (kind == 1 && btn == 2) {
            /* Middle click - paste */
            dosgui_term_paste();
        }
    }
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

/* -- PTY I/O ------------------------------------------------------ */

void dosgui_term_pty_write(const char *data, int len) {
    if (g_term.active_tab < 0) return;
    TermTab *tab = &g_term.tabs[g_term.active_tab];

    if (tab->type == TERM_SESSION_SHELL) {
        TermPtySession *pty = &tab->session.pty;
        if (pty->ptm_fd >= 0) {
            write(pty->ptm_fd, data, len);
        }
    } else if (tab->type == TERM_SESSION_HOLYC) {
        TermPtySession *pty = &tab->session.holyc.pty;
        if (pty->ptm_fd >= 0) {
            write(pty->ptm_fd, data, len);
        }
    } else if (tab->type == TERM_SESSION_CONTAINER) {
        TermContainerSession *container = &tab->session.container;
        if (container->ptm_fd >= 0) {
            write(container->ptm_fd, data, len);
        }
    }
}

void dosgui_term_pty_read(void) {
    if (g_term.active_tab < 0) return;
    TermTab *tab = &g_term.tabs[g_term.active_tab];
    if (tab->type == TERM_SESSION_SHELL) {
        term_process_pty_output(&tab->session.pty);
    } else if (tab->type == TERM_SESSION_HOLYC) {
        term_process_pty_output(&tab->session.holyc.pty);
    }
}

/* -- Helpers ------------------------------------------------------ */

void dosgui_term_update_colors(void) {
    g_term.color_fg = 0xFFFFFF;
    g_term.color_bg = 0x000000;
    g_term.color_cursor = 0xFFFFFF;
    g_term.color_selection = 0x000080;
    g_term.color_tab_active = tc()->select_bg;
    g_term.color_tab_inactive = tc()->win_face;
    g_term.color_tab_hover = tc()->btn_hover;
}

void dosgui_term_scroll(int lines) {
    if (g_term.active_tab < 0) return;
    TermTab *tab = &g_term.tabs[g_term.active_tab];
    if (tab->type != TERM_SESSION_SHELL) return;

    TermPtySession *pty = &tab->session.pty;
    pty->scrollback_view += lines;
    if (pty->scrollback_view < 0) pty->scrollback_view = 0;
    if (pty->scrollback_view > pty->scrollback_count - pty->rows) {
        pty->scrollback_view = pty->scrollback_count - pty->rows;
    }
}

void dosgui_term_copy_selection(void) {
    if (g_term.active_tab < 0) return;
    TermTab *tab = &g_term.tabs[g_term.active_tab];
    if (tab->type != TERM_SESSION_SHELL) return;

    TermPtySession *pty = &tab->session.pty;
    if (!pty->selecting && pty->sel_start_y == pty->sel_end_y && pty->sel_start_x == pty->sel_end_x) {
        return;  /* No selection */
    }

    int sel_top = pty->sel_start_y < pty->sel_end_y ? pty->sel_start_y : pty->sel_end_y;
    int sel_bot = pty->sel_start_y > pty->sel_end_y ? pty->sel_start_y : pty->sel_end_y;
    int sel_left = pty->sel_start_x < pty->sel_end_x ? pty->sel_start_x : pty->sel_end_x;
    int sel_right = pty->sel_start_x > pty->sel_end_x ? pty->sel_start_x : pty->sel_end_x;

    char *dst = g_term.clipboard;
    int remaining = sizeof(g_term.clipboard) - 1;

    for (int r = sel_top; r <= sel_bot && remaining > 0; r++) {
        if (r < 0 || r >= TERM_SCROLLBACK_LINES) continue;
        int idx = (pty->scrollback_head - pty->scrollback_count + r + TERM_SCROLLBACK_LINES) % TERM_SCROLLBACK_LINES;
        const char *line = pty->scrollback[idx];
        int cl = (r == sel_top) ? sel_left : 0;
        int cr = (r == sel_bot) ? sel_right : pty->cols - 1;
        
        for (int c = cl; c <= cr && c < TERM_MAX_COLS && remaining > 0; c++) {
            *dst++ = line[c];
            remaining--;
        }
        if (r < sel_bot && remaining > 0) {
            *dst++ = '\n';
            remaining--;
        }
    }
    *dst = '\0';
    g_term.has_selection = true;
}

void dosgui_term_paste(void) {
    if (!g_term.has_selection || g_term.clipboard[0] == '\0') return;
    
    if (g_term.active_tab >= 0) {
        TermTab *tab = &g_term.tabs[g_term.active_tab];
        if (tab->type == TERM_SESSION_SHELL) {
            dosgui_term_pty_write(g_term.clipboard, strlen(g_term.clipboard));
        }
    }
}

const char *dosgui_term_get_cwd(void) {
    if (g_term.active_tab >= 0) {
        TermTab *tab = &g_term.tabs[g_term.active_tab];
        if (tab->type == TERM_SESSION_SHELL) {
            return tab->session.pty.cwd;
        }
    }
    return getenv("HOME");
}

/* -- State Accessor ----------------------------------------------- */

TermState *dosgui_term_state(void) {
    return &g_term;
}
