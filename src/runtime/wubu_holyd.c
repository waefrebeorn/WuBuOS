/*
 * wubu_holyd.c  --  WuBuOS TempleOS HolyC DOS Daemon (Facade)
 *
 * Manages HolyC DOS sessions: REPL, compilation, VBE display,
 * input routing, 9P namespace, auto-save, and desktop integration.
 *
 * Submodules:
 *   wubu_holyd_session.c   - session create/destroy/list/info/focus
 *   wubu_holyd_exec.c      - eval/compile/run/stop + REPL + macro
 *   wubu_holyd_window.c    - window create/destroy/show/hide/resize/move/focus/list
 *   wubu_holyd_input.c     - keyboard, mouse, paste input routing
 *   wubu_holyd_9p.c        - 9P mount/unmount/export
 *   wubu_holyd_save.c      - auto-save / restore session state
 *   wubu_holyd_event.c     - publish desktop events
 *   wubu_holyd_lifecycle.c - daemon init/start/event_loop/stop/shutdown/main
 */

#define _POSIX_C_SOURCE 200809L

#include "wubu_holyd_internal.h"

/* -- GUI WM bridge ----------------------------------------------- */
/* (already included via internal header) */

/* -- Forward declarations ------------------------------------------ */
/* (holyd_get_compiler is declared in internal header, defined below) */

/* -- String Tables ----------------------------------------------- */

const char *wubu_holyd_session_state_str(WubuHolySessionState state) {
    switch (state) {
        case SESSION_STATE_INACTIVE:   return "inactive";
        case SESSION_STATE_STARTING:   return "starting";
        case SESSION_STATE_ACTIVE:     return "active";
        case SESSION_STATE_RUNNING:    return "running";
        case SESSION_STATE_PAUSED:     return "paused";
        case SESSION_STATE_ERROR:      return "error";
        case SESSION_STATE_SAVING:     return "saving";
        case SESSION_STATE_DESTROYING:return "destroying";
        default:                       return "unknown";
    }
}

const char *wubu_holyd_window_type_str(WubuHolyWindowType type) {
    switch (type) {
        case HOLY_WINDOW_TERM:    return "term";
        case HOLY_WINDOW_EDITOR:  return "editor";
        case HOLY_WINDOW_GRAPH:   return "graph";
        case HOLY_WINDOW_FILE:    return "file";
        case HOLY_WINDOW_DEBUG:   return "debug";
        case HOLY_WINDOW_DOCS:    return "docs";
        default:                  return "unknown";
    }
}

const char *wubu_holyd_cmd_str(WubuHolyCmd cmd) {
    switch (cmd) {
        case HOLYD_CMD_SESSION_CREATE:  return "session_create";
        case HOLYD_CMD_SESSION_DESTROY: return "session_destroy";
        case HOLYD_CMD_SESSION_LIST:    return "session_list";
        case HOLYD_CMD_SESSION_INFO:    return "session_info";
        case HOLYD_CMD_SESSION_FOCUS:   return "session_focus";
        case HOLYD_CMD_SESSION_SAVE:    return "session_save";
        case HOLYD_CMD_SESSION_RESTORE: return "session_restore";
        case HOLYD_CMD_EVAL:            return "eval";
        case HOLYD_CMD_COMPILE:         return "compile";
        case HOLYD_CMD_RUN:             return "run";
        case HOLYD_CMD_STOP:            return "stop";
        case HOLYD_CMD_WINDOW_CREATE:   return "window_create";
        case HOLYD_CMD_WINDOW_DESTROY:  return "window_destroy";
        case HOLYD_CMD_WINDOW_SHOW:     return "window_show";
        case HOLYD_CMD_WINDOW_HIDE:     return "window_hide";
        case HOLYD_CMD_WINDOW_RESIZE:   return "window_resize";
        case HOLYD_CMD_WINDOW_MOVE:     return "window_move";
        case HOLYD_CMD_WINDOW_FOCUS:    return "window_focus";
        case HOLYD_CMD_WINDOW_LIST:     return "window_list";
        case HOLYD_CMD_INPUT_KEY:       return "input_key";
        case HOLYD_CMD_INPUT_MOUSE:     return "input_mouse";
        case HOLYD_CMD_INPUT_PASTE:     return "input_paste";
        case HOLYD_CMD_MOUNT:           return "mount";
        case HOLYD_CMD_UNMOUNT:         return "unmount";
        case HOLYD_CMD_EXPORT:          return "export";
        case HOLYD_CMD_PING:            return "ping";
        case HOLYD_CMD_STATS:           return "stats";
        case HOLYD_CMD_LOG:             return "log";
        case HOLYD_CMD_SHUTDOWN:        return "shutdown";
        case HOLYD_CMD_RELOAD:          return "reload";
        case HOLYD_CMD_VERSION:         return "version";
        default:                        return "unknown";
    }
}

const char *wubu_holyd_version(void) {
    return WUBU_HOLYD_VERSION;
}

/* -- Logging ------------------------------------------------------ */

void holyd_log(WubuHoly *d, int level, const char *fmt, ...) {
    if (level > d->config.log_level) return;
    FILE *f = fopen(d->config.log_path, "a");
    if (!f) f = stderr;
    time_t now = time(NULL);
    struct tm *tm = localtime(&now);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);
    const char *lvl[] = {"ERR", "WRN", "INF", "DBG"};
    fprintf(f, "[%s] %s ", ts, lvl[level < 4 ? level : 3]);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(f, fmt, ap);
    va_end(ap);
    fprintf(f, "\n");
    fflush(f);
    if (f != stderr) fclose(f);
}

/* -- Socket Server ------------------------------------------------ */

int holyd_socket_create(WubuHoly *d) {
    struct sockaddr_un addr;
    int fd;

    unlink(d->config.socket_path);
    fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if (fd < 0) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, d->config.socket_path, sizeof(addr.sun_path) - 1);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(fd);
        return -1;
    }
    if (listen(fd, 8) < 0) {
        close(fd);
        return -1;
    }

    chmod(d->config.socket_path, 0666);
    return fd;
}

void holyd_socket_destroy(WubuHoly *d, int fd) {
    (void)d;
    if (fd >= 0) {
        close(fd);
        unlink(d->config.socket_path);
    }
}

/* -- Session Find Helper ------------------------------------------- */

WubuHolySession *holyd_find_session(WubuHoly *d, const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < d->session_count; i++) {
        if (strcmp(d->sessions[i].name, name) == 0)
            return &d->sessions[i];
    }
    return NULL;
}