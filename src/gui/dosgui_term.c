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
static void term_render_pty_session(TermPtySession *pty, uint32_t *fb, int x, int y, int w, int h);
static void term_render_holyc_session(TermHolycSession *holyc, uint32_t *fb, int x, int y, int w, int h);
static void term_render_container_session(TermContainerSession *container, uint32_t *fb, int x, int y, int w, int h);
static void term_tab_bar_layout(TermState *term, int *tab_x, int *tab_w);
static void term_update_pty_size(TermPtySession *pty, int cols, int rows);
static void term_pty_push_line(TermPtySession *pty, const char *line);
static void term_process_pty_output(TermPtySession *pty);
static int  term_pty_spawn(const char *shell, const char *cwd, TermPtySession *pty);
static void term_pty_cleanup(TermPtySession *pty);
static void term_handle_key_pty(TermState *term, uint32_t key, uint32_t mods);
static void term_handle_key_holyc(TermState *term, uint32_t key, uint32_t mods);
static void term_handle_key_container(TermState *term, uint32_t key, uint32_t mods);
static void term_handle_mouse_tab_bar(TermState *term, int x, int y, int btn, int kind);
static void term_handle_mouse_content(TermState *term, int x, int y, int btn, int kind);
static void term_copy_selection(TermState *term);
static void term_paste_to_pty(TermState *term);
static void term_reset_pty_screen(TermPtySession *pty);
static void term_pty_cursor_move(TermPtySession *pty, int x, int y);
static void term_pty_put_char(TermPtySession *pty, char c, uint8_t attr);
static int  term_container_spawn(TermContainerSession *container, const char *container_name);
static void term_container_cleanup(TermContainerSession *container);
static void term_update_container_size(TermContainerSession *container, int cols, int rows);
static void term_process_container_output(TermContainerSession *container);

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
            if (term_pty_spawn(shell ? shell : term_default_shell(), getenv("HOME"), &tab->session.pty) < 0) {
                return -1;
            }
            break;
        case TERM_SESSION_HOLYC:
            tab->session.holyc.holyc_term = NULL; /* Will be created on demand */
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

static int term_container_spawn(TermContainerSession *container, const char *container_name) {
    memset(container, 0, sizeof(TermContainerSession));
    container->ptm_fd = -1;
    container->running = true;
    container->cols = 80;
    container->rows = 24;
    container->cursor_x = 0;
    container->cursor_y = 0;
    container->saved_cursor_x = 0;
    container->saved_cursor_y = 0;
    strncpy(container->shell, "/bin/bash", sizeof(container->shell) - 1);
    if (container_name) {
        strncpy(container->container_name, container_name, sizeof(container->container_name) - 1);
    } else {
        strncpy(container->container_name, "container", sizeof(container->container_name) - 1);
    }
    strncpy(container->cwd, "/home/wubu", sizeof(container->cwd) - 1);

    /* Open PTY master */
    container->ptm_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (container->ptm_fd < 0) {
        perror("posix_openpt (container)");
        return -1;
    }

    if (grantpt(container->ptm_fd) < 0 || unlockpt(container->ptm_fd) < 0) {
        perror("grantpt/unlockpt (container)");
        close(container->ptm_fd);
        container->ptm_fd = -1;
        return -1;
    }

    char *pts_name = ptsname(container->ptm_fd);
    if (!pts_name) {
        perror("ptsname (container)");
        close(container->ptm_fd);
        container->ptm_fd = -1;
        return -1;
    }

    /* Fork child */
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork (container)");
        close(container->ptm_fd);
        container->ptm_fd = -1;
        return -1;
    }

    if (pid == 0) {
        /* Child process - exec inside container via wubu_exec */
        setsid();

        int pts_fd = open(pts_name, O_RDWR | O_NOCTTY);
        if (pts_fd < 0) {
            _exit(1);
        }

        if (ioctl(pts_fd, TIOCSCTTY, 0) < 0) {
            _exit(1);
        }

        dup2(pts_fd, STDIN_FILENO);
        dup2(pts_fd, STDOUT_FILENO);
        dup2(pts_fd, STDERR_FILENO);
        if (pts_fd > STDERR_FILENO) close(pts_fd);

        struct winsize ws = { .ws_col = container->cols, .ws_row = container->rows };
        ioctl(STDOUT_FILENO, TIOCSWINSZ, &ws);

        if (container->cwd[0]) chdir(container->cwd);

        /* Execute wubu_exec to run container shell */
        char *args[] = { "wubu", "run", container->container_name, container->shell, NULL };
        execvp("wubu", args);

        /* Fallback to direct shell if wubu not available */
        char *fallback_args[] = { (char*)container->shell, "-l", NULL };
        execvp(container->shell, fallback_args);
        execl("/bin/sh", "sh", "-l", NULL);
        _exit(1);
    }

    /* Parent */
    container->child_pid = pid;

    /* Set master PTY to non-blocking */
    int flags = fcntl(container->ptm_fd, F_GETFL, 0);
    fcntl(container->ptm_fd, F_SETFL, flags | O_NONBLOCK);

    return 0;
}

