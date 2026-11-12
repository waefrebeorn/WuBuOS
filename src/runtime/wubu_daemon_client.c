/*
 * wubu_daemon_client.c  --  WuBuOS daemon wire client (shared)
 *
 * Extracted from daemon_panel.c so the archd/holyd wire transport
 * is one reusable module. Opaque code, minimal includes, C11. See
 * wubu_daemon_client.h for the public contract.
 */

#define _POSIX_C_SOURCE 200809L

#include "../egojar.h"
#include "../egojar_net.h"
#include <sys/socket.h>
#include "wubu_daemon_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* -- Unix socket helpers ------------------------------------------ */

int wubu_daemon_connect(DaemonConn *conn, const char *path) {
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

void wubu_daemon_disconnect(DaemonConn *conn) {
    if (conn->fd >= 0) {
        close(conn->fd);
        conn->fd = -1;
    }
    conn->state = DAEMON_DISCONNECTED;
    conn->recv_len = 0;
}

int wubu_daemon_send(DaemonConn *conn, const char *json) {
    if (conn->fd < 0) return -1;
    size_t len = strlen(json);
    ssize_t sent = write(conn->fd, json, len);
    return (sent == (ssize_t)len) ? 0 : -1;
}

int wubu_daemon_recv_line(DaemonConn *conn, char *line, int max_len) {
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

/* -- JSON helpers ------------------------------------------------- */

const char *wubu_json_get_string(const char *json, const char *key,
                                 char *val, int val_size) {
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
