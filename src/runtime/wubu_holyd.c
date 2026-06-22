/*
 * wubu_holyd.c  --  WuBuOS TempleOS HolyC DOS Daemon Implementation
 *
 * Manages HolyC DOS sessions: REPL, compilation, VBE display,
 * input routing, 9P namespace, auto-save, and desktop integration.
 *
 * Design principles (from TempleOS + Ubuntu desktop research):
 *   - Like TempleOS: persistent HolyC compiler, VBE framebuffer, direct input
 *   - Like Ubuntu gnome-shell: session management, window tracking, focus
 *   - Like Ubuntu systemd-logind: multi-seat, session lifecycle
 *   - Like SteamOS gamescope: display composition for HolyC windows
 */

#define _POSIX_C_SOURCE 200809L

#include "wubu_holyd.h"
#include "../compiler/holyc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>

/* -- GUI WM bridge ----------------------------------------------- */
#include "../gui/dosgui_wm.h"

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

static void holyd_log(WubuHoly *d, int level, const char *fmt, ...) {
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
    if (f != stderr) fclose(f);
}

/* -- Socket Server ------------------------------------------------ */

static int holyd_socket_create(WubuHoly *d) {
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

/* -- Session Operations ------------------------------------------- */

static WubuHolySession *holyd_find_session(WubuHoly *d, const char *name) {
    if (!name) return NULL;
    for (int i = 0; i < d->session_count; i++) {
        if (strcmp(d->sessions[i].name, name) == 0)
            return &d->sessions[i];
    }
    return NULL;
}

int wubu_holyd_session_create(WubuHoly *d, const char *name,
                                int width, int height) {
    if (!d || !name) return -1;
    if (d->session_count >= d->config.max_sessions) return -1;
    if (holyd_find_session(d, name)) return -1; /* Already exists */

    WubuHolySession *s = &d->sessions[d->session_count];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, WUBU_HOLYD_MAX_SESSION_NAME - 1);
    s->state = SESSION_STATE_STARTING;
    s->created = time(NULL);
    s->last_active = time(NULL);
    s->save_interval_sec = d->config.save_interval_sec;
    s->focused_window = -1;

    /* Create session directory */
    snprintf(s->save_path, sizeof(s->save_path), "%s/%s",
             d->config.sessions_path, name);
    mkdir(s->save_path, 0755);

    /* Create default terminal window */
    WubuHolyWindow *w = &s->windows[0];
    w->id = d->session_count * WUBU_HOLYD_MAX_WINDOWS + 0;
    strncpy(w->title, "HolyC Terminal", sizeof(w->title) - 1);
    w->type = HOLY_WINDOW_TERM;
    w->x = 10; w->y = 10;
    w->w = width > 0 ? width : d->config.default_width;
    w->h = height > 0 ? height : d->config.default_height;
    w->visible = true;
    w->focused = true;
    strncpy(w->session_name, name, WUBU_HOLYD_MAX_SESSION_NAME - 1);
    s->window_count = 1;
    s->focused_window = w->id;

    /* Allocate framebuffer */
    w->fb_size = w->w * w->h * sizeof(uint32_t);
    w->framebuffer = (uint32_t *)calloc(1, w->fb_size);
    if (!w->framebuffer) {
        s->state = SESSION_STATE_ERROR;
        return -1;
    }

    /* Initialize compiler placeholder */
    s->compiler = NULL; /* Will be initialized on first eval */
    s->compiler_initialized = false;

    s->state = SESSION_STATE_ACTIVE;
    d->session_count++;

    holyd_log(d, 2, "Session '%s' created (%dx%d)", name, w->w, w->h);
    wubu_holyd_publish_event(d, "session_created", name, NULL);
    return 0;
}

int wubu_holyd_session_destroy(WubuHoly *d, const char *name) {
    WubuHolySession *s = holyd_find_session(d, name);
    if (!s) return -1;

    s->state = SESSION_STATE_DESTROYING;
    holyd_log(d, 2, "Destroying session '%s'", name);

    /* Free framebuffers */
    for (int i = 0; i < s->window_count; i++) {
        if (s->windows[i].framebuffer) {
            free(s->windows[i].framebuffer);
            s->windows[i].framebuffer = NULL;
        }
    }

    /* Remove from array */
    int idx = (int)(s - d->sessions);
    memmove(&d->sessions[idx], &d->sessions[idx + 1],
            (d->session_count - idx - 1) * sizeof(WubuHolySession));
    d->session_count--;

    wubu_holyd_publish_event(d, "session_destroyed", name, NULL);
    return 0;
}