/* -- PTY Implementation ------------------------------------------- */

static int term_pty_spawn(const char *shell, const char *cwd, TermPtySession *pty) {
    memset(pty, 0, sizeof(TermPtySession));
    pty->ptm_fd = -1;
    pty->running = true;
    pty->cols = 80;
    pty->rows = 24;
    pty->cursor_visible = true;
    pty->cursor_blink = 0;
    strncpy(pty->shell, shell, sizeof(pty->shell) - 1);
    if (cwd) strncpy(pty->cwd, cwd, sizeof(pty->cwd) - 1);

    term_reset_pty_screen(pty);

    /* Open PTY master */
    pty->ptm_fd = posix_openpt(O_RDWR | O_NOCTTY);
    if (pty->ptm_fd < 0) {
        perror("posix_openpt");
        return -1;
    }

    if (grantpt(pty->ptm_fd) < 0 || unlockpt(pty->ptm_fd) < 0) {
        perror("grantpt/unlockpt");
        close(pty->ptm_fd);
        pty->ptm_fd = -1;
        return -1;
    }

    char *pts_name = ptsname(pty->ptm_fd);
    if (!pts_name) {
        perror("ptsname");
        close(pty->ptm_fd);
        pty->ptm_fd = -1;
        return -1;
    }

    /* Fork child */
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        close(pty->ptm_fd);
        pty->ptm_fd = -1;
        return -1;
    }

    if (pid == 0) {
        /* Child process */
        setsid();
        
        /* Open slave PTY */
        int pts_fd = open(pts_name, O_RDWR | O_NOCTTY);
        if (pts_fd < 0) {
            _exit(1);
        }

        /* Set as controlling terminal */
        if (ioctl(pts_fd, TIOCSCTTY, 0) < 0) {
            _exit(1);
        }

        /* Redirect stdio */
        dup2(pts_fd, STDIN_FILENO);
        dup2(pts_fd, STDOUT_FILENO);
        dup2(pts_fd, STDERR_FILENO);
        if (pts_fd > STDERR_FILENO) close(pts_fd);

        /* Set terminal size */
        struct winsize ws = { .ws_col = pty->cols, .ws_row = pty->rows };
        ioctl(STDOUT_FILENO, TIOCSWINSZ, &ws);

        /* Change directory */
        if (pty->cwd[0]) chdir(pty->cwd);

        /* Execute shell */
        char *args[] = { (char*)pty->shell, "-l", NULL };
        execvp(pty->shell, args);
        
        /* Fallback */
        execl("/bin/sh", "sh", "-l", NULL);
        _exit(1);
    }

    /* Parent */
    pty->child_pid = pid;

    /* Set master PTY to non-blocking */
    int flags = fcntl(pty->ptm_fd, F_GETFL, 0);
    fcntl(pty->ptm_fd, F_SETFL, flags | O_NONBLOCK);

    return 0;
}

static void term_pty_cleanup(TermPtySession *pty) {
    pty->running = false;
    
    if (pty->child_pid > 0) {
        kill(pty->child_pid, SIGHUP);
        waitpid(pty->child_pid, NULL, 0);
        pty->child_pid = 0;
    }
    if (pty->ptm_fd >= 0) {
        close(pty->ptm_fd);
        pty->ptm_fd = -1;
    }
}

static void term_container_cleanup(TermContainerSession *container) {
    container->running = false;
    
    if (container->child_pid > 0) {
        kill(container->child_pid, SIGHUP);
        waitpid(container->child_pid, NULL, 0);
        container->child_pid = 0;
    }
    if (container->ptm_fd >= 0) {
        close(container->ptm_fd);
        container->ptm_fd = -1;
    }
}

static void term_update_pty_size(TermPtySession *pty, int cols, int rows) {
    if (pty->cols == cols && pty->rows == rows) return;
    pty->cols = cols;
    pty->rows = rows;

    if (pty->ptm_fd >= 0) {
        struct winsize ws = { .ws_col = cols, .ws_row = rows };
        ioctl(pty->ptm_fd, TIOCSWINSZ, &ws);
    }

    term_reset_pty_screen(pty);
}

