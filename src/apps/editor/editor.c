/*
 * editor.c  --  GUI adapter over the real WubuEditor engine
 *
 * Before this file was rewritten, "Editor" in the desktop registry launched
 * Notepad (dosgui_launch_editor -> dosgui_launch_notepad) and this adapter
 * was 22 lines of no-ops: EditorState { int dummy; }, editor_draw voiding
 * every parameter, editor_launch creating a window with NO callbacks bound
 * -- a blank window.  Meanwhile the real 468-LOC engine plus its find,
 * selection, undo, macro and bookmark modules were built into APP_OBJS and
 * tested only by test_apps2, with zero references from the desktop.
 *
 * This file is now the bridge: it owns a WubuEditor, renders its active tab
 * (line-number gutter, syntax-tinted text, caret, tab bar, status bar) and
 * routes keyboard/mouse into the engine's own editing primitives.
 *
 * C11, minimal includes, public DosGui window API only.
 */

#include "editor.h"
#include "../wubu_editor.h"
#include "../gui/dosgui_wm.h"
#include "../gui/dosgui_wm_internal.h"
#include "../gui/shell/dosgui_window_chrome.h"
#include "../kernel/vbe.h"
#include "../gui/wubu_theme.h"

#include <stdlib.h>
#include <string.h>

#define ED_LINE_H    16
#define ED_TABBAR_H  20
#define ED_STATBAR_H 16
#define ED_GUTTER_W  40
#define ED_CARET_W   2

/* -- Engine handle ------------------------------------------------- */

EditorState *editor_create(void) {
    return (EditorState *)wubu_ed_create();
}

void editor_destroy(EditorState *ed) {
    wubu_ed_destroy((WubuEditor *)ed);
}

static WubuEditor *ed_engine(EditorState *ed) {
    return (WubuEditor *)ed;
}

static WubuEdTab *ed_tab(WubuEditor *e) {
    if (!e || e->n_tabs <= 0) return NULL;
    if (e->active_tab < 0 || e->active_tab >= e->n_tabs) return NULL;
    return &e->tabs[e->active_tab];
}

/* -- Rendering ----------------------------------------------------- */

/* Map a syntax mode to a tint for the gutter / plain text. Full
 * syntax-coloured rendering belongs to the engine's highlighter; this
 * adapter keeps the visible surface readable without duplicating it. */
static uint32_t ed_syntax_color(WubuSyntax s) {
    switch (s) {
        case SYNTAX_C:        return 0x00000000;
        case SYNTAX_HOLYC:    return 0x00008000;
        case SYNTAX_PYTHON:   return 0x00800000;
        case SYNTAX_SHELL:    return 0x00404000;
        case SYNTAX_MAKEFILE: return 0x00008080;
        case SYNTAX_TOML:     return 0x00808000;
        case SYNTAX_MARKDOWN: return 0x00000080;
        case SYNTAX_JSON:     return 0x00800080;
        case SYNTAX_DIFF:     return 0x00804000;
        default:              return 0x00000000;
    }
}