int wubu_holyd_session_list(WubuHoly *d, WubuHolySession *out, int max) {
    if (!d || !out) return -1;
    int count = d->session_count < max ? d->session_count : max;
    memcpy(out, d->sessions, count * sizeof(WubuHolySession));
    return count;
}

int wubu_holyd_session_info(WubuHoly *d, const char *name, WubuHolySession *out) {
    WubuHolySession *s = holyd_find_session(d, name);
    if (!s || !out) return -1;
    *out = *s;
    return 0;
}

int wubu_holyd_session_focus(WubuHoly *d, const char *name) {
    WubuHolySession *s = holyd_find_session(d, name);
    if (!s) return -1;
    s->last_active = time(NULL);
    holyd_log(d, 2, "Session '%s' focused", name);
    wubu_holyd_publish_event(d, "session_focused", name, NULL);
    return 0;
}

/* -- Code Execution ----------------------------------------------- */

int wubu_holyd_eval(WubuHoly *d, const char *session,
                      const char *code, char *output, size_t out_size) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) { snprintf(output, out_size, "Session '%s' not found", session); return -1; }
    if (s->state != SESSION_STATE_ACTIVE) {
        snprintf(output, out_size, "Session '%s' not active (state=%s)",
                 session, wubu_holyd_session_state_str(s->state));
        return -1;
    }

    s->state = SESSION_STATE_RUNNING;
    s->last_active = time(NULL);
    d->evals_performed++;

    holyd_log(d, 2, "Eval in session '%s': %.60s...", session, code);

    /* Initialize compiler on first eval */
    if (!s->compiler_initialized) {
        s->compiler_initialized = true;
        holyd_log(d, 2, "Compiler initialized for session '%s'", session);
    }

    /* Compile and execute HolyC code via hc_eval() */
    int64_t result = hc_eval(code);

    /* Format the result */
    snprintf(output, out_size, "%ld", (long)result);

    s->state = SESSION_STATE_ACTIVE;
    s->exit_code = (int)(result & 0xFF);
    wubu_holyd_publish_event(d, "eval_complete", session, NULL);
    return 0;
}

int wubu_holyd_compile(WubuHoly *d, const char *session,
                         const char *code, void **out_binary, size_t *out_size) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    *out_binary = hc_compile(code, out_size);
    return (*out_binary) ? 0 : -1;
}

int wubu_holyd_run(WubuHoly *d, const char *session,
                     const void *binary, size_t size) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    if (!binary || size == 0) return -1;
    /* Execute compiled binary via JIT */
    int64_t result = ((int64_t(*)(void))(binary))();
    (void)result;
    return 0;
}

int wubu_holyd_stop(WubuHoly *d, const char *session) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    if (s->state == SESSION_STATE_RUNNING) {
        s->state = SESSION_STATE_ACTIVE;
        holyd_log(d, 2, "Session '%s' stopped", session);
    }
    return 0;
}

/* -- Window Operations -------------------------------------------- */

int wubu_holyd_window_create(WubuHoly *d, const char *session,
                               WubuHolyWindowType type,
                               int x, int y, int w, int h,
                               const char *title, int *out_window_id) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    if (s->window_count >= WUBU_HOLYD_MAX_WINDOWS) return -1;

    WubuHolyWindow *win = &s->windows[s->window_count];
    memset(win, 0, sizeof(*win));
    win->id = (int)(s - d->sessions) * WUBU_HOLYD_MAX_WINDOWS + s->window_count;
    win->type = type;
    win->x = x; win->y = y;
    win->w = w; win->h = h;
    win->visible = true;
    if (title) strncpy(win->title, title, sizeof(win->title) - 1);
    strncpy(win->session_name, session, WUBU_HOLYD_MAX_SESSION_NAME - 1);

    win->fb_size = w * h * sizeof(uint32_t);
    win->framebuffer = (uint32_t *)calloc(1, win->fb_size);
    if (!win->framebuffer) return -1;

    s->window_count++;
    if (out_window_id) *out_window_id = win->id;

    holyd_log(d, 2, "Window %d created in session '%s' (%s %dx%d)",
              win->id, session, wubu_holyd_window_type_str(type), w, h);
    wubu_holyd_publish_event(d, "window_created", session, title);
    return 0;
}

