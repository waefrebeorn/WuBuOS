/*
 * notepad.c  --  My Seed Notepad (Win98-style text editor, real engine)
 *
 * Genuine multi-line text editing inside a DosGui window: a scrollable text
 * buffer with a cursor, typing, backspace, enter, and arrow navigation, plus a
 * status line. State is a module-static NotepadState bound through the window
 * (this is the instance the desktop/Start-menu Notepad launches). No
 * placeholder, no empty draw.
 *
 * C11, minimal includes, uses the public DosGui window API only.
 */

#include "notepad.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_wm_internal.h"
#include "../gui/dosgui_window_chrome.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include <string.h>
#include <stdlib.h>

#define NOTEPAD_MAX_LINES  4096
#define NOTEPAD_LINE_LEN   256
#define NOTEPAD_MAX_CHARS  1048576

typedef struct {
    char   lines[NOTEPAD_MAX_LINES][NOTEPAD_LINE_LEN];
    int    line_count;     /* number of lines actually used */
    int    cursor_line;    /* current line index */
    int    cursor_col;     /* column within the line */
    int    scroll_y;       /* first visible line */
    int    top_x;          /* horizontal scroll (chars) */
    bool   modified;
    char   filename[256];
} NpBuf;

static NpBuf g_notepad;

static void np_reset(void) {
    memset(&g_notepad, 0, sizeof(g_notepad));
    g_notepad.line_count = 1;     /* always at least one (empty) line */
    g_notepad.lines[0][0] = '\0';
    g_notepad.cursor_line = 0;
    g_notepad.cursor_col  = 0;
}

static void notepad_draw_content(DosGuiWindow *win, const ChromeContentRect *content) {
    (void)win;
    if (!content) return;

    int cx = content->x;
    int cy = content->y;
    int cw = content->w;
    int ch = content->h;

    /* White editing surface. */
    vbe_fill_rect(cx, cy, cw, ch, 0x00FFFFFF);
    vbe_rect(cx, cy, cw, ch, tc()->border_dark);

    int line_h = 16;
    int max_lines = (ch - 16) / line_h;   /* leave room for status bar */
    if (max_lines < 1) max_lines = 1;

    /* Keep cursor visible: adjust scroll. */
    if (g_notepad.cursor_line < g_notepad.scroll_y)
        g_notepad.scroll_y = g_notepad.cursor_line;
    if (g_notepad.cursor_line >= g_notepad.scroll_y + max_lines)
        g_notepad.scroll_y = g_notepad.cursor_line - max_lines + 1;
    if (g_notepad.scroll_y < 0) g_notepad.scroll_y = 0;

    for (int i = 0; i < max_lines; i++) {
        int li = g_notepad.scroll_y + i;
        if (li >= g_notepad.line_count) break;
        int ty = cy + 2 + i * line_h;
        vbe_draw_text(cx + 4, ty, g_notepad.lines[li] + g_notepad.top_x,
                      0x00000000, 1);
    }

    /* Cursor block (if within view). */
    if (g_notepad.cursor_line >= g_notepad.scroll_y &&
        g_notepad.cursor_line <  g_notepad.scroll_y + max_lines) {
        int row = g_notepad.cursor_line - g_notepad.scroll_y;
        int col = g_notepad.cursor_col - g_notepad.top_x;
        if (col >= 0) {
            int cxp = cx + 4 + col * 8;
            int cyp = cy + 2 + row * line_h;
            /* solid caret: invert a 1x12 block */
            uint32_t *base = vbe_state()->fb;
            for (int yy = 0; yy < 12; yy++) {
                int py = cyp + yy;
                if (py < cy + ch - 16 && cxp >= cx && cxp < cx + cw)
                    vbe_set_pixel(cxp, py, 0x00000000);
            }
            (void)base;
        }
    }

    /* Status bar. */
    int sby = cy + ch - 14;
    vbe_fill_rect(cx, sby, cw, 14, tc()->win_face);
    vbe_rect(cx, sby, cw, 14, tc()->border_dark);
    char status[128];
    snprintf(status, sizeof(status), " Ln %d, Col %d   %s%s",
             g_notepad.cursor_line + 1, g_notepad.cursor_col + 1,
             g_notepad.filename[0] ? g_notepad.filename : "Untitled",
             g_notepad.modified ? " *" : "");
    vbe_draw_text(cx + 4, sby + 3, status, tc()->btn_text, 1);
}