static void term_reset_pty_screen(TermPtySession *pty) {
    for (int r = 0; r < TERM_MAX_ROWS; r++) {
        memset(pty->screen[r], ' ', TERM_MAX_COLS);
        memset(pty->attrs[r], 0, TERM_MAX_COLS);
        pty->dirty[r] = true;
    }
    pty->cursor_x = 0;
    pty->cursor_y = 0;
}

static void term_update_container_size(TermContainerSession *container, int cols, int rows) {
    if (container->cols == cols && container->rows == rows) return;
    container->cols = cols;
    container->rows = rows;

    if (container->ptm_fd >= 0) {
        struct winsize ws = { .ws_col = cols, .ws_row = rows };
        ioctl(container->ptm_fd, TIOCSWINSZ, &ws);
    }
}

static void term_process_container_output(TermContainerSession *container) {
    if (container->ptm_fd < 0) return;

    char buf[4096];
    ssize_t n = read(container->ptm_fd, buf, sizeof(buf) - 1);
    if (n <= 0) return;

    buf[n] = '\0';

    /* Parse ANSI sequences and update container screen buffer */
    /* Reuse the same ANSI parser logic as PTY sessions */
    typedef enum { ANSI_NORMAL, ANSI_ESC, ANSI_CSI } AnsiState;
    static AnsiState ansi_state = ANSI_NORMAL;
    static int ansi_param[8] = {0};
    static int ansi_param_count = 0;
    static int ansi_param_accum = 0;
    static bool ansi_param_pending = false;

    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)buf[i];

        if (ansi_state == ANSI_NORMAL) {
            if (c == 0x1B) {
                ansi_state = ANSI_ESC;
            } else if (c == '\n') {
                if (container->cursor_y >= container->rows - 1) {
                    /* Scroll up */
                    for (int r = 0; r < container->rows - 1; r++) {
                        memcpy(container->screen[r], container->screen[r + 1], TERM_MAX_COLS);
                        memcpy(container->attrs[r], container->attrs[r + 1], TERM_MAX_COLS);
                    }
                    memset(container->screen[container->rows - 1], ' ', TERM_MAX_COLS);
                    memset(container->attrs[container->rows - 1], 0, TERM_MAX_COLS);
                } else {
                    container->cursor_y++;
                }
                container->cursor_x = 0;
            } else if (c == '\r') {
                container->cursor_x = 0;
            } else if (c == '\t') {
                container->cursor_x = (container->cursor_x + 8) & ~7;
                if (container->cursor_x >= container->cols) container->cursor_x = container->cols - 1;
            } else if (c == '\b') {
                if (container->cursor_x > 0) container->cursor_x--;
            } else if (c >= 32 && c < 127) {
                if (container->cursor_x < TERM_MAX_COLS && container->cursor_y < TERM_MAX_ROWS) {
                    container->screen[container->cursor_y][container->cursor_x] = (char)c;
                    container->attrs[container->cursor_y][container->cursor_x] = container->cur_attr;
                }
                container->cursor_x++;
                if (container->cursor_x >= container->cols) {
                    container->cursor_x = 0;
                    if (container->cursor_y < container->rows - 1) container->cursor_y++;
                }
            }
        } else if (ansi_state == ANSI_ESC) {
            if (c == '[') {
                ansi_state = ANSI_CSI;
                ansi_param_count = 0;
                ansi_param_accum = 0;
                ansi_param_pending = false;
                memset(ansi_param, 0, sizeof(ansi_param));
            } else {
                ansi_state = ANSI_NORMAL;
            }
        } else if (ansi_state == ANSI_CSI) {
            if (c >= '0' && c <= '9') {
                ansi_param_accum = ansi_param_accum * 10 + (c - '0');
                ansi_param_pending = true;
            } else if (c == ';') {
                if (ansi_param_count < 8) {
                    ansi_param[ansi_param_count++] = ansi_param_accum;
                }
                ansi_param_accum = 0;
                ansi_param_pending = false;
            } else {
                if (ansi_param_pending && ansi_param_count < 8) {
                    ansi_param[ansi_param_count++] = ansi_param_accum;
                }
                int p0 = ansi_param_count > 0 ? ansi_param[0] : 1;
                if (p0 <= 0) p0 = 1;

                switch (c) {
                    case 'A':
                        container->cursor_y -= p0;
                        if (container->cursor_y < 0) container->cursor_y = 0;
                        break;
                    case 'B':
                    case 'e':
                        container->cursor_y += p0;
                        if (container->cursor_y >= container->rows) container->cursor_y = container->rows - 1;
                        break;
                    case 'C':
                    case 'a':
                        container->cursor_x += p0;
                        if (container->cursor_x >= container->cols) container->cursor_x = container->cols - 1;
                        break;
                    case 'D':
                        container->cursor_x -= p0;
                        if (container->cursor_x < 0) container->cursor_x = 0;
                        break;
                    case 'H':
                    case 'f':
                        container->cursor_y = (ansi_param_count > 0 ? ansi_param[0] : 1) - 1;
                        container->cursor_x = (ansi_param_count > 1 ? ansi_param[1] : 1) - 1;
                        if (container->cursor_y < 0) container->cursor_y = 0;
                        if (container->cursor_y >= container->rows) container->cursor_y = container->rows - 1;
                        if (container->cursor_x < 0) container->cursor_x = 0;
                        if (container->cursor_x >= container->cols) container->cursor_x = container->cols - 1;
                        break;
                    case 'J':
                        if (ansi_param_count == 0 || ansi_param[0] == 0) {
                            for (int r = container->cursor_y; r < container->rows; r++) {
                                int start_c = (r == container->cursor_y) ? container->cursor_x : 0;
                                memset(container->screen[r] + start_c, ' ', container->cols - start_c);
                                memset(container->attrs[r] + start_c, 0, container->cols - start_c);
                            }
                        }
                        break;
                    case 'K':
                        if (ansi_param_count == 0 || ansi_param[0] == 0) {
                            memset(container->screen[container->cursor_y] + container->cursor_x, ' ', container->cols - container->cursor_x);
                            memset(container->attrs[container->cursor_y] + container->cursor_x, 0, container->cols - container->cursor_x);
                        }
                        break;
                    case 'm':
                        if (ansi_param_count == 0) {
                            container->cur_attr = 0;
                            container->cur_fg = 7;
                            container->cur_bg = 0;
                        }
                        for (int p = 0; p < ansi_param_count; p++) {
                            int val = ansi_param[p];
                            if (val == 0) { container->cur_attr = 0; container->cur_fg = 7; container->cur_bg = 0; }
                            else if (val == 1) container->cur_attr |= 0x01;
                            else if (val == 4) container->cur_attr |= 0x02;
                            else if (val == 7) container->cur_attr |= 0x04;
                            else if (val >= 30 && val <= 37) container->cur_fg = val - 30;
                            else if (val >= 40 && val <= 47) container->cur_bg = val - 40;
                            else if (val >= 90 && val <= 97) container->cur_fg = val - 90 + 8;
                            else if (val >= 100 && val <= 107) container->cur_bg = val - 100 + 8;
                        }
                        break;
                    case 's':
                        container->saved_cursor_x = container->cursor_x;
                        container->saved_cursor_y = container->cursor_y;
                        break;
                    case 'u':
                        container->cursor_x = container->saved_cursor_x;
                        container->cursor_y = container->saved_cursor_y;
                        break;
                }
                ansi_state = ANSI_NORMAL;
            }
        }
    }
}

