/*
 * wubu_holyd.h  --  WuBuOS TempleOS HolyC DOS Daemon
 *
 * The HolyC DOS daemon manages the TempleOS-style DOS environment
 * within WuBuOS. It is the "shell" of the HolyC DOS layer — managing
 * REPL sessions, HolyC compilation, VBE display, and input routing.
 *
 * Architecture (learned from TempleOS + Ubuntu desktop):
 *   - Like TempleOS: HolyC JIT compiler, VBE framebuffer, direct hardware
 *   - Like Ubuntu gnome-shell: window management, input routing, session management
 *   - Like Ubuntu systemd-logind: session tracking, multi-seat support
 *   - Like SteamOS gamescope: display compositor for HolyC windows
 *
 * Daemon responsibilities:
 *   1. Session management: create, destroy, switch HolyC DOS sessions
 *   2. HolyC compilation: JIT compile HolyC code, manage compiler state
 *   3. VBE display: framebuffer management, window composition, text rendering
 *   4. Input routing: keyboard/mouse → focused HolyC window
 *   5. 9P namespace: expose HolyC filesystem to WuBuOS desktop
 *   6. Auto-save: periodic snapshot of HolyC session state
 *   7. Integration: publish events to WuBuOS desktop (window create/destroy/focus)
 *
 * Communication: Unix domain socket + JSON protocol
 *   Client: wubu_holyctl (CLI tool) or WuBuOS desktop (GUI)
 *   Server: wubu_holyd (this daemon)
 *
 * Session model:
 *   Each HolyC DOS session has:
 *     - A persistent HCCompiler (survives across evals)
 *     - A VBE framebuffer (320x200 to 1920x1080)
 *     - An input queue (keyboard + mouse)
 *     - A 9P mount point (/wubu/holyc/<session>)
 *     - A window ID (registered with WuBuOS WM)
 */

#ifndef WUBU_HOLYD_H
#define WUBU_HOLYD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <time.h>

#define WUBU_HOLYD_SOCKET_PATH  "/run/wubu/holyd.sock"
#define WUBU_HOLYD_PID_PATH     "/run/wubu/holyd.pid"
#define WUBU_HOLYD_LOG_PATH     "/var/log/wubu/holyd.log"
#define WUBU_HOLYD_SESSIONS_PATH "/var/wubu/sessions"
#define WUBU_HOLYD_MAX_SESSIONS  16
#define WUBU_HOLYD_MAX_WINDOWS  32
#define WUBU_HOLYD_MAX_SESSION_NAME 64
#define WUBU_HOLYD_MAX_PATH     512
#define WUBU_HOLYD_MAX_CODE     65536   /* Max HolyC source per eval */
#define WUBU_HOLYD_MAX_OUTPUT   8192    /* Max output buffer */
#define WUBU_HOLYD_MAX_RESPONSE 16384
#define WUBU_HOLYD_VERSION      "0.1.0"

/* -- Session States ----------------------------------------------- */

typedef enum {
    SESSION_STATE_INACTIVE = 0, /* Session exists, no compiler */
    SESSION_STATE_STARTING,     /* Compiler initializing */
    SESSION_STATE_ACTIVE,       /* Ready for input */
    SESSION_STATE_RUNNING,      /* Code executing */
    SESSION_STATE_PAUSED,       /* Execution paused (debug) */
    SESSION_STATE_ERROR,        /* Compiler/runtime error */
    SESSION_STATE_SAVING,       /* Auto-save in progress */
    SESSION_STATE_DESTROYING,   /* Being torn down */
} WubuHolySessionState;

/* -- Window Types (TempleOS-style) -------------------------------- */

typedef enum {
    HOLY_WINDOW_TERM = 0,       /* HolyC terminal (REPL) */
    HOLY_WINDOW_EDITOR,         /* HolyC code editor */
    HOLY_WINDOW_GRAPH,          /* VBE graph/plot window */
    HOLY_WINDOW_FILE,           /* File browser */
    HOLY_WINDOW_DEBUG,          /* Debugger window */
    HOLY_WINDOW_DOCS,           /* Documentation browser */
} WubuHolyWindowType;

/* -- Window Record ------------------------------------------------ */

