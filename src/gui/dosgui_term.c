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
void term_tab_bar_layout(TermState *term, int *tab_x, int *tab_w);
static void term_handle_mouse_tab_bar(TermState *term, int x, int y, int btn, int kind);
static void term_handle_mouse_content(TermState *term, int x, int y, int btn, int kind);
static void term_copy_selection(TermState *term);
static void term_paste_to_pty(TermState *term);


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