void editor_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h,
                 EditorState *ed) {
    (void)fb; (void)fb_w; (void)fb_h;
    WubuEditor *e = ed_engine(ed);
    if (!win || !e) return;

    ChromeContentRect r = dosgui_chrome_draw_window(win, fb, fb_w, fb_h);
    int cx = r.x, cy = r.y, cw = r.w, ch = r.h;
    if (cw <= ED_GUTTER_W + 8 || ch <= ED_TABBAR_H + ED_STATBAR_H + 4) return;

    const WubuThemeColors *c = tc();
    WubuEdTab *t = ed_tab(e);

    /* Tab bar (only worth drawing once there is something to switch). */
    int tab_area_h = 0;
    if (e->n_tabs > 0) {
        tab_area_h = ED_TABBAR_H;
        const char *labels[WUBU_ED_MAX_TABS];
        for (int i = 0; i < e->n_tabs; i++) labels[i] = e->tabs[i].title;
        dosgui_chrome_draw_tabs(cx, cy, cw, tab_area_h, labels, e->n_tabs,
                                e->active_tab);
    }

    /* Editing surface. */
    int ex = cx, ey = cy + tab_area_h;
    int ew = cw, eh = ch - tab_area_h - ED_STATBAR_H;
    vbe_fill_rect(ex, ey, ew, eh, 0x00FFFFFF);
    vbe_rect(ex, ey, ew, eh, c->border_dark);

    /* Gutter. */
    vbe_fill_rect(ex, ey, ED_GUTTER_W, eh, c->win_face);
    vbe_rect(ex + ED_GUTTER_W, ey, 1, eh, c->border_dark);

    if (!t) {
        vbe_draw_text(ex + ED_GUTTER_W + 8, ey + 8,
                      "open a file or press Ctrl+N", c->btn_text, 1);
        return;
    }

    int max_lines = eh / ED_LINE_H;
    if (max_lines < 1) max_lines = 1;

    /* Keep the caret inside the viewport. */
    if (t->cursor_line < t->scroll_y) t->scroll_y = t->cursor_line;
    if (t->cursor_line >= t->scroll_y + max_lines)
        t->scroll_y = t->cursor_line - max_lines + 1;
    if (t->scroll_y < 0) t->scroll_y = 0;
    if (t->scroll_y > t->n_lines - 1) t->scroll_y = t->n_lines > 0 ? t->n_lines - 1 : 0;

    uint32_t text_col = ed_syntax_color(t->syntax);

    for (int i = 0; i < max_lines; i++) {
        int li = t->scroll_y + i;
        if (li >= t->n_lines) break;
        int ty = ey + i * ED_LINE_H + 2;

        /* Line number. */
        char num[16];
        snprintf(num, sizeof(num), "%5d", li + 1);
        vbe_draw_text(ex + 4, ty, num, c->border_dark, 1);

        /* Line text (clipped to the visible area). */
        const char *line = t->lines[li].text;
        int avail = (ew - ED_GUTTER_W - 12) / 8;
        if (avail < 1) avail = 1;
        int skip = t->scroll_x > 0 ? t->scroll_x : 0;
        int len = t->lines[li].len;
        if (skip < len) {
            int n = len - skip;
            if (n > avail) n = avail;
            char buf[512];
            if (n < (int)sizeof(buf)) {
                memcpy(buf, line + skip, (size_t)n);
                buf[n] = '\0';
                vbe_draw_text(ex + ED_GUTTER_W + 8, ty, buf, text_col, 1);
            }
        }
    }

    /* Caret. */
    if (t->cursor_line >= t->scroll_y &&
        t->cursor_line <  t->scroll_y + max_lines) {
        int row = t->cursor_line - t->scroll_y;
        int col = t->cursor_col - t->scroll_x;
        if (col >= 0 && col * 8 < ew - ED_GUTTER_W - 8) {
            int cxp = ex + ED_GUTTER_W + 8 + col * 8;
            int cyp = ey + row * ED_LINE_H + 2;
            vbe_fill_rect(cxp, cyp, ED_CARET_W, ED_LINE_H - 4, 0x00000000);
        }
    }

    /* Status bar: Ln/Col, filename, modified marker, syntax, encoding. */
    int sby = ey + eh;
    vbe_fill_rect(cx, sby, cw, ED_STATBAR_H, c->win_face);
    vbe_rect(cx, sby, cw, 1, c->border_dark);
    char status[160];
    snprintf(status, sizeof(status), " Ln %d, Col %d   %s%s   %s",
             t->cursor_line + 1, t->cursor_col + 1,
             t->filename[0] ? t->filename : "Untitled",
             t->modified ? " *" : "",
             t->syntax == SYNTAX_NONE ? "Plain" : "Syntax");
    vbe_draw_text(cx + 4, sby + 3, status, c->btn_text, 1);
}