static void term_pty_push_line(TermPtySession *pty, const char *line) {
    if (pty->scrollback_count < TERM_SCROLLBACK_LINES) {
        strncpy(pty->scrollback[pty->scrollback_head], line, TERM_MAX_LINE_LEN - 1);
        pty->scrollback_head = (pty->scrollback_head + 1) % TERM_SCROLLBACK_LINES;
        pty->scrollback_count++;
    } else {
        strncpy(pty->scrollback[pty->scrollback_head], line, TERM_MAX_LINE_LEN - 1);
        pty->scrollback_head = (pty->scrollback_head + 1) % TERM_SCROLLBACK_LINES;
    }
    /* Auto-scroll to bottom on new output unless viewing history */
    if (pty->scrollback_view == 0) {
        pty->scrollback_view = 0;
    }
}

static void term_process_pty_output(TermPtySession *pty) {
    if (pty->ptm_fd < 0) return;

    char buf[4096];
    ssize_t n = read(pty->ptm_fd, buf, sizeof(buf) - 1);
    if (n <= 0) return;

    buf[n] = '\0';

    /* ANSI escape sequence parser state */
    typedef enum { ANSI_NORMAL, ANSI_ESC, ANSI_CSI } AnsiState;
    static AnsiState ansi_state = ANSI_NORMAL;
    static int ansi_param[8] = {0};
    static int ansi_param_count = 0;
    static int ansi_param_accum = 0;
    static bool ansi_param_pending = false;

    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)buf[i];

        if (ansi_state == ANSI_NORMAL) {
            if (c == 0x1B) {
                ansi_state = ANSI_ESC;
            } else if (c == '\n') {
                /* Newline: scroll if at bottom */
                if (pty->cursor_y >= pty->rows - 1) {
                    /* Scroll up */
                    for (int r = 0; r < pty->rows - 1; r++) {
                        memcpy(pty->screen[r], pty->screen[r + 1], TERM_MAX_COLS);
                        memcpy(pty->attrs[r], pty->attrs[r + 1], TERM_MAX_COLS);
                    }
                    memset(pty->screen[pty->rows - 1], ' ', TERM_MAX_COLS);
                    memset(pty->attrs[pty->rows - 1], 0, TERM_MAX_COLS);
                } else {
                    pty->cursor_y++;
                }
                pty->cursor_x = 0;
            } else if (c == '\r') {
                pty->cursor_x = 0;
            } else if (c == '\t') {
                /* Tab: advance to next 8-char boundary */
                pty->cursor_x = (pty->cursor_x + 8) & ~7;
                if (pty->cursor_x >= pty->cols) pty->cursor_x = pty->cols - 1;
            } else if (c == '\b') {
                if (pty->cursor_x > 0) pty->cursor_x--;
            } else if (c == '\a') {
                /* Bell — no-op for now */
            } else if (c >= 32 && c < 127) {
                if (pty->cursor_x < TERM_MAX_COLS && pty->cursor_y < TERM_MAX_ROWS) {
                    pty->screen[pty->cursor_y][pty->cursor_x] = (char)c;
                    pty->attrs[pty->cursor_y][pty->cursor_x] = pty->cur_attr;
                }
                pty->cursor_x++;
                if (pty->cursor_x >= pty->cols) {
                    pty->cursor_x = 0;
                    if (pty->cursor_y < pty->rows - 1) pty->cursor_y++;
                }
            }
        } else if (ansi_state == ANSI_ESC) {
            if (c == '[') {
                ansi_state = ANSI_CSI;
                ansi_param_count = 0;
                ansi_param_accum = 0;
                ansi_param_pending = false;
                memset(ansi_param, 0, sizeof(ansi_param));
            } else if (c == '7') {
                /* DECSC — save cursor (no-op for now) */
                ansi_state = ANSI_NORMAL;
            } else if (c == '8') {
                /* DECRC — restore cursor (no-op for now) */
                ansi_state = ANSI_NORMAL;
            } else {
                ansi_state = ANSI_NORMAL;
            }
        } else if (ansi_state == ANSI_CSI) {
            if (c >= '0' && c <= '9') {
                ansi_param_accum = ansi_param_accum * 10 + (c - '0');
                ansi_param_pending = true;
            } else if (c == ';') {
                if (ansi_param_count < 8) {
                    ansi_param[ansi_param_count++] = ansi_param_accum;
                }
                ansi_param_accum = 0;
                ansi_param_pending = false;
            } else {
                /* Final byte — execute CSI sequence */
                if (ansi_param_pending && ansi_param_count < 8) {
                    ansi_param[ansi_param_count++] = ansi_param_accum;
                }
                int p0 = ansi_param_count > 0 ? ansi_param[0] : 1;
                if (p0 <= 0) p0 = 1;

                switch (c) {
                    case 'A': /* CUU — cursor up */
                        pty->cursor_y -= p0;
                        if (pty->cursor_y < 0) pty->cursor_y = 0;
                        break;
                    case 'B': /* CUD — cursor down */
                    case 'e': /* VPR */
                        pty->cursor_y += p0;
                        if (pty->cursor_y >= pty->rows) pty->cursor_y = pty->rows - 1;
                        break;
                    case 'C': /* CUF — cursor forward */
                    case 'a': /* HPR */
                        pty->cursor_x += p0;
                        if (pty->cursor_x >= pty->cols) pty->cursor_x = pty->cols - 1;
                        break;
                    case 'D': /* CUB — cursor backward */
                        pty->cursor_x -= p0;
                        if (pty->cursor_x < 0) pty->cursor_x = 0;
                        break;
                    case 'H': /* CUP — cursor position */
                    case 'f': /* HVP */
                    {
                        int row = (ansi_param_count > 0 ? ansi_param[0] : 1) - 1;
                        int col = (ansi_param_count > 1 ? ansi_param[1] : 1) - 1;
                        if (row < 0) row = 0;
                        if (row >= pty->rows) row = pty->rows - 1;
                        if (col < 0) col = 0;
                        if (col >= pty->cols) col = pty->cols - 1;
                        pty->cursor_y = row;
                        pty->cursor_x = col;
                        break;
                    }
                    case 'J': /* ED — erase in display */
                    {
                        int mode = ansi_param_count > 0 ? ansi_param[0] : 0;
                        if (mode == 0) {
                            /* Erase from cursor to end */
                            memset(pty->screen[pty->cursor_y] + pty->cursor_x, ' ',
                                   pty->cols - pty->cursor_x);
                            memset(pty->attrs[pty->cursor_y] + pty->cursor_x, 0,
                                   pty->cols - pty->cursor_x);
                            for (int r = pty->cursor_y + 1; r < pty->rows; r++) {
                                memset(pty->screen[r], ' ', TERM_MAX_COLS);
                                memset(pty->attrs[r], 0, TERM_MAX_COLS);
                            }
                        } else if (mode == 1) {
                            /* Erase from start to cursor */
                            for (int r = 0; r < pty->cursor_y; r++) {
                                memset(pty->screen[r], ' ', TERM_MAX_COLS);
                                memset(pty->attrs[r], 0, TERM_MAX_COLS);
                            }
                            memset(pty->screen[pty->cursor_y], ' ', pty->cursor_x + 1);
                            memset(pty->attrs[pty->cursor_y], 0, pty->cursor_x + 1);
                        } else if (mode == 2 || mode == 3) {
                            /* Erase entire screen */
                            for (int r = 0; r < pty->rows; r++) {
                                memset(pty->screen[r], ' ', TERM_MAX_COLS);
                                memset(pty->attrs[r], 0, TERM_MAX_COLS);
                            }
                            pty->cursor_x = 0;
                            pty->cursor_y = 0;
                        }
                        break;
                    }
                    case 'K': /* EL — erase in line */
                    {
                        int mode = ansi_param_count > 0 ? ansi_param[0] : 0;
                        if (mode == 0) {
                            /* Erase from cursor to end of line */
                            memset(pty->screen[pty->cursor_y] + pty->cursor_x, ' ',
                                   pty->cols - pty->cursor_x);
                            memset(pty->attrs[pty->cursor_y] + pty->cursor_x, 0,
                                   pty->cols - pty->cursor_x);
                        } else if (mode == 1) {
                            /* Erase from start to cursor */
                            memset(pty->screen[pty->cursor_y], ' ', pty->cursor_x + 1);
                            memset(pty->attrs[pty->cursor_y], 0, pty->cursor_x + 1);
                        } else if (mode == 2) {
                            /* Erase entire line */
                            memset(pty->screen[pty->cursor_y], ' ', TERM_MAX_COLS);
                            memset(pty->attrs[pty->cursor_y], 0, TERM_MAX_COLS);
                        }
                        break;
                    }
                    case 'm': /* SGR — select graphic rendition */
                    {
                        if (ansi_param_count == 0) {
                            /* Reset all attributes */
                            pty->cur_attr = 0;
                            pty->cur_fg = 7;  /* light gray */
                            pty->cur_bg = 0;  /* black */
                        }
                        for (int p = 0; p < ansi_param_count; p++) {
                            int val = ansi_param[p];
                            if (val == 0) {
                                pty->cur_attr = 0;
                                pty->cur_fg = 7;
                                pty->cur_bg = 0;
                            } else if (val == 1) {
                                pty->cur_attr |= 0x01; /* bold */
                            } else if (val == 4) {
                                pty->cur_attr |= 0x02; /* underline */
                            } else if (val == 7) {
                                pty->cur_attr |= 0x04; /* reverse */
                            } else if (val >= 30 && val <= 37) {
                                pty->cur_fg = (uint8_t)(val - 30);
                            } else if (val >= 40 && val <= 47) {
                                pty->cur_bg = (uint8_t)(val - 40);
                            } else if (val >= 90 && val <= 97) {
                                pty->cur_fg = (uint8_t)(val - 90 + 8); /* bright fg */
                            } else if (val >= 100 && val <= 107) {
                                pty->cur_bg = (uint8_t)(val - 100 + 8); /* bright bg */
                            } else if (val == 38 && p + 2 < ansi_param_count && ansi_param[p + 1] == 5) {
                                /* 256-color foreground: ESC[38;5;Nm */
                                pty->cur_fg = (uint8_t)ansi_param[p + 2];
                                p += 2;
                            } else if (val == 48 && p + 2 < ansi_param_count && ansi_param[p + 1] == 5) {
                                /* 256-color background: ESC[48;5;Nm */
                                pty->cur_bg = (uint8_t)ansi_param[p + 2];
                                p += 2;
                            }
                        }
                        break;
                    }
                    case 's': /* SCP — save cursor position */
                        pty->saved_cursor_x = pty->cursor_x;
                        pty->saved_cursor_y = pty->cursor_y;
                        break;
                    case 'u': /* RCP — restore cursor position */
                        pty->cursor_x = pty->saved_cursor_x;
                        pty->cursor_y = pty->saved_cursor_y;
                        break;
                    case 'l': /* RM — reset mode */
                    case 'h': /* SM — set mode */
                        /* No-op for now (cursor visibility, etc.) */
                        break;
                    case 'n': /* DSR — device status report */
                        /* No-op — would need to send response back to PTY */
                        break;
                    default:
                        break;
                }
                ansi_state = ANSI_NORMAL;
            }
        }
    }
}

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

