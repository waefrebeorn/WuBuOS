#include "../../egojar.h"
#include "cmd.h"
#include "../../kernel/vbe.h"
#include "../../gui/wubu_theme.h"
#include "../../gui/wm.h"
#include "../../gui/shell/window_chrome.h"
#include "../../runtime/wubu_host_exec.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <pty.h>

#define CMD_MAX_HISTORY 1000
#define CMD_MAX_LINE 4096
#define CMD_TAB_STOP 8
/* Scrollback screen: the terminal is a real pty shell, so it must show what
 * the shell prints, not just the local input line. */
#define CMD_SCREEN_ROWS 128
#define CMD_SCREEN_COLS 200

typedef struct {
    char lines[CMD_MAX_HISTORY][CMD_MAX_LINE];
    int count;
    int head;
    int current;
} HistoryBuffer;

typedef struct WubuCmd {
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
    /* Scrollback screen, fed from the pty. */
    char screen[CMD_SCREEN_ROWS][CMD_SCREEN_COLS];
    int screen_row, screen_col;
    /* ANSI state */
    char ansi_buffer[32];
    int ansi_len;
    bool in_escape;
    bool in_csi;
} WubuCmd;

static WubuCmd g_cmd = {0};

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

static void cmd_screen_put(char c) {
    if (g_cmd.screen_col >= CMD_SCREEN_COLS - 1) return;
    g_cmd.screen[g_cmd.screen_row][g_cmd.screen_col++] = c;
}

static void cmd_screen_newline(void) {
    g_cmd.screen[g_cmd.screen_row][g_cmd.screen_col] = '\0';
    g_cmd.screen_row++;
    g_cmd.screen_col = 0;
    if (g_cmd.screen_row >= CMD_SCREEN_ROWS) {
        /* Scroll: drop the oldest line, keep the cursor on the last row. */
        memmove(&g_cmd.screen[0], &g_cmd.screen[1],
                sizeof(g_cmd.screen[0]) * (CMD_SCREEN_ROWS - 1));
        g_cmd.screen_row = CMD_SCREEN_ROWS - 1;
    }
    g_cmd.screen[g_cmd.screen_row][0] = '\0';
}

/* Feed raw pty bytes into the scrollback screen.  Handles the four control
 * characters a shell actually emits (\n \r \b \t) and skips CSI sequences
 * (cursor movement, erase, colour) -- enough to keep the visible text
 * honest without a full terminal emulator. */
