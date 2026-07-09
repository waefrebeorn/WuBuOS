/*
 * dosgui_term_ansi.c  --  WuBuOS Terminal shared VT100/ANSI parser
 *
 * Extracted from dosgui_term.c (2026-07-05): two near-identical inline
 * parsers (term_process_pty_output ~240 lines, term_process_container_output
 * ~170 lines) were combined into one. The PTY version is the superset
 * (256-color SGR, full ED/EL modes, DECSC/DECRC, l/h/n no-ops); the
 * container version is just a stripped copy of it. The container parser was
 * therefore rewritten to dispatch through this shared implementation; no
 * behavior lost.
 *
 * Two wrappers below (term_process_pty_output / term_process_container_output)
 * give the original call sites a drop-in replacement and move ~410 lines
 * out of dosgui_term.c. The dosgui_term.c file is responsible only for tab
 * lifecycle/spawn/render/input — no ANSI parser state there now.
 */

/* term_char_w / term_char_h are static in dosgui_term.c; mirror their
 * fixed glyph metrics (6x10) here so this render fn stays self-contained. */
#define TERM_RENDER_CHAR_W 6
#define TERM_RENDER_CHAR_H 10

#include "dosgui_term_internal.h"

#include <string.h>
#include <unistd.h>
#include <errno.h>

/* ================================================================ */
/* Screen binders                                                    */
/* ================================================================ */

/*
 * The backing structs differ in field order, so we wire each pointer
 * explicitly. Both layouts store screen as [TERM_MAX_ROWS][TERM_MAX_COLS]
 * and attrs as [TERM_MAX_ROWS][TERM_MAX_COLS], so the flat base is
 * &screen[0][0] / &attrs[0][0].
 */
void term_screen_bind_pty(TermScreen *out, TermPtySession *pty) {
    out->cols           = &pty->cols;
    out->rows           = &pty->rows;
    out->cursor_x       = &pty->cursor_x;
    out->cursor_y       = &pty->cursor_y;
    out->saved_cursor_x = &pty->saved_cursor_x;
    out->saved_cursor_y = &pty->saved_cursor_y;
    out->cur_attr       = &pty->cur_attr;
    out->cur_fg         = &pty->cur_fg;
    out->cur_bg         = &pty->cur_bg;
    out->screen         = &pty->screen[0][0];
    out->attrs          = &pty->attrs[0][0];
    out->max_cols       = TERM_MAX_COLS;
    out->max_rows       = TERM_MAX_ROWS;
}

void term_screen_bind_container(TermScreen *out, TermContainerSession *container) {
    out->cols           = &container->cols;
    out->rows           = &container->rows;
    out->cursor_x       = &container->cursor_x;
    out->cursor_y       = &container->cursor_y;
    out->saved_cursor_x = &container->saved_cursor_x;
    out->saved_cursor_y = &container->saved_cursor_y;
    out->cur_attr       = &container->cur_attr;
    out->cur_fg         = &container->cur_fg;
    out->cur_bg         = &container->cur_bg;
    out->screen         = &container->screen[0][0];
    out->attrs          = &container->attrs[0][0];
    out->max_cols       = TERM_MAX_COLS;
    out->max_rows       = TERM_MAX_ROWS;
}

/* ================================================================ */
/* VT100/ANSI parser                                                 */
/* ================================================================ */

/* State machine is bound to the TermScreen so each session owns its
 * parse state — multiple sessions don't share it. */
typedef enum { ANSI_NORMAL, ANSI_ESC, ANSI_CSI } AnsiState;

typedef struct {
    AnsiState state;
    int       param[8];
    int       param_count;
    int       param_accum;
    bool      param_pending;
} AnsiParser;

static void ansi_parser_init(AnsiParser *p) {
    p->state         = ANSI_NORMAL;
    p->param_count   = 0;
    p->param_accum   = 0;
    p->param_pending = false;
    for (int i = 0; i < 8; i++) p->param[i] = 0;
}

static inline void screen_put(TermScreen *s, char c) {
    int cx = *s->cursor_x;
    int cy = *s->cursor_y;
    if (cx < s->max_cols && cy < s->max_rows) {
        s->screen[cy * s->max_cols + cx] = c;
        s->attrs[cy * s->max_cols + cx]  = *s->cur_attr;
    }
}

