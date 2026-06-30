#include "cmd.h"
#include "../../kernel/vbe.h"
#include "../../gui/wubu_theme.h"
#include "../../gui/dosgui_wm.h"
#include "../../runtime/wubu_host_exec.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <termios.h>
#include <pty.h>
#include <signal.h>

#define CMD_MAX_HISTORY 1000
#define CMD_MAX_LINE 4096
#define CMD_TAB_STOP 8

typedef struct {
    char lines[CMD_MAX_HISTORY][CMD_MAX_LINE];
    int count;
    int head;
    int current;
} HistoryBuffer;

typedef struct {
    int cols, rows;
    int pty_fd;
    pid_t child_pid;
    char input_buffer[CMD_MAX_LINE];
    int input_len;
    int cursor_pos;
    int scrollback_pos;
    HistoryBuffer history;
    char cwd[1024];
    bool running;
    /* ANSI state */
    char ansi_buffer[32];
    int ansi_len;
    bool in_escape;
    bool in_csi;
} CmdState;

static CmdState g_cmd = {0};

static void cmd_pty_write(const char *data, int len) {
    if (g_cmd.pty_fd > 0) {
        write(g_cmd.pty_fd, data, len);
    }
}

static void cmd_add_to_history(const char *line) {
    if (!line || !*line) return;
    strncpy(g_cmd.history.lines[g_cmd.history.head], line, CMD_MAX_LINE - 1);
    g_cmd.history.lines[g_cmd.history.head][CMD_MAX_LINE - 1] = '\0';
    g_cmd.history.head = (g_cmd.history.head + 1) % CMD_MAX_HISTORY;
    if (g_cmd.history.count < CMD_MAX_HISTORY) g_cmd.history.count++;
    g_cmd.history.current = g_cmd.history.head;
}

static const char* cmd_history_prev(void) {
    if (g_cmd.history.count == 0) return NULL;
    int prev = (g_cmd.history.current - 1 + CMD_MAX_HISTORY) % CMD_MAX_HISTORY;
    if (prev == g_cmd.history.head) return NULL;
    g_cmd.history.current = prev;
    return g_cmd.history.lines[g_cmd.history.current];
}

static const char* cmd_history_next(void) {
    if (g_cmd.history.count == 0) return NULL;
    if (g_cmd.history.current == g_cmd.history.head) return NULL;
    int next = (g_cmd.history.current + 1) % CMD_MAX_HISTORY;
    g_cmd.history.current = next;
    return (next == g_cmd.history.head) ? "" : g_cmd.history.lines[g_cmd.history.current];
}

static void cmd_handle_ansi(const char *data, int len) {
    /* Simplified ANSI parser - handles basic sequences */
    for (int i = 0; i < len; i++) {
        char c = data[i];
        if (c == '\033') {
            g_cmd.in_escape = true;
            g_cmd.in_csi = false;
            g_cmd.ansi_len = 0;
            continue;
        }
        if (g_cmd.in_escape) {
            if (c == '[') {
                g_cmd.in_csi = true;
                continue;
            } else if (c >= '@' && c <= '~') {
                /* ESC sequence without CSI */
                g_cmd.in_escape = false;
                continue;
            }
        }
        if (g_cmd.in_csi) {
            if (c >= '0' && c <= '9' || c == ';') {
                if (g_cmd.ansi_len < 31) {
                    g_cmd.ansi_buffer[g_cmd.ansi_len++] = c;
                }
                continue;
            } else if (c >= '@' && c <= '~') {
                g_cmd.ansi_buffer[g_cmd.ansi_len] = '\0';
                /* Handle CSI sequence */
                g_cmd.in_escape = false;
                g_cmd.in_csi = false;
                continue;
            }
        }
        g_cmd.in_escape = false;
        g_cmd.in_csi = false;
    }
}

WubuCmd* wubu_cmd_create(int cols, int rows) {
    g_cmd.cols = cols;
    g_cmd.rows = rows;
    g_cmd.input_len = 0;
    g_cmd.cursor_pos = 0;
    g_cmd.scrollback_pos = 0;
    g_cmd.history.count = 0;
    g_cmd.history.head = 0;
    g_cmd.history.current = 0;
    getcwd(g_cmd.cwd, sizeof(g_cmd.cwd));
    g_cmd.running = true;
    g_cmd.in_escape = false;
    g_cmd.in_csi = false;
    g_cmd.pty_fd = -1;
    g_cmd.child_pid = 0;
    return &g_cmd;
}

void wubu_cmd_destroy(WubuCmd *cmd) {
    if (g_cmd.child_pid > 0) {
        kill(g_cmd.child_pid, SIGTERM);
        waitpid(g_cmd.child_pid, NULL, 0);
        g_cmd.child_pid = 0;
    }
    if (g_cmd.pty_fd > 0) {
        close(g_cmd.pty_fd);
        g_cmd.pty_fd = -1;
    }
}

int wubu_cmd_spawn_shell(WubuCmd *cmd, const char *shell_path) {
    int pty;
    struct winsize ws = { .ws_row = g_cmd.rows, .ws_col = g_cmd.cols };
    
    pid_t pid = forkpty(&pty, NULL, NULL, &ws);
    if (pid == 0) {
        /* Child */
        if (shell_path) {
            execl(shell_path, shell_path, NULL);
        } else {
            execl("/bin/bash", "bash", NULL);
        }
        _exit(1);
    } else if (pid > 0) {
        /* Parent */
        g_cmd.pty_fd = pty;
        g_cmd.child_pid = pid;
        fcntl(pty, F_SETFL, O_NONBLOCK);
        return 0;
    }
    return -1;
}