int wubu_holyd_window_destroy(WubuHoly *d, const char *session, int window_id) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    for (int i = 0; i < s->window_count; i++) {
        if (s->windows[i].id == window_id) {
            if (s->windows[i].framebuffer) free(s->windows[i].framebuffer);
            memmove(&s->windows[i], &s->windows[i + 1],
                    (s->window_count - i - 1) * sizeof(WubuHolyWindow));
            s->window_count--;
            wubu_holyd_publish_event(d, "window_destroyed", session, NULL);
            return 0;
        }
    }
    return -1;
}

int wubu_holyd_window_show(WubuHoly *d, const char *session, int window_id) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    for (int i = 0; i < s->window_count; i++) {
        if (s->windows[i].id == window_id) {
            s->windows[i].visible = true;
            wubu_holyd_publish_event(d, "window_shown", session, NULL);
            return 0;
        }
    }
    return -1;
}

int wubu_holyd_window_hide(WubuHoly *d, const char *session, int window_id) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    for (int i = 0; i < s->window_count; i++) {
        if (s->windows[i].id == window_id) {
            s->windows[i].visible = false;
            wubu_holyd_publish_event(d, "window_hidden", session, NULL);
            return 0;
        }
    }
    return -1;
}

int wubu_holyd_window_resize(WubuHoly *d, const char *session,
                               int window_id, int w, int h) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    for (int i = 0; i < s->window_count; i++) {
        if (s->windows[i].id == window_id) {
            s->windows[i].w = w;
            s->windows[i].h = h;
            if (s->windows[i].framebuffer) {
                free(s->windows[i].framebuffer);
                s->windows[i].fb_size = w * h * sizeof(uint32_t);
                s->windows[i].framebuffer = (uint32_t *)calloc(1, s->windows[i].fb_size);
            }
            wubu_holyd_publish_event(d, "window_resized", session, NULL);
            return 0;
        }
    }
    return -1;
}

int wubu_holyd_window_move(WubuHoly *d, const char *session,
                             int window_id, int x, int y) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    for (int i = 0; i < s->window_count; i++) {
        if (s->windows[i].id == window_id) {
            s->windows[i].x = x;
            s->windows[i].y = y;
            return 0;
        }
    }
    return -1;
}

int wubu_holyd_window_focus(WubuHoly *d, const char *session, int window_id) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    for (int i = 0; i < s->window_count; i++) {
        s->windows[i].focused = (s->windows[i].id == window_id);
    }
    s->focused_window = window_id;
    s->last_active = time(NULL);
    wubu_holyd_publish_event(d, "window_focused", session, NULL);
    return 0;
}

int wubu_holyd_window_list(WubuHoly *d, const char *session,
                             WubuHolyWindow *out, int max) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s || !out) return -1;
    int count = s->window_count < max ? s->window_count : max;
    memcpy(out, s->windows, count * sizeof(WubuHolyWindow));
    return count;
}

/* -- Input Routing ------------------------------------------------ */

int wubu_holyd_input_key(WubuHoly *d, const char *session,
                           int keycode, int modifiers) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    /* Add to input queue */
    int tail = s->input_tail;
    /* Simple: store keycode as char for now */
    if (keycode >= 0 && keycode < 256 && tail < (int)sizeof(s->input_buf) - 1) {
        s->input_buf[tail] = (char)keycode;
        s->input_tail = tail + 1;
    }
    s->last_active = time(NULL);
    return 0;
}

