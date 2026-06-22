/*
 * dosgui_daemon_panel.c  --  WuBuOS Desktop Daemon Integration Panel
 *
 * Cell 400-402: Bridges wubu_archd and wubu_holyd events into the DosGui
 * desktop. Shows daemon status in the system tray, container list in a
 * desktop window, and HolyC session windows from the start menu.
 *
 * Architecture:
 *   wubu_archd (Unix socket) → daemon panel → system tray icon + container list window
 *   wubu_holyd (Unix socket) → daemon panel → HolyC terminal windows
 *
 * The panel connects to both daemons via their JSON protocol, subscribes
 * to events, and renders status into the desktop's existing notification
 * center and window manager.
 */

#define _POSIX_C_SOURCE 200809L

#include "dosgui_daemon_panel.h"
#include "dosgui_wm.h"
#include "dosgui_desktop.h"
#include "wubu_notify.h"
#include "wubu_theme.h"
#include "../kernel/vbe.h"
#include "../runtime/wubu_archd.h"
#include "../runtime/wubu_holyd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <time.h>

/* -- Panel State -------------------------------------------------- */

typedef enum {
    DAEMON_DISCONNECTED = 0,
    DAEMON_CONNECTING,
    DAEMON_CONNECTED,
    DAEMON_ERROR
} DaemonConnState;

typedef struct {
    DaemonConnState state;
    int             fd;         /* Unix socket fd */
    char            socket_path[256];
    char            recv_buf[4096];
    int             recv_len;
    int             event_count;
    time_t          last_heartbeat;
} DaemonConn;

typedef struct {
    /* Daemon connections */
    DaemonConn  archd;
    DaemonConn  holyd;

    /* System tray */
    int         archd_tray_id;      /* dosgui_systray_add id */
    int         holyd_tray_id;
    int         archd_notif_count;
    int         holyd_notif_count;

    /* Container list window */
    DosGuiWindow *container_win;
    int           container_count;
    char          container_names[16][64];
    char          container_states[16][16];

    /* HolyC session window */
    DosGuiWindow *holyd_win;
    int           holyd_session_count;
    char          holyd_sessions[8][64];

    /* Epoll for non-blocking daemon reads */
    int         epoll_fd;

    /* Tick counter */
    int         tick_count;
} DaemonPanel;

static DaemonPanel g_panel = {0};

/* -- Unix Socket Helpers ------------------------------------------ */

static int daemon_connect(DaemonConn *conn, const char *path) {
    if (!conn || !path) return -1;

    conn->fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (conn->fd < 0) {
        conn->state = DAEMON_ERROR;
        return -1;
    }

    struct sockaddr_un addr;
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);

    int rc = connect(conn->fd, (struct sockaddr *)&addr, sizeof(addr));
    if (rc < 0 && errno != EINPROGRESS) {
        close(conn->fd);
        conn->fd = -1;
        conn->state = DAEMON_ERROR;
        return -1;
    }

    strncpy(conn->socket_path, path, sizeof(conn->socket_path) - 1);
    conn->state = DAEMON_CONNECTED;
    conn->recv_len = 0;
    conn->event_count = 0;
    conn->last_heartbeat = time(NULL);
    return 0;
}

static void daemon_disconnect(DaemonConn *conn) {
    if (conn->fd >= 0) {
        close(conn->fd);
        conn->fd = -1;
    }
    conn->state = DAEMON_DISCONNECTED;
    conn->recv_len = 0;
}

static int daemon_send(DaemonConn *conn, const char *json) {
    if (conn->fd < 0) return -1;
    size_t len = strlen(json);
    ssize_t sent = write(conn->fd, json, len);
    return (sent == (ssize_t)len) ? 0 : -1;
}

static int daemon_recv_line(DaemonConn *conn, char *line, int max_len) {
    if (conn->fd < 0) return -1;

    /* Try to read more data */
    if (conn->recv_len < (int)sizeof(conn->recv_buf) - 1) {
        ssize_t n = read(conn->fd,
                         conn->recv_buf + conn->recv_len,
                         sizeof(conn->recv_buf) - conn->recv_len - 1);
        if (n > 0) {
            conn->recv_len += (int)n;
            conn->recv_buf[conn->recv_len] = '\0';
        }
    }

    /* Extract a line */
    char *nl = memchr(conn->recv_buf, '\n', conn->recv_len);
    if (!nl) return 0; /* No complete line yet */

    int line_len = (int)(nl - conn->recv_buf);
    if (line_len >= max_len) line_len = max_len - 1;
    memcpy(line, conn->recv_buf, line_len);
    line[line_len] = '\0';

    /* Shift remaining buffer */
    int remaining = conn->recv_len - line_len - 1;
    if (remaining > 0) {
        memmove(conn->recv_buf, nl + 1, remaining);
    }
    conn->recv_len = remaining;
    conn->recv_buf[remaining] = '\0';

    return line_len;
}