typedef struct {
    int     id;                 /* Window ID (WM reference) */
    char    title[128];
    WubuHolyWindowType type;
    int     x, y;               /* Position */
    int     w, h;               /* Size */
    bool    visible;
    bool    focused;
    uint32_t *framebuffer;      /* VBE framebuffer (if direct) */
    int     fb_size;            /* Buffer size in bytes */
    char    session_name[WUBU_HOLYD_MAX_SESSION_NAME];
} WubuHolyWindow;

/* -- Session Record ----------------------------------------------- */

typedef struct {
    char name[WUBU_HOLYD_MAX_SESSION_NAME];
    WubuHolySessionState state;
    time_t created;
    time_t last_active;
    int window_count;
    WubuHolyWindow windows[WUBU_HOLYD_MAX_WINDOWS];
    int focused_window;         /* Window ID, -1 if none */

    /* Compiler state (opaque pointer to HCCompiler) */
    void *compiler;             /* HCCompiler* — actual type from holyc.h */
    bool compiler_initialized;

    /* Execution state */
    int exit_code;              /* Last eval exit code */
    int error_count;
    char last_error[512];

    /* Auto-save */
    time_t last_save;
    int save_interval_sec;
    char *save_path;            /* Dynamically allocated session directory */

    /* 9P namespace */
    char *mount_point;          /* Dynamically allocated mount path */
    bool mounted;

    /* Input queue */
    int input_head;
    int input_tail;
    char input_buf[4096];

    /* Mouse state tracking for WM dispatch */
    int prev_buttons;           /* Previous frame button bitmask */
} WubuHolySession;

/* -- Daemon Configuration ----------------------------------------- */

typedef struct {
    char socket_path[WUBU_HOLYD_MAX_PATH];
    char log_path[WUBU_HOLYD_MAX_PATH];
    char sessions_path[WUBU_HOLYD_MAX_PATH];
    int  max_sessions;
    int  default_width;         /* Default VBE width */
    int  default_height;        /* Default VBE height */
    int  save_interval_sec;     /* Auto-save interval */
    int  log_level;             /* 0=error, 1=warn, 2=info, 3=debug */
    bool daemonize;
    bool auto_mount_9p;         /* Auto-mount 9P namespace */
    bool debug_mode;            /* Enable debugger */
} WubuHolyConfig;

/* -- Daemon State ------------------------------------------------- */

typedef struct {
    WubuHolyConfig config;
    WubuHolySession sessions[WUBU_HOLYD_MAX_SESSIONS];
    int session_count;
    bool running;
    int server_fd;              /* Unix socket */
    int epoll_fd;               /* Event loop */
    time_t start_time;
    uint64_t requests_handled;
    uint64_t evals_performed;
    uint64_t errors;
} WubuHoly;

/* -- Request/Response Protocol ------------------------------------ */

typedef enum {
    /* Session lifecycle */
    HOLYD_CMD_SESSION_CREATE = 1,
    HOLYD_CMD_SESSION_DESTROY,
    HOLYD_CMD_SESSION_LIST,
    HOLYD_CMD_SESSION_INFO,
    HOLYD_CMD_SESSION_FOCUS,
    HOLYD_CMD_SESSION_SAVE,
    HOLYD_CMD_SESSION_RESTORE,

    /* Code execution */
    HOLYD_CMD_EVAL,             /* Evaluate HolyC expression */
    HOLYD_CMD_COMPILE,          /* Compile HolyC to binary */
    HOLYD_CMD_RUN,              /* Run compiled code */
    HOLYD_CMD_STOP,             /* Stop running code */

    /* Window management */
    HOLYD_CMD_WINDOW_CREATE,
    HOLYD_CMD_WINDOW_DESTROY,
    HOLYD_CMD_WINDOW_SHOW,
    HOLYD_CMD_WINDOW_HIDE,
    HOLYD_CMD_WINDOW_RESIZE,
    HOLYD_CMD_WINDOW_MOVE,
    HOLYD_CMD_WINDOW_FOCUS,
    HOLYD_CMD_WINDOW_LIST,

    /* Input */
    HOLYD_CMD_INPUT_KEY,
    HOLYD_CMD_INPUT_MOUSE,
    HOLYD_CMD_INPUT_PASTE,

    /* 9P namespace */
    HOLYD_CMD_MOUNT,
    HOLYD_CMD_UNMOUNT,
    HOLYD_CMD_EXPORT,

    /* Debug */
    HOLYD_CMD_BREAKPOINT_SET,
    HOLYD_CMD_BREAKPOINT_CLEAR,
    HOLYD_CMD_STEP,
    HOLYD_CMD_CONTINUE,
    HOLYD_CMD_STACK_TRACE,
    HOLYD_CMD_WATCH_VAR,

    /* Health & monitoring */
    HOLYD_CMD_PING,
    HOLYD_CMD_STATS,
    HOLYD_CMD_LOG,

    /* Daemon control */
    HOLYD_CMD_SHUTDOWN,
    HOLYD_CMD_RELOAD,
    HOLYD_CMD_VERSION,
} WubuHolyCmd;