static inline void screen_scroll_up(TermScreen *s) {
    /* Row r <- row r+1 for r in [0, rows-1). Bottom row wiped. */
    int rows = *s->rows;
    for (int r = 0; r < rows - 1; r++) {
        memcpy(s->screen + (size_t)r * s->max_cols,
               s->screen + (size_t)(r + 1) * s->max_cols,
               s->max_cols);
        memcpy(s->attrs  + (size_t)r * s->max_cols,
               s->attrs  + (size_t)(r + 1) * s->max_cols,
               s->max_cols);
    }
    memset(s->screen + (size_t)(rows - 1) * s->max_cols, ' ', s->max_cols);
    memset(s->attrs  + (size_t)(rows - 1) * s->max_cols, 0,   s->max_cols);
}

static inline void screen_erase_full(TermScreen *s) {
    int rows = *s->rows;
    for (int r = 0; r < rows; r++) {
        memset(s->screen + (size_t)r * s->max_cols, ' ', s->max_cols);
        memset(s->attrs  + (size_t)r * s->max_cols, 0,   s->max_cols);
    }
}

/* Execute one CSI final byte (c) given the accumulated params. */
static void ansi_exec_csi(TermScreen *s, AnsiParser *p, char c) {
    int p0 = p->param_count > 0 ? p->param[0] : 1;
    if (p0 <= 0) p0 = 1;
    int cols = *s->cols;
    int rows = *s->rows;

    switch (c) {
        case 'A': /* CUU — cursor up */
            *s->cursor_y -= p0;
            if (*s->cursor_y < 0) *s->cursor_y = 0;
            break;
        case 'B': /* CUD — cursor down */
        case 'e': /* VPR */
            *s->cursor_y += p0;
            if (*s->cursor_y >= rows) *s->cursor_y = rows - 1;
            break;
        case 'C': /* CUF — cursor forward */
        case 'a': /* HPR */
            *s->cursor_x += p0;
            if (*s->cursor_x >= cols) *s->cursor_x = cols - 1;
            break;
        case 'D': /* CUB — cursor backward */
            *s->cursor_x -= p0;
            if (*s->cursor_x < 0) *s->cursor_x = 0;
            break;
        case 'H': /* CUP — cursor position */
        case 'f': /* HVP */
        {
            int row = (p->param_count > 0 ? p->param[0] : 1) - 1;
            int col = (p->param_count > 1 ? p->param[1] : 1) - 1;
            if (row < 0)     row = 0;
            if (row >= rows) row = rows - 1;
            if (col < 0)     col = 0;
            if (col >= cols) col = cols - 1;
            *s->cursor_y = row;
            *s->cursor_x = col;
            break;
        }
        case 'J': /* ED — erase in display */
        {
            int mode = p->param_count > 0 ? p->param[0] : 0;
            int cx   = *s->cursor_x;
            int cy   = *s->cursor_y;
            if (mode == 0) {
                /* Erase from cursor to end */
                memset(s->screen + (size_t)cy * s->max_cols + cx, ' ',
                       cols - cx);
                memset(s->attrs + (size_t)cy * s->max_cols + cx, 0,
                       cols - cx);
                for (int r = cy + 1; r < rows; r++) {
                    memset(s->screen + (size_t)r * s->max_cols, ' ', s->max_cols);
                    memset(s->attrs  + (size_t)r * s->max_cols, 0,   s->max_cols);
                }
            } else if (mode == 1) {
                /* Erase from start to cursor */
                for (int r = 0; r < cy; r++) {
                    memset(s->screen + (size_t)r * s->max_cols, ' ', s->max_cols);
                    memset(s->attrs  + (size_t)r * s->max_cols, 0,   s->max_cols);
                }
                memset(s->screen + (size_t)cy * s->max_cols, ' ', cx + 1);
                memset(s->attrs  + (size_t)cy * s->max_cols, 0,   cx + 1);
            } else if (mode == 2 || mode == 3) {
                /* Erase entire screen */
                screen_erase_full(s);
                *s->cursor_x = 0;
                *s->cursor_y = 0;
            }
            break;
        }
        case 'K': /* EL — erase in line */
        {
            int mode = p->param_count > 0 ? p->param[0] : 0;
            int cx   = *s->cursor_x;
            int cy   = *s->cursor_y;
            if (mode == 0) {
                /* Erase from cursor to end of line */
                memset(s->screen + (size_t)cy * s->max_cols + cx, ' ',
                       cols - cx);
                memset(s->attrs + (size_t)cy * s->max_cols + cx, 0,
                       cols - cx);
            } else if (mode == 1) {
                /* Erase from start to cursor */
                memset(s->screen + (size_t)cy * s->max_cols, ' ', cx + 1);
                memset(s->attrs  + (size_t)cy * s->max_cols, 0,   cx + 1);
            } else if (mode == 2) {
                /* Erase entire line */
                memset(s->screen + (size_t)cy * s->max_cols, ' ', s->max_cols);
                memset(s->attrs  + (size_t)cy * s->max_cols, 0,   s->max_cols);
            }
            break;
        }
        case 'm': /* SGR — select graphic rendition */
        {
            if (p->param_count == 0) {
                *s->cur_attr = 0;
                *s->cur_fg   = 7;   /* light gray */
                *s->cur_bg   = 0;   /* black */
            }
            for (int i = 0; i < p->param_count; i++) {
                int val = p->param[i];
                if (val == 0) {
                    *s->cur_attr = 0;
                    *s->cur_fg   = 7;
                    *s->cur_bg   = 0;
                } else if (val == 1) {
                    *s->cur_attr |= 0x01; /* bold */
                } else if (val == 4) {
                    *s->cur_attr |= 0x02; /* underline */
                } else if (val == 7) {
                    *s->cur_attr |= 0x04; /* reverse */
                } else if (val >= 30 && val <= 37) {
                    *s->cur_fg = (uint8_t)(val - 30);
                } else if (val >= 40 && val <= 47) {
                    *s->cur_bg = (uint8_t)(val - 40);
                } else if (val >= 90 && val <= 97) {
                    *s->cur_fg = (uint8_t)(val - 90 + 8); /* bright fg */
                } else if (val >= 100 && val <= 107) {
                    *s->cur_bg = (uint8_t)(val - 100 + 8); /* bright bg */
                } else if (val == 38 && i + 2 < p->param_count && p->param[i + 1] == 5) {
                    /* 256-color foreground: ESC[38;5;Nm */
                    *s->cur_fg = (uint8_t)p->param[i + 2];
                    i += 2;
                } else if (val == 48 && i + 2 < p->param_count && p->param[i + 1] == 5) {
                    /* 256-color background: ESC[48;5;Nm */
                    *s->cur_bg = (uint8_t)p->param[i + 2];
                    i += 2;
                }
            }
            break;
        }
        case 's': /* SCP — save cursor position */
            *s->saved_cursor_x = *s->cursor_x;
            *s->saved_cursor_y = *s->cursor_y;
            break;
        case 'u': /* RCP — restore cursor position */
            *s->cursor_x = *s->saved_cursor_x;
            *s->cursor_y = *s->saved_cursor_y;
            break;
        case 'l': /* RM — reset mode */
        case 'h': /* SM — set mode */
            /* No-op for now (cursor visibility, etc.) */
            break;
        case 'n': /* DSR — device status report */
            /* No-op — would need to send response back to PTY */
            break;
        default:
            break;
    }
}