int wubu_holyd_input_mouse(WubuHoly *d, const char *session,
                             int x, int y, int buttons) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;

    /* Route to focused window's mouse handler */
    if (s->focused_window >= 0 && s->focused_window < s->window_count) {
        WubuHolyWindow *win = &s->windows[s->focused_window];
        /* Compute local coordinates relative to window */
        int local_x = x - win->x;
        int local_y = y - win->y;
        /* Only forward if mouse is within window bounds */
        if (local_x >= 0 && local_x < win->w &&
            local_y >= 0 && local_y < win->h && win->visible) {
            /* Hit test: window accepts the mouse event.
             * Forward to the DosGui WM which handles focus,
             * drag, resize, and client-area dispatch. */
            {
                /* Translate buttons bitmask to btn+kind.
                 * buttons: bit0=LMB, bit1=RMB, bit2=MMB.
                 * kind: 0=move, 1=down, 2=up.
                 * We detect transitions via prev_buttons. */
                int prev = s->prev_buttons;
                int kind = 0; /* default: move (no change) */
                int btn = 0;
                if (buttons && !prev) {
                    kind = 1; /* press */
                    btn = (buttons & 1) ? 1 : (buttons & 4) ? 3 : 2;
                } else if (!buttons && prev) {
                    kind = 2; /* release */
                    btn = (prev & 1) ? 1 : (prev & 4) ? 3 : 2;
                }
                dosgui_wm_handle_mouse(x, y, btn, kind);
                s->prev_buttons = buttons;
            }
        }
    } else {
        /* No focused window: try to find the window under the cursor */
        for (int i = 0; i < s->window_count; ++i) {
            WubuHolyWindow *win = &s->windows[i];
            if (!win->visible) continue;
            if (x >= win->x && x < win->x + win->w &&
                y >= win->y && y < win->y + win->h) {
                s->focused_window = i;
                /* Clear focus from other windows */
                for (int j = 0; j < s->window_count; ++j) {
                    s->windows[j].focused = (j == i);
                }
                break;
            }
        }
    }

    s->last_active = time(NULL);
    return 0;
}

int wubu_holyd_input_paste(WubuHoly *d, const char *session,
                             const char *text) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s || !text) return -1;
    int len = strlen(text);
    int avail = (int)sizeof(s->input_buf) - s->input_tail;
    if (len > avail) len = avail;
    memcpy(s->input_buf + s->input_tail, text, len);
    s->input_tail += len;
    s->last_active = time(NULL);
    return 0;
}

/* -- 9P Namespace ------------------------------------------------- */

int wubu_holyd_mount(WubuHoly *d, const char *session, const char *path) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    strncpy(s->mount_point, path, WUBU_HOLYD_MAX_PATH - 1);
    s->mounted = true;
    holyd_log(d, 2, "Session '%s' mounted at %s", session, path);
    wubu_holyd_publish_event(d, "session_mounted", session, path);
    return 0;
}

int wubu_holyd_unmount(WubuHoly *d, const char *session) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    s->mount_point[0] = '\0';
    s->mounted = false;
    holyd_log(d, 2, "Session '%s' unmounted", session);
    wubu_holyd_publish_event(d, "session_unmounted", session, NULL);
    return 0;
}

int wubu_holyd_export(WubuHoly *d, const char *session,
                        const char *path, const char *target) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    if (!path || !target) return -1;

    /* Create parent directory for target if needed */
    char parent[512];
    strncpy(parent, target, sizeof(parent) - 1);
    char *last_slash = strrchr(parent, '/');
    if (last_slash && last_slash != parent) {
        *last_slash = '\0';
        mkdir(parent, 0755);
    }

    /* Remove existing symlink/file at target if present */
    unlink(target);

    /* Create symlink: target -> path */
    if (symlink(path, target) < 0) {
        holyd_log(d, 0, "Session '%s' export failed: symlink(%s -> %s): %s",
                  session, target, path, strerror(errno));
        return -1;
    }

    holyd_log(d, 2, "Session '%s' export %s -> %s", session, path, target);
    wubu_holyd_publish_event(d, "session_exported", session, target);
    return 0;
}

/* -- Auto-Save ---------------------------------------------------- */

int wubu_holyd_session_save(WubuHoly *d, const char *session) {
    WubuHolySession *s = holyd_find_session(d, session);
    if (!s) return -1;
    s->state = SESSION_STATE_SAVING;

    char save_file[WUBU_HOLYD_MAX_PATH];
    snprintf(save_file, sizeof(save_file), "%s/session.sav", s->save_path);
    FILE *f = fopen(save_file, "w");
    if (!f) { s->state = SESSION_STATE_ERROR; return -1; }

    /* Save session metadata */
    fprintf(f, "name=%s\n", s->name);
    fprintf(f, "state=%d\n", s->state);
    fprintf(f, "windows=%d\n", s->window_count);
    fprintf(f, "created=%ld\n", (long)s->created);
    fprintf(f, "last_active=%ld\n", (long)s->last_active);
    fclose(f);

    s->last_save = time(NULL);
    s->state = SESSION_STATE_ACTIVE;
    holyd_log(d, 2, "Session '%s' saved to %s", session, save_file);
    return 0;
}

