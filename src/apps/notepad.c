/*
 * notepad.c — My Seed Notepad (Win98-style text editor)
 */
#include "notepad.h"
#include "../gui/wm.h"
#include "../kernel/vbe.h"
#include <string.h>

#define NOTEPAD_MAX_CHARS 65536

struct NotepadState {
    char    text[NOTEPAD_MAX_CHARS];
    int     text_len;
    int     cursor_pos;
    int     scroll_y;
    int     modified;
    char    filename[256];
};

static struct NotepadState g_notepad = {0};

static void notepad_draw(WmWindow *win, void *fb, int fb_w, int fb_h) {
    (void)fb; (void)fb_w; (void)fb_h;
    int x = win->x + 4, y = win->y + WM_TITLE_HEIGHT + 4;
    /* White text area */
    vbe_fill_rect(x, y, win->w - 8, win->h - WM_TITLE_HEIGHT - 8, 0x00FFFFFF);
}

static void notepad_handle_key(WmWindow *win, uint32_t key, uint32_t mods) {
    (void)win; (void)mods;
    if (key == 8 && g_notepad.cursor_pos > 0) {
        g_notepad.cursor_pos--;
        memmove(&g_notepad.text[g_notepad.cursor_pos],
                &g_notepad.text[g_notepad.cursor_pos + 1],
                g_notepad.text_len - g_notepad.cursor_pos);
        g_notepad.text_len--;
        g_notepad.modified = 1;
    } else if (key >= 32 && key < 127 && g_notepad.text_len < NOTEPAD_MAX_CHARS - 1) {
        g_notepad.text[g_notepad.cursor_pos] = key;
        g_notepad.cursor_pos++;
        g_notepad.text_len++;
        g_notepad.modified = 1;
    }
}

void notepad_open(const char *filename) {
    g_notepad.text[0] = '\0';
    g_notepad.text_len = 0;
    g_notepad.cursor_pos = 0;
    g_notepad.modified = 0;
    if (filename) strncpy(g_notepad.filename, filename, sizeof(g_notepad.filename)-1);
    
    WmWindow *win = wm_create_window(100, 80, 500, 400,
                                    filename ? filename : "Untitled - Notepad");
    if (win) {
        win->on_draw = notepad_draw;
        win->on_key = notepad_handle_key;
    }
}