/* -- JSON Helpers ------------------------------------------------- */

static const char *json_get_string(const char *json, const char *key, char *val, int val_size) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) { val[0] = '\0'; return NULL; }
    p += strlen(pattern);
    while (*p == ' ' || *p == ':') p++;
    if (*p != '"') { val[0] = '\0'; return NULL; }
    p++;
    int i = 0;
    while (*p && *p != '"' && i < val_size - 1) {
        val[i++] = *p++;
    }
    val[i] = '\0';
    return val;
}

/* -- Event Processing --------------------------------------------- */

static void panel_process_archd_event(DaemonPanel *panel, const char *line) {
    char event_type[64] = {0};
    char root_name[64] = {0};
    char root_state[32] = {0};

    json_get_string(line, "event", event_type, sizeof(event_type));
    json_get_string(line, "root_name", root_name, sizeof(root_name));
    json_get_string(line, "state", root_state, sizeof(root_state));

    panel->archd.event_count++;

    /* Update container list */
    if (strcmp(event_type, "root-create") == 0 || strcmp(event_type, "root-state") == 0) {
        /* Find or add container */
        int idx = -1;
        for (int i = 0; i < panel->container_count; i++) {
            if (strcmp(panel->container_names[i], root_name) == 0) {
                idx = i;
                break;
            }
        }
        if (idx < 0 && panel->container_count < 16) {
            idx = panel->container_count++;
            strncpy(panel->container_names[idx], root_name, 63);
        }
        if (idx >= 0) {
            strncpy(panel->container_states[idx], root_state, 15);
        }

        /* Notify desktop */
        char notif_summary[128];
        snprintf(notif_summary, sizeof(notif_summary), "Arch: %s", root_name);
        char notif_body[256];
        snprintf(notif_body, sizeof(notif_body), "Container '%s' is now %s", root_name, root_state);
        dosgui_notif_center_add("ArchD", notif_summary, notif_body, 1);
    }

    /* Update tray icon */
    if (panel->archd_tray_id >= 0) {
        dosgui_systray_set_notification_count("archd", panel->archd.event_count);
    }
}

static void panel_process_holyd_event(DaemonPanel *panel, const char *line) {
    char event_type[64] = {0};
    char session_name[64] = {0};
    char session_state[32] = {0};

    json_get_string(line, "event", event_type, sizeof(event_type));
    json_get_string(line, "session", session_name, sizeof(session_name));
    json_get_string(line, "state", session_state, sizeof(session_state));

    panel->holyd.event_count++;

    /* Update session list */
    if (strcmp(event_type, "session-create") == 0 || strcmp(event_type, "session-state") == 0) {
        int idx = -1;
        for (int i = 0; i < panel->holyd_session_count; i++) {
            if (strcmp(panel->holyd_sessions[i], session_name) == 0) {
                idx = i;
                break;
            }
        }
        if (idx < 0 && panel->holyd_session_count < 8) {
            idx = panel->holyd_session_count++;
            strncpy(panel->holyd_sessions[idx], session_name, 63);
        }

        /* Notify desktop */
        char notif_summary[128];
        snprintf(notif_summary, sizeof(notif_summary), "HolyD: %s", session_name);
        char notif_body[256];
        snprintf(notif_body, sizeof(notif_body), "Session '%s' is now %s", session_name, session_state);
        dosgui_notif_center_add("HolyD", notif_summary, notif_body, 1);
    }

    /* Update tray icon */
    if (panel->holyd_tray_id >= 0) {
        dosgui_systray_set_notification_count("holyd", panel->holyd.event_count);
    }
}

/* -- Container List Window ---------------------------------------- */

