/*
 * dosgui_term.h  --  WuBuOS Terminal (PTY + HolyC REPL + Tabbed)
 *
 * Phase 6: Full-featured terminal with:
 *   - PTY backend for shell sessions (bash, zsh, etc.)
 *   - Tabbed sessions (multiple shells in one window)
 *   - HolyC REPL pane integration
 *   - GPU-accelerated render via VBE double-buffer
 *   - Scrollback buffer with search
 *   - Copy/paste, selection, URL detection
 *   - Keyboard shortcuts (Ctrl+Shift+T new tab, Ctrl+Shift+W close, Ctrl+PgUp/PgDn switch)
 */

#ifndef WUBU_DOSGUI_TERM_H
#define WUBU_DOSGUI_TERM_H

#include <stdint.h>
#include <stdbool.h>

/* -- Limits ------------------------------------------------------- */

#define TERM_MAX_TABS           16
#define TERM_MAX_COLS           240
#define TERM_MAX_ROWS           100
#define TERM_SCROLLBACK_LINES   10000
#define TERM_MAX_LINE_LEN       512
#define TERM_TAB_LABEL_LEN      32
#define TERM_SEARCH_BUF         128

/* Include for pid_t */
#include <sys/types.h>

/* -- Tab/Session Types -------------------------------------------- */

typedef enum {
    TERM_SESSION_SHELL     = 0,  /* PTY shell (bash, zsh, etc.) */
    TERM_SESSION_HOLYC     = 1,  /* HolyC REPL */
    TERM_SESSION_CONTAINER = 2,  /* Container shell */
} TermSessionType;

/* -- PTY Session State -------------------------------------------- */

typedef struct {
    int             ptm_fd;         /* Master PTY fd */
    int             pts_fd;         /* Slave PTY fd (kept for reference) */
    pid_t           child_pid;      /* Child process PID */
    char            cwd[4096];      /* Current working directory */
    char            shell[64];      /* Shell executable */
    bool            running;        /* Session active */

    /* Terminal state */
    int             cols, rows;
    bool            cursor_visible;
    int             cursor_x, cursor_y;
    int             cursor_blink;

    /* Scrollback buffer */
    char            scrollback[TERM_SCROLLBACK_LINES][TERM_MAX_LINE_LEN];
    int             scrollback_head;    /* Next write position */
    int             scrollback_count;   /* Lines in buffer */
    int             scrollback_view;    /* View offset (0 = bottom) */

    /* Screen buffer (current visible area) */
    char            screen[TERM_MAX_ROWS][TERM_MAX_COLS];
    uint8_t         attrs[TERM_MAX_ROWS][TERM_MAX_COLS];  /* Colors, bold, etc. */
    bool            dirty[TERM_MAX_ROWS];

    /* Current rendering state for ANSI parser */
    uint8_t         cur_attr;       /* Current attribute flags (bold/reverse/etc) */
    uint8_t         cur_fg;         /* Current foreground color index */
    uint8_t         cur_bg;         /* Current background color index */
    int             saved_cursor_x; /* Saved cursor position (DECSC) */
    int             saved_cursor_y;

    /* Selection */
    bool            selecting;
    int             sel_start_x, sel_start_y;
    int             sel_end_x, sel_end_y;
} TermPtySession;

/* -- HolyC Session State ------------------------------------------ */

/* The HolyC terminal tab embeds the wubu_holyd REPL as a real PTY-backed
 * process (`wubu_holyd --repl`), so the Desktop terminal hosts a live
 * interactive HolyC REPL (E4). */
typedef struct {
    TermPtySession  pty;            /* PTY running `wubu_holyd --repl` */
} TermHolycSession;

/* -- Container Session State -------------------------------------- */

typedef struct {
    char            container_name[64];
    int             container_pid;
    bool            connected;
    int             ptm_fd;           /* PTY master fd for container shell */
    pid_t           child_pid;        /* Child process PID */
    char            cwd[4096];        /* Current working directory */
    bool            running;          /* Session active */
    char            shell[64];        /* Shell executable inside container */
    int             cols, rows;       /* Terminal size */
    /* Screen buffer (current visible area) */
    char            screen[TERM_MAX_ROWS][TERM_MAX_COLS];
    uint8_t         attrs[TERM_MAX_ROWS][TERM_MAX_COLS];  /* Colors, bold, etc. */
    uint8_t         cur_attr;         /* Current attribute flags (bold/reverse/etc) */
    uint8_t         cur_fg;           /* Current foreground color index */
    uint8_t         cur_bg;           /* Current background color index */
    int             cursor_x, cursor_y;        /* Cursor position */
    int             saved_cursor_x, saved_cursor_y;  /* Saved cursor position (DECSC) */
    /* Selection */
    bool            selecting;
    int             sel_start_x, sel_start_y;
    int             sel_end_x, sel_end_y;
} TermContainerSession;