int wubu_holyd_session_restore(WubuHoly *d, const char *session,
                                 const char *save_path) {
    if (!d || !session || !save_path) return -1;

    /* Check if session already exists */
    if (holyd_find_session(d, session)) return -1;
    if (d->session_count >= d->config.max_sessions) return -1;

    /* Open save file */
    FILE *f = fopen(save_path, "r");
    if (!f) { holyd_log(d, 0, "Restore: cannot open %s", save_path); return -1; }

    WubuHolySession *s = &d->sessions[d->session_count];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, session, WUBU_HOLYD_MAX_SESSION_NAME - 1);
    s->state = SESSION_STATE_STARTING;
    s->focused_window = -1;
    s->save_interval_sec = d->config.save_interval_sec;

    /* Parse key=value lines */
    char line[1024];
    int width = d->config.default_width, height = d->config.default_height;
    while (fgets(line, sizeof(line), f)) {
        char key[64], val[512];
        if (sscanf(line, "%63[^=]=%511[^\n]", key, val) == 2) {
            if (strcmp(key, "name") == 0) {
                strncpy(s->name, val, WUBU_HOLYD_MAX_SESSION_NAME - 1);
            } else if (strcmp(key, "state") == 0) {
                s->state = (WubuHolySessionState)atoi(val);
            } else if (strcmp(key, "windows") == 0) {
                /* Window count hint — actual windows recreated on demand */
                int wc = atoi(val);
                if (wc > WUBU_HOLYD_MAX_WINDOWS) wc = WUBU_HOLYD_MAX_WINDOWS;
                s->window_count = 0; /* Will be recreated */
                (void)wc;
            } else if (strcmp(key, "created") == 0) {
                s->created = atol(val);
            } else if (strcmp(key, "last_active") == 0) {
                s->last_active = atol(val);
            } else if (strcmp(key, "width") == 0) {
                width = atoi(val);
            } else if (strcmp(key, "height") == 0) {
                height = atoi(val);
            }
        }
    }
    fclose(f);

    /* Create session directory */
    snprintf(s->save_path, sizeof(s->save_path), "%s/%s",
             d->config.sessions_path, s->name);
    mkdir(s->save_path, 0755);

    /* Recreate default terminal window */
    WubuHolyWindow *w = &s->windows[0];
    w->id = d->session_count * WUBU_HOLYD_MAX_WINDOWS + 0;
    strncpy(w->title, "HolyC Terminal", sizeof(w->title) - 1);
    w->type = HOLY_WINDOW_TERM;
    w->x = 10; w->y = 10;
    w->w = width; w->h = height;
    w->visible = true;
    w->focused = true;
    strncpy(w->session_name, s->name, WUBU_HOLYD_MAX_SESSION_NAME - 1);
    w->fb_size = w->w * w->h * sizeof(uint32_t);
    w->framebuffer = (uint32_t *)calloc(1, w->fb_size);
    if (!w->framebuffer) {
        s->state = SESSION_STATE_ERROR;
        return -1;
    }
    s->window_count = 1;
    s->focused_window = w->id;
    s->last_save = time(NULL);
    s->state = SESSION_STATE_ACTIVE;
    d->session_count++;

    holyd_log(d, 2, "Session '%s' restored from %s", session, save_path);
    wubu_holyd_publish_event(d, "session_restored", session, NULL);
    return 0;
}

/* -- Event Bus ---------------------------------------------------- */

int wubu_holyd_publish_event(WubuHoly *d, const char *event_type,
                               const char *session, const char *data) {
    if (!d || !event_type) return -1;
    holyd_log(d, 2, "EVENT: %s session=%s", event_type, session ? session : "*");
    char event_path[WUBU_HOLYD_MAX_PATH];
    snprintf(event_path, sizeof(event_path), "%s/events", d->config.sessions_path);
    FILE *f = fopen(event_path, "a");
    if (!f) return -1;
    time_t now = time(NULL);
    fprintf(f, "{\"time\":%ld,\"event\":\"%s\",\"session\":\"%s\",\"data\":\"%s\"}\n",
            (long)now, event_type, session ? session : "", data ? data : "");
    fclose(f);
    return 0;
}