void wubu_cmd_write_pty(WubuCmd *cmd, const char *data, int len) {
    cmd_pty_write(data, len);
}

int wubu_cmd_read_pty(WubuCmd *cmd, char *buf, int maxlen) {
    if (g_cmd.pty_fd <= 0) return 0;
    int n = read(g_cmd.pty_fd, buf, maxlen - 1);
    if (n > 0) {
        buf[n] = '\0';
        cmd_handle_ansi(buf, n);
    }
    return n;
}

void wubu_cmd_key(WubuCmd *cmd, int key) {
    switch (key) {
        case '\n':
        case '\r':
            if (g_cmd.input_len > 0) {
                g_cmd.input_buffer[g_cmd.input_len] = '\0';
                cmd_add_to_history(g_cmd.input_buffer);
                cmd_pty_write(g_cmd.input_buffer, g_cmd.input_len);
                cmd_pty_write("\n", 1);
                g_cmd.input_len = 0;
                g_cmd.cursor_pos = 0;
            }
            break;
        case '\t':
            /* Tab completion - simplified */
            break;
        case 127: /* Backspace */
        case '\b':
            if (g_cmd.cursor_pos > 0) {
                memmove(&g_cmd.input_buffer[g_cmd.cursor_pos - 1],
                        &g_cmd.input_buffer[g_cmd.cursor_pos],
                        g_cmd.input_len - g_cmd.cursor_pos + 1);
                g_cmd.cursor_pos--;
                g_cmd.input_len--;
            }
            break;
        case 27: /* Escape - handle arrow keys via ANSI */
            /* Arrow keys send ESC [ A/B/C/D */
            break;
        default:
            if (key >= 32 && key < 127 && g_cmd.input_len < CMD_MAX_LINE - 1) {
                memmove(&g_cmd.input_buffer[g_cmd.cursor_pos + 1],
                        &g_cmd.input_buffer[g_cmd.cursor_pos],
                        g_cmd.input_len - g_cmd.cursor_pos + 1);
                g_cmd.input_buffer[g_cmd.cursor_pos] = key;
                g_cmd.cursor_pos++;
                g_cmd.input_len++;
            }
            break;
    }
}

void wubu_cmd_text(WubuCmd *cmd, const char *text) {
    for (int i = 0; text[i] && g_cmd.input_len < CMD_MAX_LINE - 1; i++) {
        wubu_cmd_key(cmd, text[i]);
    }
}

void wubu_cmd_resize(WubuCmd *cmd, int cols, int rows) {
    g_cmd.cols = cols;
    g_cmd.rows = rows;
    if (g_cmd.pty_fd > 0) {
        struct winsize ws = { .ws_row = rows, .ws_col = cols };
        ioctl(g_cmd.pty_fd, TIOCSWINSZ, &ws);
        kill(g_cmd.child_pid, SIGWINCH);
    }
}

static void cmd_draw_prompt(void *win, int cx, int cy, int cw, int ch) {
    const WubuThemeColors *tc = wubu_theme_colors();
    int x = cx + 4;
    int y = cy + ch - 24;
    
    vbe_fill_rect(x - 2, y - 2, cw - 4, 20, 0x00000000);
    
    char prompt[256];
    snprintf(prompt, sizeof(prompt), "wubu@arch:%s$ %s", g_cmd.cwd, g_cmd.input_buffer);
    vbe_draw_text(x, y, prompt, 0x0000FF00, 1);
    
    /* Cursor */
    int cursor_x = x + vbe_text_width(prompt, 1);
    if ((g_cmd.cursor_pos / 2) % 2 == 0) {
        vbe_vline(cursor_x, y, y + 16, 0x0000FF00);
    }
}

void wubu_cmd_draw(WubuCmd *cmd, void *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb_w; (void)fb_h;
    if (!cmd || !win) return;
    
    const WubuThemeColors *tc = wubu_theme_colors();
    DosGuiWindow *w = (DosGuiWindow*)win;
    
    int cx = w->x + DOSGUI_BORDER;
    int cy = w->y + DOSGUI_TITLE_H;
    int cw = w->w - 2 * DOSGUI_BORDER;
    int ch = w->h - DOSGUI_TITLE_H - DOSGUI_BORDER;

    vbe_fill_rect(cx, cy, cw, ch, 0x00000000);
    vbe_3d_sunken_colors(cx - 1, cy - 1, cw + 2, ch + 2,
                          tc->border_light, tc->border_face,
                          tc->border_dark, tc->border_darkest);

    /* Read PTY output */
    char buf[4096];
    int n = wubu_cmd_read_pty(cmd, buf, sizeof(buf));
    if (n > 0) {
        /* Would render ANSI output here - simplified */
    }
    
    /* Draw prompt line */
    cmd_draw_prompt(win, cx, cy, cw, ch);
    
    /* Status bar */
    int sb_y = cy + ch - 20;
    vbe_fill_rect(cx, sb_y, cw, 20, tc->btn_face);
    vbe_3d_sunken_colors(cx, sb_y, cw, 20, tc->border_light, tc->border_face, tc->border_dark, tc->border_darkest);
    char status[128];
    snprintf(status, sizeof(status), "CMD Terminal  |  %dx%d  |  PID: %d  |  %s", 
             g_cmd.cols, g_cmd.rows, g_cmd.child_pid, g_cmd.running ? "Running" : "Stopped");
    vbe_draw_text(cx + 8, sb_y + 6, status, 0x00000000, 1);
}