static void cmd_screen_feed(const char *data, int len) {
    for (int i = 0; i < len; i++) {
        char c = data[i];
        if (c == '\033') {
            if (i + 1 < len && data[i + 1] == '[') {
                i += 2;
                while (i < len && !(data[i] >= '@' && data[i] <= '~')) i++;
                continue;
            }
            i++;                       /* ESC + single char */
            continue;
        }
        if (c == '\n') { cmd_screen_newline(); continue; }
        if (c == '\r') { g_cmd.screen_col = 0; continue; }
        if (c == '\b') { if (g_cmd.screen_col > 0) g_cmd.screen_col--; continue; }
        if (c == '\t') {
            do { cmd_screen_put(' '); } while (g_cmd.screen_col % CMD_TAB_STOP);
            continue;
        }
        if (c >= 32 && c < 127) cmd_screen_put(c);
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
    memset(g_cmd.screen, 0, sizeof(g_cmd.screen));
    g_cmd.screen_row = 0;
    g_cmd.screen_col = 0;
    return &g_cmd;
}

void wubu_cmd_destroy(WubuCmd *cmd) {
    if (g_cmd.child_pid > 0) {
        /* forkpty put the child in its own process group (session leader).
         * Kill the whole group so the shell and any children it spawned die,
         * then reap. SIGKILL guarantees no interactive-shell ignore of TERM. */
        kill(-g_cmd.child_pid, SIGKILL);
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
        cmd_screen_feed(buf, n);
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

void wubu_cmd_history_add(WubuCmd *cmd, const char *line) {
    if (!line || !*line) return;
    HistoryBuffer *h = &g_cmd.history;
    if (h->count < CMD_MAX_HISTORY) {
        strncpy(h->lines[h->count], line, CMD_MAX_LINE - 1);
        h->lines[h->count][CMD_MAX_LINE - 1] = '\0';
        h->count++;
        h->head = h->count;
    } else {
        /* Ring buffer: drop the oldest, shift down. */
        for (int i = 1; i < CMD_MAX_HISTORY; i++)
            strncpy(h->lines[i - 1], h->lines[i], CMD_MAX_LINE);
        strncpy(h->lines[CMD_MAX_HISTORY - 1], line, CMD_MAX_LINE - 1);
        h->lines[CMD_MAX_HISTORY - 1][CMD_MAX_LINE - 1] = '\0';
        h->head = CMD_MAX_HISTORY;
    }
    h->current = h->head;
}

const char *wubu_cmd_history_prev(WubuCmd *cmd) {
    HistoryBuffer *h = &g_cmd.history;
    if (h->count == 0) return NULL;
    if (h->current > 0) h->current--;
    return h->lines[h->current];
}

const char *wubu_cmd_history_next(WubuCmd *cmd) {
    HistoryBuffer *h = &g_cmd.history;
    if (h->count == 0) return NULL;
    if (h->current < h->head) h->current++;
    return h->lines[h->current];
}

void wubu_cmd_draw(WubuCmd *cmd, void *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb_w; (void)fb_h;
    if (!cmd || !win) return;

    const WubuThemeColors *tc = wubu_theme_colors();
    DosGuiWindow *w = (DosGuiWindow*)win;

    /* Centralized chrome draws the frame/title bar; we draw content
     * within the content rect. */
    ChromeContentRect content = chrome_draw_window(w, fb, fb_w, fb_h);
    int cx = content.x, cy = content.y, cw = content.w, ch = content.h;

    vbe_fill_rect(cx, cy, cw, ch, 0x00000000);
    vbe_3d_sunken_colors(cx - 1, cy - 1, cw + 2, ch + 2,
                          tc->border_light, tc->border_face,
                          tc->border_dark, tc->border_darkest);

    /* Drain the pty into the scrollback screen. */
    char buf[4096];
    while (wubu_cmd_read_pty(cmd, buf, sizeof(buf)) > 0) { }

    const int line_h = 12;
    const int input_h = 20;
    const int status_h = 20;
    int nlines = (ch - input_h - status_h) / line_h;
    if (nlines < 1) nlines = 1;
    int total = g_cmd.screen_row + 1;
    int start = total - nlines;
    if (start < 0) start = 0;
    for (int i = 0; i < nlines && start + i < total; i++) {
        vbe_draw_text(cx + 4, cy + i * line_h,
                      g_cmd.screen[start + i], 0x0000FF00, 1);
    }

    /* Input line, above the status bar (they used to overlap, which hid the
     * prompt entirely). */
    int input_y = cy + ch - status_h - input_h;
    vbe_fill_rect(cx, input_y, cw, input_h, 0x00000000);
    char prompt[256];
    snprintf(prompt, sizeof(prompt), "wubu@arch:%s$ %s",
             g_cmd.cwd, g_cmd.input_buffer);
    vbe_draw_text(cx + 4, input_y + 4, prompt, 0x0000FF00, 1);
    int cursor_x = cx + 4 + vbe_text_width(prompt, 1);
    if ((g_cmd.cursor_pos / 2) % 2 == 0)
        vbe_vline(cursor_x, input_y + 4, input_y + 18, 0x0000FF00);

    /* Status bar. */
    int sb_y = cy + ch - status_h;
    vbe_fill_rect(cx, sb_y, cw, status_h, tc->btn_face);
    vbe_3d_sunken_colors(cx, sb_y, cw, status_h,
                          tc->border_light, tc->border_face,
                          tc->border_dark, tc->border_darkest);
    char status[128];
    snprintf(status, sizeof(status), "CMD Terminal  |  %dx%d  |  PID: %d  |  %s",
             g_cmd.cols, g_cmd.rows, g_cmd.child_pid,
             g_cmd.running ? "Running" : "Stopped");
    vbe_draw_text(cx + 8, sb_y + 6, status, 0x00000000, 1);
}