/* -- Daemon Lifecycle --------------------------------------------- */

int wubu_holyd_init(WubuHoly *d, const WubuHolyConfig *config) {
    if (!d || !config) return -1;
    memset(d, 0, sizeof(*d));
    d->config = *config;

    if (!d->config.sessions_path[0])
        strncpy(d->config.sessions_path, WUBU_HOLYD_SESSIONS_PATH, WUBU_HOLYD_MAX_PATH - 1);
    if (!d->config.socket_path[0])
        strncpy(d->config.socket_path, WUBU_HOLYD_SOCKET_PATH, WUBU_HOLYD_MAX_PATH - 1);
    if (!d->config.log_path[0])
        strncpy(d->config.log_path, WUBU_HOLYD_LOG_PATH, WUBU_HOLYD_MAX_PATH - 1);
    if (d->config.max_sessions <= 0)
        d->config.max_sessions = WUBU_HOLYD_MAX_SESSIONS;
    if (d->config.default_width <= 0) d->config.default_width = 800;
    if (d->config.default_height <= 0) d->config.default_height = 600;
    if (d->config.save_interval_sec <= 0) d->config.save_interval_sec = 300;

    mkdir(d->config.sessions_path, 0755);
    mkdir("/run/wubu", 0755);

    d->start_time = time(NULL);
    holyd_log(d, 2, "Holyd initialized: sessions_path=%s socket=%s",
              d->config.sessions_path, d->config.socket_path);
    return 0;
}

int wubu_holyd_start(WubuHoly *d) {
    if (!d) return -1;
    d->server_fd = holyd_socket_create(d);
    if (d->server_fd < 0) {
        holyd_log(d, 0, "Failed to create socket: %s", strerror(errno));
        return -1;
    }
    d->epoll_fd = epoll_create1(0);
    if (d->epoll_fd < 0) { close(d->server_fd); return -1; }

    struct epoll_event ev = { .events = EPOLLIN, .data.fd = d->server_fd };
    epoll_ctl(d->epoll_fd, EPOLL_CTL_ADD, d->server_fd, &ev);

    d->running = true;
    holyd_log(d, 2, "Holyd started on %s", d->config.socket_path);
    return 0;
}

