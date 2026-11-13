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
#include "../gui/wm.h"
#include "../gui/wm_internal.h"
#include "../gui/shell/window_chrome.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"
#include "guest_file.h"
#include <string.h>
#include <stdlib.h>

#define NOTEPAD_MAX_LINES  4096
#define NOTEPAD_LINE_LEN   256
#define NOTEPAD_MAX_CHARS  1048576

/* Inline prompt modes.  There is no general dialog system yet, so asking for a
 * filename happens in the status line: type a name, Enter confirms, Esc cancels.
 * This keeps open/save usable today instead of blocked on a dialog framework. */
typedef enum { NP_MODE_EDIT = 0, NP_MODE_OPEN, NP_MODE_SAVEAS } NpMode;

#define NOTEPAD_DEFAULT_NAME "untitled.txt"

typedef struct {
    char   lines[NOTEPAD_MAX_LINES][NOTEPAD_LINE_LEN];
    int    line_count;     /* number of lines actually used */
    int    cursor_line;    /* current line index */
    int    cursor_col;     /* column within the line */
    int    scroll_y;       /* first visible line */
    int    top_x;          /* horizontal scroll (chars) */
    bool   modified;
    char   filename[256];
    NpMode mode;
    char   prompt[256];
} NpBuf;

static NpBuf g_notepad;

static void np_reset(void) {
    memset(&g_notepad, 0, sizeof(g_notepad));
    g_notepad.line_count = 1;     /* always at least one (empty) line */
    g_notepad.lines[0][0] = '\0';
    g_notepad.cursor_line = 0;
    g_notepad.cursor_col  = 0;
}

/* Flatten to newline-separated text.  Returns the length, or -1 if the document
 * cannot fit in a single guest file -- refusing is mandatory here, because
 * silently truncating a save destroys the user's work. */
static int np_flatten(uint8_t *out, uint32_t cap) {
    uint32_t n = 0;
    for (int i = 0; i < g_notepad.line_count; i++) {
        int l = (int)strlen(g_notepad.lines[i]);
        if (n + (uint32_t)l + 1 > cap) return -1;
        memcpy(out + n, g_notepad.lines[i], (size_t)l);
        n += (uint32_t)l;
        out[n++] = '\n';
    }
    return (int)n;
}

/* Split newline-separated text into lines.  A trailing newline does not create
 * an extra empty line -- that artefact of the flatten format would otherwise
 * make every saved document gain a blank final row. */
static void np_unflatten(const uint8_t *in, uint32_t len) {
    int line = 0;
    uint32_t start = 0;
    for (uint32_t i = 0; i < len; i++) {
        if (in[i] != '\n') continue;
        if (line >= NOTEPAD_MAX_LINES) break;
        uint32_t l = i - start;
        if (l >= NOTEPAD_LINE_LEN) l = NOTEPAD_LINE_LEN - 1;
        memcpy(g_notepad.lines[line], in + start, l);
        g_notepad.lines[line][l] = '\0';
        line++;
        start = i + 1;
    }
    if (start < len && line < NOTEPAD_MAX_LINES) {
        uint32_t l = len - start;
        if (l >= NOTEPAD_LINE_LEN) l = NOTEPAD_LINE_LEN - 1;
        memcpy(g_notepad.lines[line], in + start, l);
        g_notepad.lines[line][l] = '\0';
        line++;
    }
    g_notepad.line_count = line > 0 ? line : 1;
}

bool notepad_save(const char *name) {
    /* NULL means "I have no preference"; an explicitly empty string is a
     * caller mistake and is refused rather than silently given a default. */
    if (!name) name = NOTEPAD_DEFAULT_NAME;
    if (!*name) return false;
    static uint8_t buf[GUEST_FILE_MAX];
    int n = np_flatten(buf, sizeof(buf));
    if (n < 0) return false;
    if (guest_file_write(name, buf, (uint32_t)n) != 0) return false;
    strncpy(g_notepad.filename, name, sizeof(g_notepad.filename) - 1);
    g_notepad.modified = false;
    return true;
}

bool notepad_load(const char *name) {
    if (!name || !*name) return false;
    static uint8_t buf[GUEST_FILE_MAX];
    uint32_t len = 0;
    if (guest_file_read(name, buf, sizeof(buf), &len) != 0) return false;
    np_reset();
    np_unflatten(buf, len);
    strncpy(g_notepad.filename, name, sizeof(g_notepad.filename) - 1);
    g_notepad.cursor_line = 0;
    g_notepad.cursor_col = 0;
    g_notepad.scroll_y = 0;
    return true;
}