static void container_win_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb_w;
    const WubuThemeColors *tc = wubu_theme_colors();

    /* Background */
    vbe_fill_rect(win->x, win->y, win->w, win->h, tc->startmenu_bg);

    /* Title bar */
    vbe_fill_rect(win->x, win->y, win->w, DOSGUI_TITLE_H, tc->win_title_active);
    vbe_draw_text(win->x + 8, win->y + 4, "Arch Linux Containers", tc->win_title_text, 1);

    /* Container list */
    int row_h = 20;
    int list_y = win->y + DOSGUI_TITLE_H + 4;

    for (int i = 0; i < g_panel.container_count && i < 16; i++) {
        int row_y = list_y + i * row_h;
        if (row_y + row_h > win->y + win->h) break;

        uint32_t bg = (i % 2 == 0) ? tc->startmenu_bg : tc->win_face;
        vbe_fill_rect(win->x + 2, row_y, win->w - 4, row_h - 1, bg);

        /* Status indicator */
        uint32_t status_color = 0x00AA00; /* green = active */
        if (strcmp(g_panel.container_states[i], "inactive") == 0) status_color = 0x888888;
        else if (strcmp(g_panel.container_states[i], "failed") == 0) status_color = 0xCC0000;
        else if (strcmp(g_panel.container_states[i], "activating") == 0) status_color = 0xCCAA00;
        vbe_fill_rect(win->x + 6, row_y + 6, 8, 8, status_color);

        /* Name */
        vbe_draw_text(win->x + 20, row_y + 3, g_panel.container_names[i], tc->startmenu_text, 1);

        /* State */
        vbe_draw_text(win->x + 200, row_y + 3, g_panel.container_states[i], tc->border_dark, 1);
    }

    if (g_panel.container_count == 0) {
        vbe_draw_text(win->x + 8, list_y + 4, "No containers running", tc->border_dark, 1);
    }

    /* Connection status */
    const char *conn_str = "disconnected";
    uint32_t conn_color = 0xCC0000;
    if (g_panel.archd.state == DAEMON_CONNECTED) {
        conn_str = "connected";
        conn_color = 0x00AA00;
    } else if (g_panel.archd.state == DAEMON_CONNECTING) {
        conn_str = "connecting...";
        conn_color = 0xCCAA00;
    }
    char status_line[64];
    snprintf(status_line, sizeof(status_line), "ArchD: %s", conn_str);
    vbe_draw_text(win->x + 8, win->y + win->h - 16, status_line, conn_color, 1);
}

/* -- HolyC Session Window ----------------------------------------- */

static void holyd_win_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb_w;
    const WubuThemeColors *tc = wubu_theme_colors();

    vbe_fill_rect(win->x, win->y, win->w, win->h, 0x000000); /* HolyC black */

    /* Title bar */
    vbe_fill_rect(win->x, win->y, win->w, DOSGUI_TITLE_H, 0x0000AA);
    vbe_draw_text(win->x + 8, win->y + 4, "HolyC DOS Sessions", 0xFFFFFF, 1);

    /* Session list */
    int row_h = 18;
    int list_y = win->y + DOSGUI_TITLE_H + 4;

    for (int i = 0; i < g_panel.holyd_session_count && i < 8; i++) {
        int row_y = list_y + i * row_h;
        if (row_y + row_h > win->y + win->h) break;

        /* HolyC-style green text on black */
        vbe_draw_text(win->x + 8, row_y, "> ", 0x00FF00, 1);
        vbe_draw_text(win->x + 24, row_y, g_panel.holyd_sessions[i], 0x00FF00, 1);
    }

    if (g_panel.holyd_session_count == 0) {
        vbe_draw_text(win->x + 8, list_y, "No HolyC sessions", 0x008800, 1);
    }

    /* Connection status */
    const char *conn_str = "disconnected";
    uint32_t conn_color = 0x008800;
    if (g_panel.holyd.state == DAEMON_CONNECTED) {
        conn_str = "connected";
        conn_color = 0x00FF00;
    }
    char status_line[64];
    snprintf(status_line, sizeof(status_line), "HolyD: %s", conn_str);
    vbe_draw_text(win->x + 8, win->y + win->h - 16, status_line, conn_color, 1);
}

/* -- System Tray Callbacks ---------------------------------------- */

void archd_tray_click(void) {
    /* Toggle container list window */
    if (g_panel.container_win) {
        dosgui_wm_destroy(g_panel.container_win);
        g_panel.container_win = NULL;
    } else {
        g_panel.container_win = dosgui_wm_create(100, 80, 400, 300, "Arch Containers");
        if (g_panel.container_win) {
            g_panel.container_win->on_draw = container_win_draw;
        }
    }
}

void holyd_tray_click(void) {
    /* Toggle HolyC session window */
    if (g_panel.holyd_win) {
        dosgui_wm_destroy(g_panel.holyd_win);
        g_panel.holyd_win = NULL;
    } else {
        g_panel.holyd_win = dosgui_wm_create(120, 100, 400, 250, "HolyC Sessions");
        if (g_panel.holyd_win) {
            g_panel.holyd_win->on_draw = holyd_win_draw;
        }
    }
}

/* -- Public API --------------------------------------------------- */