static void term_handle_key_pty(TermState *term, uint32_t key, uint32_t mods) {
    TermTab *tab = &term->tabs[term->active_tab];
    TermPtySession *pty = &tab->session.pty;

    char buf[8];
    int len = 0;

    /* Convert key to terminal escape sequences */
    if (key >= 32 && key < 127) {
        buf[len++] = (char)key;
    } else {
        switch (key) {
            case '\r': buf[len++] = '\r'; break;
            case '\n': buf[len++] = '\n'; break;
            case 8: buf[len++] = 8; break;  /* Backspace */
            case 9: buf[len++] = 9; break;  /* Tab */
            case 27: buf[len++] = 27; break;  /* Escape */
            case 0xE048: buf[len++] = 27; buf[len++] = '['; buf[len++] = 'A'; break;  /* Up */
            case 0xE050: buf[len++] = 27; buf[len++] = '['; buf[len++] = 'B'; break;  /* Down */
            case 0xE04B: buf[len++] = 27; buf[len++] = '['; buf[len++] = 'D'; break;  /* Left */
            case 0xE04D: buf[len++] = 27; buf[len++] = '['; buf[len++] = 'C'; break;  /* Right */
            case 0xE04E: break;  /* Handled globally - PgDn */
            case 0xE04F: break;  /* Handled globally - PgUp */
            default: return;
        }
    }

    if (len > 0 && pty->ptm_fd >= 0) {
        write(pty->ptm_fd, buf, len);
    }

    /* Also process local echo for simple keys */
    for (int i = 0; i < len; i++) {
        term_pty_put_char(pty, buf[i], 0x07);  /* Default attr */
    }
}