static void np_prompt_key(uint32_t key) {
    size_t n = strlen(g_notepad.prompt);
    if (key == 27) { g_notepad.mode = NP_MODE_EDIT; return; }
    if (key == '\r' || key == '\n') {
        char name[sizeof(g_notepad.prompt)];
        memcpy(name, g_notepad.prompt, sizeof(name));
        NpMode m = g_notepad.mode;
        g_notepad.mode = NP_MODE_EDIT;
        if (!*name) return;
        if (m == NP_MODE_OPEN) notepad_load(name);
        else                   notepad_save(name);
        return;
    }
    if (key == 8) { if (n > 0) g_notepad.prompt[n - 1] = '\0'; return; }
    if (key >= 32 && key < 127 && n < sizeof(g_notepad.prompt) - 1) {
        g_notepad.prompt[n] = (char)key;
        g_notepad.prompt[n + 1] = '\0';
    }
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
    char status[320];
    if (g_notepad.mode == NP_MODE_OPEN) {
        snprintf(status, sizeof(status), " Open: %s", g_notepad.prompt);
    } else if (g_notepad.mode == NP_MODE_SAVEAS) {
        snprintf(status, sizeof(status), " Save as: %s", g_notepad.prompt);
    } else {
        snprintf(status, sizeof(status), " Ln %d, Col %d   %s%s",
                 g_notepad.cursor_line + 1, g_notepad.cursor_col + 1,
                 g_notepad.filename[0] ? g_notepad.filename : "Untitled",
                 g_notepad.modified ? " *" : "");
    }
    vbe_draw_text(cx + 4, sby + 3, status, tc()->btn_text, 1);
}

void notepad_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    if (!win) return;

    /* Draw chrome (frame + title bar + buttons) and get content rect. */
    ChromeContentRect content = chrome_draw_window(win, fb, fb_w, fb_h);

    /* Draw notepad content within chrome-provided rect. */
    notepad_draw_content(win, &content);
}

void notepad_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    (void)win;
    if (key == 0) return;

    /* A prompt swallows ordinary editing keys until it is confirmed or
     * cancelled, otherwise typing a filename would also edit the document. */
    if (g_notepad.mode != NP_MODE_EDIT) { np_prompt_key(key); return; }

    bool ctrl  = (mods & 0x04) != 0;
    bool shift = (mods & 0x01) != 0;
    if (ctrl && (key == 's' || key == 'S')) {
        if (shift || !g_notepad.filename[0]) {
            g_notepad.mode = NP_MODE_SAVEAS;
            g_notepad.prompt[0] = '\0';
            if (g_notepad.filename[0])
                strncpy(g_notepad.prompt, g_notepad.filename,
                        sizeof(g_notepad.prompt) - 1);
        } else {
            notepad_save(g_notepad.filename);
        }
        return;
    }
    if (ctrl && (key == 'o' || key == 'O')) {
        g_notepad.mode = NP_MODE_OPEN;
        strncpy(g_notepad.prompt, g_notepad.filename,
                sizeof(g_notepad.prompt) - 1);
        return;
    }

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

/* Click-to-position: the same geometry notepad_draw_content renders with
 * (content rect, 16px line height, 4px left padding, 8px cell advance), so
 * the caret lands where the user actually clicked. */
void notepad_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    if (btn != 1 || kind != 1) return;   /* left press only */
    if (!win) return;

    ChromeContentRect r = chrome_draw_window(win, NULL, 0, 0);
    int line = (y - r.y - 2) / 16 + g_notepad.scroll_y;
    int col  = (x - r.x - 4) / 8 + g_notepad.top_x;

    if (line < 0) line = 0;
    if (line >= g_notepad.line_count) line = g_notepad.line_count - 1;
    if (col < 0) col = 0;

    int len = g_notepad.lines[line] ? (int)strlen(g_notepad.lines[line]) : 0;
    if (col > len) col = len;

    g_notepad.cursor_line = line;
    g_notepad.cursor_col  = col;
}

/* -- Inspection accessors ------------------------------------------ */

void notepad_reset(void) { np_reset(); }

int notepad_line_count(void) { return g_notepad.line_count; }

const char *notepad_line_text(int index) {
    if (index < 0 || index >= g_notepad.line_count) return NULL;
    return g_notepad.lines[index];
}

int notepad_cursor_line(void) { return g_notepad.cursor_line; }
int notepad_cursor_col(void)  { return g_notepad.cursor_col; }
bool notepad_modified(void)   { return g_notepad.modified; }
const char *notepad_filename(void) { return g_notepad.filename; }

DosGuiWindow *notepad_launch(void) {
    return notepad_open(NULL);
}

DosGuiWindow *notepad_open(const char *filename) {
    np_reset();

    /* Opening a name means opening its CONTENTS.  The previous version only
     * copied the name into the title bar, so double-clicking a document (or the
     * Run dialog handing over a report) showed a window titled after the file
     * with an empty buffer -- the bytes were on disk and nobody saw them. */
    if (filename && !notepad_load(filename)) {
        /* Not on disk yet: still open under that name so the first Ctrl+S
         * creates it, exactly like Windows does for a new file. */
        strncpy(g_notepad.filename, filename, sizeof(g_notepad.filename) - 1);
    }

    DosGuiWindow *win = wm_create(100, 80, 500, 400,
                                         filename ? filename : "Untitled - Notepad");
    if (win) {
        win->on_draw  = notepad_draw;
        win->on_key   = notepad_key;
        win->on_mouse = notepad_mouse;
    }
    return win;
}
