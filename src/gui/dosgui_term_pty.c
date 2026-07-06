/*
 * dosgui_term_pty.c  --  WuBuOS Terminal PTY Backend Implementation
 *
 * Extracted from dosgui_term.c (2026-07-05): PTY session management,
 * container PTY, input handling, and I/O operations.
 *
 * ~567 lines of PTY-specific code moved out of the monolithic file.
 */

#include "dosgui_term_pty.h"
#include "dosgui_term_internal.h"  /* for term_ansi_parse / term_screen_bind_* */
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
#include <stdbool.h>

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

static const char *term_default_shell(void) {
    const char *shell = getenv("SHELL");
    return shell ? shell : "/bin/bash";
}

/* -- PTY Session Spawning ----------------------------------------- */

int term_pty_spawn(const char *shell, const char *cwd, TermPtySession *pty) {
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

void term_pty_cleanup(TermPtySession *pty) {
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

/* -- PTY Size Management ------------------------------------------ */

void term_update_pty_size(TermPtySession *pty, int cols, int rows) {
    if (pty->cols == cols && pty->rows == rows) return;
    pty->cols = cols;
    pty->rows = rows;

    if (pty->ptm_fd >= 0) {
        struct winsize ws = { .ws_col = cols, .ws_row = rows };
        ioctl(pty->ptm_fd, TIOCSWINSZ, &ws);
    }

    term_reset_pty_screen(pty);
}

void term_reset_pty_screen(TermPtySession *pty) {
    for (int r = 0; r < TERM_MAX_ROWS; r++) {
        memset(pty->screen[r], ' ', TERM_MAX_COLS);
        memset(pty->attrs[r], 0, TERM_MAX_COLS);
        pty->dirty[r] = true;
    }
    pty->cursor_x = 0;
    pty->cursor_y = 0;
}

/* -- PTY I/O ------------------------------------------------------ */

void term_pty_push_line(TermPtySession *pty, const char *line) {
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

/* Note: term_process_pty_output is in dosgui_term_ansi.c */

void term_pty_put_char(TermPtySession *pty, char c, uint8_t attr) {
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

void term_pty_cursor_move(TermPtySession *pty, int x, int y) {
    if (x >= 0 && x < pty->cols) pty->cursor_x = x;
    if (y >= 0 && y < pty->rows) pty->cursor_y = y;
}

/* -- PTY Input Handling ------------------------------------------- */

void term_handle_key_pty(TermState *term, uint32_t key, uint32_t mods) {
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

/* -- Container PTY Implementation --------------------------------- */

int term_container_spawn(TermContainerSession *container, const char *container_name) {
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

void term_container_cleanup(TermContainerSession *container) {
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

void term_update_container_size(TermContainerSession *container, int cols, int rows) {
    if (container->cols == cols && container->rows == rows) return;
    container->cols = cols;
    container->rows = rows;

    if (container->ptm_fd >= 0) {
        struct winsize ws = { .ws_col = cols, .ws_row = rows };
        ioctl(container->ptm_fd, TIOCSWINSZ, &ws);
    }
}

/* Note: term_process_container_output is in dosgui_term_ansi.c */

void term_handle_key_container(TermState *term, uint32_t key, uint32_t mods) {
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

/* HolyC REPL key handling - stub for now */
void term_handle_key_holyc(TermState *term, uint32_t key, uint32_t mods) {
    (void)term; (void)key; (void)mods;
    /* HolyC REPL key handling - similar to dosgui_wm HolycTerm */
    /* For now, just pass to HolyC if we have a window */
    dosgui_term_holyc_eval(NULL);  /* Trigger redraw/eval */
}