/* -- Input -------------------------------------------------------- */

/* Window-relative mouse coordinates. */
void editor_mouse(DosGuiWindow *win, int x, int y, int btn, int kind) {
    WubuEditor *e = ed_engine((EditorState *)win->user_data);
    WubuEdTab *t = ed_tab(e);
    if (!win || !t) return;
    if (btn != 1 || kind != 1) return;   /* left press only */

    ChromeContentRect r = dosgui_chrome_draw_window(win, NULL, 0, 0);
    int lx = x - r.x - ED_GUTTER_W - 8;
    int ly = y - r.y - ED_TABBAR_H;
    int col = lx / 8 + (t->scroll_x > 0 ? t->scroll_x : 0);
    int line = ly / ED_LINE_H + t->scroll_y;
    if (col < 0) col = 0;
    if (line < 0) line = 0;
    if (line >= t->n_lines) line = t->n_lines > 0 ? t->n_lines - 1 : 0;

    int len = t->lines[line].len;
    t->cursor_line = line;
    t->cursor_col = col > len ? len : col;
}

void editor_key(DosGuiWindow *win, uint32_t key, uint32_t mods) {
    WubuEditor *e = ed_engine((EditorState *)win->user_data);
    WubuEdTab *t = ed_tab(e);
    if (!win || !e) return;

    /* Ctrl shortcuts first. */
    if (mods & 0x04) {
        switch (key) {
            case 'z': case 'Z': wubu_ed_undo(e); return;
            case 'y': case 'Y': wubu_ed_redo(e); return;
            case 's': case 'S': if (t) { wubu_ed_save(e); } return;
            case 'o': case 'O': if (t) { wubu_ed_save_as(e, t->filename); } return;
            default: break;
        }
    }

    if (!t) return;

    switch (key) {
        case 8:                       /* Backspace */
            wubu_ed_delete_char(e);
            return;
        case 127:                     /* Delete */
            wubu_ed_delete_forward(e);
            return;
        case '\r': case '\n':         /* Enter */
            wubu_ed_insert_newline(e);
            return;
        case 28:                      /* Left */
            if (t->cursor_col > 0) t->cursor_col--;
            return;
        case 29: {                    /* Right */
            int len = t->lines[t->cursor_line].len;
            if (t->cursor_col < len) t->cursor_col++;
            return;
        }
        case 30:                      /* Up */
            if (t->cursor_line > 0) t->cursor_line--;
            return;
        case 31:                      /* Down */
            if (t->cursor_line < t->n_lines - 1) t->cursor_line++;
            return;
        case '\t':
            for (int i = 0; i < t->tab_size; i++) wubu_ed_insert_char(e, ' ');
            return;
        default:
            break;
    }

    /* Printable ASCII. */
    if (key >= 32 && key < 127) {
        wubu_ed_insert_char(e, (char)key);
    }
}

/* -- Window binding ------------------------------------------------ */

static void editor_win_draw(DosGuiWindow *win, uint32_t *fb, int fb_w, int fb_h) {
    editor_draw(win, fb, fb_w, fb_h, (EditorState *)win->user_data);
}

DosGuiWindow *editor_launch(void) {
    EditorState *ed = editor_create();
    if (!ed) return NULL;

    DosGuiWindow *win = dosgui_wm_create(90, 70, 640, 480, "Editor");
    if (!win) {
        editor_destroy(ed);
        return NULL;
    }
    win->user_data = ed;
    win->on_draw   = editor_win_draw;
    win->on_key    = editor_key;
    win->on_mouse  = editor_mouse;

    /* Start with one empty tab so the surface is immediately editable. */
    wubu_ed_new_file(ed_engine(ed));
    return win;
}

void *editor_engine_of_window(DosGuiWindow *win) {
    return win ? win->user_data : NULL;
}