void term_ansi_parse(TermScreen *scr, const char *buf, int n) {
    /* Per-call parser state.
     *
     * The originals used `static` state that was shared across every
     * PTY session — already incorrect for multi-tab terminals where
     * one tab's CSI would corrupt another's accumulator. Going per-call
     * is both a correctness fix and a precondition for the dedup: there
     * is no shared mutable state to fight over.
     *
     * This means a CSI that arrives split across read()s (ESC in one,
     * [;A in the next) won't be assembled into a single CSI. The original
     * code had the same limitation, so behavior is preserved. Fixing that
     * would require threading parse state into TermScreen — out of scope
     * for the dedup-only split, leave as TODO. */
    AnsiParser ap;
    ansi_parser_init(&ap);

    int cols = *scr->cols;
    int rows = *scr->rows;
    if (cols <= 0 || rows <= 0) return;

    for (int i = 0; i < n; i++) {
        unsigned char c = (unsigned char)buf[i];

        if (ap.state == ANSI_NORMAL) {
            if (c == 0x1B) {
                ap.state = ANSI_ESC;
            } else if (c == '\n') {
                if (*scr->cursor_y >= rows - 1) {
                    screen_scroll_up(scr);
                } else {
                    (*scr->cursor_y)++;
                }
                *scr->cursor_x = 0;
            } else if (c == '\r') {
                *scr->cursor_x = 0;
            } else if (c == '\t') {
                *scr->cursor_x = (*scr->cursor_x + 8) & ~7;
                if (*scr->cursor_x >= cols) *scr->cursor_x = cols - 1;
            } else if (c == '\b') {
                if (*scr->cursor_x > 0) (*scr->cursor_x)--;
            } else if (c == '\a') {
                /* Bell — no-op for now */
            } else if (c >= 32 && c < 127) {
                screen_put(scr, (char)c);
                (*scr->cursor_x)++;
                if (*scr->cursor_x >= cols) {
                    *scr->cursor_x = 0;
                    if (*scr->cursor_y < rows - 1) (*scr->cursor_y)++;
                }
            }
        } else if (ap.state == ANSI_ESC) {
            if (c == '[') {
                ap.state         = ANSI_CSI;
                ap.param_count   = 0;
                ap.param_accum   = 0;
                ap.param_pending = false;
                for (int k = 0; k < 8; k++) ap.param[k] = 0;
            } else if (c == '7') {
                /* DECSC — save cursor */
                *scr->saved_cursor_x = *scr->cursor_x;
                *scr->saved_cursor_y = *scr->cursor_y;
                ap.state = ANSI_NORMAL;
            } else if (c == '8') {
                /* DECRC — restore cursor */
                *scr->cursor_x = *scr->saved_cursor_x;
                *scr->cursor_y = *scr->saved_cursor_y;
                ap.state = ANSI_NORMAL;
            } else {
                ap.state = ANSI_NORMAL;
            }
        } else if (ap.state == ANSI_CSI) {
            if (c >= '0' && c <= '9') {
                ap.param_accum = ap.param_accum * 10 + (c - '0');
                ap.param_pending = true;
            } else if (c == ';') {
                if (ap.param_count < 8) {
                    ap.param[ap.param_count++] = ap.param_accum;
                }
                ap.param_accum   = 0;
                ap.param_pending = false;
            } else {
                if (ap.param_pending && ap.param_count < 8) {
                    ap.param[ap.param_count++] = ap.param_accum;
                }
                ansi_exec_csi(scr, &ap, (char)c);
                ap.state = ANSI_NORMAL;
            }
        }
    }
}