int dosgui_daemon_panel_init(void) {
    memset(&g_panel, 0, sizeof(g_panel));
    g_panel.archd.fd = -1;
    g_panel.holyd.fd = -1;

    /* Add system tray icons */
    g_panel.archd_tray_id = dosgui_systray_add("archd", 0x00AA00,
                                                 archd_tray_click, NULL);
    g_panel.holyd_tray_id = dosgui_systray_add("holyd", 0x0000AA,
                                                 holyd_tray_click, NULL);

    /* Try to connect to daemons (non-blocking) */
    daemon_connect(&g_panel.archd, WUBU_ARCHD_SOCKET_PATH);
    daemon_connect(&g_panel.holyd, WUBU_HOLYD_SOCKET_PATH);

    /* Send subscribe commands */
    if (g_panel.archd.state == DAEMON_CONNECTED) {
        daemon_send(&g_panel.archd, "{\"cmd\":\"subscribe\",\"events\":[\"root-create\",\"root-state\",\"root-delete\"]}\n");
    }
    if (g_panel.holyd.state == DAEMON_CONNECTED) {
        daemon_send(&g_panel.holyd, "{\"cmd\":\"subscribe\",\"events\":[\"session-create\",\"session-state\",\"session-destroy\"]}\n");
    }

    return 0;
}

void dosgui_daemon_panel_shutdown(void) {
    daemon_disconnect(&g_panel.archd);
    daemon_disconnect(&g_panel.holyd);

    if (g_panel.archd_tray_id >= 0) {
        dosgui_systray_remove("archd");
        g_panel.archd_tray_id = -1;
    }
    if (g_panel.holyd_tray_id >= 0) {
        dosgui_systray_remove("holyd");
        g_panel.holyd_tray_id = -1;
    }

    if (g_panel.container_win) {
        dosgui_wm_destroy(g_panel.container_win);
        g_panel.container_win = NULL;
    }
    if (g_panel.holyd_win) {
        dosgui_wm_destroy(g_panel.holyd_win);
        g_panel.holyd_win = NULL;
    }
}

void dosgui_daemon_panel_tick(void) {
    g_panel.tick_count++;

    /* Read events from archd */
    if (g_panel.archd.state == DAEMON_CONNECTED && g_panel.archd.fd >= 0) {
        char line[1024];
        int retries = 16;
        while (retries-- > 0) {
            int len = daemon_recv_line(&g_panel.archd, line, sizeof(line));
            if (len <= 0) break;
            if (len > 2) { /* skip empty lines */
                panel_process_archd_event(&g_panel, line);
            }
        }
    }

    /* Read events from holyd */
    if (g_panel.holyd.state == DAEMON_CONNECTED && g_panel.holyd.fd >= 0) {
        char line[1024];
        int retries = 16;
        while (retries-- > 0) {
            int len = daemon_recv_line(&g_panel.holyd, line, sizeof(line));
            if (len <= 0) break;
            if (len > 2) {
                panel_process_holyd_event(&g_panel, line);
            }
        }
    }

    /* Reconnect disconnected daemons every 300 ticks (~5 seconds at 60fps) */
    if (g_panel.tick_count % 300 == 0) {
        if (g_panel.archd.state == DAEMON_DISCONNECTED || g_panel.archd.state == DAEMON_ERROR) {
            daemon_connect(&g_panel.archd, WUBU_ARCHD_SOCKET_PATH);
            if (g_panel.archd.state == DAEMON_CONNECTED) {
                daemon_send(&g_panel.archd, "{\"cmd\":\"subscribe\",\"events\":[\"root-create\",\"root-state\",\"root-delete\"]}\n");
            }
        }
        if (g_panel.holyd.state == DAEMON_DISCONNECTED || g_panel.holyd.state == DAEMON_ERROR) {
            daemon_connect(&g_panel.holyd, WUBU_HOLYD_SOCKET_PATH);
            if (g_panel.holyd.state == DAEMON_CONNECTED) {
                daemon_send(&g_panel.holyd, "{\"cmd\":\"subscribe\",\"events\":[\"session-create\",\"session-state\",\"session-destroy\"]}\n");
            }
        }
    }
}

int dosgui_daemon_panel_archd_state(void) {
    return g_panel.archd.state;
}

int dosgui_daemon_panel_holyd_state(void) {
    return g_panel.holyd.state;
}

int dosgui_daemon_panel_container_count(void) {
    return g_panel.container_count;
}

const char *dosgui_daemon_panel_container_name(int idx) {
    if (idx < 0 || idx >= g_panel.container_count) return NULL;
    return g_panel.container_names[idx];
}

const char *dosgui_daemon_panel_container_state(int idx) {
    if (idx < 0 || idx >= g_panel.container_count) return NULL;
    return g_panel.container_states[idx];
}

int dosgui_daemon_panel_holyd_session_count(void) {
    return g_panel.holyd_session_count;
}

const char *dosgui_daemon_panel_holyd_session_name(int idx) {
    if (idx < 0 || idx >= g_panel.holyd_session_count) return NULL;
    return g_panel.holyd_sessions[idx];
}
