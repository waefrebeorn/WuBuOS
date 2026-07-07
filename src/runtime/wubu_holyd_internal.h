/*
 * wubu_holyd_internal.h  --  Internal header for wubu_holyd submodules
 * Shared includes, internal helpers, and cross-module declarations.
 */

#ifndef WUBU_HOLYD_INTERNAL_H
#define WUBU_HOLYD_INTERNAL_H

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

/* WM bridge for window registration */
#include "../gui/dosgui_wm.h"

/* -- Internal helper declarations -------------------------------- */

/* Path join: base + "/" + name, dynamically allocated */
static inline char *holyd_path_join(const char *base, const char *name) {
    if (!base || !name) return NULL;
    size_t base_len = strlen(base);
    size_t name_len = strlen(name);
    char *result = malloc(base_len + 1 + name_len + 1);
    if (!result) return NULL;
    memcpy(result, base, base_len);
    result[base_len] = '/';
    memcpy(result + base_len + 1, name, name_len);
    result[base_len + 1 + name_len] = '\0';
    return result;
}

/* Safe snprintf with dynamic realloc */
static inline int holyd_snprintf_alloc(char **out, size_t *out_size, const char *fmt, ...) {
    va_list ap, ap2;
    va_start(ap, fmt);
    va_copy(ap2, ap);
    int needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) return -1;
    if ((size_t)needed + 1 > *out_size) {
        *out_size = needed + 1;
        *out = realloc(*out, *out_size);
        if (!*out) return -1;
    }
    int written = vsnprintf(*out, *out_size, fmt, ap2);
    va_end(ap2);
    return written;
}

/* Find session by name (internal, defined in facade) */
WubuHolySession *holyd_find_session(WubuHoly *d, const char *name);

/* Logging (internal, defined in facade) */
void holyd_log(WubuHoly *d, int level, const char *fmt, ...);

/* Socket create/destroy (internal, defined in facade) */
int  holyd_socket_create(WubuHoly *d);
void holyd_socket_destroy(WubuHoly *d, int fd);

/* Get compiler for session (creates if needed, defined in facade) */
HCCompiler *holyd_get_compiler(WubuHolySession *s, WubuHoly *d);

/* -- Session operations (wubu_holyd_session.c) ------------------- */
int  wubu_holyd_session_create(WubuHoly *d, const char *name,
                                int width, int height);
int  wubu_holyd_session_destroy(WubuHoly *d, const char *name);
int  wubu_holyd_session_list(WubuHoly *d, WubuHolySession *out, int max);
int  wubu_holyd_session_info(WubuHoly *d, const char *name,
                              WubuHolySession *out);
int  wubu_holyd_session_focus(WubuHoly *d, const char *name);

/* -- Code execution (wubu_holyd_exec.c) -------------------------- */
int  wubu_holyd_eval(WubuHoly *d, const char *session,
                      const char *code, char *output, size_t out_size);
int  wubu_holyd_compile(WubuHoly *d, const char *session,
                         const char *code, void **out_binary, size_t *out_size);
int  wubu_holyd_run(WubuHoly *d, const char *session,
                     const void *binary, size_t size);
int  wubu_holyd_stop(WubuHoly *d, const char *session);
int  wubu_holyd_repl_start(WubuHoly *d, const char *session);
int  wubu_holyd_repl_eval(WubuHoly *d, const char *session,
                           const char *code, char *output, size_t out_size);
int  wubu_holyd_repl_stop(WubuHoly *d, const char *session);
int  wubu_holyd_macro_define(WubuHoly *d, const char *session,
                              const char *name, const char *value);
int  wubu_holyd_macro_undef(WubuHoly *d, const char *session,
                             const char *name);

/* -- Window operations (wubu_holyd_window.c) --------------------- */
int  wubu_holyd_window_create(WubuHoly *d, const char *session,
                               WubuHolyWindowType type,
                               int x, int y, int w, int h,
                               const char *title, int *out_window_id);
int  wubu_holyd_window_destroy(WubuHoly *d, const char *session, int window_id);
int  wubu_holyd_window_show(WubuHoly *d, const char *session, int window_id);
int  wubu_holyd_window_hide(WubuHoly *d, const char *session, int window_id);
int  wubu_holyd_window_resize(WubuHoly *d, const char *session,
                               int window_id, int w, int h);
int  wubu_holyd_window_move(WubuHoly *d, const char *session,
                             int window_id, int x, int y);
int  wubu_holyd_window_focus(WubuHoly *d, const char *session, int window_id);
int  wubu_holyd_window_list(WubuHoly *d, const char *session,
                             WubuHolyWindow *out, int max);

/* -- Input routing (wubu_holyd_input.c) -------------------------- */
int  wubu_holyd_input_key(WubuHoly *d, const char *session,
                           int keycode, int modifiers);
int  wubu_holyd_input_mouse(WubuHoly *d, const char *session,
                             int x, int y, int buttons);
int  wubu_holyd_input_paste(WubuHoly *d, const char *session,
                             const char *text);

/* -- 9P namespace (wubu_holyd_9p.c) ------------------------------ */
int  wubu_holyd_mount(WubuHoly *d, const char *session,
                       const char *path);
int  wubu_holyd_unmount(WubuHoly *d, const char *session);
int  wubu_holyd_export(WubuHoly *d, const char *session,
                        const char *path, const char *target);

/* -- Auto-save (wubu_holyd_save.c) ------------------------------- */
int  wubu_holyd_session_save(WubuHoly *d, const char *session);
int  wubu_holyd_session_restore(WubuHoly *d, const char *session,
                                 const char *save_path);

/* -- Event bus (wubu_holyd_event.c) ------------------------------ */
int  wubu_holyd_publish_event(WubuHoly *d, const char *event_type,
                               const char *session, const char *data);

/* -- Daemon lifecycle (wubu_holyd_lifecycle.c) ------------------- */
int  wubu_holyd_init(WubuHoly *d, const WubuHolyConfig *config);
int  wubu_holyd_start(WubuHoly *d);
void wubu_holyd_event_loop(WubuHoly *d);
void wubu_holyd_daemon_stop(WubuHoly *d);
void wubu_holyd_shutdown(WubuHoly *d);

/* -- String tables (stay in wubu_holyd.c facade -- pure utility) */
const char *wubu_holyd_session_state_str(WubuHolySessionState state);
const char *wubu_holyd_window_type_str(WubuHolyWindowType type);
const char *wubu_holyd_cmd_str(WubuHolyCmd cmd);
const char *wubu_holyd_version(void);

#endif /* WUBU_HOLYD_INTERNAL_H */