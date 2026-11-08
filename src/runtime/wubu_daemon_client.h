/*
 * wubu_daemon_client.h  --  WuBuOS daemon wire client (shared)
 *
 * Reusable Unix-socket + line-JSON transport for in-process daemons
 * (wubu_archd, wubu_holyd, future daemons). Extracted from the
 * once-monolithic dosgui_daemon_panel so any daemon consumer calls
 * the same wire layer instead of copying it.
 *
 * A DaemonConn is a value type (owned by the caller); its fields are
 * visible so a parent may embed it (e.g. the desktop keeps one of
 * these per daemon).
 */

#ifndef WUBU_DAEMON_CLIENT_H
#define WUBU_DAEMON_CLIENT_H

#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    DAEMON_DISCONNECTED = 0,
    DAEMON_CONNECTING,
    DAEMON_CONNECTED,
    DAEMON_ERROR
} DaemonConnState;

typedef struct {
    DaemonConnState state;
    int             fd;            /* Unix socket fd, -1 when closed */
    char            socket_path[256];
    char            recv_buf[4096];
    int             recv_len;
    int             event_count;
    time_t          last_heartbeat;
} DaemonConn;

/* Open a non-blocking Unix socket; returns 0 on connect-issued, -1 on error. */
int  wubu_daemon_connect(DaemonConn *conn, const char *path);

/* Close the fd and reset state. Safe on a never-connected conn. */
void wubu_daemon_disconnect(DaemonConn *conn);

/* Send one whole JSON line. Returns 0 if fully written, -1 on error. */
int  wubu_daemon_send(DaemonConn *conn, const char *json);

/*
 * Read up to one newline-terminated line. Returns line length (>0) when a
 * complete line is available, 0 when the buffer holds no full line yet, -1
 * on transport error. Reads non-blocking.
 */
int  wubu_daemon_recv_line(DaemonConn *conn, char *line, int max_len);

/*
 * Minimal JSON string look-up (flat key : "value"). Writes into val,
 * NUL-terminated. Returns val on hit, NULL if the key is absent. This is
 * deliberately simpler than the token-based wubu_json parser; use one or
 * the other, do not maintain both semantics.
 */
const char *wubu_json_get_string(const char *json, const char *key,
                                 char *val, int val_size);

#ifdef __cplusplus
}
#endif

#endif /* WUBU_DAEMON_CLIENT_H */