typedef struct {
    WubuHolyCmd cmd;
    char session_name[WUBU_HOLYD_MAX_SESSION_NAME];
    int window_id;
    char data[4096];            /* JSON payload or HolyC source */
} WubuHolyRequest;

typedef struct {
    int status;                 /* 0=success, -1=error */
    char message[512];
    char output[WUBU_HOLYD_MAX_OUTPUT];
    char data[WUBU_HOLYD_MAX_RESPONSE];
} WubuHolyResponse;

/* -- Daemon Lifecycle --------------------------------------------- */

int  wubu_holyd_init(WubuHoly *d, const WubuHolyConfig *config);
int  wubu_holyd_start(WubuHoly *d);
void wubu_holyd_event_loop(WubuHoly *d);  /* Main event loop */
void wubu_holyd_daemon_stop(WubuHoly *d);
void wubu_holyd_shutdown(WubuHoly *d);

/* -- Session Operations ------------------------------------------- */

int  wubu_holyd_session_create(WubuHoly *d, const char *name,
                                int width, int height);
int  wubu_holyd_session_destroy(WubuHoly *d, const char *name);
int  wubu_holyd_session_list(WubuHoly *d, WubuHolySession *out, int max);
int  wubu_holyd_session_info(WubuHoly *d, const char *name,
                              WubuHolySession *out);
int  wubu_holyd_session_focus(WubuHoly *d, const char *name);

/* -- Code Execution ----------------------------------------------- */

int  wubu_holyd_eval(WubuHoly *d, const char *session,
                      const char *code, char *output, size_t out_size);
int  wubu_holyd_compile(WubuHoly *d, const char *session,
                         const char *code, void **out_binary, size_t *out_size);
int  wubu_holyd_run(WubuHoly *d, const char *session,
                     const void *binary, size_t size);
int  wubu_holyd_stop(WubuHoly *d, const char *session);

/* -- Window Operations -------------------------------------------- */

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

/* -- Input Routing ------------------------------------------------ */

int  wubu_holyd_input_key(WubuHoly *d, const char *session,
                           int keycode, int modifiers);
int  wubu_holyd_input_mouse(WubuHoly *d, const char *session,
                             int x, int y, int buttons);
int  wubu_holyd_input_paste(WubuHoly *d, const char *session,
                             const char *text);

/* -- 9P Namespace ------------------------------------------------- */

int  wubu_holyd_mount(WubuHoly *d, const char *session,
                       const char *path);
int  wubu_holyd_unmount(WubuHoly *d, const char *session);
int  wubu_holyd_export(WubuHoly *d, const char *session,
                        const char *path, const char *target);

/* -- Auto-Save ---------------------------------------------------- */

int  wubu_holyd_session_save(WubuHoly *d, const char *session);
int  wubu_holyd_session_restore(WubuHoly *d, const char *session,
                                 const char *save_path);

/* -- Event Bus (publish to WuBuOS desktop) ------------------------ */

int  wubu_holyd_publish_event(WubuHoly *d, const char *event_type,
                               const char *session, const char *data);

/* -- Utility ------------------------------------------------------ */

const char *wubu_holyd_session_state_str(WubuHolySessionState state);
const char *wubu_holyd_window_type_str(WubuHolyWindowType type);
const char *wubu_holyd_cmd_str(WubuHolyCmd cmd);
const char *wubu_holyd_version(void);

#endif /* WUBU_HOLYD_H */