void wubu_holyd_event_loop(WubuHoly *d) {
    if (!d || !d->running) return;

    struct epoll_event events[8];
    time_t last_autosave = 0;

    while (d->running) {
        int nfds = epoll_wait(d->epoll_fd, events, 8, 1000);
        time_t now = time(NULL);

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == d->server_fd) {
                int client = accept(d->server_fd, NULL, NULL);
                if (client >= 0) {
                    /* Set non-blocking mode via fcntl to avoid accept4/SOCK_NONBLOCK warning */
                    int flags = fcntl(client, F_GETFL, 0);
                    if (flags >= 0)
                        fcntl(client, F_SETFL, flags | O_NONBLOCK);
                    struct epoll_event cev = { .events = EPOLLIN | EPOLLERR | EPOLLHUP, .data.fd = client };
                    epoll_ctl(d->epoll_fd, EPOLL_CTL_ADD, client, &cev);
                }
            } else {
                int fd = events[i].data.fd;

                /* Check for error/hangup events first */
                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    epoll_ctl(d->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    continue;
                }

                char buf[8192];
                ssize_t n = read(fd, buf, sizeof(buf) - 1);
                if (n < 0) {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        epoll_ctl(d->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                        close(fd);
                    }
                    continue;
                }
                if (n == 0) {
                    /* Client disconnected */
                    epoll_ctl(d->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                    continue;
                }

                buf[n] = '\0';
                /* Protocol: "CMD:session:data" */
                char cmd_str[64], sess[64], data[4096];
                cmd_str[0] = sess[0] = data[0] = '\0';
                sscanf(buf, "%63[^:]:%63[^:]:%4095[^\n]", cmd_str, sess, data);

                WubuHolyResponse resp = {0};
                d->requests_handled++;

                if (strcmp(cmd_str, "ping") == 0) {
                    resp.status = 0;
                    snprintf(resp.message, sizeof(resp.message), "pong");
                } else if (strcmp(cmd_str, "version") == 0) {
                    resp.status = 0;
                    snprintf(resp.data, sizeof(resp.data), "%s", WUBU_HOLYD_VERSION);
                } else if (strcmp(cmd_str, "stats") == 0) {
                    resp.status = 0;
                    snprintf(resp.data, sizeof(resp.data),
                             "{\"sessions\":%d,\"evals\":%lu,\"uptime\":%ld}",
                             d->session_count, d->evals_performed,
                             (long)(now - d->start_time));
                } else if (strcmp(cmd_str, "session_create") == 0) {
                    int w = 0, h = 0;
                    sscanf(data, "%d,%d", &w, &h);
                    resp.status = wubu_holyd_session_create(d, sess, w, h);
                } else if (strcmp(cmd_str, "session_destroy") == 0) {
                    resp.status = wubu_holyd_session_destroy(d, sess);
                } else if (strcmp(cmd_str, "session_list") == 0) {
                    resp.status = 0;
                    resp.data[0] = '\0';
                    for (int j = 0; j < d->session_count; j++) {
                        char entry[256];
                        int entry_len = snprintf(entry, sizeof(entry), "%s:%s:%d\n",
                                 d->sessions[j].name,
                                 wubu_holyd_session_state_str(d->sessions[j].state),
                                 d->sessions[j].window_count);
                        if (entry_len > 0 && (int)strlen(resp.data) + entry_len < (int)sizeof(resp.data) - 1)
                            strncat(resp.data, entry, sizeof(resp.data) - strlen(resp.data) - 1);
                    }
                } else if (strcmp(cmd_str, "session_focus") == 0) {
                    resp.status = wubu_holyd_session_focus(d, sess);
                } else if (strcmp(cmd_str, "eval") == 0) {
                    resp.status = wubu_holyd_eval(d, sess, data,
                                                   resp.output, sizeof(resp.output));
                } else if (strcmp(cmd_str, "window_create") == 0) {
                    int type = 0, x = 10, y = 10, w = 400, h = 300;
                    char title[128] = "HolyC Window";
                    sscanf(data, "%d,%d,%d,%d,%d,%127[^\n]", &type, &x, &y, &w, &h, title);
                    int wid = 0;
                    resp.status = wubu_holyd_window_create(d, sess, (WubuHolyWindowType)type,
                                                            x, y, w, h, title, &wid);
                    if (resp.status == 0) {
                        snprintf(resp.data, sizeof(resp.data), "%d", wid);
                    }
                } else if (strcmp(cmd_str, "window_destroy") == 0) {
                    resp.status = wubu_holyd_window_destroy(d, sess, atoi(data));
                } else if (strcmp(cmd_str, "window_show") == 0) {
                    resp.status = wubu_holyd_window_show(d, sess, atoi(data));
                } else if (strcmp(cmd_str, "window_hide") == 0) {
                    resp.status = wubu_holyd_window_hide(d, sess, atoi(data));
                } else if (strcmp(cmd_str, "window_focus") == 0) {
                    resp.status = wubu_holyd_window_focus(d, sess, atoi(data));
                } else if (strcmp(cmd_str, "input_key") == 0) {
                    int keycode = 0, modifiers = 0;
                    sscanf(data, "%d,%d", &keycode, &modifiers);
                    resp.status = wubu_holyd_input_key(d, sess, keycode, modifiers);
                } else if (strcmp(cmd_str, "session_save") == 0) {
                    resp.status = wubu_holyd_session_save(d, sess);
                } else if (strcmp(cmd_str, "mount") == 0) {
                    resp.status = wubu_holyd_mount(d, sess, data);
                } else if (strcmp(cmd_str, "unmount") == 0) {
                    resp.status = wubu_holyd_unmount(d, sess);
                } else if (strcmp(cmd_str, "shutdown") == 0) {
                    resp.status = 0;
                    d->running = false;
                } else {
                    resp.status = -1;
                    snprintf(resp.message, sizeof(resp.message),
                             "Unknown command: %s", cmd_str);
                    d->errors++;
                }

                /* Bounds-safe response: truncate to fit buffer */
                char resp_buf[WUBU_HOLYD_MAX_RESPONSE + 64];
                int resp_len = snprintf(resp_buf, sizeof(resp_buf), "%d:%s:%s:%s\n",
                         resp.status,
                         resp.message,
                         resp.output,
                         resp.data);
                if (resp_len > 0) {
                    ssize_t to_write = resp_len < (int)sizeof(resp_buf) ? resp_len : (int)sizeof(resp_buf) - 1;
                    write(fd, resp_buf, (size_t)to_write);
                }
                epoll_ctl(d->epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                close(fd);
            }
        }

        /* Auto-save: use configured interval, default 60s if not set */
        int autosave_interval = d->config.save_interval_sec > 0 ? d->config.save_interval_sec : 60;
        if (now - last_autosave >= autosave_interval) {
            last_autosave = now;
            for (int j = 0; j < d->session_count; j++) {
                WubuHolySession *s = &d->sessions[j];
                if (s->state == SESSION_STATE_ACTIVE &&
                    s->save_interval_sec > 0 &&
                    (now - s->last_save) >= s->save_interval_sec) {
                    wubu_holyd_session_save(d, s->name);
                }
            }
        }
    }
}