/* -- Tab Structure ------------------------------------------------ */

typedef struct {
    TermSessionType type;
    char            label[TERM_TAB_LABEL_LEN];
    bool            active;
    bool            dirty;          /* Needs redraw */

    union {
        TermPtySession     pty;
        TermHolycSession   holyc;
        TermContainerSession container;
    } session;
} TermTab;

/* -- Main Terminal State ------------------------------------------ */

typedef struct {
    TermTab           tabs[TERM_MAX_TABS];
    int               tab_count;
    int               active_tab;

    /* Window reference */
    int               win_id;         /* Parent window ID */
    int               x, y, w, h;     /* Window position */

    /* UI State */
    int               tab_bar_h;      /* Height of tab bar */
    bool              show_tab_bar;
    int               hovered_tab;    /* Tab under mouse */
    bool              tab_drag;       /* Dragging tab */
    int               drag_tab_idx;   /* Tab being dragged */
    int               drag_offset_x;  /* Drag offset */

    /* Search */
    bool              searching;
    char              search_buf[TERM_SEARCH_BUF];
    int               search_pos;
    int               search_match_line;
    int               search_match_col;

    /* Copy/Paste */
    char              clipboard[4096];
    bool              has_selection;

    /* Colors (from theme) */
    uint32_t          color_fg;
    uint32_t          color_bg;
    uint32_t          color_cursor;
    uint32_t          color_selection;
    uint32_t          color_tab_active;
    uint32_t          color_tab_inactive;
    uint32_t          color_tab_hover;
} TermState;

/* -- Global Instance ---------------------------------------------- */

extern TermState g_term;

/* -- Public API --------------------------------------------------- */

/* Lifecycle */
int  dosgui_term_init(void);
void dosgui_term_shutdown(void);

/* Window management */
void dosgui_term_show(int x, int y, int w, int h);
void dosgui_term_hide(void);
bool dosgui_term_is_open(void);
void dosgui_term_toggle(void);

/* Tab management */
int  dosgui_term_new_tab(TermSessionType type, const char *label, const char *shell);
void dosgui_term_close_tab(int idx);
void dosgui_term_switch_tab(int idx);
void dosgui_term_move_tab(int from, int to);
int  dosgui_term_get_active_tab(void);
TermTab *dosgui_term_get_tab(int idx);

/* Session control */
int  dosgui_term_spawn_shell(const char *shell, const char *cwd);
int  dosgui_term_spawn_holyc(void);
int  dosgui_term_spawn_container(const char *container_name);

/* Input handling */
void dosgui_term_handle_key(uint32_t key, uint32_t mods);
void dosgui_term_handle_mouse(int x, int y, int btn, int kind);
void dosgui_term_handle_resize(int w, int h);

/* Rendering (called from WM) */
void dosgui_term_render(uint32_t *fb, int fb_w, int fb_h);
void dosgui_term_render_tab_bar(uint32_t *fb, int fb_w, int fb_h);
void dosgui_term_render_content(uint32_t *fb, int fb_w, int fb_h);

/* PTY I/O */
void dosgui_term_pty_write(const char *data, int len);
void dosgui_term_pty_read(void);        /* Non-blocking read from PTY */

/* Helpers */
void dosgui_term_update_colors(void);
void dosgui_term_scroll(int lines);
void dosgui_term_copy_selection(void);
void dosgui_term_paste(void);
const char *dosgui_term_get_cwd(void);

/* HolyC REPL tab is PTY-backed (wubu_holyd --repl); routing handled via
 * dosgui_term_pty_write / dosgui_term_pty_read like the shell tab. */
/* State accessor */
TermState *dosgui_term_state(void);

#endif /* WUBU_DOSGUI_TERM_H */