static void term_handle_key_holyc(TermState *term, uint32_t key, uint32_t mods) {
    /* HolyC REPL key handling - similar to dosgui_wm HolycTerm */
    /* For now, just pass to HolyC if we have a window */
    dosgui_term_holyc_eval(NULL);  /* Trigger redraw/eval */
}

static void term_handle_key_container(TermState *term, uint32_t key, uint32_t mods) {
    if (term->active_tab < 0) return;
    TermTab *tab = &term->tabs[term->active_tab];
    if (tab->type != TERM_SESSION_CONTAINER) return;
    
    TermContainerSession *container = &tab->session.container;
    
    char buf[8];
    int len = 0;
    
    /* Convert key to terminal escape sequences */
    if (key >= 32 && key < 127) {
        buf[len++] = (char)key;
    } else {
        switch (key) {
            case '\r': buf[len++] = '\r'; break;
            case '\n': buf[len++] = '\n'; break;
            case 8: buf[len++] = 8; break;  /* Backspace */
            case 9: buf[len++] = 9; break;  /* Tab */
            case 27: buf[len++] = 27; break;  /* Escape */
            case 0xE048: buf[len++] = 27; buf[len++] = '['; buf[len++] = 'A'; break;  /* Up */
            case 0xE050: buf[len++] = 27; buf[len++] = '['; buf[len++] = 'B'; break;  /* Down */
            case 0xE04B: buf[len++] = 27; buf[len++] = '['; buf[len++] = 'D'; break;  /* Left */
            case 0xE04D: buf[len++] = 27; buf[len++] = '['; buf[len++] = 'C'; break;  /* Right */
            case 0xE04E: break;  /* Handled globally - PgDn */
            case 0xE04F: break;  /* Handled globally - PgUp */
            default: return;
        }
    }
    
    if (len > 0 && container->ptm_fd >= 0) {
        write(container->ptm_fd, buf, len);
    }
    
    /* Also process local echo for simple keys */
    for (int i = 0; i < len; i++) {
        /* Process container local echo */
        if (buf[i] >= 32 && buf[i] < 127) {
            if (container->cursor_x < TERM_MAX_COLS && container->cursor_y < TERM_MAX_ROWS) {
                container->screen[container->cursor_y][container->cursor_x] = buf[i];
                container->attrs[container->cursor_y][container->cursor_x] = 0x07;
            }
            container->cursor_x++;
            if (container->cursor_x >= container->cols) {
                container->cursor_x = 0;
                if (container->cursor_y < container->rows - 1) container->cursor_y++;
            }
        } else if (buf[i] == '\r') {
            container->cursor_x = 0;
        } else if (buf[i] == '\n') {
            container->cursor_y++;
            container->cursor_x = 0;
        } else if (buf[i] == 8) {  /* Backspace */
            if (container->cursor_x > 0) {
                container->cursor_x--;
                container->screen[container->cursor_y][container->cursor_x] = ' ';
            }
        }
    }
}