void wubu_holyd_daemon_stop(WubuHoly *d) {
    if (!d) return;
    d->running = false;
    holyd_log(d, 2, "Holyd stopping");
}

void wubu_holyd_shutdown(WubuHoly *d) {
    if (!d) return;
    d->running = false;
    /* Save all active sessions, then free resources */
    for (int i = 0; i < d->session_count; i++) {
        if (d->sessions[i].state == SESSION_STATE_ACTIVE) {
            wubu_holyd_session_save(d, d->sessions[i].name);
        }
        /* Free all window framebuffers */
        for (int j = 0; j < d->sessions[i].window_count; j++) {
            if (d->sessions[i].windows[j].framebuffer) {
                free(d->sessions[i].windows[j].framebuffer);
                d->sessions[i].windows[j].framebuffer = NULL;
            }
        }
    }
    if (d->server_fd >= 0) close(d->server_fd);
    if (d->epoll_fd >= 0) close(d->epoll_fd);
    unlink(d->config.socket_path);
    holyd_log(d, 2, "Holyd shutdown complete");
}

/* -- Main Entry Point --------------------------------------------- */

#ifndef WUBD_TEST_MAIN
int main(int argc, char *argv[]) {
    WubuHolyConfig config = {0};
    strncpy(config.sessions_path, WUBU_HOLYD_SESSIONS_PATH, WUBU_HOLYD_MAX_PATH - 1);
    strncpy(config.socket_path, WUBU_HOLYD_SOCKET_PATH, WUBU_HOLYD_MAX_PATH - 1);
    strncpy(config.log_path, WUBU_HOLYD_LOG_PATH, WUBU_HOLYD_MAX_PATH - 1);
    config.max_sessions = WUBU_HOLYD_MAX_SESSIONS;
    config.default_width = 800;
    config.default_height = 600;
    config.save_interval_sec = 300;
    config.log_level = 2;
    config.daemonize = true;
    config.auto_mount_9p = true;
    config.debug_mode = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--no-daemon") == 0) config.daemonize = false;
        else if (strcmp(argv[i], "--debug") == 0) { config.log_level = 3; config.debug_mode = true; }
        else if (strcmp(argv[i], "--sessions") == 0 && i + 1 < argc) {
            strncpy(config.sessions_path, argv[++i], WUBU_HOLYD_MAX_PATH - 1);
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("wubu_holyd -- WuBuOS TempleOS HolyC DOS Daemon\n");
            printf("  --no-daemon     Run in foreground\n");
            printf("  --debug         Verbose logging + debug mode\n");
            printf("  --sessions PATH Sessions directory (default: %s)\n", WUBU_HOLYD_SESSIONS_PATH);
            return 0;
        }
    }

    signal(SIGPIPE, SIG_IGN);

    WubuHoly daemon;
    if (wubu_holyd_init(&daemon, &config) != 0) {
        fprintf(stderr, "Failed to initialize holyd\n");
        return 1;
    }

    if (config.daemonize) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return 1; }
        if (pid > 0) { printf("wubu_holyd started (PID %d)\n", pid); return 0; }
        setsid();
        int devnull = open("/dev/null", O_RDWR);
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        close(devnull);
    }

    if (wubu_holyd_start(&daemon) != 0) {
        fprintf(stderr, "Failed to start holyd\n");
        return 1;
    }

    wubu_holyd_event_loop(&daemon);
    wubu_holyd_shutdown(&daemon);
    return 0;
}
#endif /* WUBD_TEST_MAIN */