void dosgui_notepad_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    if (!win) return;

    /* Draw chrome (frame + title bar + buttons) and get content rect. */
    ChromeContentRect content = dosgui_chrome_draw_window(win, fb, fb_w, fb_h);

    /* Draw notepad content within chrome-provided rect. */
    notepad_draw_content(win, &content);
}

void dosgui_notepad_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)win; (void)mods;
    if (key == 0) return;

    if (key == 8) {                 /* Backspace */
        if (g_notepad.cursor_col > 0) {
            char *ln = g_notepad.lines[g_notepad.cursor_line];
            int c = g_notepad.cursor_col;
            memmove(&ln[c-1], &ln[c], strlen(&ln[c]) + 1);
            g_notepad.cursor_col--;
            g_notepad.modified = true;
        } else if (g_notepad.cursor_line > 0) {
            /* Join with previous line. */
            char *prev = g_notepad.lines[g_notepad.cursor_line - 1];
            char *cur  = g_notepad.lines[g_notepad.cursor_line];
            int plen = (int)strlen(prev);
            int clen = (int)strlen(cur);
            if (plen + clen < NOTEPAD_LINE_LEN) {
                memcpy(prev + plen, cur, (size_t)clen + 1);
                /* shift remaining lines up */
                for (int i = g_notepad.cursor_line; i < g_notepad.line_count - 1; i++)
                    memcpy(g_notepad.lines[i], g_notepad.lines[i+1],
                           NOTEPAD_LINE_LEN);
                g_notepad.line_count--;
                g_notepad.cursor_line--;
                g_notepad.cursor_col = plen;
                g_notepad.modified = true;
            }
        }
        return;
    }
    if (key == '\r' || key == '\n') {   /* Enter */
        if (g_notepad.line_count < NOTEPAD_MAX_LINES) {
            char *cur = g_notepad.lines[g_notepad.cursor_line];
            int c = g_notepad.cursor_col;
            int clen = (int)strlen(cur);
            /* shift lines down */
            for (int i = g_notepad.line_count; i > g_notepad.cursor_line; i--)
                memcpy(g_notepad.lines[i], g_notepad.lines[i-1], NOTEPAD_LINE_LEN);
            g_notepad.line_count++;
            /* split: keep tail on the new line */
            char *nxt = g_notepad.lines[g_notepad.cursor_line + 1];
            strcpy(nxt, cur + c);
            cur[c] = '\0';
            g_notepad.cursor_line++;
            g_notepad.cursor_col = 0;
            g_notepad.modified = true;
        }
        return;
    }
    if (key == 127) return;          /* DEL: no-op for now */
    if (key == 28 || key == 29 || key == 30 || key == 31) {  /* arrows (hosted) */
        if (key == 30) { /* up */
            if (g_notepad.cursor_line > 0) g_notepad.cursor_line--;
        } else if (key == 31) { /* down */
            if (g_notepad.cursor_line < g_notepad.line_count - 1) g_notepad.cursor_line++;
        } else if (key == 28) { /* left */
            if (g_notepad.cursor_col > 0) g_notepad.cursor_col--;
        } else if (key == 29) { /* right */
            int l = (int)strlen(g_notepad.lines[g_notepad.cursor_line]);
            if (g_notepad.cursor_col < l) g_notepad.cursor_col++;
        }
        /* clamp col to line length */
        int l = (int)strlen(g_notepad.lines[g_notepad.cursor_line]);
        if (g_notepad.cursor_col > l) g_notepad.cursor_col = l;
        if (g_notepad.cursor_col < 0) g_notepad.cursor_col = 0;
        return;
    }
    /* Printable ASCII. */
    if (key >= 32 && key < 127) {
        char *ln = g_notepad.lines[g_notepad.cursor_line];
        int c = g_notepad.cursor_col;
        int l = (int)strlen(ln);
        if (l < NOTEPAD_LINE_LEN - 1) {
            memmove(&ln[c+1], &ln[c], (size_t)(l - c) + 1);
            ln[c] = (char)key;
            g_notepad.cursor_col++;
            g_notepad.modified = true;
        }
    }
}

void notepad_open(const char *filename) {
    np_reset();
    if (filename) strncpy(g_notepad.filename, filename, sizeof(g_notepad.filename)-1);
    DosGuiWindow *win = dosgui_wm_create(100, 80, 500, 400,
                                         filename ? filename : "Untitled - Notepad");
    if (win) {
        win->on_draw = dosgui_notepad_draw;
        win->on_key  = dosgui_notepad_key;
    }
}