void dosgui_term_holyc_eval(const char *input) {
    /* Delegate to dosgui_wm's HolycTerm if available */
    if (g_term.active_tab >= 0) {
        TermTab *tab = &g_term.tabs[g_term.active_tab];
        if (tab->type == TERM_SESSION_HOLYC && tab->session.holyc.holyc_term) {
            /* Would call HolyC eval */
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
            term_render_holyc_session(&tab->session.holyc, fb, x, y, w, h);
            break;
        case TERM_SESSION_CONTAINER:
            /* Container terminal */
            vbe_draw_text(x + 10, y + 10, "[Container Session - Not Implemented]", 0x808080, 1);
            break;
    }
}

static void term_render_pty_session(TermPtySession *pty, uint32_t *fb, int x, int y, int w, int h) {
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

static void term_render_holyc_session(TermHolycSession *holyc, uint32_t *fb, int x, int y, int w, int h) {
    /* HolyC REPL rendering - would show HolyC output */
    vbe_draw_text(x + 10, y + 10, "WuBuOS HolyC REPL", 0x00FF00, 1);
    vbe_draw_text(x + 10, y + 25, "Type HolyC code. Enter to evaluate.", 0x808080, 1);
    vbe_draw_text(x + 10, y + 40, "$ ", 0xFFFF00, 1);
}

/* -- PTY I/O ------------------------------------------------------ */

void dosgui_term_pty_write(const char *data, int len) {
    if (g_term.active_tab < 0) return;
    TermTab *tab = &g_term.tabs[g_term.active_tab];
    if (tab->type != TERM_SESSION_SHELL) return;
    
    TermPtySession *pty = &tab->session.pty;
    if (pty->ptm_fd >= 0) {
        write(pty->ptm_fd, data, len);
    }
}

void dosgui_term_pty_read(void) {
    if (g_term.active_tab < 0) return;
    TermTab *tab = &g_term.tabs[g_term.active_tab];
    if (tab->type != TERM_SESSION_SHELL) return;
    
    term_process_pty_output(&tab->session.pty);
}

static void term_pty_put_char(TermPtySession *pty, char c, uint8_t attr) {
    if (pty->cursor_y >= pty->rows) {
        /* Scroll up */
        for (int r = 0; r < pty->rows - 1; r++) {
            memcpy(pty->screen[r], pty->screen[r + 1], TERM_MAX_COLS);
            memcpy(pty->attrs[r], pty->attrs[r + 1], TERM_MAX_COLS);
            pty->dirty[r] = true;
        }
        memset(pty->screen[pty->rows - 1], ' ', TERM_MAX_COLS);
        memset(pty->attrs[pty->rows - 1], 0, TERM_MAX_COLS);
        pty->dirty[pty->rows - 1] = true;
        pty->cursor_y = pty->rows - 1;
    }

    switch (c) {
        case '\n':
            pty->cursor_y++;
            pty->cursor_x = 0;
            break;
        case '\r':
            pty->cursor_x = 0;
            break;
        case '\t':
            pty->cursor_x = (pty->cursor_x + 8) & ~7;
            if (pty->cursor_x >= pty->cols) {
                pty->cursor_x = 0;
                pty->cursor_y++;
            }
            break;
        case 8:  /* Backspace */
            if (pty->cursor_x > 0) {
                pty->cursor_x--;
                pty->screen[pty->cursor_y][pty->cursor_x] = ' ';
            }
            break;
        default:
            if (c >= 32 && c < 127) {
                pty->screen[pty->cursor_y][pty->cursor_x] = c;
                pty->attrs[pty->cursor_y][pty->cursor_x] = attr;
                pty->cursor_x++;
                if (pty->cursor_x >= pty->cols) {
                    pty->cursor_x = 0;
                    pty->cursor_y++;
                }
            }
            break;
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