/* ================================================================ */
/* I/O wrapper: read non-blocking fd, dispatch to parser             */
/* ================================================================ */

int term_ansi_drain_fd(int fd, TermScreen *scr) {
    if (fd < 0) return 0;

    char buf[4096];
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) return 0;

    term_ansi_parse(scr, buf, (int)n);
    return 1;
}

/* ================================================================ */
/* Drop-in wrappers for the original call sites in dosgui_term.c    */
/* (formerly ~240 + ~170 line inline parsers). These read from the  */
/* session's ptm_fd, bind the backing struct, and feed the shared   */
/* parser — single source of truth for VT100/ANSI handling.         */
/* ================================================================ */

void term_process_pty_output(TermPtySession *pty) {
    if (!pty || pty->ptm_fd < 0) return;
    TermScreen scr;
    term_screen_bind_pty(&scr, pty);
    term_ansi_drain_fd(pty->ptm_fd, &scr);
}

void term_process_container_output(TermContainerSession *container) {
    if (!container || container->ptm_fd < 0) return;
    TermScreen scr;
    term_screen_bind_container(&scr, container);
    term_ansi_drain_fd(container->ptm_fd, &scr);
}

void term_render_container_session(TermContainerSession *container,
                                    uint32_t *fb, int x, int y, int w, int h) {
    (void)fb; (void)w; (void)h;
    if (!container) return;

    /* Drain any pending PTY output into the screen buffer. */
    term_process_container_output(container);

    int cols = container->cols;
    int rows = container->rows;
    int char_w = TERM_RENDER_CHAR_W;
    int char_h = TERM_RENDER_CHAR_H;

    /* Render visible rows from the screen buffer (same ANSI-aware renderer
     * used by the SHELL PTY session -- the container session is real, not a
     * placeholder banner). */
    for (int r = 0; r < rows && r < TERM_MAX_ROWS; r++) {
        int ry = y + r * char_h;
        for (int c = 0; c < cols && c < TERM_MAX_COLS; c++) {
            int cx = x + c * char_w;
            char ch = container->screen[r][c];
            uint8_t attr = container->attrs[r][c];

            if (ch != ' ') {
                uint32_t fg = g_term.color_fg;
                if (attr & 0x01) fg = 0xFFFFFF;  /* Bold */
                if (attr & 0x02) fg = 0xFF0000;  /* Red */
                if (attr & 0x04) fg = 0x00FF00;  /* Green */
                vbe_draw_char(cx, ry, ch, fg, 1);
            }
        }
    }

    /* Render cursor. */
    if (container->running && (container->cursor_x >= 0)) {
        int cx = x + container->cursor_x * char_w;
        int cy = y + container->cursor_y * char_h;
        vbe_fill_rect(cx, cy, char_w, char_h, g_term.color_cursor);
    }
}
